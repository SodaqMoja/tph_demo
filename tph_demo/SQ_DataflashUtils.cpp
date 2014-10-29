#include <Arduino.h>
#include <Sodaq_dataflash.h>
#include "SQ_Diag.h"
#include "SQ_DataflashUtils.h"

#include "pindefs.h"
#include "DataRecord.h"

int curPage;
static int curByte;
int uploadPage;


/*
 * Initialize the dataflash, find curPage and uploadPage
 */
void initializeDataflash(uint32_t ts)
{
  // We need the current timestamp as a pseudo random value
  findCurAndUploadPage(&curPage, &uploadPage, ts);
  DIAGPRINT(F("uploadPage:")); DIAGPRINTLN(uploadPage);
  DIAGPRINT(F("curPage:")); DIAGPRINTLN(curPage);
  if (uploadPage >= 0 && uploadPage == curPage) {
    // Data flash is totally filled up. Forget about oldest upload page
    uploadPage = getNextPage(uploadPage);
    DIAGPRINT(F(" > uploadPage:")); DIAGPRINTLN(uploadPage);
    // No need to verify validity
  }
  initNewPage(curPage, ts);
}

/*
 * \brief Read a whole page into a buffer
 *
 * Buffer_Read_Str returns corrupted data if we're trying
 * to read the whole page at once. Here we do it in chunks
 * of 16 bytes.
 */
void readPage(int page, uint8_t *buffer, unsigned int size)
{
  dflash.readPageToBuf1(page);
  // Read in chunks of max 16 bytes
  int byte_offset = 0;
  while (size > 0) {
    int size1 = size >= 16 ? 16 : size;
    dflash.readStrBuf1(byte_offset, buffer, size1);
    byte_offset += size1;
    buffer += size1;
    size -= size1;
  }
}

/*
 * \brief Read the page header
 */
bool readPageHeader(int page, PageHeader_t *hdr)
{
  size_t byte_offset = 0;
  size_t size = sizeof(PageHeader_t);

  dflash.readPageToBuf1(page);
  uint8_t *buffer = (uint8_t *)hdr;
  while (size > 0) {
    int size1 = size >= 16 ? 16 : size;
    dflash.readStrBuf1(byte_offset, buffer, size1);
    byte_offset += size1;
    buffer += size1;
    size -= size1;
  }
  return isValidHeader(hdr);
}

/*
 * \brief Read one record from the page
 */
bool readPageNthRecord(int page, uint8_t nth, DataRecord_t *rec)
{
  size_t byte_offset = sizeof(PageHeader_t) + nth * sizeof(DataRecord_t);
  size_t size = sizeof(DataRecord_t);
  if ((byte_offset + size) > DF_PAGE_SIZE) {
    // The record is crossing page boundary
    clearRecord(rec);
    return false;
  }

  dflash.readPageToBuf1(page);
  uint8_t *buffer = (uint8_t *)rec;
  while (size > 0) {
    int size1 = size >= 16 ? 16 : size;
    dflash.readStrBuf1(byte_offset, buffer, size1);
    byte_offset += size1;
    buffer += size1;
    size -= size1;
  }
  //dumpBuffer((uint8_t *)rec, sizeof(*rec));
  return rec->isValidRecord();
}

/*
 * \brief Is this a valid page header
 */
bool isValidHeader(PageHeader_t *hdr)
{
  if (strncmp(hdr->magic, HEADER_MAGIC, sizeof(HEADER_MAGIC)) != 0) {
    return false;
  }
  if (hdr->version != DATA_VERSION) {
    return false;
  }
  // The timestamp should be OK too. How can it be bad?
  return true;
}

/*
 * \brief Is this a valid page to upload
 *
 * Note. This invalidates the "internal buffer" of the Data Flash
 */
bool isValidUploadPage(int page)
{
  PageHeader_t hdr;
  readPageHeader(page, &hdr);

  return isValidHeader(&hdr);
}

/*
 * \brief Search for curPage and uploadPage in the data flash
 *
 * Search through the whole date flash and try to find the best page for
 * curPage and for the uploadPage.
 */
void findCurAndUploadPage(int *curPage, int *uploadPage, uint16_t randomNum)
{
#if ENABLE_DIAG
  uint32_t start = millis();
#endif

  int myCurPage = -1;
  int myUploadPage = -1;
  uint32_t uploadTs;
  PageHeader_t hdr;

  // First round, search for upload page
  for (int page = 0; page < DF_NR_PAGES; ++page) {
    readPage(page, (uint8_t*)&hdr, sizeof(hdr));

    if (isValidHeader(&hdr)) {
      DIAGPRINT(F("Valid page: ")); DIAGPRINTLN(page);
      if (myUploadPage < 0) {
        myUploadPage = page;
        uploadTs = hdr.ts;
      } else {
        // Make sure we remember the oldest upload record
        if (hdr.ts < uploadTs) {
          myUploadPage = page;
          uploadTs = hdr.ts;
        }
      }
    }
  }

  if (myUploadPage >= 0) {
    // Starting from upload page, look for the next free spot.
    // TODO Verify this logic
    int page = myUploadPage;
    for (int nr = 0; nr < DF_NR_PAGES; ++nr, page = getNextPage(page)) {
      readPage(page, (uint8_t*)&hdr, sizeof(hdr));
      if (!isValidHeader(&hdr)) {
        myCurPage = page;
        break;
      }
    }
    if (myCurPage < 0) {
      // None of the pages is empty.
      // Use the page of the oldest upload, the caller will take care of this
      myCurPage = myUploadPage;
    }
  } else {
    // No upload page found.
    // Start at a random place
    myCurPage = randomNum % DF_NR_PAGES;
    myUploadPage = -1;
  }

  *curPage = myCurPage;
  *uploadPage = myUploadPage;

#if ENABLE_DIAG
  uint32_t elapse = millis() - start;
  DIAGPRINT(F("Find uploadPage in (ms) ")); DIAGPRINTLN(elapse);
#endif
}

uint32_t getPageTS(int page)
{
  if (page < 0) {
    return -1;
  }
  PageHeader_t hdr;
  readPage(page, (uint8_t*)&hdr, sizeof(hdr));
  return hdr.ts;
}

void initNewPage(int page, uint32_t ts)
{
  //DIAGPRINT(F("initNewPage:")); DIAGPRINTLN(page);
  curByte = 0;
  // Write new header
  PageHeader_t hdr;
  hdr.ts = ts;
  strncpy(hdr.magic, HEADER_MAGIC, sizeof(hdr.magic));
  hdr.version = DATA_VERSION;

  dflash.writeStrBuf1(curByte, (uint8_t *)&hdr, sizeof(hdr));
  curByte += sizeof(hdr);

  for (int b = curByte; b < DF_PAGE_SIZE; ++b) {
    dflash.writeByteBuf1(b, 0xff);
  }

  // Write flash internal buffer to the actual flash memory
  // The reason is that we don't want to loose the info if the software crashes
  dflash.writeBuf1ToPage(curPage);
}

/*
 * \brief Set curPage to the next page and go to the next
 *
 * This function also takes care of making sure we always have
 * an empty page between curPage and uploadPage.
 * If fact uploadPage must be at least 2 ahead because the
 * next page for curPage is going to be filled in shortly after
 * this.
 */
void newCurPage(uint32_t ts)
{
  curPage = getNextPage(curPage);
  DIAGPRINT(F("> New curPage:")); DIAGPRINTLN(curPage);
  int pageAfterCurPage = getNextPage(curPage);
  if (isValidUploadPage(pageAfterCurPage)) {
    // This triggers when the flash is completely full. It will
    // be very rare, but still...

    if (uploadPage == pageAfterCurPage) {
      // Shift uploadPage
      uploadPage = getNextPage(uploadPage);
      if (!isValidUploadPage(uploadPage)) {
        // Not likely, but better be safe.
        uploadPage = -1;
      }
      DIAGPRINT(F(" >> new uploadPage:")); DIAGPRINTLN(uploadPage);
    }

    DIAGPRINT(F(" >> Erase page after:")); DIAGPRINTLN(pageAfterCurPage);
    erasePage(pageAfterCurPage);
  }

  initNewPage(curPage, ts);
}

/*
 * \brief Erase and Reset the current page
 */
void erasePage(int page)
{
  //DIAGPRINT(F("erasePage:")); DIAGPRINTLN(page);
  if (page < 0) {
    return;
  }
  dflash.pageErase(page);
}

/*
 * \brief Add one record to the current page
 *
 * First it checks if there is enough space in the current
 * flash page. If not the page is "closed" and a new page
 * is prepared for the addition.
 */
void addCurPageRecord(DataRecord_t *rec, uint32_t ts)
{
  //DIAGPRINT(F("addCurPageRecord:")); DIAGPRINTLN(curPage);
  //DIAGPRINT(F(" curByte:")); DIAGPRINTLN(curByte);
  // Is there enough room for a new record in the current page?
  if (curPage < 0 || curByte >= (int)(DF_PAGE_SIZE - sizeof(*rec))) {
    // No, so start using the next page
    //DIAGPRINT(F("addCurPageRecord:")); DIAGPRINTLN(curPage);
    //dumpPage(curPage);
    newCurPage(ts);
  }

  // Write the record to the page
  dflash.writeStrBuf1(curByte, (unsigned char *)rec, sizeof(*rec));
  curByte += sizeof(*rec);

  // Write flash internal buffer to the actual flash memory
  // The reason is that we don't want to loose the info if the software crashes
  dflash.writeBuf1ToPage(curPage);

  if (uploadPage < 0) {
    // Remember this as the first page to upload
    uploadPage = curPage;
  }
}

void restoreCurPage()
{
  dflash.readPageToBuf1(curPage);
}

#ifdef ENABLE_DIAG

/*
 * \brief This function reads all pages in the data flash and measures how long
 * it takes.
 *
 * This function is just meant for diagnostics.
 */
void readAllPages()
{
#if 0
  uint32_t start = millis();
#endif

  for (uint16_t page = 0; page < DF_NR_PAGES; page++) {
    PageHeader_t hdr;
    readPage(page, (uint8_t*)&hdr, sizeof(hdr));
  }

#if 0
  uint32_t elapse = millis() - start;
  DIAGPRINT(F("Nr secs to read all pages: ")); DIAGPRINTLN(elapse / 1000);
#endif
}

/*
 * \brief Dump the contents of a data flash page
 */
void dumpPage(int page)
{
  if (page < 0)
    return;

  DIAGPRINT(F("page ")); DIAGPRINTLN(page);
  dflash.readPageToBuf1(page);
  uint8_t buffer[16];
  for (uint16_t i = 0; i < DF_PAGE_SIZE; i += sizeof(buffer)) {
    size_t nr = sizeof(buffer);
    if ((i + nr) > DF_PAGE_SIZE) {
      nr = DF_PAGE_SIZE - i;
    }
    dflash.readStrBuf1(i, buffer, nr);

    dumpBuffer(buffer, nr);
  }
}

#endif

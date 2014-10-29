/*
 * Utility software to upload data records to an FTP server.
 */

#include <string.h>
#include <avr/wdt.h>

#include <GPRSbee.h>
#include "SQ_Diag.h"
#include "SQ_DataflashUtils.h"

#include "SQ_UploadPages.h"

/*
 * The path of the FTP account
 *
 * At the moment we only support a simple, straight "root" path.
 */
#define FTPPATH "/"

static bool addPageHeaderToFTP(int page);
static int addOnePageToFTP(int page);
static void addRecToString(const DataRecord_t & rec, String & str);
static void erasePages(size_t nr_pages_sent);

static void diagPrintlnFailed()
{
  DIAGPRINTLN(F(" - failed"));
}

/*
 * \brief Upload all available page
 *
 * Please make sure that there is enough battery power.
 */
#define MAX_NR_PAGES_SENT 200
#define MAX_NR_RECORDS_SENT 400
bool uploadPages(const char *filename, const ConfigParms & parms)
{
  size_t nr_pages_sent = 0;
  size_t nr_recs_sent = 0;
  bool retval = false;  // Assume the worst
  int page;
  int nrPageRecs;

  DIAGPRINT(F("uploadPages: ")); DIAGPRINTLN(uploadPage);
  if (uploadPage < 0) {
    return true;
  }

  if (!gprsbee.openFTP(parms.getAPN(), parms.getFTPserver(), parms.getFTPuser(), parms.getFTPpassword())) {
    DIAGPRINT(F("openFTP")); diagPrintlnFailed();
    goto end;
  }

  // Open up the FTP session
  if (!gprsbee.openFTPfile(filename, FTPPATH)) {
    DIAGPRINT(F("openFTPfile")); diagPrintlnFailed();
    goto close;
  }

  if (!isValidUploadPage(uploadPage)) {
    // This is nasty. An invalid page. How can this happen?
    DIAGPRINTLN(F("uploadPages: - INVALID PAGE!"));

    // Erase this page, otherwise nothing gets done anymore.
    erasePages(1);
    goto close_file;
  }

  if (!addPageHeaderToFTP(uploadPage)) {
    // Upload failed, somehow
    goto close_file;
  }
  // Append the records from the pages into the FTP session
  page = uploadPage;
  do {
    if ((nrPageRecs = addOnePageToFTP(page)) < 0) {
      // Upload failed, somehow
      goto close_file;
    }
    nr_recs_sent += nrPageRecs;
    if (nr_recs_sent >= MAX_NR_RECORDS_SENT) {
      break;
    }
    if (++nr_pages_sent >= MAX_NR_PAGES_SENT) {
      break;
    }

    page = getNextPage(page);
    if (page == curPage || !isValidUploadPage(page)) {
      page = -1;
    }
  } while (page >= 0);

  // Getting here we know that the upload was successful
  retval = true;

close_file:
  // Close the FTP session
  if (!gprsbee.closeFTPfile()) {
    DIAGPRINT(F("closeFTPfile")); diagPrintlnFailed();
    // Continue with closeFTP which will switch off the SIM900
    goto close;
  }

  if (retval) {
    erasePages(nr_pages_sent);
  }

close:
  if (!gprsbee.closeFTP()) {
    DIAGPRINT(F("closeFTP")); diagPrintlnFailed();
  }

end:
  // Restore dataflash Buf1
  restoreCurPage();
  if (!retval) {
    DIAGPRINT(F("uploadPages")); diagPrintlnFailed();
  }
  return retval;
}

/*
 * Erase the pages that were sent successfully
 *
 * Mark the successful sent pages invalid/free (or at least mark it as uploaded)
 * so that they will not be sent anymore.
 */
static void erasePages(size_t nr_pages_sent)
{
  for (size_t i = 0; i < nr_pages_sent; ++i) {
    wdt_reset();
    erasePage(uploadPage);
    uploadPage = getNextPage(uploadPage);

    // Just in case we hit the situation that all valid page were sent.
    if (!isValidUploadPage(uploadPage)) {
      uploadPage = -1;
      break;
    }
  }
}

static const char *linePtr;
uint8_t readNextByte2()
{
  return *linePtr++;
}
static bool addLineToFTP(String & str)
{
  str += '\r';
  str += '\n';
  size_t len = str.length();
  linePtr = str.c_str();
  if (!gprsbee.sendFTPdata(readNextByte2, len)) {
    // An error.
    DIAGPRINT(F("addLineToFTP")); diagPrintlnFailed();
    return false;
  }
  return true;
}

/*
 * \brief Upload a CSV header with the field names
 */
static bool addPageHeaderToFTP(int page)
{
  String str;
  DataRecord_t::addHeaderToString(str);
  return addLineToFTP(str);
}

static void addRecToString(const DataRecord_t & rec, String & str)
{
  rec.addToString(str);
  str += '\r';
  str += '\n';
}

static size_t getRecLength(const DataRecord_t & rec)
{
  String str;
  addRecToString(rec, str);
  return str.length();
}

/*
 * \brief Upload a single page, all the records in it
 */
static int uPage;
static DataRecord_t * uRec;
static uint8_t uRecIx;
static String *uStr;
static size_t uStrIx;
uint8_t readNextByte()
{
  if (!uStr) {
    return 0;
  }
  while (uStrIx >= uStr->length()) {
    // Read next record into string
    *uStr = "";
    uStrIx = 0;
    wdt_reset();
    readPageNthRecord(uPage, uRecIx++, uRec);           // No room for failure
    addRecToString(*uRec, *uStr);
  }
  return (*uStr)[uStrIx++];
}

static int addOnePageToFTP(int page)
{
  if (page < 0) {
    return -1;
  }

  DIAGPRINT(F("addOnePageToFTP: ")); DIAGPRINTLN(page);
  //dumpPage(page);

  DataRecord_t rec;

  // Find out length
  size_t len = 0;
  int nrRecs = 0;
  for (uint8_t i = 0; i < NR_RECORDS_PER_PAGE; ++i) {
    wdt_reset();
    if (!readPageNthRecord(page, i, &rec)) {
      break;
    }
    len += getRecLength(rec);
    ++nrRecs;
  }

  // Send the whole page
  String str;
  uStr = &str;
  uStrIx = 0;
  uPage = page;
  uRec = &rec;
  uRecIx = 0;
  //DIAGPRINT(F("addOnePageToFTP: len=")); DIAGPRINTLN(len);
  if (len > 0 && !gprsbee.sendFTPdata(readNextByte, len)) {
    // An error.
    DIAGPRINT(F("addOneRecToFTP")); diagPrintlnFailed();
    return -1;
  }

  //DIAGPRINTLN(F("addOnePageToFTP - end"));

  // All went well.
  return nrRecs;
}

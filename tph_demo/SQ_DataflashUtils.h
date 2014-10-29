/*
 * MyDataflash.h
 *
 * This module is the interface between the application
 * and Sodaq_dataflash. It keeps track of the current
 * page to write (curPage) and the first page for uploading
 * (uploadPage).
 *
 * There are also several utility function such as nextPage
 * which makes the dataflash a sort of ring buffer.
 */
#ifndef SQ_DATAFLASHUTILS_H_
#define SQ_DATAFLASHUTILS_H_


#include <Sodaq_dataflash.h>
#include "SQ_Diag.h"

#include "DataRecord.h"


struct PageHeader_t
{
  uint32_t      ts;
  uint32_t      version;
  char          magic[6];
};
typedef struct PageHeader_t PageHeader_t;


#define NR_RECORDS_PER_PAGE     ((DF_PAGE_SIZE - sizeof(PageHeader_t)) / sizeof(DataRecord_t))

extern int curPage;
extern int uploadPage;

static inline int getNextPage(int page)
{
  page++;
  if (page >= DF_NR_PAGES) {
    page = 0;
  }
  return page;
}

void initializeDataflash(uint32_t ts);
bool isValidHeader(PageHeader_t *hdr);
bool isValidUploadPage(int page);
void findCurAndUploadPage(int *curPage, int *uploadPage, uint16_t randomNum);
uint32_t getPageTS(int page);
void readPage(int page, uint8_t *buffer, unsigned int size);
bool readPageNthRecord(int page, uint8_t nth, DataRecord_t *rec);
bool readPageHeader(int page, PageHeader_t *hdr);

void initNewPage(int page, uint32_t ts);
void erasePage(int page);
void newCurPage(uint32_t ts);
void addCurPageRecord(DataRecord_t *rec, uint32_t ts);

void restoreCurPage();

#ifdef ENABLE_DIAG
void readAllPages();
void dumpPage(int page);
#else
#define readAllPages()
#define dumpPage(page)
#endif

#endif /* SQ_DATAFLASHUTILS_H_ */

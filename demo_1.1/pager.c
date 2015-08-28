/*
** This is the implementation of the page cache subsystem or "pager".
*/
#include "pager.h"
#include "pcache.h"
#include "os.h"

/*
Each open database file is controlled by an instance of Pager structure.
*/
struct Pager {
  SqlVFS *pvfs;          // VFS used to open database file.
  char *filename;        // Database file name.
  SqlFile *fd;           // File descriptor of database file.
 
  PCache* pcache;        // Pointor to cache module.
};


// Open a Pager connection. 
int pagerOpen(
  SqlVFS *pvfs,                    // The VFS to use to open the file
  Pager **pppager,                 // OUT: return the Pager structure here
  const char* filename,            // Name of the database file to open
  int sz_extra,                    // Extra bytes append to each in-memory page
  int flags                        // flags controlling this file
  ) {
  Pager *ppager = 0;
  void *ptr = 0;
  int sz_filename = 0;
  int sz_pcache = pcacheSize();

  if (filename && filename[0]) {
    sz_filename = strlen(filename);
  } else {
    return SQL_ERROR;
  }

  ptr = malloc(
    sizeof(Pager) +
    sz_filename + 1 +    // append a '\0' to filename
    pvfs->sz_file +
    sz_pcache
    );

  ppager = (Pager *)ptr;
  ppager->filename = (char *)(ptr += sizeof(Pager));
  ppager->fd = (SqlFile *)(ptr += (sz_filename + 1));
  ppager->pcache = (PCache *)(ptr += pvfs->sz_file);

  // copy database filename to pager->filename
  assert(filename && filename[0]);
  memcpy(ppager->filename, filename, sz_filename);
  ppager->filename[sz_filename] = 0;

  // open database file
  if (osOpen(pvfs, ppager->filename, ppager->fd, flags) == SQL_ERROR) {
    // do nothing now.
    return SQL_ERROR;
  }

  // create PCache module
  if (pcacheOpen(SQL_DEFAULT_PAGE_SIZE, sz_extra, 
        (PCache *)ppager->pcache) == SQL_ERROR)
    return SQL_ERROR;

  *pppager = ppager;
  return SQL_OK;
};


// Functions used to obtain and release page references.
int pagerGet(Pager *pager, Pgno pgno, DbPage **pppage) {
  DbPage *pdb = pcacheGet(pager->pcache, pgno);
  if (pdb) {
    ++pdb->nref;
    *pppage = pdb;
    return SQL_OK;
  }

  pdb = pcacheFetch(pager->pcache, pgno);
  pdb->pgno = pgno;
  pdb->pager = pager;
  pdb->nref = 1;
  pdb->flags = PGHDR_CLEAN;
  pdb->pdirty_next = 0;
  pdb->pdirty_prev = 0;

  osRead(pager->fd, pdb->pdata, SQL_DEFAULT_PAGE_SIZE, 
          SQL_DATABASE_HEADER_SIZE + (pgno - 1) * SQL_DEFAULT_PAGE_SIZE);

  *pppage = pdb;

  return SQL_ERROR;
}

// Make a page writable.
int pagerWrite(DbPage* page) {
  pcacheMakeDirty(page->pager->pcache, page);

  return SQL_OK;
}

// Write the dirty page to database file.
static void _pagerWritePage(PgHdr *pdirty) {
  void *pdata = pdirty->pdata;
  int offset = SQL_DATABASE_HEADER_SIZE + 
                (pdirty->pgno - 1) * SQL_DEFAULT_PAGE_SIZE;
  int sz = SQL_DEFAULT_PAGE_SIZE;

  int ret = osWrite(pdirty->pager->fd, pdata, sz, offset);
  printf("%d %d\n", SQL_OK, ret);
  if (SQL_OK != ret) {
    // do nothing now
  }
}

// Sync database file for pager.
int pagerCommit(Pager *pager) {
  PCache *pcache = pager->pcache;
  PgHdr *pdirty = pcacheGetDirty(pcache);

  while (pdirty) {
    PgHdr *next = pdirty->pdirty_next;

    _pagerWritePage(pdirty);
    pcacheMakeClean(pcache, pdirty);
  
    pdirty = next;
  }

  return SQL_OK;
}
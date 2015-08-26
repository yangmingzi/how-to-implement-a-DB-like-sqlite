/*
** This is the implementation of the page cache subsystem or "pager".
*/
#include "pager.h"
#include "pcache.h"

struct Pager {
  SqlVFS *pvfs;          // VFS used to open database file
  char *filename;
  SqlFile *fd;           // file descriptor of database file

  PCache* pcache;        // pointor to cache module
};


// Open and close a Pager connection. 
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
    sz_filename + 1 +
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
    *pppage = pdb;
    return SQL_OK;
  }

  printf("%p\n", pdb);
  pdb = pcacheFetch(pager->pcache, pgno);
  printf("%p\n", pdb);

  DbPage *ppp = pcacheGet(pager->pcache, pgno);
  printf("--> %p\n", ppp);
  /*
    do read-op
  */

  return SQL_ERROR;
}

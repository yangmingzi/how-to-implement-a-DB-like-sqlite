#ifndef _PCACHE_H_
#define _PCACHE_H_

#include "pager.h"

typedef struct PgHdr PgHdr;
typedef struct PCache PCache;

/*
Every page in the cache is controlled by an instance of PgHdr.
*/
struct PgHdr {
  void *pdata;                // Page data
  void *pextra;               // Extra data
  Pager *pager;               // The pager controlling this PgHdr
  Pgno pgno;                  // Page number
  int nref;                   // Number of reference to this page
  int flags;                  // Page flags defined below.

  PgHdr *pdirty_next;         // Next dirty page.
  PgHdr *pdirty_prev;         // Previous dirty page.
};
#define PGHDR_CLEAN  0x001
#define PGHDR_DIRTY  0x002

// Get the size of PCache structure.
int pcacheSize();

// Open a PCache structure.
int pcacheOpen(
  int sz_page,            // Size of each page
  int sz_extra,           // Size of extra space of each page
  PCache *pcache          // Preallocated space for the PCache
  );

// Get a page by page number.
// If the page is not in cache, return NULL
PgHdr *pcacheGet(PCache *pcache, Pgno pgno);

// Get a page by page number.
// If the page is not in cache, create a new one and return it.
PgHdr *pcacheFetch(PCache *pcache, Pgno pgno);


// Make the page dirty.
void pcacheMakeDirty(PCache *pcache, PgHdr *p);

// Make the page clean
void pcacheMakeClean(PCache *pcache, PgHdr *p);

// Get the first dirty page
PgHdr *pcacheGetDirty(PCache *pcache);

#endif
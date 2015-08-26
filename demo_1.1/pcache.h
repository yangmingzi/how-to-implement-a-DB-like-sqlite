#ifndef _PCACHE_H_
#define _PCACHE_H_

#include "pager.h"

typedef struct PgHdr PgHdr;
typedef struct PCache PCache;


struct PgHdr {
  void *pdata;
  void *pextra;
  PgHdr *pdirty;
  Pager *pager;
  Pgno pgno;
  int nref;
  PCache *pcache;

  PgHdr *pdirty_next;
  PgHdr *pdirty_prev;
};

int pcacheSize();

int pcacheOpen(
  int sz_page,            // Size of each page
  int sz_extra,           // Size of extra space of each page
  PCache *pcache          // Preallocated space for the PCache
  );

PgHdr *pcacheGet(PCache *pcache, Pgno pgno);
PgHdr *pcacheFetch(PCache *pcache, Pgno pgno);

#endif
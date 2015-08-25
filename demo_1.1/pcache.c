#include "sqlite.h"
#include "sqliteint.h"
#include "pcache.h"

struct PCache {
  PgHdr *pdirty, *pdirty_tail;
  int nref;
  int sz_page;
  int sz_extra;

  SqlPCache *pcache;
};

int pcacheSize() {
  return sizeof(PCache);
}

int pcacheOpen(
  int sz_page,            // Size of each page
  int sz_extra,           // Size of extra space of each page
  PCache *pcache          // Preallocated space for the PCache
  ) {
  memset(pcache, 0, sizeof(PCache));
  pcache->sz_page = 1;
  pcache->sz_extra = sz_extra;
  return pcacheSetPageSize(pcache, sz_page);
}

int pcacheSetPageSize(PCache *pcache, int sz_page) {
  assert(pcache->nref == 0 && pcache->pdirty == 0);
  if (pcache->sz_page) {
    SqlPCache *pnew = 0;
    global_config.sql_pcache_methods.xCreate(1, 2);
    if (pnew == 0) return SQL_ERROR;

    pcache->pcache = pnew;
    pcache->sz_page = sz_page;
  }
}
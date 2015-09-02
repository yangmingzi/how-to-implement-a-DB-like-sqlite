#include "sqlite.h"
#include "sqliteint.h"
#include "pcache.h"

/*
Core cache module.
*/
struct PCache {
  PgHdr *pdirty_head, *pdirty_tail;  // Header pointer and tail pointer of dirty page list.
  int nref;                          // Number of reference to pages in this cache module.
  int sz_page;                       // Size of each page.
  int sz_extra;                      // Size of extra space in each page.

  SqlPCache *pcache;                 // Pluginble cache module.
};

// Get the size of PCache structure.
int pcacheSize() {
  return sizeof(PCache);
}

// Create plugin pcache module and set its page size.
static int _pcacheSetPageSize(PCache *pcache, int sz_page) {
  assert(pcache->nref == 0 && pcache->pdirty_head == 0);
  if (pcache->sz_page) {
    SqlPCache *pnew = 0;
    pnew = global_config.sql_pcache_methods.xCreate(
      sz_page,                 // Page size.
      sizeof(PgHdr),           // Extra content.
      MAX_PAGES_IN_CACHE);     // Maxinum number of cached pages.
    if (pnew == 0) return SQL_ERROR;

    pcache->pcache = pnew;
    pcache->sz_page = sz_page;
  }
}

// Open a cache module.
int pcacheOpen(
  int sz_page,            // Size of each page
  int sz_extra,           // Size of extra space of each page
  PCache *pcache          // Preallocated space for the PCache
  ) {
  memset(pcache, 0, sizeof(PCache));
  pcache->sz_page = 1;
  pcache->sz_extra = sz_extra;
  return _pcacheSetPageSize(pcache, sz_page);
}

// Convert SqlPCachePage type to PgHdr.
static PgHdr *_pcacheFetchFinish(SqlPCachePage *p) {
  PgHdr *pgd = (PgHdr *)p->extra;
  pgd->pdata = p->content;
  return pgd;
}

// Get a page by page number, if it isn't in cache, create a new one 
// and return it.
PgHdr *pcacheFetch(PCache *pcache, Pgno pgno) {
  SqlPCachePage *ppage 
    = global_config.sql_pcache_methods.xFetch(pcache->pcache, pgno);

  // No memory for PCache1 to create a new slot.
  if (ppage == 0) return 0;

  assert(ppage && ppage->extra);
  return _pcacheFetchFinish(ppage);
}

// Get a page by page number, if it isn't in cache, return 0.
PgHdr *pcacheGet(PCache *pcache, Pgno pgno) {
  SqlPCachePage *ppage 
    = global_config.sql_pcache_methods.xGet(pcache->pcache, pgno);

  if (ppage == 0) return 0;
  return ppage->extra;
}

// Make a page dirty.
void pcacheMakeDirty(PCache *pcache, PgHdr *p) {
  assert(p->nref > 0);
  if (p->flags & PGHDR_DIRTY) return ;

  p->flags |= PGHDR_DIRTY;
  if (pcache->pdirty_head)  pcache->pdirty_head->pdirty_prev = p;
  p->pdirty_next = pcache->pdirty_head;
  p->pdirty_prev = 0;
  pcache->pdirty_head = p;
}

// Make a page clean
void pcacheMakeClean(PCache *pcache, PgHdr *p) {
  if (p->flags & PGHDR_DIRTY)
    p->flags ^= PGHDR_DIRTY;
  else return;

  if (pcache->pdirty_head == p) pcache->pdirty_head = p->pdirty_next;
  if (pcache->pdirty_tail == p) pcache->pdirty_tail = p->pdirty_prev;
  if (p->pdirty_next) p->pdirty_next->pdirty_prev = p->pdirty_prev;
  if (p->pdirty_prev) p->pdirty_prev->pdirty_next = p->pdirty_next;

  p->pdirty_next = 0;
  p->pdirty_prev = 0;
}

// Get the header pointer of dirty pages list.
PgHdr *pcacheGetDirty(PCache *pcache) {
  return pcache->pdirty_head;
}
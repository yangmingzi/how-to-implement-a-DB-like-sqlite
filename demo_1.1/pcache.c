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

static void _pcacheFinishConvertInit(
  SqlPCachePage *p,
  PCache *pcache,
  Pgno pgno
  ) {
  PgHdr *ret;
  ret = (PgHdr*)p->extra;

  ret->ppage = p;
  ret->pdata = p->content;
  ret->pextra = p->extra;
  memset(ret->pextra, 0, pcache->sz_extra);
  ret->pcache = pcache;
  ret->pgno = pgno;
  ret->flags = PGHDR_CLEAN;
}

// Convert SqlPCachePage type to PgHdr.
#define PCACHE_FINISH_CONVERT_NEED_INIT 1
#define PCACHE_FINISH_CONVERT_DONT_INIT 2
static PgHdr *_pcacheFinishConvert(
  SqlPCachePage *p,     // Page obtained by plugin pcache module.
  PCache *pcache,       // Obtain the page from this cache.
  Pgno pgno,            // Page number.
  int flags             // flags are defined above.
  ) {
  assert(p != 0);
  PgHdr *pgd = (PgHdr *)p->extra;

  if (flags & PCACHE_FINISH_CONVERT_NEED_INIT) {
    _pcacheFinishConvertInit(p, pcache, pgno);
  }

  ++pgd->nref;
  return pgd;
}

// Get a page by page number, if it isn't in cache, create a new one 
// and return it.
PgHdr *pcacheFetch(PCache *pcache, Pgno pgno) {
  SqlPCachePage *ppage 
    = global_config.sql_pcache_methods.xFetch(pcache->pcache, pgno);

  // No memory for PCache1 to create a new slot.
  if (ppage == 0) return 0;

  return  _pcacheFinishConvert(ppage, pcache, pgno,
            PCACHE_FINISH_CONVERT_NEED_INIT);
}

// Get a page by page number, if it isn't in cache, return 0.
PgHdr *pcacheGet(PCache *pcache, Pgno pgno) {
  SqlPCachePage *ppage 
    = global_config.sql_pcache_methods.xGet(pcache->pcache, pgno);

  if (ppage == 0) return 0;
  return  _pcacheFinishConvert(ppage, pcache, pgno,
            PCACHE_FINISH_CONVERT_DONT_INIT);
}

// Make a page dirty.
void pcacheMakeDirty(PgHdr *p) {
  assert(p->nref > 0);
  if (p->flags & PGHDR_DIRTY) return ;
  PCache *pcache = p->pcache;

  p->flags |= PGHDR_DIRTY;
  if (pcache->pdirty_head)  pcache->pdirty_head->pdirty_prev = p;
  p->pdirty_next = pcache->pdirty_head;
  p->pdirty_prev = 0;
  pcache->pdirty_head = p;
}

// Make a page clean
void pcacheMakeClean(PgHdr *p) {
  if ((p->flags & PGHDR_DIRTY) == 0) return;
  PCache *pcache = p->pcache;
  p->flags ^= PGHDR_DIRTY;

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

// Release a reference of a page.
void pcacheRelease(PgHdr *p) {
  assert(p->nref > 0);

  --(p->nref);
  if (p->nref == 0) {
    assert((p->flags & PGHDR_DIRTY) == 0);
    global_config.sql_pcache_methods.xUnpin(p->ppage);
  }
}
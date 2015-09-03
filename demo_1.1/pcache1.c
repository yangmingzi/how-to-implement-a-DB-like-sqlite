/*
-------------------------
| page | extra | PgHdr1 |
-------------------------
*/

#include "sqlite.h"

typedef struct PCache1 PCache1;
typedef struct PgHdr1 PgHdr1;

// Each page cached by pcache1 is represented by a instance of 
// this structure.
struct PgHdr1 {
  SqlPCachePage base;            // Received by PCache module.

  unsigned int key;              // Key value (page number)
  unsigned char is_pinned;       // Page in use, not in the LRU list.
  PgHdr1 *pnext;                 // Next page in hash table.
  PCache1 *pcache;               // Cache module controlling this page.

  PgHdr1 *plru_next;             // Next page in LRU list.
  PgHdr1 *plru_prev;             // Previous page in LRU list.
};

// Each page cache module is an instance of this structure.
struct PCache1 {
  PgHdr1 *plru_head;       // Head pointer of LRU list.
  PgHdr1 *plru_tail;       // Tail pointer of LRU list.

  int sz_page;             // Page size.
  int sz_extra;            // Size of extra content.
  int mx_pages;             // Maxinum number of pages can be cached in memory.

  unsigned int nhash;      // Number of slots in hash table.
  unsigned int npage;      // Total number of pages in hash table.

  PgHdr1 **aphash;         // Hash table. 
};

// Extent hash table.
static void _pcache1ResizeHash(PCache1 *pcache) {
  PgHdr1 **apnew;
  unsigned int nnew;

  nnew = pcache->nhash * 2;
  if (nnew < 256) {
    nnew = 256;
  }

  apnew = (PgHdr1 **)malloc(sizeof(PgHdr1 *) * nnew);
  assert(apnew);

  if (apnew) {
    int i;
    for (i = 0; i < pcache->nhash; ++i) {
      PgHdr1 *p;
      PgHdr1 *pnext = pcache->aphash[i];
      while ((p = pnext) != 0) {
        unsigned int h = p->key % nnew;
        pnext = p->pnext;
        p->pnext = apnew[h];
        apnew[h] = p;
      }
    }
    free(pcache->aphash);
    pcache->aphash = apnew;
    pcache->nhash = nnew;
  }
}

// Create a PCache1 module.
static SqlPCache *_pcache1Create(int sz_page, int sz_extra, int mx_pages) {
  PCache1 *pcache = (PCache1 *)malloc(sizeof(PCache1));
  memset(pcache, 0, sizeof(PCache1));

  if (pcache == 0) return 0;

  pcache->sz_page = sz_page;
  pcache->sz_extra = sz_extra;
  pcache->mx_pages = mx_pages;
  _pcache1ResizeHash(pcache);

  assert(pcache->nhash);
  return (SqlPCache *)pcache;
}

// Remove a page from hash table.
static void _pcache1RemoveFromHash(PgHdr1 *page, int free_flag) {
  unsigned int h;
  PCache1 *pcache = page->pcache;
  PgHdr1 **pp;

  h = page->key % pcache->nhash;
  for(pp = &pcache->aphash[h]; (*pp) != page && (*pp); pp = &(*pp)->pnext);
  assert(*pp == page);
  *pp = (*pp)->pnext;

  --pcache->npage;
  // if( freeFlag ) pcache1FreePage(pPage);
}

// Add or remove a page from LRU list.
#define PCACHE1_LRU_ADD 1
#define PCACHE1_LRU_REMOVE 2
static void _pcache1AddRemoveLRU(PgHdr1 *page, int flags) {
  PCache1 *pcache1 = page->pcache;
  assert(pcache1);

  if (flags & PCACHE1_LRU_ADD) {
    page->plru_next = pcache1->plru_head;
    page->plru_prev = 0;
    pcache1->plru_head = page;
    if (pcache1->plru_tail == 0)
      pcache1->plru_tail = page;
  }

  if (flags & PCACHE1_LRU_REMOVE) {
    if (page->plru_prev)
      page->plru_prev->plru_next = page->plru_next;
    if (page->plru_next)
      page->plru_next->plru_prev = page->plru_prev;
    if (pcache1->plru_head == page)
      pcache1->plru_head = page->plru_next;
    if (pcache1->plru_tail == page)
      pcache1->plru_tail = page->plru_prev; 
  }
}

// Unpin a page.
static void _pcache1Unpin(SqlPCachePage *page) {
  PgHdr1 *p = (PgHdr1 *)page;
  assert(p);
  _pcache1RemoveFromHash(p, 0);
  _pcache1AddRemoveLRU(p, PCACHE1_LRU_ADD);
}

// Get a page by page number, if it isn't in cache, create a new one 
// and return it.
static SqlPCachePage *_pcache1Fetch(
  SqlPCache *pcache, 
  unsigned int key
  ) 
{
  PCache1 *pcache1 = (PCache1 *)pcache;

  unsigned int h = key % pcache1->nhash;
  PgHdr1 *p = 0;

  if (pcache1->plru_head) {
    p = pcache1->plru_head;
    _pcache1AddRemoveLRU(p, PCACHE1_LRU_REMOVE);
  }

  // If the number of cached pages equal or 
  // larger than mx_pages, return 0.
  if (p == 0 && pcache1->npage >= pcache1->mx_pages) 
    return 0;

  // There is no unpinned page in LRU list, so we malloc a new slot
  if (p == 0) {
    void *pnew = malloc(pcache1->sz_page + pcache1->sz_extra + 
                      sizeof(PgHdr1));
    memset(pnew, 0, pcache1->sz_page + pcache1->sz_extra + 
                      sizeof(PgHdr1)); 
    p = (PgHdr1 *)(pnew + pcache1->sz_page + pcache1->sz_extra);
    p->base.content = pnew;
    p->base.extra = pnew + pcache1->sz_page;
  }

  p->key = key;
  p->pnext = pcache1->aphash[h];
  p->is_pinned = 1;
  p->pcache = pcache1;

  pcache1->aphash[h] = p;
  ++pcache1->npage;

  return &(p->base);
}

// Get a page by page number, if it isn't in cache, return 0.
static SqlPCachePage *_pcache1Get(
  SqlPCache *pcache, 
  unsigned int key) 
{  
  PCache1 *pcache1 = (PCache1 *)pcache;

  unsigned int h = key % pcache1->nhash;
  PgHdr1 *p = pcache1->aphash[h];
  while (p && p->key != key) p = p->pnext;

  if (p) return &(p->base);
  return 0;
}

void pcache1GlobalRegister(SqlPCacheMethods *methods) {
  methods->xCreate = _pcache1Create;
  methods->xGet = _pcache1Get;
  methods->xFetch = _pcache1Fetch;
  methods->xUnpin = _pcache1Unpin;
};
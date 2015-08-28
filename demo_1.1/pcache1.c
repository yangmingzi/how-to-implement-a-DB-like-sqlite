/*
-------------------------
| page | extra | PgHdr1 |
-------------------------
*/

#include "sqlite.h"

typedef struct PCache1 PCache1;
typedef struct PgHdr1 PgHdr1;

struct PgHdr1 {
  SqlPCachePage base;

  unsigned int key;
  unsigned char is_pinned;
  PgHdr1 *pnext;

  PgHdr1 *plru_next;
  PgHdr1 *plru_prev;
};

struct PCache1 {
  PgHdr1 *plru_head;
  PgHdr1 *plru_tail;

  int sz_page;
  int sz_extra;

  unsigned int nhash;
  unsigned int npage;

  PgHdr1 **aphash;
};

static void pcache1ResizeHash(PCache1 *pcache) {
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

static SqlPCache *pcache1Create(int sz_page, int sz_extra) {
  PCache1 *pcache = (PCache1 *)malloc(sizeof(PCache1));
  memset(pcache, 0, sizeof(PCache1));

  if (pcache == 0) return 0;

  pcache->sz_page = sz_page;
  pcache->sz_extra = sz_extra;
  pcache1ResizeHash(pcache);

  assert(pcache->nhash);
  return (SqlPCache *)pcache;
}

static SqlPCachePage *pcache1Fetch(
  SqlPCache *pcache, 
  unsigned int key
  ) 
{
  PCache1 *pcache1 = (PCache1 *)pcache;

  unsigned int h = key % pcache1->nhash;
  PgHdr1 *p = 0;


  if (pcache1->plru_head) {
    p = pcache1->plru_head;
  
    // Chose a victim slot, update this slot and return it to use
    p->key = key;
    p->is_pinned = 1;
    if (p->plru_next)  p->plru_next->plru_prev = p->plru_prev;
    if (p->plru_prev)  p->plru_prev->plru_next = p->plru_next;
    p->plru_next = p->plru_prev = 0;
    p->pnext = pcache1->aphash[h];

    pcache1->aphash[h] = p;
    ++pcache1->npage;

    return &(p->base);
  }

  // There is no unpinned page in LRU list, so we malloc a new slot
  void *pnew = malloc(pcache1->sz_page + pcache1->sz_extra + 
                    sizeof(PgHdr1));
  memset(pnew, 0, pcache1->sz_page + pcache1->sz_extra + 
                    sizeof(PgHdr1));

  p = (PgHdr1 *)(pnew + pcache1->sz_page + pcache1->sz_extra);
  p->base.content = pnew;
  p->base.extra = pnew + pcache1->sz_page;

  p->key = key;
  p->pnext = pcache1->aphash[h];
  p->is_pinned = 1;

  pcache1->aphash[h] = p;
  ++pcache1->npage;

  return &(p->base);
}

static SqlPCachePage *pcache1Get(
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
  methods->xCreate = pcache1Create;
  methods->xGet = pcache1Get;
  methods->xFetch = pcache1Fetch;
};
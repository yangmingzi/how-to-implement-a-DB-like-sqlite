#include "sqlite.h"

typedef struct PCache1 PCache1;
typedef struct PgHdr1 PgHdr1;

struct PgHdr1 {
  unsigned int key;
  unsigned char is_pinned;
  PgHdr1 *pnext;
  PCache1 *pcache;

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

static SqlPCache* pcache1Create(int sz_page, int sz_extra) {
  PCache1 *pcache = (PCache1 *)malloc(sizeof(PCache1));
  memset(pcache, 0, sizeof(PCache1));

  if (pcache == 0) return 0;

  pcache->sz_page = sz_page;
  pcache->sz_extra = sz_extra;
  pcache1ResizeHash(pcache);

  assert(pcache->nhash);
  return (SqlPCache *)pcache;
}
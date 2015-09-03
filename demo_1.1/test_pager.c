#include <stdio.h>
#include <stdlib.h>
#include "sqlite.h"
#include "pcache.h"
#include "sqliteint.h"
#include "pager.h"
#include "os.h"

void pcache1GlobalRegister(SqlPCacheMethods *);

int main() {
  pcache1GlobalRegister(&global_config.sql_pcache_methods);

  SqlVFS* vfs = osGetVFS("unix");
  Pager *p;
  pagerOpen(vfs, &p, "data.db", 0, O_RDWR);

  DbPage *page1, *page2, *page3;
  pagerGet(p, 1, &page1);
  pagerGet(p, 2, &page2);
  pagerGet(p, 3, &page3);

  printf("%p %p %p\n", page1, page2, page3);
  pagerWrite(page1);
  memset(page1->pdata, 'a', 1024);
  pagerCommit(p);
  pagerUnref(page1);

  pagerGet(p, 2, &page2);
  printf(" >>> %d\n", pagerGet(p, 3, &page3));
  pagerWrite(page2);
  memset(page2->pdata, 'b', 1024);
  pagerCommit(p);
  pagerUnref(page2);

  // pagerUnref(page1);
  // pagerUnref(page2);
  // pagerUnref(page3);

  // printf("%p %p %p\n", page1, page2, page3);

  puts("over");
  return 0;
}

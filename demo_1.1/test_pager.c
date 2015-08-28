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

  DbPage *page;
  pagerGet(p, 2, &page);
  char c = ((char *)(page->pdata))[0];
  printf("%c\n", c);

  pagerWrite(page);
  ((char *)(page->pdata))[0] = ++c;
  pagerCommit(p);

  pagerGet(p, 2, &page);
  c = ((char *)(page->pdata))[0];
  printf("%c\n", c);

  puts("over");
  return 0;
}

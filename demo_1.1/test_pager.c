#include <stdio.h>
#include <stdlib.h>
#include "sqlite.h"
#include "sqliteint.h"
#include "pager.h"
#include "os.h"

void pcache1GlobalRegister(SqlPCacheMethods *);

int main() {
  pcache1GlobalRegister(&global_config.sql_pcache_methods);

  SqlVFS* vfs = osGetVFS("unix");
  Pager *p;
  pagerOpen(vfs, &p, "data.db", 0, O_RDONLY);

  DbPage *page;
  pagerGet(p, 1, &page);
  return 0;
}

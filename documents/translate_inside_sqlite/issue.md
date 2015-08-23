Pager

page 在数据库文件中？

pager 把page从文件读到内存，B＋tree 操作内存中的数据？

pager 负责原子提交和回滚？ 事务管理并发控制（隔离性）恢复（持久性）

the pager module implements storage persistence and transactional atomicity？ 存储的持久性和事务的原子性


3.3

There is a maximum limit on the number of slots a cache can have. 

Each page in the hash table is represented by a header object of PgHdr type. 

A flush on a file transfers modified parts of the file to the disk surface

journal 分为几种？rollback journal，statement journal

page 分类？free page；page in hush bucket；page in statement journal

SQLiteusesaverysimple hash function to determine the index value: page numbermodulo the size of the aHash array

As mentioned in the "Cache read" section, when a requested page is not in the cache and a free slot is not available in the cache, the pager victimizes a slot for replacement.

The victim is chosen from the header end of the queue, but may not always be the head element on the queue as is done in the pure LRU. 
<h1>Write-Ahead Logging</h1>
SQLite实现原子性的默认方法是通过rollback journal. 但是从3.7.0开始, "Write-ahead logging"被作为一个新的选择加入了.

使用WAL代替rollback journal有下面这些优点:

1. WAL在大多数情况下都更快.

2. WAL提高了并发性. 读者不会阻塞写者, 写者也不会阻塞读者了. 读写操作可以并发进行.

3. 硬盘I/O操作在WAL模式下更趋近于线性, 更加连贯.

4. WAL减少了很多fsync()的使用, 因此减少了很多fsync()引发的问题.

但是也有这些缺点:

1. WAL通常需要VFS提供共享内存支持. 内置的unix和windows版本能够达到这个要求, 但是许多第三方的VFS可能就不行了.

2. 所有的进程必须在同一台主机上; WAL不支持网络文件系统.

3. 对于同时涉及多个数据库的事务, 在各个数据库内部能实现原子性, 但是相互之间可能就不一定能保持原子性了.

4. 在WAL下, 你不能改变页大小.

5. **

6. 在读取操作很多, 写入操作很少的应用中, WAL或许会比rollbacl journal更慢一点(1% ~ 2%).

7. **

8. **

9. **

<br></br>

###WAL怎样工作
rollback journal的工作方式是在修改数据库之前, 先对需要修改的页做一份拷贝, 这样, 当错误发生后, 我们还能根据之前拷贝的内容修复数据库. 修复完或者没有确定没有错误发生后, 再将拷贝的文件删除.

而WAL反转了整个操作. 原始的内容被保留在数据库中, 被修改的内容被写到了WAL文件内. 而COMMIT的操作由一条特殊的记录表示, 也会被写入到WAL文件中. 因此, COMMIT操作能够在不修改数据库的情况下进行, 其他读者能继续读取自己需要的数据, 因为COMMIT操作都"发生"在WAL文件中了. 同时, 多个事务的还能被写入到同一个WAL文件中.

<br></br>
###Checkpointing
当然, 最后我们需要将WAL文件中"发生"的事务给同步到数据库文件中, 我们将这个操作称之为"checkpointing". 

WAL和rollback journal另一个不同的地方是: rollback journal中只有两种操作, reading和writing, 而WAL中有三种, reading, writing, checkpointing. 

SQLite会在WAL文件中的内容达到某个阈值时进行checkpointing, 默认为1000页. (当然你可以通过SQLITE_DEFAULT_WAL_AUTOCHECKPOINT这个宏定义在编译时设置它.) 引用在WAL模式下不需要对checkpointing做任何多余的事情. 如果你想的话, 你也可以调整自动checkpointing的阈值, 也可以关闭自动checkpointing再自己设置一个线程在程序空闲时进行checkpointing.

<br></br>

###并发性

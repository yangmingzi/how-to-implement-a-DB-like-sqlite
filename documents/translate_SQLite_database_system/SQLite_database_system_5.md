<h1>第5章.Pager层</h1>
###学习目的:
读完本章内容后, 你会知道:

+   Pager Cache是什么, 我们为什么会需要它.
+   常见的cache管理技术.
+  普通transaction的执行和回滚步骤.

###本章大纲
本章讨论Pager层. Pager在数据库和操作系统间实现了一层"page"的抽象. 它帮助上层模块能够快速访问"需要的page", 通过page cache技术. 这一层也实现了ACID性质, 能够处理并发和回滚. 同时它也管理日志和lock操作. 

###5.1 Pager

###5.2 Pager接口
本章我介绍几个Pager提供的接口, Tree层使用这些接口来访问数据库. 在此之前, 我先讨论下Pager和Tree之间的交互协议. 

###5.2.1 Pager-client交互协议
Pager之上的模块都对log管理和low-level lock一无所知. Tree把所有操作都当作transaction, 它不关心Pager层怎么实现ACID. 而Pager把transaction拆分为locking, logging, reading, writing等操作. Tree通过page number(页号)向Pager请求某页内容. Pager则返回一个指针, 这个指针就指向该page所在位置. 在修改page之前, Tree先通知Pager, 使之能够能够存储一些信息, 以备回滚和修复数据. 当Tree修改完page后, Pager才会将page从内存写回到disk.

###5.2.2 Pager接口结构
Pager被一个结构体Pager表示. 每个被打开的database file都被一个Pager管理, 每个Pager也只对应一个database file. 当Tree准备打开某个database file使用时, 必须先建立一个新Pager, 然后在Pager上对database file进行操作. 一个进程能有多个transaction, 这些transaction有自己的Pager, 这些Pager打开同一个database file.

一些Pager维护的变量被在下图中. 

###5.2.3 Pager接口函数

1. sqlite3PagerOpen: 创建一个Pager, 打开或者创建一个database file, 创建并初始化page cache, 并返回一个指向Pager的指针. 此时database file还为被加上任何lock, journal file也还为被创建. 

2. sqlite3PagerClose: 销毁一个Pager并关闭database file. 如果是个临时文件, 则删除它. 如果此时某个transaction真正执行, 那么这个transaction会被强行回滚. 所有被缓存的page将会被删除, 缓存用的内存将会被free掉. 

3. sqlite3PagerGet: 这个函数将会为调用者(Tree)将page数据准备在内存中. Tree将page number作为参数传入. 函数返回一个指向指定page的指针. 在第一次调用时, 它将会为database file加上一把SHARED lock. 
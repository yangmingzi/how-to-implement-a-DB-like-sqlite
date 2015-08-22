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

4. sqlite3PagerWrite: 这个函数使某个page变得可写(但是并不会将此page写入到database file). 在修改page前, 必须调用这个函数, 不然Pager可能不会知道page被进行了修改. (值得注意的是, 为了效率, SQLite不会在Pager和Tree间拷贝page, 而是让Tree通过指针直接操作page). 

5. sqlite3PagerLookup: 该函数返回一个指向某个page的指针. 如果page此时不在内存中, 返回NULL. 

6. sqlite3PagerRef: 将指定page的reference count加1, 如果该page在freelist中, 则将该页从freelist中移除. 

7. sqlite3PagerUnref: 将指定page的reference count减1. 当被减到0时, 该page被放入freelist中. 当所有page都被放入freelist后, 会将database file的SHARED lock释放. 

8. sqlite3PagerBegin: 创立一个显式的write-transaction(隐式的write-transaction由sqlite3PagerWrite创建). 如果此时database file已经被上了RESERVED lock, 则什么也不做. 否则就申请一个RESERVED lock. 根据参数的不同, 它还能立即申请一个EXCLUSIVE lock, 而不是等着Tree执行写入操作时才申请. 

9. sqlite3PagerCommitPhaseOne: 将当前transaction提交: 将file-change-counter加1, 同步journal file, 同步所有database file的修改.

10. sqlite3PagerCommitPhaseTwo: 完成对journal file的操作(比如删除或者非法化).

11. sqlite3PagerRollback: 中止当前所有的transaction, 回滚database file的所有操作, 将EXCLUSIVE lock降低为SHARED lock. 内存中所有的page将会被回溯成初始内容, 结束journal file. 这个函数不会返回失败. 

12. sqlite3PagerOpenSavepoint: 

13. sqlite3PagerSavepoint: 

<br>
###5.3 Page Cache
Page被cache与用户空间中. 下图展示了通常的cache场景. 图中的两个进程打开了同一个database file. 两个进程都有自己的cache. 

###5.3.1 Cache状态
Page cache的状态被维护在Pager结构体中, 其状态决定了Pager能做的操作. 两个变量, eState和eLock被用来控制Pager的行为. Pager的cache state只能是下面7个状态之一. 

1. PAGER_OPEN: 当一个Pager被建立时, 这是它的初始化状态. 此时Pager还没有对文件进行读取, 其cache是空的. 

2. PAGER_READER: 当Pager处于这个状态时, 至少有一个read-transaction打开了某个database connection, 此时Pager能够读取文件.

3. PAGER_WRITER_LOCKED: 当Pager处于这个状态时, 一个write-transaction打开了某个database connection. 此时它能够读取database file, 但是并未对其进行修改.

4. PAGER_WRITER_CACHEMOD: 当Pager处于这个状态时, 它给了Tree能修改cache的权限, 而此时Tree或许已经对cache做了某些更新.

5. PAGER_WRITER_DBMOD: Pager处于这个状态时, 说明它已经开始回写page到database file.

6. PAGER_WRITER_FINISHED:

7. PAGER_ERROR:

而eLock维护的lock状态, 只能是下面4种之一:

1. NO_LOCK:

2. SHARED_LOCK

3. RESERVED_LOCK

4. EXCLUSIVE_LOCK

一开始eLock被初始化为NO_LOCK, 当Tree使用sqlite3PagerGet时, eLock被转化为SHARED_LOCK状态, 当Tree用sqlite3PagerUnref释放掉所有的page后, 状态又变回了NO_LOCK. 当Tree第一次调用sqlite3PagerWrite时, Pager成了RESERVED_LOCK. 当Pager回写数据前, 将会变成EXCLUSIVE_LOCK状态. 在sqlite3PagerRollback和sqlite3PagerCommitPhasetwo之间, Pager将会再变成NO_LOCK.

###5.3.2 Cache优化
每个page cache被通过一个PCache作为handle及性能管理. 下图展示PCache的少数变量. SQLite提供了插件式的cache框架. 它也提供了自己的cache管理模块(下面会说). 除非用户自己提供一个, 不然这个模块就会成为默认的cache管理器.

通常来说, 为了快速查找, 被cache的内容都会被按照一定结构进行组织. SQLite使用了hash table用来组织被cache的pages, 并且在table中使用page-slots来存放这些页. 其cache是全关联(fully associative)的, 任何slot能cache任何page. Hash table一开始是空的, 接着逐渐的创建新的slots并插入到hash table中. 这里有一个最大slots数量的限制, PCache.nMax. 对于临时database, 其默认值是500, 而其他情况默认值是2000.(In-memory database没有这个限制, 只要操作系统还有内存分给SQLite).

每个被cache的page都被表示为一个PgHdr类型. 下图展示了SQLite自己提供的cache模块, PCache1. Hash table中的每个slot都被表示为PgHdr1, 而slot的实际内容被存储与PgHdr1前, 实际slot的大小被PCache1.szSize指定. 在slot实际内容的头部, 就是PgHdr的内容, 然后是page内容, 最后是一些private data. 所有的page都只能通过PCache1.apHash访问, 这个hash数组的长度被PCache1.nHash指定. apHash的每个元素就是一组slots, 这些slots被按照单向链表组织.

PgHdr只对Pager有效, 对Tree以及更高的模块都是透明的. 在PgHdr结构体中维护着不少变量. pgno为page number. needSync标志为true的话, 以为着journal file需要被刷新. dirty标志为真表示当前page已经被修改. nRef为引用计数. 如果nRef为0, 那么该page就会被free. 

###5.3.3 Cache读取
Cache并不是直接通过地址访问的. 实际上使用cache的用户并不知道cache被存放在哪, 也不用知道. Cache被通过page number寻找. 为了读取某个page, Tree对page number调用sqlite3PagerGet. 接着这个函数执行如下步骤:

1. 在所有cache中查找. 
1.对Page number使用一个简单的hash函数, 将其映射到apHash这个数组上.
2.通过apHash数组, 在得到这个Page number对应的链表.
3.在这个链表上依次搜寻, 直到找到该page number或者到达链表尾部. 接着将其nRef加1.

2. 如果Page number没有被找到, 则视为cache miss. 接着在freelist中查找一个slot用来读取需要的page.(如果cache空间还没达到最大限度, 则会直接申请新的slot而不是在freelist中查找)

3. 如果freelist没有可用的slot, 接下来将会从正在使用的slot中拿出一个来为当前page所使用, 这个被选中的slot称为victim slot. 

4. 如果victim slot处于dirty状态, 将其写回到database file.(通过WAL协议, 同时也刷新journal file).

5. 两种情况. 
1.如果当前请求page的空间小于或等于slot的空间, 则直接将page读入到slot中, 并激活这个page, 将其nRef设置为1. 接着返回slot中page的指针给Tree.
2.如果空间大于slot的空间, 则不会读入该page, 而是将其初始化为0. 
在两种情况中, nRef都会被设置为1.

SQLite严格遵守fetch-on-demand原则, 使得page的读取逻辑非常简单.(5.3.5会介绍)

当用户拿到page后, Pager就不知道它会对page做什么操作. SQLite遵循下面几条原则: 用户申请page, 使用page, 释放page. 当page被拿到用户手上时, 该page已经被激活(nRef大于0). 当用户使用sqlite3PagerUnref时, nRef被减1, 当nRef为0时, 该page被非法化. 被激活的page不能被拿来循环使用. 为了避免出现"所有的page都被激活", SQLite需要一小部分page来保证总有slot能够被使用. 在SQLite3.7.8中, 这个数是10.

###5.3.4 Cache更新
拿到page后, 用户能直接修改page内容, 但是在修改前, 必须先调用sqlite3PagerWrite. 这个函数返回后, 用户就能随意修改page了.

用户第一次调用sqlite3PagerWrite时, Pager将原page内容写入到journal rollback file内, 并设置PgHdr.needSync为true. 随后, 当日志被回写到disk中后, Pager将needSync清除. (SQLite遵循WAL 协议: Pager不会将修改过的page写回到database file, 除非needSync被清除). 每次sqlite3PagerWrite被调用时, PgHdr.dirty被设置为true. 只有当page被写回到disk后, dirty才会被清除. 因为用户会在什么时候修改page, 这对Pager来说并不能知道, 所以SQLite遵循延迟更新协议. 只有当Pager调用flush时, database file才会被更新. 

###5.3.5 Cache获取协议
Cache获取协议决定什么时候将page放入cache. Fetch-on-demand协议决定只有当用户需要时, 才将page放入cache. 你应当注意当获取page时, 用户将被阻塞, 直到将page从database file读入内存过后才会恢复. SQLite严格遵守fetch-on-demand, 不会有任何预读入之类的操作. 同时SQLite每次只从database file中读取1个page.

###5.3.6 Cache管理
通常情况下, page cache的大小是有限的, 除非database file特别小, 不然只能cache一部分内容, 因此cache空间必须被小心的管理. 基本的理念是只cache用户当前需要的那些page. 我们设计cache管理时, 必须考虑三个问题. 

1. Cache中的page都是从database file中拷贝而来, 当cache中的page被更新时, database file中的也会更新.

2. 当被请求page不在cache中时, 需要新建一页page, 并从database file中拷贝出它.

3. 当cache满时, 需要一个置换算法, 用来移除老的page, 并将空间留给新page.

Cache空间是有限的, 所以需要循环的使用这些空间, 用来"适应"整个很大的database file. 在下图中, 总共有26页的内容, 但是我们需要用5页空间的cache来"适应"它们. Cache管理对cache速率, 甚至是整个系统的速率都有很大影响. 下图中, 因为只有5页的cache空间, 所以管理它们并不难. Cache管理最困难的时当cache满了的时候. Cache的效率和page命中率息息相关. 所以对于Cache置换, 最重要的就是决定应该置换出哪些page. 如果置换page选取得不好, 那么cache就会有大量脏数据, 其中充斥着不被需要的page.

### 5.3.6.1 Cache置换
Cache置换指的是当cache满时, 需要将一些旧的pages移除, 留出空间给新的pages. 如5.3.3节中所说, 当cahce空间满时, 且freelist中没有可用的slot, 那么就会选中一个victim slot作为替换. 然而victim slot不是那么容易就能决定的. SQLite置换策略的是让被保留的pages尽量能够满足最多的请求(最大命中率). 如果cache的命中率很低, 毋庸置疑, 其速度肯定会很糟糕. 

但是矛盾的是, 我们并不知道哪些页将在未来被请求(如果我们知道, 就能保留这些pages, 以得到最大命中率). 而我们的方法是, 根据之前的历史记录来决定移除哪个page. 假设现在cache中有page P, 假如P被置换出去后, 没过多久就又被申请, 接着又被置换进来, 那么P肯定就不是一个好的置换选择. 常见的置换策略有很多种, SQLite使用的是LRU(least recently used)策略.

### 5.3.6.2 LRU置换算法

### 5.3.6.3 SQLite cache置换架构
SQLite将未被激活的pages逻辑上组织成一个队列, 让某个page不再被使用时, 它将被附加到队列的尾部. Victim slot从队列的头选出. 
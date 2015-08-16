<h1>第4章.事务管理</h1>
###学习目的:
读完本章内容后, 你会知道:

+   SQLite怎么实现ACID
+  SQLite管理lock的方式
+  SQLite怎么避免deadlocks
+  SQLite怎么实现journaling protocols
+ SQLite怎么记录savepoint

###本章大纲
DBMS的主要职责之一就是帮助用户维护database. DBMS能够在多用户同时操作的情况下对database进行保护, 以及在事务失败时进行回滚和恢复. 为了实现这些功能, SQLite以事务为单位执行操作. SQLite在实现ACID时会依赖操作系统和journals. 你可以认为SQLite只支持 flat transaction, 不支持nesting transaction. 本章我会介绍SQLite怎么实现ACID, 下一章我会介绍SQLite中进行事务管理的pager层.

###4.1 事务类型
几乎所有的DBMS都使用locking mechanisms进行并发控制, 使用journals来保存恢复信息. 在database被修改之前, DBMS将一些日志信息写入journals. DBMS会保证这些日志信息被完整的存储了之后, 才进行database修改. 当transaction失败后, DBMS能从日志文件中读取足够的信息来恢复. 在SQLite中, lock方式和log方式都取决于transaction的类型. 我将会在本章对这些类型进行介绍.

###4.1.1 System transaction
SQLite将每条语句放在transaction内执行, 支持读和写 transaction. 上层应用只能通过读写transaction读取数据, 只能通过写transaction写入数据到database. 在执行SQL语句时, 不必显式的让SQLite为其包裹transaction, SQLite会自动进行. 这就是autocommit模式. 这样形成的transaction称之为system or auto or implicit transactions. 对于SELECT语句,  SQLite自动将其置入一个read transaction内执行. 对于non-SELECT语句, SQLite先创建一个read transaction, 然后将其转换为一个write transaction. 每个transaction将会在执行后被自动提交. 对于上层应用而言, 它并不知道transaction的存在. 

应用能并发的执行SELECT语句, 但是不能并发执行non-SELECT语句. non-SELECT会被原子性的执行, SQLite会为多个non-SELECT维护mutex. 而SELECT语句则不会被原子性执行. 

###4.1.2 User transaction
默认的autocommit模式对某些应用来说可能会有很昂贵的代价, 特别是对那些写敏感的操作. 因为SQLite对每条non-SELECT语句都会要求重打开, 写入, 关闭journal files. 除此之外, 还会有并发控制带来的额外负担, 比如需要对database file申请和释放lock. 为了避免上述开销被重复, 用户可以开启一个user transactiong. 可以把多条SQL语句写在"BEGIN TRANSACTION"和"COMMIT [or ROLLBACK] TRANSACTION"之间. 

###4.1.3 Savepoint
SQLite为user transaction提供了savepoint. 应用能在user transaction内或外执行savepoint指令. Savepoint是transaction内的点, 它表示当前transaction的一种状态. 一个transaction能包含多个savepoint. 当transaction失败时, 我们能回滚到某个savepoint.

###4.1.4 Statement subtransaction


###4.2 Lock management
多并发的系统需要有一套同步机制来隔离并发transaction相互的影响. 隔离性这个词经常被用在并发控制, 序列化和锁机制中. 对于事务隔离, 有许多概念, 比如read commited, cursor stable, repeatable read, serializable. Serializable是最严格的隔离等级, 而SQLite实现了它.

SQLite使用了lock机制来实现Serializable, 并不使用进程间的共享内存. SQLite使用的是操作系统提供的file lock. 

SQLite允许同时对同一个database file进行任意数量的read transaction, 但是同一时刻只能有一个write transaction. SQLite的lock机制非常简单, 它只对database file进行lock, 而不是对数据库中某一行或者列进行lock. 在下面的内容中, 我将会叙述SQLite如何管理lock.

###4.2.1 Lock类型以及其相容性
从某个transaction的角度来看, 一个database file可能有下面5种类型. 你应该清楚它们是SQLite实现的, 而不是操作系统提供的. 

+ NOLOCK: Transaction没有取得任何类型的锁, 它不能进行读写操作. NOLOCK是transaction被建立时的初始状态. 

+ SHARDED: 这个锁只允许读操作. 任何数量的transaction能够同时取得该锁. 该锁不运行写操作. 

+ EXCLUSIVE: 这是唯一能够允许写操作的lock, 同时刻在database file上只能有一把EXCLUSIVE lcok. 其他lock不允许与该lock共存.

+ RESERVED: 该锁运行读取操作. 但是和SHARED不同, RESERVED lock暗示其他transaction它将要进行写操作. 同时刻只能有一把RESERVED锁, 且能够与其他SHARED lock共存. 其他transaction在此期间能够申请新SHARED locks.

+ PENDING: 该锁允许读操作. PENDING lcok意味着这个transaction马上很快将会进行写入操作. 它时取得EXCLUSIVE之前的一个过渡lock. Transaction处于这个状态时其实实在等待其他SHARED lock被释放. 同时刻只能有一个PENDING lock, 且它能与其他SHARED lock共存, 但是不允许申请新的SHARED lock.

SQLite锁的共存性如下图:
![Pic4.1](/home/qw4990/桌面/SQLITE_BOOK/Pic4.1.png)

<b>讨论: </b>对于一般的数据库来说, 起码需要EXCLUSIVE lock, 其他的locks只是用来进行并发控制. 如果只含有EXCLUSIVE lock, transaction将会被序列化的执行. 有了SHARED 和 EXCLUSIVE locks, 则能够对读取操作进行并发. 实际中, transaction在SHARED lock下, 读取数据, 修改, 然后申请EXCLUSIVE lock来写回数据. 如果有两个这样的transaction同时执行, 那么就可能会引发死锁. 而RESERVED 和 PENDING就是用来预防死锁形成. 这两个锁同时也能帮助预防写者饿死(writer starvation). 

####4.2.2 Lock申请协议
Transaction在读写操作前申请合适的锁, 来保证多transaction之间不会产生冲突. Pager层的责任之一就是为transaction生成适当的locks.

Transaction开始的时候处于NOLOCK. 在读取前, 它会申请一个SHARED lock. 此时它就成为了read transaction, 能够读取database file中的任何pages. 在改变database file之前, 它会申请一个RESERVED lock. 此时它变成了write transaction, 但是此时它的影响对其他transaction来说是不可见的. 当得到RESERVED lock后, transaction就能修改内存中的page. 在将修改内容写回到文件之前, 它需要申请EXCLUSIVE lock. 此时它将成为一个full write transaction, 在将修改内容写回到文件后, 其修改就立即对其他可见了.

Locking状态图如下. 注意Pager不能取得PENDING lcok, PENDING lock只是取得EXCLUSIVE前的一个临时状态.
![Pic4.2](/home/qw4990/桌面/SQLITE_BOOK/Pic4.2.png)

对于一般的read transaction, 状态通常是从NOLOCK到SHARED再回到NOLOCK. 对于write transaction而言, 通常是NOLOCK到SHARED到RESERVED最后到EXCLUSIVE, 再回到NOLOCK. SHARED直接到PENDING的情况发生在有journal需要回滚时.

在lock管理时, 实现了两个函数, sqlite3OsLock, sqlite3OsUnlock 用来申请和释放locks. 它们被定义在os.h文件内. 这两个函数将被Pager层调用. 当如database被关闭时, Pager会释放掉所有的锁. 我会在4.2.7中继续讨论这两个函数. 

###4.2.3 显式locking

###4.2.4 Deadlock和starvation
尽管lock机制解决了并发性问题, 但是它又带来了其他的问题. 假象有2个transaction对同一个database file持有SHARED lock. 同时它们都想申请RESERVED lock. 现在其中一个得到了RESERVED lock, 而另一个则继续等待. 处于RESERVED的, 现在申请了EXCLUSIVE lock, 于是它开始等待另一个transaction释放其SHARED lock, 但此时另外一个却在等待RESERVED lock. 于是引发了一个deadlock.

Deadlock是一个头疼的问题, 这里有两种方法可以消除deadlock.(1)预防, (2)检测和消除. SQLite能预防deadlock, SQLite采用非阻塞式的取锁方式 - 申请lock时, 要么得到一个lock, 要么得到一个错误代码, 而不会被阻塞起来等待. 当某次取锁失败后, transaction会进行有限次的重新尝试. 如果所有尝试都失败了, 将会返回SQLITE_BUSY错误. 由于上述方式的预防, 系统中几乎不会存在deadlock. 

####4.2.5 Linux lock primitives
SQLite的locks通过操作系统的文件锁实现. Linux实现了两种native lock, read lock和write lock. 它们可以被作用在文件上一段连续的区域上. write lock不能和其他lock共存, 包括read lock和write lock. 但是read lock 和 write lock 能共存在同一文件的不同区域. 一个进程在文件某个区域只能申请一种类型的lock. 如果在已上锁的区域上申请新的lock, 那新的lock将会将原lock覆盖掉. 

<b>注意: </b>文件上的lock并不是文件内容的一部分, 它们只是内核维护的一种数据结构. 

####4.2.6 SQLite lock实现
SQLite通过操作系统提供的函数实现其自己的locking结构. 由于linux只有read lock, write lock两种native lock, 因此SQLite通过锁住不同的文件位置的方法, 来实现其4种lock. 它通过fcntl来申请和释放lock. 本章讨论操作系统locks到SQLite locks的转换方法. 

+ SHARED lock 实现于在database file的整个shared区间上设置一把read lock. 

+ EXCLUSIVE lock 实现于对整个database file的shared区间设置一把write lock.

+ RESERVED lock 实现于在某个特定的byte上设置一把write lock, 这个byte被称为reserved lock bytes.

+ PENDING lock 实现于在某个特定的byte上设置一把write lock, 这个特定的byte不同于RESERVED中的, 且在SHARED的区间外.

下图演示了上述方法. SQLite预留了510bytes作为shared的上锁区间, 这个值被宏SHARED_SIZE定义. 区间开始于宏SHARED_FIRST. 宏PENDING_BYTE (=0x40000000, 是1GB后的第一个bytes) 则是PENDING lock的默认上锁位置. RESERVED_BYTE被设计在PENDING_BYTE后一个byte. 而SHARED_FIRST被设置在PENDING_BYTE后两个. 所有的上锁区间都能适配所有大小的page, 即使是最小512bytes的page. 这些宏被定义在os.h中.
![Pic4.3](/home/qw4990/桌面/SQLITE_BOOK/Pic4.3.png) 

####4.2.6.1 SQLite lock到native lock
下面几段话叙述了SQLite怎么对database file上锁.

+ 为了取得SHARED lock, 进程先对PENDING_BYTE取得一个read lock, 以确保没有PENDING lcok. 如果成功, SHARED lock自己的区间将会被上一个read lock, 接着PENDING_BYTE的read lock将会被释放. 

+ 进程只会在SHARED的状态下去申请RESERVED lock. 进程会直接在RESERVED_BYTE加上write lock. 但是它并不是释放SHARED lock (这个举措用来保证其他进程在此间其不能得到EXCLUSIVE lock).

+ 进程或许会从SHARED直接想要取得PENDING. 为此, 它先取得PENDING_BYTE的write lock (此举用来阻止新SHARED lock). 此时它并不释放SHARED lock (这个举措用来保证其他进程在此间其不能得到EXCLUSIVE lock).
<b>注意: </b>进程能跳过RESERVED直接取得PENDING lock. 这个性质被用于SQLite回滚journal. 

+ 进程取得PENDING lock后或许会申请EXCLUSIVE lock. 首先先在shared区间加上write lock. 因为所有的SQLite lock都会在shared区间上申请read lock, 所以此举能够保证此时没有其他的锁. 

下图是上述步骤的转换图. PENDING lock 和 EXCLUSIVE lock有一点小不同, 它们可能会也可能不会对RESERVED_BYTE有write lock, 这取决于SQLite怎么设置这些locks.
![Pic4.4](/home/qw4990/桌面/SQLITE_BOOK/Pic4.4.png)


####4.2.6.2 Native locks的工程性问题
上述lock机制有两个工程上的问题. 第一个问题的是Linux native lock的性质造成. 第二个则是SQLite自身的问题. Native lock是以进程为单位分配的, 而SQLite locks是以线程为单位分配(如下图). 图中, 进程1有两个线程: 线程1有1个transaction, 线程2有两个, 它们通过三个不同的连接访问database file.进程2有一个transaction. 四个并发的transaction中, 三个有SHARED lock, 一个有RESERVED lock. Transaction对操作系统是透明的, 操作系统只能看到进程. 我们需要有一个映射用来将transaction中的SQLite lock和进程中的native lock对应起来. 

如果一个进程只打开一个连接, 那么进程和transaction是同步的, 如进程2的情况. 当进程通过不同的连接并发处理多个transaction时, 情况就复杂了, 如进程1的情况. 什么样的native lock才能对应进程1中的3个transaction呢? 我将在下面个两个章节中处理这两个问题. 现在唯一值得庆幸的是, 一个连接最多打开一个database file. (也就是在处理transaction时, 进程打开database file, 执行transaction, 然后关闭database file; 下一个transaction来临时, 再重新打开)
![Pic4.5](/home/qw4990/桌面/SQLITE_BOOK/Pic4.5.png)

####4.2.6.3 Linux系统问题
SQLite使用了POSIX协议的locking结构, 并且使用fcntl来申请和释放lock. 正如之前所说, locks只是操作系统维护在内存中的一种数据结构. 值得一提的是linux的lock是针对inode进行维护的, 而不是文件名. 在Linux中,  使用软或硬连接能将不同的文件名指向相同的inode, 这会对native lock造成某些奇怪的影响. 第二点需要注意的是虽然locks是通过fcntl和file descriptor进行上锁, 但是它们两者之间没有什么联系, 只和inode有关系. 如果一个进程对某个文件在相同的区域用两个不同的file descriptor进行上锁, 那么第二个lock将会覆盖掉第一个 (这两次操作来自同一个进程不同的线程).

下面通过几个例子来演示一下上述问题. 假设file1和file2是两个有不同文件名的相同文件(指向不同的inode). 假设某进程打开了它们:
int fd1 = open("file1", ...);
int fd2 = open("file2", ...);
假设线程1通过fd1取得了一个read lock; 线程2通过fd2在相同位置取得了一个write lock, 此时之前的read lock就被覆盖了, 因为两个lock都来自同一个进程, 且在相同位置. 

这意味着如一个进程多线程的打开了database file, 并执行不同的操作, 那么它们之间会相互的影响. 于是我们不能简单的使用native lock来进行并发控制. 

<b>线程控制内容暂时省略...</b>

####4.6.2.4 多线程应用

####4.2.7 Lock APIs
Lock机制被实现在两个函数中, sqlite3OsLock和sqlite3OsUnlock. 本章我将描述它们的算法步骤.

####4.2.7.1 sqlite3OsLock
sqlite3OsLock的完整定义为int sqlite3OsLock(unixFile *id, int locktype). 其中id是SQLite定义的file descriptor. 对于unix, 这个函数被实现在os_unix.c中. 你应该注意到locktype不能为PENDING. 这个函数只能按照下面的顺序申请lock: NOLOCK, SHARED, RESERVED, PENDING, EXCLUSIVE.

Sqlite3OsLock的步骤如下:

1. int idLocktype = id->eFileLock;  //得到当前的SQLite lock
2. int pLock = id->inodeInfo->eFileLock; // 得到进程的SQLite lock
3. 
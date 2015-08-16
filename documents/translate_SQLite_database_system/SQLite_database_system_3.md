<h1>第3章.存储结构</h1>
###学习目的:
读完本章内容后, 你会知道:

+   SQLite数据库文件的结构
+  各种记录文件的结构及格式
+ SQLite中page的概念及其作用
+ 数据库怎么实现平台无关化

###本章大纲
本章描述SQLite底层的文件结构及格式. SQLite数据库文件被分割成了固定大小的page, 用于存储"B/B+tree pages", "freelist pages"和其他page结构. 在默认的journaling mode中, 记录文件在改动前将原本的"page"给记录下来; 而在WAL mode中, 记录文件存储的是改动后的"page". 本章将讨论这些文件的命名规则及内部结构.

###3.1 数据库命名规则
SQLite中, 数据库被存储于一个单独的大文件中, 称之为database file. 当一个应用试图用sqlite3_open API打开某个数据库时,  实际上就是给这个函数一个database file的名字作为参数. 这个文件名字可以是相对路径或者绝对路径. 任何能被系统接受的文件名都可以. 但是这里有两个例外:

+  如果被传入的文件名为空或者全部为空格, SQLite会创建一个新的临时文件. 
+ 如果给定的文件名是":memory", SQLite将会在内存中建立一个数据库. 

在上述两种情况中, 被建立的数据库都是临时的, 当应用关闭数据库时, 它们将不复存在.

<b>database file命名:</b> 以.db结尾, 用于区分.
<b>URI命名:</b> 始于SQLite3.7.7, 支持用URI命名方式. URI应该开始于file:. 其他信息请访问[URI命名规则](http://www.sqlite.org/uri.html)
<b>SQLite中的临时文件:</b> SQLite使用了许多种临时文件, 比如rollback journal, statement journal, multidatabase master journal, transient index, transient database.
<b>临时文件命名:</b> SQLite随机生成每个临时文件名. 这些名字以etilqs_开头, 接着是16个字母或数字, 没有后缀名. 你可以修改宏定义SQLITE_TEMP_FILE_PREFIX来修改这些文件的前缀. 这些临时文件可能存储在(1)/var/tmp (2)/usr/tmp (3)/tmp中. 你可以修改TMPDIR环境变量来指定存储路径. 如果系统在关闭这些临时文件前崩溃了, 那么它们可能被保留下来.

不管以何种方式打开一个数据库, 我们都将被打开的数据库称作main database. SQLite还为每个"数据库链接"维护着一个temp database. 如图3.1. 当一个应用打开不同的链接时, 每个链接会有自己的临时文件. SQLite会在链接关闭的时候删除这些临时文件. main database和temp database文件的结构是一致的. 
![Pic3.1](/home/qw4990/桌面/SQLITE_BOOK/Pic3.1.png)
SQLite中有两个不同但相互关联的概念. 通过使用attach指令, 你能以一个不同的名字, 关联一个数据库文件到你的链接上. 举个例子, 指令 attach /home/sibsankar/MyDB as DB1 使得MyDB这个数据库文件被链接打开. 随后你就能使用Select * from DB1.table1对这个数据库进行查找. 

<b>链接的疑惑:</b> sqlite3_open API使用数据库文件名打开一个链接. 这个链接能够连接到main database和temp database. ===
<br>

###3.2 数据库文件结构
除了in-memory database, SQLite将整个数据库都存储于一个文件中. 在其生命周期中, 文件大小不断的改变. 操作系统将数据库文件视为普通的文件, 但是并不解释其文件内容. 其实现了对数据库文件任意bytes间的read/write甚至sync操作. 在剩下的内容中, 我将阐述SQLite怎样将数据库文件按页(page)的方式进行组织.

####3.2.1 Page抽象
为了方便空间管理, SQLite将database file按固定大小的页(page)组织. 因此, 一个database file的大小总是page大小的倍数. Page被从1开始编号. 0号page被视作NULL. 这些page被线性存储, 如图3.2. 你可以将database file视作一连串的page. 而page的编号则用于访问这些page.
![Pic3.2](/home/qw4990/桌面/SQLITE_BOOK/Pic3.2.png)

####3.2.2 Page大小
默认的Page大小是1024bytes, 但是在编译时期可以进行设置. Page大小必须是2的次方, 并且大于等于512(2^9), 小于等于65536(2^16). 这个最大限制是因为Page大小被存储为一个2bytes的short型整数. 一个database file最多能有2^31 - 1页page; 这个数字被写在了宏PAGER_MAX_PGNP中. 因此SQLite的数据库最大能到140TB(2^16 * 2^31 = 2^47). 除此之外, 最大限制也受制于操作系统.
 
####3.2.3 Page类型
根据Page的用途, 它们被分为四类: free, tree, pointer-map, lock-byte. Free pages是暂未被使用的. Tree pages又能被细分为leaf, internal和overflow类型. B+Tree的internal page存储着树上的跳转信息, leaf page存着数据. 如果某条信息太大, leaf page存不下来, 那么存不下的信息将会被保存到一个或多个overflow page上.

####3.2.5 freelist格式
未被使用的pages被组织在文件头32位偏移处. 而free page的总数被存在36位偏移处. Freelist的组织如图3.5. Freelist有两种子类型: trunk pages和leaf pages. trunk pages中
存储了leaf pages的编号.
![Pic3.5](/home/qw4990/桌面/SQLITE_BOOK/Pic3.5.png)
Trunk page的结构如下: (1)4-bytes的空间存储下一个trunk page的编号; (2)4-bytes的空间存该trunk page内的leaf page编号个数; (3)0或者多个4-bytes的leaf page编号.

当某page变成free状态时, 它并不会被操作系统释放掉, 而是被添加到Freelist中. 当你向SQLite中添加新数据时, SQLite会从freelist中拿出free page来存储新内容. 只有当freelist为空时, SQLite才会向操作系统申请新的空间. 

有些时候, freelist中的page可能会过多. 此时你可以运行VACUUM指令来整理过多的空间. SQLite此时会将freelist过多的页码让操作系统释放, 以减少database file的大小. 

<b>Freelist 整理:</b> 当你使用VACUUM指令来整理freelist时, SQLite会将整个database拷贝成一个临时文件, 用于事务保护.
<br>

###3.3 Journal File 结构
SQLite使用三种journal file, 分别是rollback journal, statement journal, master journal. (它们被称为legacy journals. 在SQLite 3.7.0版本后, SQLite团队引入了WAL journaling模式. Database file能够处于这两种模式之一.) 我将在接下来的三章介绍legacy journals, 而关于WAL的内容, 将会被放到10.17节.
<br>

####3.3.1 Rollback journal
SQLite会对每个数据库维护一个rollback journal file. (In-memory database不需要). Rollback journal file总是和其database file在同一个文件夹下. 其名字为database file名字加上"-journal". journal文件在默认模式下只是临时文件, SQLite会在每次write-transaction的时候创建它们, 并在事务完成时删除. 

SQLite会将rollback journal file分成多个变长度的log segment, 如图3.6. 每段log segment开始于segment header, 接着是一个或多个log records.
![Pic3.6](/home/qw4990/桌面/SQLITE_BOOK/Pic3.6.png)

####3.3.1.1 Segment header结构
Segment header结构如图3.7. Segment header开始与8个特殊的bytes: 0xD9, 0xD5, 0x05, 0xF9, 0x20, 0xA1, 0x63, 0xD7. 我们将这个序列称为magic number. 这个序列是被随机选取的, 只是被用来做一点常规检查, 并没有什么卵用. Number of records (简称为nRec) 记录有多少个log segment, nRec在异步事务处理时会被初始化为-1, 在普通事务处理时被初始化为0. Random number被用来计算checksum. Initial database page count记录原database中一共有多少pages. Sector size表示database file所在磁盘扇区大小. Page size表示page大小. 剩下的空间留作备用. 

![Pic3.7](/home/qw4990/桌面/SQLITE_BOOK/Pic3.7.png)

<b>Sector size检测:</b> 其检测需要操作系统支持, 如果不能, 则默认为512bytes. SQLite认为操作系统只能以Sector为最小单位进行读写操作.

Rollback journal file通常只含有一个log segment. 但是某些情况下, 会存在多segment的文件, 此时SQLite会多次对segment header进行读写(我将在5.4.1.3讨论这情况). 每次进行segment header读写时, 都会以sector为单位. 

<b>Journal file保留: </b>默认的模式下, SQLite会在事务完成后删除journal files. 但是你能通过prama journal_mode指令来保留它们. journal_mode有DELETE, PERSIST, TRUNCATE三种, 默认是DELETE. 还有几种其他的journal_mode我会在之后进行介绍. 如果应用使用了exclusive locking 模式 (pragma locking_mode = exclusive), SQLite会创建并保留Journal file直到应用取消这个锁. 

<b>Journal的维护: </b>rollback journal只有当其含有合法的segment header时才有效. 无效的journal将不会被用于事务保护. 

<b>异步事务: </b>SQLite支持比普通事务更快的异步事务. 异步事务处理期间, SQLite不会刷新journal和database file, 并且journal file只会有一个log segment. 此时nRec会为-1. SQLite不建议使用异步事务, 但是你能够执行pragma指令来开启. 

####3.3.1.2 Log record结构
非SELECT语句都会产生log records. 在修改任何page之前, 原page的内容都会被写入journal的新log record中. 图3.8展示了一个log record的结构. 它包含了一个4-bytes的checksum. 这个checksum涵盖了page number到page content的内容. segment header中的random number被作为计算checksum的键值. 这个Random number非常重要, 如果某条log recorder的垃圾数据刚好来自同页的另外一份journal file, 那么其checksum就刚好是正确的. 而对不同的journal file使用不同的rondom number作为键值, 则能将这个风险最小化. 
![Pic3.8](/home/qw4990/桌面/SQLITE_BOOK/Pic3.8.png)

####3.3.2 Statement journal

####3.3.3 Multi-database transaction journal 和 master journal

####总结
SQLite把所有的数据信息存储到一个大文件内 (database file). 但同时也支持临时和in-memory数据库, 它们将会在应用打开时被建立, 并在应用关闭时被删除. 

每个database file都是一个固定页大小组织的文件. 逻辑上, database file就是一连串的pages. 默认的page大小是1024, 但是能被设置在512到65536之间, 同时其大小必须为2的次方. database file最多能包含2^31 - 2页. Pages被分为4种: free, tree, lock-byte, pointer-map. Tree page能被细分为internal, leaf, overflow. 

第一个page被成为anchor page. database file的前100bytes包含了整个数据库的一些基本信息, 比如page size等. 所有的free pages都被以rooted trunked tree结构组织. 

SQLite使用三种journal files: rollback, statement, master. rollback和master journal和database file在同个文件夹下, 而statement通常在一个临时目录, 比如/tmp. rollback journal存储变长的log records. 而master journal存储多数据库事务下所有rollback journal的名称. statement journal则为每条单独的insert/update/delete语句提供记录. 




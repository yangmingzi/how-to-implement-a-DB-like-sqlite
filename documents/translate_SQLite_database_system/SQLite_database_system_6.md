<h1>Tree模块</h1>

###本章目的
1. SQLite怎么用B+树组织表, 怎么用B-树组织索引.

2. 这些树结构是怎样被存储于database file中的.

3. B-/B+树的构造和维护算法.

4. Internal, leaf, overflow pages等数据结构.

###本章梗概
在数据库中, 数据能用各种方式来进行维护, 比如数据, 链表, hash表等. SQLite使用B+树来组织表的内容, 使用B-树来组织索引. 它们都是根据key值维护的数据结构. 本章讲了一种B+/B-树在数据库系统中较为通用的实现. 

###6.1 预览
在前一章中, 我们讨论了SQLite怎样在database file上实现了一层page的抽象. 本章中, 我们将探讨SQLite怎么在page上实现一层tuple的抽象. 而tuple抽象的用户则是更上层的VM模块, VM直接将database file看作一系列按照key值维护过的tuple.

本章描述了SQLite中的tuple管理结构. 关系型数据库中的数据都被存储为一条条tuple. 上层的VM更是直接将database file看作一系列tuple的组合. 因此VM必须能够快速的对tuple进行存储, 检索, 维护. 而Tree模块的作用就在于此, tree模块能快速的查找tuple, 并将其从page翻译成tuple传递给上层VM模块.

而光拿tuple来说, 其本身也能通过多种方式进行组织, hash表, 序列, 链表等. 每种组织方式都有其自己独特的机制. SQLite使用了B+树来存储某个关系的所有tuple(也就是一张表), 同时使用B-树来维护表上的索引. 从这个层面上来说, 一个SQLite数据库, 其实就是一系列的B+/B-树. 这些树都被存储在一个单独的大文件中(database file), 同时被按页分开, 也就是说不存在某一个页, 存储了多个B+树或者B-树的内容. 

B-和B+树是两种相似的数据结构. 在剩下的章节中, 我将主要介绍B+树. 在tree模块中, 主要实现了这些操作, 对tuple的读, 写, 删除, 插入. 

###6.2 接口函数
Tree模块为VM模块提供了一些接口函数, 这些函数都只应该在SQLite内部使用, SQLite的开发者不应该在自己的代码中调用这些函数. 这些函数都被定义在btree.c中, 且以sqlite3Btree作为前缀.

1. sqlite3BtreeOpen: 打开一个database file, 这个函数会调用sqlite3PagerOpen来打开文件. 函数结束时, 它会创建一个Btree结构体, 并返回给VM使用.

2. sqlite3BtreeClose: 这个函数关闭一个database connection, 同时销毁其对应的Btree结构体. 这个函数会调用sqlite3PagerClose用来关闭对应的Pager对象. 

3. sqlite3BtreeCursor: 这个函数在特定的树上创建一个新的cursor, 其实也就是标记了这棵树的某条记录, 以待读写操作. cursor能是read-cursor或者write-cursor, 但是不能同时两者都是. 一棵树上能同时打开多个cursor. 但是read-cursor和write-cursor不能同时存在在一棵树上. 在创建第一个cursor时, 这个函数会通过pager对database file上一把shared lock.

4. sqlite3BtreeCloseCursor: 这个函数关闭一个之前打开的cursor. shared lock会在最后一个cursor关闭时被释放. 

5. sqlite3BtreeClearCursor: 这个函数清楚所有的cursor.

6. sqlite3BtreeFirst: 将某个cursor移动到树的第一条记录, 也就是最左边的元素.

7. sqlite3BtreeLase: 将某个cursor移动到树的最后一条记录, 也就是最右边的元素. 

8. sqlite3BtreeNext: 将某个cursor移动到下一条记录.

9. sqlite3BtreePrevious: 将某个cursor移动到前一条记录.

10. sqlite3BtreeMovetoUnpacked: 将某个cursor移动到某个匹配key的元素上. 如果找不到这么一个元素, 那么cursor会指向匹配key的前一个或者后一个元素上.

11. sqlite3BtreeBeginTrans: 

12. sqlite3BtreeCommitPhaseOne:

13. sqlite3BtreeCommitPhaseTwo:

14. sqlite3BtreeRollback:

15. sqlite3BtreeBeginStmt:

16. sqlite3BtreeCreateTable: 在database file中新建一个空的表. 用于维护新表的树的类型(B / B+)通过参数决定. 当这个函数被调用时, 数据库上必须不存在任何cursor, 否则将会返回一个错误. 

17. sqlite3BtreeDropTable:

18. sqlite3BtreeClearTable:

19. sqlite3BtreeDelete: 删除cursor当前指向的记录. 删除后, 此cursor会随机指向一个位于删除记录左边的某条记录.

20. sqlite3BtreeInsert: 通过cursor, 向B+/B-树中插入新的记录. 新记录的key必须通过(pKey, nKey)给出, 新记录的数据必须通过(pData, nData)给出. 对于B+树, key中只有nKey会被用上, pKey被忽略. 而B-树中, pData和nData将被狐狸. cursor在这里只是用于支出新记录应该被插入到哪个表. 插入完成后, cursor会随机指向被插入的左边某个位置.

21. sqlite3BtreeKeySize: 返回cursor指向记录的key大小. 如果cursor非法, 返回0.

22. sqlite3BtreeKey: 返回cursor指向记录的key.

23. sqlite3BtreeDataSize: 返回cursor指向记录的数据大小, 如果cursor非法, 返回0.

24. sqlite3BtreeData: 返回cursor指向记录的数据.

25. sqlite3BtreeGetMeta: 

###6.3 B+树结构
B树, 其中'B'表示'balanced', 我觉得是目前为止数据库中最重要的数据结构. 它能够根据key值有序的组织数据. B树是一种高度平衡的n-叉树, n > 2, 它的所有叶节点在同一层. B树中, 数据和辅助信息(如key值)同时被存储于内部节点和叶节点. B树上的所有树操作都有很高的效率, 这些操作有插入, 删除, 查找, 查找下一个.
的数据被统一存放在叶节点. 其中记录被组织成了(key value, data value)键值对, 并且按照key的顺序被组织. 内部节点只包含用于辅助信息和儿子指针. 内部节点的key也是按照顺序组织的.

对于内部节点, 它们能有不同数量的儿子节点, 这些儿子节点的个数必须要在一个事先设置的区间内. 这个区间的下限通常是其上限的一般, 也就是可以被表示为[n, 2n]. 但是根节点可以打破这个限制, 它没有下限, 只有上限, 也就是它的儿子数可以是[0, 2n]. 所有的叶子节点都位于同样的高度, 同时它们之间也被相互串联. 根节点只能作为内部节点.

假设现在的儿子节点上限是n + 1, n > 1, 那么在每个内部节点中, 最多存放n个key, n + 1个儿子指针. key和儿子指针的逻辑组织如下图. 同时对于每个内部节点, 必须满足下面的条件:

1. 在Ptr(0)子树中的所有key, 必须小于等于Key(0).

2. 在Ptr(1)子树中的所有key, 必须小于等于Key(1). 以此类推.

3. 在Ptr(n)子树中的所有key, 必须大于Key(n - 1).

内部节点的key只能被用来做检索, 其复杂度是O(logm), 其中m是树中记录的总数.

###6.3.1 B+树上的操作

###6.3.2 SQLite中的B+树
Tree被创建的时候, 将会找到其存放根的那一页, 称为root page. 每棵树都通过其root page number识别. 每棵树的root page number被存放在master catalog table中, 这个表的root page number永远是1.

SQLite将树的节点存储与不同的页里, 并且一页存放一个节点. 实际的数据被存在leaf pages和overflow pages. 实际上tree模块并不知道数据的内容和key, 它只是把这些输入当作bytes流进行读写而已. 对于每个节点, key和data被组合成了一个payload. Tree会尝试在单个leaf page中存放尽量多的payload, 如果某个payload的大小大于了page size, 那么多出的内容会被存储在overflow page内. 内部节点, 也能有overflow page.

###6.4 Page结构
database file被分割成了许多大小固定的页. tree模块管理这些页, 每页要么是tree page(internal, leaf, overflow page), 要么是free page. 3.5节中已经展示了free page怎么被组织的. 本节中我主要讲internal, leaf, overflow page.
除了第一页, 其他页都能做任何类型的页使用. 第一页中的前100bytes为database file header, 剩下的内容则是B+树根节点的内容. 
###6.4.1 Tree page结构
逻辑上, internal page和leaf page被分割为cells. 比如在B+树的内部节点中, 一个cell就是一组key和其儿子指针的组合; 而对于叶节点, 一个cell就是一个payload. 我们会在6.4.1.3讨论cell. 在实际中, 一页内容被分割为4个部分:

1. page header

2. cell内容区

3. cell指针区

4. 未使用区域

如下图, cell指针区和cell内容区会随着使用逐渐增加. cell指针被用来维护cell的顺序. 

###6.4.1.1 Page header结构
Page header只包含了这一页的某些信息. Pager header的内容如下图给出. 其中flags字段指明当前页的类型, 可能是: (1)B+树internal page, (2)B+树leaf page, (3)B树internal page, (4)B树leaf page. 对于internal page, header中还包含了最右节点的指针. 

###6.4.1.2 Cell存储位置
Cells被存储在每页的末端位置, 它们会随着使用向页的开头进行扩展. Cell指针数组开始于page header结束的下一个bytes. 如下图, cell指针数组的大小被存储在page header中. 每个cell指针都是一个大小为2-bytes的整数, 标识了其cell相对于当前页开头位置的位移. Cell指针被有序存放, 相比之下cell内容本身或许不是有序的. 

因为我们会对页进行插入和删除cell的操作, 所以页内可能会有碎片空间. 这些碎片空间被通过链表组织在一起, 按照它们地址排序. 碎片空间链表头也被存储在page header中. 每块碎片空间至少有4bytes的大小. 每块碎片的前4bytes信息用来存储组织信息, 前2bytes是下一块碎片的位移, 后2bytes是当前碎片的大小. 因为碎片空间的大小最小需要4bytes, 更小的空间将无法被组织在碎片链表中, 这些更小碎片空间的总和也被记录在page header中, 这个值最多为255, 在达到这个值之前, SQLite会清理它们. 

###6.4.1.3 Cell结构
Cell实际上就是变长的字节流. 一个cell存储一份payload. Cell的结构如下图给出. 其中var(1-9)表示长度为1-9bytes的变长整数. 

对于internal page, 每个cell都包含一个4bytes的儿子指针; 对于leaf page, cell中不含有儿子指针. 接下来的两个数表示数据和key的大小. 因此, 总结一下:

1. 在table tree internal page中, cell包含4bytes的儿子指针(也就是儿子节点那一页的page number), 和一个变长整数的rowid. 

2. table tree leaf page的cell中, 包含了两个变长整数分别表示数据长度, rowid, 然后是本条记录的数据, 和4bytes的overflow page number.

3. index tree internal page的cell中, 包含了4bytes的儿子指针, 一个变长整数表示key的长度, 接着是一部分key的内容, 和overflow page number.

4. index tree leaf page的cell中, 包含了4bytes的儿子指针, 一个变长整数表示key的长度, 接着是一部分key的内容, 和overflow page number.

<b>变长整数: </b>SQLite中使用了变长整数. 变长整数由1-9个bytes组成, 在前1-8个byte中, 第一位bit用来标识是否还有下一个byte, 后7位bit用来组成实际数字. 而最后一个byte的全部8位bit都用来组成实际数字. 所以9个bytes长的变长整数最多能编码64个bit的整数. 这被称之为Huffman编码. SQLite使用变成整数而不是直接使用64个bit的原因是大多数情况下, 使用变长整数只需要用到1-2个bytes, 比直接使用64个bit的8byte节省了不少空间. 

SQLite在存储payload的时候有一个限制, 即使当前页有多余空间, payload或许也不会被完全存储在这一页内. Payload有一个单页内最大存储限制, 这个值被写在数据库文件头中, 21位移处. 如果payload大于了这个约束, 那么SQLite会将多的内容存在overflow page中. 

<b>Overflow计算: </b>假设p是payload大小, u是页内剩余空间. 
Leaf table-tree page: 如果(p < u - 36), 则将整个payload放入页内; 否则令M = (((u - 12) * 32 / 255) - 32); 存min{u - 35, (M + (p - M) % (u - 4))}到页中, 其余放在overflow page内. 
Internal table-tree page: 没有payload和overflow.
Leaf/Internal index-tree page: 如果(p < (((u - 12) * 64 / 255) - 22)), 将整个payload存入页内; 不然令M = (((u - 12) * 32 / 255) - 23); 存min{M + (p - M) % 4, (((u - 12) * 64 / 255) - 23)}到页内, 其余放overflow page.

###6.4.2 Overflow page结构
多个小的记录记录可以共存与一个页, 但是大的记录只能分割成多个overflow page, 这些overflow page被组织成一个链表. 除了链表的最后一页, 其他页都是放满了数据的. 除了数据, 这些页也需要一些必须的空间用来维护链表结构: 前4bytes存储下一页的页号. 最后一页可以不放满, 但是不能同时存储来自两个payload的信息.

###6.5 Tree模块功能
Tree模块帮助VM将所有的表和索引组织为B+/B-树. 由于B+/B-树的平衡性, 使得VM能够在O(logm)的复杂度下, 添加, 删除, 查找这些数据, 其中m为数据记录的总数.

###6.5.1 Contral data structure
Tree模块并不关心存储于其中的记录具体含义, 只有VM才会取解读这些记录. Tree模块通过Pager来访问数据文件, 同时它本身创建了4个对象(Btree, BtShared, MemPage, BtCursor), 用来管理内存中的页. 下面的四个章节我们会介绍这四个对象.

###6.5.1.1 Btree结构
当VM通过sqlite3BtreeOpen打开一个数据库文件时, tree模块就会创建一个Btree. VM只需要直到数据库文件被组织成了btree这么一个结构, 而不用关心其他细节. Btree主要有下面几个变量: 
(1) db: a pointer to the library connection holding this Btree object, 
(2) pBt: 一个指向BtShared的指针 
(3) inTrans: 一个标识符, 表示现在是否在进行事务操作 
(4) 其他的控制变量. inTrans这个变量决定了Btree的状态, 为下面三个之一: TRANS_NONE, TRANS_RAED, TRANS_WRITE. 

###6.5.1.2 BtShared结构
对于tree模块而言, BtShared就表示一个数据库文件. 这个结构体有下面这些变量: 
(1) pPager: 指向Pager的一个指针 
(2) pCursor: 一个链表, 维护了tree上被创建的cursor 
(3) pageSize: 页的大小 
(4) nTransaction: 树上事务的总数 
(5) inTransaction: 事务状态 
(6) pSchema: 指向schema cache的指针 
(7) db: a pointer to the library connection that is currently using this object
(8) pPage1: a pointer to the in-memory copy of MemPage object for database Page1.
(9) mutex: access synchronizer
(10) 其他变量.

###6.5.1.3 MemPage结构
如下图所示, 对于每个被缓存于内存中的页, pager会分配一些额外的空间给它们, 这些空间被Tree模块用来存储一些额外信息. Pager会在初始化时将这些信息全部设置为0, 这些空间就由结构MemPage来控制. 下图展示了这个结构的一些变量. 其中pParent变量指向了这一页的父页, 这使得我们能从任何一页回溯到这棵树的根节点. aData变量则指向了这一页实际存储的内容.

###6.5.1.4 BtCursor结构
为了操作数据库中某棵特殊的树, VM必须在树上打开一个cursor(通过sqlite3BtreeCursor()函数). Cursor逻辑上其实就是一个指针, 指向了树中某一条具体的数据. 对于每棵树, SQLite都会创建一个BtCursor, 用作在树上操作tuple的句柄. BtCursor不能被多个database connection共享.

下图展示了cursor的一些主要变量. apPage是一连串的MemPage对象, 包含了从根节点到当前节点的所有页. aiIdx[]包含了这些页对应的cell指针. eState标识这个cursor的状态: valid(指向了一条合法的记录), invalid(指向了非法的记录), requirseek(树已经被做了修改), fault(出现了某些错误).

VM能在同一棵树上打开多个cursor. 一个cursor要么是read-cursor, 要么是write-cursor, 但是不能同时为两者. 同时一棵树上不能同时存在read-cursor和write-cursor. 这样能够使得read-cursor能放心的读取树上内容并在树上游走, 而不用担心树被其他cursor做了修改. read-cursor只能读取, write-cursor能读取或者写入. SQLite只有在查询的时候会创建read-cursor, 其他情况都是创建write-cursor.

###6.5.1.5 整体控制架构
上述四种结构之间的联系如下图所示. Btree对象和其关联的其他几个对象, 完成了对下层数据库文件的抽象, VM将只通过Btree来对数据库文件打交道. 注意, 多个Btree能够共享一个BtShared对象. 

###6.5.2 空间管理
Tree模块会从VM接受插入和删除payload的指令, 而这两个指令都会涉及到空间的管理, 因此空间管理对tree模块来说至关重要, 在本小节我们将会讨论这个问题.

###6.5.2.1 Free pages的管理
当某页从树中被删除的时候, 这一页将会被添加到freelist以备后用. 当需要页的时候, tree会从freelist内取出这些页. 如果freelist为空, 那么将会向操作系统请求新的空间, 这些新的空间将会被附加到文件的尾部.

你能够通过vacunm指令来消除freelist. 这个指令能够减小数据库文件的大小. "autovacunm"模式下, 每次COMMIT后, SQLite都会自动执行vacunm指令.

###6.5.2.2 Page空间管理
在页中, 有三种类型的空余空间. 

1. cell指针区和cell数据区之间未用的区域.

2. 被通过链表组织起来的大块碎片空间.(大于等于4bytes的空间)

3. 无法被组织起来的小碎片空间. 但是这些小空间的综合被记录在page header中.

空间的分配主要针对前两种情况. 

<b>Cell分配 </b>
空间分配器不会分配小于4bytes的空间, 并且需求的空间大小会被向上平滑为4的倍数. 假设现在一个新请求想申请nRequired的空间, nRequired >= 4共nFree的空余空间. 如果nRequired > nFree, 那么返回失败. 否则执行下面几个操作:

1. 遍历碎片空间链表, 检查是否有某快碎片大于等于这块空间, 如果找到:
(a)如果找到的空间小于nRequired + 4, 那么将这块空间从碎片链表中删除, 分配出对应的空间使用, 并把剩下不能被分配的空间(这部分空间一定小于4bytes), 计算入小碎片空间的大小内
(b)否则, 直接将这块碎片空间的大小减少nRequired.

2. 如果找不到, 那么将会对这页做碎片整理, 将cell区域压到页空间底部, 并清除小碎片.

3. 接着从未使用空间中分出nRequired的空间.

<b>Cell销毁</b>
假设我们向销毁nFree大小的空间(nFree >= 4). 管理器会新建一个大小为nFree的碎片, 然后将其插入到碎片链表内. 在插入时, 同时尝试合并这块碎片上下相关的碎片空间. 如果在两块准备合并的空间中存在小碎片, 则同时将小碎片合并进来. 

###总结
Tree模块通过Pager和数据库文件打交道, 同时也在"页"之上建立了自己的抽象, 也就是"tuple", 并通过这个抽象, 为VM提供服务. 不管是表单, 还是索引, 都被视作一系列的tuple, 并分别通过B+树和B-树组织. 每页按照树上的类型, 能分为, internal, leaf, overflow page. 不会有某个页同时存储了多个树的内容. 

Tree模块树的创建和删除, 也实现了书上的tuple的读取, 查找, 删除. 然而这个模块并不会修改tuple, 而只是把tuple当作一系列字符流.

SQLite中使用的B+树算法能够在Knuth的著作"编程的艺术"第三卷的"排序和检索"中找到. Internal node存贮辅助查找的信息, 而leaf node存储实际的tuples. 每个内部节点都有一个子节点的上下限. 
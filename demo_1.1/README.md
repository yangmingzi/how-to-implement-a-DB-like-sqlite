在这个demo中, 我们会实现SQLite的后三层(OS层, Pager层, B-tree层)的基本功能.


注意, 仅仅完成基本功能, 不考虑多线程, 多进程, 事务, 回滚等功能的支持.


demo_1.1的重点在于:

1.熟悉SQLite对底三层的结构以及对应的抽象, 如VFS, Pager, SQLite_file等.

2.熟悉SQLite中Pager的cache方法.

3.熟悉B-tree在数据库管理中的基本应用.

4.对SQLite数据库整体后端有一个初步认识.

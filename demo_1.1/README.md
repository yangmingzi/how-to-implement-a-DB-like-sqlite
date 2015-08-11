在这个demo中, 我们会实现SQLite的后三层(OS层, Pager层, B-tree层)的基本功能.


注意, 仅仅完成基本功能, 不考虑多线程, 多进程, 事务, 回滚等功能的支持.


demo_1.1的重点在于:

1.熟悉SQLite对底三层的结构以及对应的抽象, 如VFS, Pager, SQLite_file等.

2.熟悉SQLite中Pager的cache方法.

3.熟悉B-tree在数据库管理中的基本应用.

4.对SQLite数据库整体后端有一个初步认识.

其他资料:
http://www.codeyj.com/SQLite%E6%BA%90%E7%A0%81%E6%94%BB%E7%95%A5&%E5%AE%9E%E7%8E%B0%E4%B8%80%E4%B8%AA%E6%95%B0%E6%8D%AE%E5%BA%93_BPlusTree%E4%B8%8EPager

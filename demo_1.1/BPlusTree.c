#include "BPlusTree.h"

///////////////////////////////////////////////////////////
// functions for operation on bytes
///////////////////////////////////////////////////////////

/*
  Translate integer to bytes to storage
  The integer can be byte, short, int type
*/
void int2Bytes(void *t, int offset, int size, int number) {
  assert(t);
  assert(size == 1 || size == 2 || size == 4);

  byte *bt = (byte *)t;
  if (size == 1) {
    byte bx = (byte)number;
    memcpy(bt + offset, &bx, 1);
  } else if (size == 2) {
    short sx = (short)number;
    memcpy(bt + offset, &sx, 2);
  } else if (size == 4) {
    memcpy(bt + offset, &number, 4);
  }
}

/*
  Translate bytes to integer and return it
  The integer can be byte, short, int type
*/
int bytes2Int(void *f, int offset, int size) {
  assert(f);
  assert(size == 1 || size == 2 || size == 4);

  byte *bf = (byte *)f;
  if (size == 1) {
    byte bx;
    memcpy(&bx, bf + offset, 1);
    return (int)bx;
  } else if (size == 2) {
    short sx;
    memcpy(&sx, bf + offset, 2);
    return (int)sx;
  } else if (size == 4) {
    int ix;
    memcpy(&ix, bf + offset, 4);
    return ix;
  }
}


/*
  the structure of each node:
  ------------------------------------------
  | node header - 10 bytes                 |
  ------------------------------------------
  | cell pointer arrary - 2 bytes per cell |
  ------------------------------------------
  | unallocated space                      |
  ------------------------------------------
  | cell content area                      |
  ------------------------------------------


  the structure of node header:
  Type         Variable            Description
  byte         is_leaf;            Flags. 1: internal node; 2: leaf
  short        cell_size;          Number of cells on thie node
  short        first_cell_pos;     First byte of the cell content area
  byte         frag_size;          Number of fragmented free bytes
  Node         *right_child;       Right child. Omitted on leaves


  the structure of a cell of a leaf:
  Size          Description
  4             Key
  4             Number of bytes of data
  *             Data


  the structure of a cell of a intnode:
  Size          Description
  4             Key
  4             Pointer to its child
*/

/*
  Because of memory alignment, we cannot calculate the size of 
  this structure by simply adding the size of its elements.
  So I define this structure to help to calculate HEADER_SIZE.
*/
struct FOR_CALCULATE_HEADER_SIZE {
  byte is_leaf;
  short cell_size;
  short first_cell_pos;
  byte frag_size;
  Node *right_child;
};
const int HEADER_SIZE = sizeof(FOR_CALCULATE_HEADER_SIZE);

/*
  The Node structure represent a node in B+Tree.
  THE_ORDER, the branching factor. https://en.wikipedia.org/wiki/B%2B_tree
  PAGE_SIZE is the total size of each node in bytes.
*/
struct Node {
  static const int THE_ORDER = 10;
  static const int PAGE_SIZE = 1448;

  byte is_leaf;
  short cell_size;
  short first_cell_pos;
  byte frag_size;
  Node *right_child;

  byte content[PAGE_SIZE - HEADER_SIZE];
}*root = 0;

/*
  The cell structure
*/
struct IntCell {
  Key key;
  Node *child_point;
};
struct LeafCell {
  Key key;
  int size;
  byte *data_point;
};

bool spaceCheck(Node *node, int size);

/*
  Create and return a new IntNode
*/
Node *getIntNode() {
  Node *p = new Node();
  p->is_leaf = 0;
  p->cell_size = 0;
  p->frag_size = 0;
  p->right_child = 0;
  p->first_cell_pos = Node::PAGE_SIZE;
  return p;
}

/*
  Get the k-th cell in the IntCell
*/
bool getKthIntCell(Node *node, int kth, IntCell *cell) {
  assert(node && cell);
  assert(node->is_leaf == 0);
  assert(kth >= 0 && kth < node->cell_size);  // kth start from 0

  int p_offset = HEADER_SIZE;
  p_offset += kth * 2;

  int cell_offset = bytes2Int(node, p_offset, 2);

  cell->key = bytes2Int(node, cell_offset, 4);
  cell->child_point = (Node *)bytes2Int(node, cell_offset + 4, 4);

  return 1;
}

/*
  Insert a IntCell into this IntNode
*/
bool insertIntCell(Node *node, Key k, Node *child) {
  assert(node && child);
  assert(node->is_leaf == 0);
  if (!spaceCheck(node, 8)) return 0;

  byte *bp = (byte *)node;
  struct IntCell cell;
  int kth = 0;
  while (kth < node->cell_size) {
    getKthIntCell(node, kth, &cell);
    if (cell.key > k) break;
    ++kth;
  }
  for (int i = node->cell_size - 1; i >= kth; --i) {
    byte *p = bp + HEADER_SIZE + i * 2;
    memcpy(p + 2, p, 2);
  }

  node->first_cell_pos -= 8;
  int2Bytes(bp, node->first_cell_pos, 4, k);
  int2Bytes(bp, node->first_cell_pos + 4, 4, (int)child);

  int2Bytes(bp, HEADER_SIZE + kth * 2, 2, node->first_cell_pos);

  ++node->cell_size;

  return 1;
}

/*
  Remove k-th cell in the IntNode
*/
bool removeKthIntCell(Node *node, int kth) {
  assert(node);
  assert(node->is_leaf == 0);
  assert(kth >= 0 && kth < node->cell_size);

  byte *bp = (byte *)node;
  for (int i = kth; i < node->cell_size; ++i) {
    byte *p = bp + HEADER_SIZE + i * 2;
    memcpy(p, p + 2, 2);
  }

  --node->cell_size;
  node->frag_size += sizeof(IntCell);

  return 1;
}

/*
  Check if the Node pointed by child is a child of the Node pointed by node
  If not, return -1
  Else return the rank of the child in its father
*/
int getKnumByChild(Node *node, Node *child) {
  assert(node && child);
  assert(node->is_leaf == 0);

  struct IntCell cell;
  for (int i = 0; i < node->cell_size; ++i) {
    getKthIntCell(node, i, &cell);
    if (cell.child_point == child) return i;
  }
  return -1;
}

/*
  Create and return a new LeafNode
*/
Node *getLeafNode() {
  Node *p = new Node();
  p->is_leaf = 1;
  p->cell_size = 0;
  p->frag_size = 0;
  p->right_child = 0;
  p->first_cell_pos = Node::PAGE_SIZE;
  return p;
}

/*
  Insert a IntCell into this IntNode
*/
bool getKthLeafCell(Node *node, int kth, LeafCell *cell) {
  assert(node && cell);
  assert(node->is_leaf == 1);
  assert(kth >= 0 && kth < node->cell_size);

  byte *bp = (byte *)node;
  int p_offset = HEADER_SIZE;
  p_offset += kth * 2;  // 2 bytes per pointer

  int cell_offset = bytes2Int(node, p_offset, 2);
  cell->key = bytes2Int(node, cell_offset, 4);
  cell->size = bytes2Int(node, cell_offset + 4, 4);
  cell->data_point = bp + cell_offset + 8;

  return 1;
}

/*
  Insert a IntCell into this IntNode
*/
bool insertLeafCell(Node *node, Key k, void *p_data, int size_data) {
  assert(node && p_data);
  assert(node->is_leaf == 1);
  if (!spaceCheck(node, size_data + 8)) return 0;

  byte *bp = (byte *)node;
  struct LeafCell cell;
  int kth = 0;
  while (kth < node->cell_size) {
    getKthLeafCell(node, kth, &cell);
    if (cell.key == k) return 0;  // reject duplicate insert
    if (cell.key > k) break;
    ++kth;
  }
  for (int i = node->cell_size - 1; i >= kth; --i) {
    byte *p = bp + HEADER_SIZE + i * 2;
    memcpy(p + 2, p, 2);
  }

  // insert new cell content
  node->first_cell_pos -= (size_data + 8);  // key + size + data
  int2Bytes(bp, node->first_cell_pos, sizeof(Key), k);
  int2Bytes(bp, node->first_cell_pos + 4, 4, size_data);
  memcpy((byte *)node + node->first_cell_pos + 8, p_data, size_data);

  // updata the pointer
  int2Bytes(bp, HEADER_SIZE + kth * 2, 2, node->first_cell_pos);

  ++(node->cell_size);

  return 1;
}

/*
  Remove k-th cell in the IntNode
*/
bool removeKthLeafCell(Node *node, int kth) {
  assert(node);
  assert(node->is_leaf == 1);
  assert(kth >= 0 && kth < node->cell_size);

  struct LeafCell cell;
  getKthLeafCell(node, kth, &cell);

  byte *bp = (byte *)node;
  for (int i = kth; i < node->cell_size; ++i) {
    byte *p = bp + HEADER_SIZE + i * 2;
    memcpy(p, p + 2, 2);
  }

  --node->cell_size;
  node->frag_size += 8 + cell.size;

  return 1;
}

/*
  Clean the fragmented free space in the node
*/
void spaceCleanUp(Node *node) {
  static Node *leafnode = getLeafNode();
  static Node *intnode = getIntNode();
  static Node *temp_node = getIntNode();
  static LeafCell leafcell;
  static IntCell intcell;

  if (node->is_leaf) {
    memcpy(temp_node, leafnode, Node::PAGE_SIZE);
    for (int i = 0; i < node->cell_size; ++i) {
      getKthLeafCell(node, i, &leafcell);
      insertLeafCell(temp_node, leafcell.key, leafcell.data_point, leafcell.size);
    }
  } else {
    memcpy(temp_node, intnode, Node::PAGE_SIZE);
    for (int i = 0; i < node->cell_size; ++i) {
      getKthIntCell(node, i, &intcell);
      insertIntCell(temp_node, intcell.key, intcell.child_point);
    }
    temp_node->right_child = node->right_child;
  }

  memcpy(node, temp_node, Node::PAGE_SIZE);
}

/*
  Check if the node has enough space to store size bytes
*/
bool spaceCheck(Node *node, int size) {
  if (node->frag_size > (1 << 6)) {
    spaceCleanUp(node);
  }

  if (node->first_cell_pos - HEADER_SIZE - 
      node->cell_size * 2 < size) {
    printf("%d\n", node->is_leaf);
    printf("%d\n", node->cell_size);
    printf("%d\n", node->first_cell_pos);
    puts("size error");
    exit(0);
  }

  return 1;
}

/*
  Search an entry with Key from tree
  If success, p_data is pointed to the data, size_data is 
  set to the size of data, and return 1
  If fail, return 0 and set p_data and size_data to 0
*/
bool search(Node *node, Key k, void *&p_data, int &size_data) {
  if (node->is_leaf) {
    struct LeafCell cell;
    for (int i = 0; i < node->cell_size; ++i)
      if (getKthLeafCell(node, i, &cell)) {
        if (cell.key == k) {
          p_data = cell.data_point;
          size_data = cell.size;
          return 1;
        }
      } 

      // not found
      size_data = 0;
      p_data = (void *)0;
      return 0;

  } else {
    struct IntCell cell;
    for (int i = 0; i < node->cell_size; ++i)
      if (getKthIntCell(node, i, &cell)) {
        if (k > cell.key) continue;
        else return search(cell.child_point, k, p_data, size_data);
      }
    return search(node->right_child, k, p_data, size_data);
  }
};

/*
  Splite a LeafNode to two nodes
*/
void splitLeafNode(Node *node, Node *&father) {
  assert(node);
  assert(node->is_leaf == 1);
  assert(node->cell_size >= Node::THE_ORDER);

  struct LeafCell cell;
  Node *new_node = getLeafNode();
  while (node->cell_size > Node::THE_ORDER / 2) {
    int kth = Node::THE_ORDER / 2;
    getKthLeafCell(node, kth, &cell);
    insertLeafCell(new_node, cell.key, cell.data_point, cell.size);
    removeKthLeafCell(node, kth);
  }

  getKthLeafCell(node, node->cell_size - 1, &cell);  // maximum key in left
  if (father) {
    int kth = getKnumByChild(father, node);
    if (kth == -1) {  // right child
      assert(node == father->right_child);

      insertIntCell(father, cell.key, node);
      father->right_child = new_node;
    } else {  // kth child
      struct IntCell intcell;
      getKthIntCell(father, kth, &intcell);
      removeKthIntCell(father, kth);

      insertIntCell(father, intcell.key, new_node);
      insertIntCell(father, cell.key, node);
    }
  } else {
    father = getIntNode();

    father->right_child = new_node;
    insertIntCell(father, cell.key, node);  
  }
}

/*
  Splite a IntNode to two nodes
*/
void splitIntNode(Node *node, Node *&father) {
  assert(node);
  assert(node->is_leaf == 0);
  assert(node->cell_size >= Node::THE_ORDER);

  struct IntCell cell;
  Node *new_node = getIntNode();
  while (node->cell_size > Node::THE_ORDER / 2 + 1) {
    int kth = Node::THE_ORDER / 2 + 1;
    getKthIntCell(node, kth, &cell);
    insertIntCell(new_node, cell.key, cell.child_point);
    removeKthIntCell(node, kth);
  }
  new_node->right_child = node->right_child;
  getKthIntCell(node, Node::THE_ORDER / 2, &cell);
  removeKthIntCell(node, Node::THE_ORDER / 2);
  node->right_child = cell.child_point;

  if (father) {
    int kth = getKnumByChild(father, node);
    if (kth == -1) {  // right child
      assert(node == father->right_child);

      insertIntCell(father, cell.key, node);
      father->right_child = new_node;
    } else {  // kth child
      struct IntCell intcell;
      getKthIntCell(father, kth, &intcell);
      removeKthIntCell(father, kth);

      insertIntCell(father, intcell.key, new_node);
      insertIntCell(father, cell.key, node);
    }
  } else {
    father = getIntNode();

    father->right_child = new_node;
    insertIntCell(father, cell.key, node);
  }
}

/*
  Help function of insert
*/
bool insert2(Node *node, Key k, void *p_data, int size_data, Node *&father) {
  if (node->is_leaf) {
    if (!insertLeafCell(node, k, p_data, size_data))
      return 0;  // duplicate insertion
    if (node->cell_size >= Node::THE_ORDER) splitLeafNode(node, father);
    return 1;
  } else {
    struct IntCell cell;
    for (int kth = 0; kth < node->cell_size; ++kth) {
      getKthIntCell(node, kth, &cell);
      if (k > cell.key) continue;
      if (!insert2(cell.child_point, k, p_data, size_data, node))
        return 0;
      goto INSERTED;
    }
    if (!insert2(node->right_child, k, p_data, size_data, node))
      return 0;

    INSERTED:;

    if (node->cell_size >= Node::THE_ORDER) splitIntNode(node, father);
    return 1;
  }
}

/*
  Insert the data pointed by p_data with the key to the B+tree
  The size_data represent the number of bytes of the data
  The B+Tree would copy the data to its node
  If there is the same key, B+Tree would igonre this insert operation 
  and return false
*/
bool insert(Tree *&root, Key k, void *p_data, int size_data) {
  Node *p = 0;

  if (!insert2(root, k, p_data, size_data, p))
    return 0;

  if (p) root = p;
  return 1;
}

/*
  Create a B+Tree and return its root pointer
*/
Tree *getTree() {
  return getLeafNode();
}


//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// functions for test
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
void printfNode(Node *node) {
  for (int i = 0; i < node->cell_size; ++i) {
    struct IntCell intcell;
    struct LeafCell leafcell;

    if (node->is_leaf) {
      getKthLeafCell(node, i, &leafcell);
      printf("%d ", leafcell.key);
    } else {
      getKthIntCell(node, i, &intcell);
      printf("%d ", intcell.key);
    }
  }

  puts("");
}

Node *getKthChild(Node *node, int kth) {
  if (kth == node->cell_size) return node->right_child;  

  struct IntCell intcell;
  getKthIntCell(node, kth, &intcell);
  return intcell.child_point;
}

void printfTree(Node *node, int indent) {
  for (int i = 0;i < node->cell_size; ++i) {
    struct IntCell intcell;
    struct LeafCell leafcell;

    if (node->is_leaf) {
      getKthLeafCell(node, i, &leafcell);
      for (int j = 0; j < indent; ++j) printf(" ");
      printf("%d\n", leafcell.key);
    } else {
      getKthIntCell(node, i, &intcell);
      printfTree(intcell.child_point, indent + 2);
      for (int j = 0; j < indent; ++j) printf(" ");
      printf("%d\n", intcell.key);
    }
  }

  if (node->is_leaf == 0) {
    if (node->right_child) {
      printfTree(node->right_child, indent + 2);
    } else {
      for (int j = 0; j < indent + 2; ++j) printf(" ");
      puts("null");
    }
  }
}
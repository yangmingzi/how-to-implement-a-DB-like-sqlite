#include "BPlusTree.h"
#include "pager.h"

void read(void *from, int off_set, int size, void *to) {
  memcpy(to, from + off_set, size);
}

void write(void *from, void *to, int off_set, int size) {
  memcpy(to + off_set, from, size);
}

static const int THE_ORDER = 10;
static const int PAGE_SIZE = 2048;
static const int HEADER_SIZE = 256;

struct Node {
  u8 is_leaf;
  u16 cell_size;
  u16 first_cell_pos;
  u8 frag_size;
  Pgno right_child;

  u8 content[PAGE_SIZE - HEADER_SIZE];
};

struct IntCell {
  Key key;
  Pgno child;
};

struct LeafCell {
  Key key;
  u32 size;
  u8 *data;
};

bool spaceCheck(Node *node, u32 size);

//////////////////////////////////////////////
// Functions for IntNode
//////////////////////////////////////////////
Node *getIntNode() {
  Node *p = new Node();
  p->is_leaf = 0;
  p->cell_size = 0;
  p->frag_size = 0;
  p->right_child = 0;
  p->first_cell_pos = PAGE_SIZE;
  return p;
}

void getKthIntCell(Node *node, int kth, IntCell *cell) {
  assert(node & cell);
  assert(node->is_leaf == 0);
  assert(kth >= 0 && kth < node->cell_size); // index from 0

  int p_offsert = HEADER_SIZE;
  p_offsert += kth * 2;                      // 2 bytes per pointer

  u16 cell_offset;
  read(node, p_offsert, 2, &cell_offset);

  read(node, cell_offset, 4, &cell->key);
  read(node, cell_offset + 4, 4, &cell->child);
}

bool insertIntCell(Node *node, Key k, Pgno child) {
  assert(node);
  assert(node->is_leaf == 0);
  if (!spaceCheck(node, 8)) return 0;

  u8 *bp = (u8 *)node;
  struct IntCell cell;
  int kth = 0;
  while (kth < node->cell_size) {
    getKthIntCell(node, kth, &cell);
    if (cell.key > k) break;
    ++kth;
  }

  for (int i = node->cell_size - 1; i >= kth; --i) {
    u8 *p = bp + HEADER_SIZE + i * 2;
    memcpy(p + 2, p , 2);
  }

  node->first_cell_pos -= 8;
  write(&k, bp, node->first_cell_pos, 4);
  write(&child, bp, node->first_cell_pos + 4, 4);
  write(&node->first_cell_pos, bp, HEADER_SIZE + kth * 2, 2);

  ++node->cell_size;
  return 1;
}

bool removeKthIntCell(Node *node, int kth) {
  assert(node);
  assert(node->is_leaf == 0);
  assert(kth >= 0 && kth < node->cell_size);

  u8 *bp = (u8 *)node;
  for (int i = kth; i < node->cell_size; ++i) {
    u8 *p = bp + HEADER_SIZE + i * 2;
    memcpy(p, p + 2, 2);
  }

  --node->cell_size;
  node->frag_size += sizeof(IntCell);

  return 1;
}

int getKnumByChild(Node *node, Pgno child) {
  assert(node);
  assert(node->is_leaf == 0);

  struct Intcell cell;
  for (int i = 0; i < node->cell_size; ++i) {
    getKthIntCell(node, i, &cell);
    if (cell.child_point == child) return i;
  }

  return -1;
}

//////////////////////////////////////////////////////////
// Functions for leaf node
//////////////////////////////////////////////////////////
Node *getLeafNode() {
  return 0;
}

bool getKthLeafCell(Node *node, int kth, LeafCell *cell) {
  assert(node && cell);
  assert(node->is_leaf == 1);
  assert(kth >= 0 && kth < node->cell_size);

  u8 *bp = (u8 *)node;
  int p_offset = HEADER_SIZE;
  p_offset += kth * 2;  // 2 bytes per pointer

  int cell_offset;
  read(node, p_offset, 2, &cell_offset);

  read(node, cell_offset, 4, &cell->key);
  read(node, cell_offset + 4, 4, &cell->size);
  cell->data_point = bp + cell_offset + 8;

  return 1;
}

bool insertLeafCell(Node *node, Key k, void *p_data, int size_data) {
  assert(node && p_data);
  assert(node->is_leaf == 1);
  if (!spaceCheck(node, size_data + 8)) return 0;

  u8 *bp = (u8 *)node;
  struct LeafCell cell;
  int kth = 0;
  while (kth < node->cell_size) {
    getKthLeafCell(node, kth, &cell);
    if (cell.key == k) return 0;                  // reject duplicate insertion
    if (cell.key > k) break;
    ++kth;
  }

  for (int i = node->cell_size - 1; i >= kth; --i) {
    u8 *p = bp + HEADER_SIZE + i * 2;
    memcpy(p + 2, p, 2);
  }

  node->first_cell_pos -= (size_data + 8);
  write(&k, bp, node->first_cell_pos, sizeof(Key));
  write(&size_data, bp, node->first_cell_pos + 4, 4);
  memcpy((u8 *)node + node->first_cell_pos + 8, p_data, size_data);

  write(&node->first_cell_pos, bp, HEADER_SIZE + kth * 2, 2);

  ++node->cell_size;
  return 1;
}

bool removeKthLeafCell(Node *node, int kth) {
  assert(node);
  assert(node->is_leaf == 1);
  assert(kth >= 0 && kth < node->cell_size);

  struct LeafCell cell;
  getKthLeafCell(node, kth, &cell);

  u8 *bp = (u8 *)node;
  for (int i = kth; i < node->cell_size; ++i) {
    u8 *p = bp + HEADER_SIZE + i * 2;
    memcpy(p, p + 2, 2);
  }

  --node->cell_size;
  node->frag_size += (8 + cell.size);

  return 1;
}

void spaceCleanUp(Node *node) {
  static Node *leafnode = getLeafNode();
  static Node *intnode = getIntNode();
  static Node *temp_node = getIntNode();
  static LeafCell leafcell;
  static IntCell intcell;

  if (node->is_leaf) {
    memcpy(temp_node, leafnode, PAGE_SIZE);
    for (int i = 0; i < node->cell_size; ++i) {
      getKthLeafCell(node, i, &leafcell);
      insertLeafCell(temp_node, leafcell.key, leafcell.data_point, leafcell.size);
    }
  } else {
    memcpy(temp_node, intnode, PAGE_SIZE);
    for (int i = 0; i < node->cell_size; ++i) {
      getKthIntCell(node, i, &intcell);
      insertIntCell(temp_node, intcell.key, intcell.child_point);
    }
    temp_node->right_child = node->right_child;
  }

  memcpy(node, temp_node, PAGE_SIZE);
}

bool spaceCheck(Node *node, int size) {
  if (node->frag_size > (1 << 6)) {
    spaceCleanUp(node);
  }

  if (node->first_cell_pos - HEADER_SIZE - node->cell_size * 2 < size) {
    printf("%d\n", node->is_leaf);
    printf("%d\n", node->cell_size);
    printf("%d\n", node->first_cell_pos);
    puts("size error");
    exit(0);
  }

  return 1;
}

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
      if (!insert2(cell.child, k, p_data, size_data, node))
        return 0;
      goto INSERTED;
    }
    if (!insert2(node->right_child, k, p_data, size_data, node))
      return 0;

    INSERTED:;

    if (node->cell_size >= THE_ORDER) splitIntNode(node, father);
    return 1;
  }
}

bool insert(Tree *&root, Key k, void *p_data, int size_data) {
  Node *p = 0;

  if (!insert2(root, k, p_data, size_data, p))
    return 0;

  if (p) root = p;
  return 1;
}

Tree *getTree(Pager *pager) {

  return getLeafNode();
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// functions for test
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// void printfNode(Node *node) {
//   for (int i = 0; i < node->cell_size; ++i) {
//     struct IntCell intcell;
//     struct LeafCell leafcell;

//     if (node->is_leaf) {
//       getKthLeafCell(node, i, &leafcell);
//       printf("%d ", leafcell.key);
//     } else {
//       getKthIntCell(node, i, &intcell);
//       printf("%d ", intcell.key);
//     }
//   }

//   puts("");
// }

// Node *getKthChild(Node *node, int kth) {
//   if (kth == node->cell_size) return node->right_child;  

//   struct IntCell intcell;
//   getKthIntCell(node, kth, &intcell);
//   return intcell.child_point;
// }

// void printfTree(Node *node, int indent) {
//   for (int i = 0;i < node->cell_size; ++i) {
//     struct IntCell intcell;
//     struct LeafCell leafcell;

//     if (node->is_leaf) {
//       getKthLeafCell(node, i, &leafcell);
//       for (int j = 0; j < indent; ++j) printf(" ");
//       printf("%d\n", leafcell.key);
//     } else {
//       getKthIntCell(node, i, &intcell);
//       printfTree(intcell.child_point, indent + 2);
//       for (int j = 0; j < indent; ++j) printf(" ");
//       printf("%d\n", intcell.key);
//     }
//   }

//   if (node->is_leaf == 0) {
//     if (node->right_child) {
//       printfTree(node->right_child, indent + 2);
//     } else {
//       for (int j = 0; j < indent + 2; ++j) printf(" ");
//       puts("null");
//     }
//   }
// }
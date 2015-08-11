#ifndef _B_PLUS_TREE_H_
#define _B_PLUS_TREE_H_


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char byte;
typedef int Key;
typedef struct Node Node;
typedef struct Node Tree;

/*
  Search in B+tree pointed by t with the key
  If not found, return false and set p_data and size_data to 0
  Else set p_data to the data, set size_data to the number of bytes of the
  data and return true
*/
bool search(Tree *t, Key k, void *&p_data, int &size_data);

/*
  Insert the data pointed by p_data with the key to the B+tree
  The size_data represent the number of bytes of the data
  The B+Tree would copy the data to its node
  If there is the same key, B+Tree would igonre this insert operation 
  and return false
*/
bool insert(Tree *&t, Key k, void *p_data, int size_data);

/*
  Remove operation, not implement yet
*/
//bool remove(Tree *&t, Key k);

/*
  Create a B+Tree and return its root pointer
*/
Tree *getTree();


/*
 functions for test
*/
void printfNode(Node *node);
void printfTree(Tree *root, int indent = 0);
Node *getKthChild(Node *node, int kth);

#endif
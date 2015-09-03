/*
This header file defines the interface of pager layer. 
The pager layer reads and writes a file a page at a time, and provide 
page cache for upper layer.
*/

#ifndef _PAGER_H_
#define _PAGER_H_

#include "sqlite.h"

// default page size, 1024 bytes
#define SQL_DEFAULT_PAGE_SIZE 1024

// size of database header
#define SQL_DATABASE_HEADER_SIZE 0

// The type used to represent a page number. The first page in 
// a file is called page 1. And 0 is used to represent "not a page".
typedef unsigned int Pgno;

// Each open file is managed by a separate instance of "Pager".
typedef struct Pager Pager;

// Handle type for pages
typedef struct PgHdr DbPage;


///////////////////////////////////////////////////////////////
// APIs for pager layer
///////////////////////////////////////////////////////////////

// Open and close a Pager connection. 
int pagerOpen(
  SqlVFS *,              // The VFS to use to open the file
  Pager **ppPager,       // OUT: return the Pager structure here
  const char*,           // Name of the database file to open
  int,                   // Extra bytes append to each in-memory page
  int                    // flags passed through to SqlVFS.xOpen() 
  );

// int pagerClose(Pager *pPager);

// Functions used to obtain and release page references.
int pagerGet(Pager *pager, Pgno pgno, DbPage **pppage);

// Release a page reference.
int pagerUnref(DbPage *);

// Operations on page references.
int pagerWrite(DbPage *);

// Sync database file for pager.
int pagerCommit(Pager *);

#endif
/*
** This header file defines the interface that the SQLite library
** presents to client programs.  If a C-function, structure, datatype,
** or constant definition does not appear in this file, then it is
** not a published API of SQLite, is subject to change without
** notice, and should not be referenced by programs that use SQLite.
*/

#ifndef _SQLITE_H_
#define _SQLITE_H_


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>


//////////////////////////////////////
//  error codes
//////////////////////////////////////
#define SQL_OK  0
#define SQL_ERROR 1
#define SQL_NOMEM 2 


//////////////////////////////////////
//  define for vfs
//////////////////////////////////////
#define SQL_DEFAULT_FILE_CREATE_MODE  744


// //////////////////////////////////////
// //  define for pager
// //////////////////////////////////////
// #define SQL_FILE_HEADER_SIZE  100
// #define SQL_PAGE_SIZE_OFFSET  0
// #define SQL_PAGE_SIZE_SIZE  4


typedef struct SqlIOMethods SqlIOMethods;
typedef struct SqlFile SqlFile;
typedef struct SqlVFS SqlVFS;

/*
  An object represents an open file in OS layer. The p_methods is a pointer 
  to an [SqlIOMethods] object that defines methods for performing IO 
  operation on this file.
*/
struct SqlFile
{
  //methods for the open file
  const struct SqlIOMethods *p_methods;
};


/*
  Every file opened by [SqlVFS.xOpen] method populates an [SqlFile] object 
  with a pointer to an instance of this object. This object defines the 
  methods used to perform various operations against the open file represented 
  by the [SqlFile] object.

  All functions in this structure return SQL_OK if success, or SQL_ERROR 
  otherwise for simplify.
*/ 
struct SqlIOMethods
{
  int (*xClose)(SqlFile *);
  int (*xRead)(SqlFile *, void *buf, int buf_sz, int offset);
  int (*xWrite)(SqlFile *, const void *content, int sz, int offset);
  int (*xTruncate)(SqlFile *, int sz);
  int (*xSync)(SqlFile *);
  int (*xFileSize)(SqlFile *, int *ret);
};


/*
  An instance of this object defines the interface between the Database core 
  and the underlying operating system. The "VFS" stands for "virtual file 
  system". 

  The file_size is the size of [SqlFile] sturcture used by this VFS. 
  max_filename is the maximum length of a pathname in this VFS.
  the vfs_name holds the name of this VFS.
  The custom_data is a pointer to a space used to store custom data.

  All functions in this structure return SQL_OK if success, or SQL_ERROR 
  otherwise for simplify.
*/
struct SqlVFS
{
  int sz_file;  //size of SqlFile
  int max_filename;  //maximun filename length
  const char *vfs_name;  //name of this vfs
  void *custom_data;  //used to store custom data

  int (*xOpen)(SqlVFS *, const char *filename, SqlFile *ret, int flags);
  int (*xDelete)(SqlVFS *, const char *filename);
  int (*xAccess)(SqlVFS *, const char *filename, int flag, int *ret);
  int (*xSleep)(SqlVFS *, int micro_secs);
  int (*xCurrentTime)(SqlVFS *, int *ret);
};

/*
This type is opaque. It is implemented by the
pluggable module. The SQLite core has no knowledge of 
its size or internal structure. It used to pass the 
pluggable cache module to SQLite core.
*/
typedef struct SqlPCache SqlPCache;

/*

*/
typedef struct SqlPCachePage SqlPCachePage;
struct SqlPCachePage {
  void *content;
  void *extra;
};

/*
*/
typedef struct SqlPCacheMethods SqlPCacheMethods;
struct SqlPCacheMethods {
  SqlPCache *(*xCreate)(int sz_page, int sz_extra, int mx_pages);
  SqlPCachePage *(*xGet)(SqlPCache *, unsigned int key);
  SqlPCachePage *(*xFetch)(SqlPCache *, unsigned int key);
  void (*xUnpin)(SqlPCachePage *);
};

#endif
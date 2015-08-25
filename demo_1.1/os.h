/*
  This header file attempt to abstract the underlying operating system
  so that the Database library will work on every system you like.
*/


#ifndef _OS_H_
#define _OS_H_

#include "sqlite.h"


/////////////////////////////////////////////////////////////
// objects for os
/////////////////////////////////////////////////////////////
//forward declares



/////////////////////////////////////////////////////////////////
//  functions for accessing files
//
//  All functions in this structure return SQL_OK if success, or SQL_ERROR 
//  otherwise for simplify.
/////////////////////////////////////////////////////////////////
int osClose(SqlFile *);
int osRead(SqlFile *, void *buf, int buf_sz, int offset);
int osWrite(SqlFile *, const void *content, int sz, int offset);
int osTruncate(SqlFile *, int sz);
int osSync(SqlFile *);
int osFileSize(SqlFile *, int *ret);


/////////////////////////////////////////////////////////////////
//  functions for accessing virtual file system
//
//  All functions in this structure return SQL_OK if success, or SQL_ERROR 
//  otherwise for simplify.
/////////////////////////////////////////////////////////////////
int osOpen(SqlVFS *, const char *filename, SqlFile *ret, int flags);
int osDelete(SqlVFS *, const char *filename);
int osAccess(SqlVFS *, const char *filename, int flag, int *ret);
int osSleep(SqlVFS *, int micro_secs);
int osCurrentTime(SqlVFS *, int *ret);


//////////////////////////////////////////////////////////////////
//  functions for getting file and VFS handles
//
//  return a pointer if success, or NULL otherwise
//////////////////////////////////////////////////////////////////
SqlFile *osGetFileHandle(SqlVFS *vfs);
SqlVFS *osGetVFS(const char *vfs_name);


#endif
/*
  This header file attempt to abstract the underlying operating system
  so that the Database library will work on every system you like.
*/


#ifndef _OS_H_
#define _OS_H_


/////////////////////////////////////////////////////////////
// objects for os
/////////////////////////////////////////////////////////////
//forward declares
typedef struct SqlIOMethods SqlIOMethods;
typedef struct SqlFile SqlFile;
typedef struct SqlVFS SqlVFS;


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
  int file_size;  //size of SqlFile
  int max_filename;  //maximun filename length
  const char *vfs_name;  //name of this vfs
  void *custom_data;  //used to store custom data

  int (*xOpen)(SqlVFS *, const char *filename, SqlFile *ret, int flags);
  int (*xDelete)(SqlVFS *, const char *filename);
  int (*xAccess)(SqlVFS *, const char *filename, int flag, int *ret);
  int (*xSleep)(SqlVFS *, int micro_secs);
  int (*xCurrentTime)(SqlVFS *, int *ret);
};


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
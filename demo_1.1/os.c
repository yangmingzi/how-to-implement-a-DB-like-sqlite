#include "os.h"
#include "base.h"

/////////////////////////////////////////////////////////////////
//  functions for accessing files
/////////////////////////////////////////////////////////////////
int osClose(SqlFile *file) {
  return file->p_methods->xClose(file);
}

int osRead(SqlFile *file, void *buf, int buf_sz, int offset) {
  return file->p_methods->xRead(file, buf, buf_sz, offset);
}

int osWrite(SqlFile *file, const void *content, int sz, int offset) {
  return file->p_methods->xWrite(file, content, sz, offset);
}

int osTruncate(SqlFile *file, int sz) {
  return file->p_methods->xTruncate(file, sz);
}

int osSync(SqlFile *file) {
  return file->p_methods->xSync(file);
}

int osFileSize(SqlFile *file, int *ret) {
  return file->p_methods->xFileSize(file, ret);
}


/////////////////////////////////////////////////////////////////
//  functions for accessing virtual file system
/////////////////////////////////////////////////////////////////
int osOpen(SqlVFS *vfs, const char *filename, SqlFile *ret, int flags) {
  return vfs->xOpen(vfs, filename, ret, flags);
}

int osDelete(SqlVFS *vfs, const char *filename) {
  return vfs->xDelete(vfs, filename);
}

int osAccess(SqlVFS *vfs, const char *filename, int flag, int *ret) {
  return vfs->xAccess(vfs, filename, flag, ret);
}

int osSleep(SqlVFS *vfs, int micro_secs) {
  return vfs->xSleep(vfs, micro_secs);
}

int osCurrentTime(SqlVFS *vfs, int *ret) {
  return vfs->xCurrentTime(vfs, ret);
}

SqlFile *osGetFileHandle(SqlVFS *vfs) {
  SqlFile *p = (SqlFile *)malloc(vfs->file_size);
  return p;
}

SqlVFS *osGetVFS(const char *vfs_name) {
  if (strcmp(vfs_name, "unix") == 0) 
    return (SqlVFS *)unixGetOS();
  return 0;
}
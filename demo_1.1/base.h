#ifndef _BASE_H_
#define _BASE_H_


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


//////////////////////////////////////
//  error codes
//////////////////////////////////////
#define SQL_OK  0
#define SQL_ERROR  -1


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

#endif
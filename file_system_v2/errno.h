#ifndef errno_h
#define errno_h

#define FILE_OPEN_ERR       1
#define FILE_NOT_EXSIT      2
#define CHECK_SUM_INCORRECT 3
#define FILE_CREATE_ERR     4
#define NO_FREE_INODE       5
#define INODE_TABLE_NOT_FREE 6
#define ADD_BLOCK_ERR       7
#define ALLOC_BLOCK_ERR     8
#define ALLOC_INODE_ERR     9
#define NO_SUCH_DIRECTORY   10
#define USERS_FULL          11
#define PERMISSION_DENIED   12
#define INCOMPATIBLE_VERSION 13
#define FILE_EXISTED        14
#define FILE_NOT_EXISTED    15
#define UNKNOWN_ERROR       0

int errno;

#endif /* errno_h */

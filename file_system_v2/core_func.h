#ifndef core_func_h
#define core_func_h

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <iostream>
#include "IO_operations.h"
#include "errno.h"
#include "Shell.h"
#include <termios.h>
#include <vector>

using std::vector;

#define VERSION             "LESLIE_FS_V1.1"
#define USER_NUM_MAX        12
#define USER_NAME_LEN_MAX   64
#define FILE_SIZE           135233  // in kb. 1 reserve block and 32 block group(4226 kb per group)
#define BLOCKS_OF_INODE     128     // 8 inodes per block, so 128 blocks contains 1024 incodes
#define GROUP_SIZE          4327424 // in byte, 4226 kb
#define BLOCK_SIZE          1024    // in byte, 1kb
#define INODE_SIZE          128
#define DIR_ENTRY_SIZE      128
#define INODES_PER_BLOCK    8

#define acl_rwx     0x15
#define acl_r       0x10
#define acl_w       0x04
#define acl_x       0x01
#define acl_root    "\x15\x10\x10\x10\x10\x10\x10\x10\x10\x10"
#define acl_public  "\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15"
#define acl_private "\x15\x00\x00\x00\x00\x00\x00\x00\x00\x00"

extern FILE *file;
extern struct super_block sup_blk;
extern struct user_info logged_user;

struct user_info {
    unsigned short user_id;
    char user_name[USER_NAME_LEN_MAX];
    char password[16];
};

struct reserved_block {
    char check_sum[16];
    struct user_info user_info[USER_NUM_MAX];
    char version[24];
};

struct super_block {
    int s_inodes_count;
    int s_blocks_count;
    int s_free_inode_count;
    int s_free_block_count;
    int s_inodes_per_group;
    int s_blocks_per_group;
};

struct group_desc {
    int bg_free_block_count;
    int bg_free_inodes_count;
    int bg_user_dirs_count;
    int inode_bit_map[32];
    int data_block_bit_map[128];
};

struct inode {
    unsigned short i_uid;   // user id
    unsigned short i_flag;  // inode state
    int i_size;             // the file size, in bytes
    time_t i_atime;         // the last time visiting the file
    time_t i_ctime;         // the last time changing the inode
    time_t i_mtime;         // the last time modifying the file
    time_t i_dtime;         // the time deleting the file
    int i_block_count;      // the block number of this file
    /*
     * an array indicating block address
     * no.0 ~ 13 are direct index
     * no.14 is one level indirect index
     * no.15 is two levels indirect index
     */
    int i_block[16];
    unsigned char i_acl[10];    // store the file's access control lists;
    unsigned char i_type;       // buzhidao
};

struct dir_entry {
    int inode;
    int file_type;
    char name[120];
};

enum file_type { UNKNOWN, COMMON, DIR, CHARDEV, BLOCKDEV, NAMEDTUBE, SOCKET, SYMLINK };

bool fileFormat(const char *filename);
int createUser(const char *username, const char *password);
bool changePSW(short uid, const char *newPSW);
bool chmod(short uid, char flag, const char *filePath);
bool initDir(void);
int makeDir(struct inode &cur_dir_inode, const char* dirName);
int allocInode(int fileType);
int seekInode(void);
void initInode(struct inode &inode, int fileType);
int seekFreeDirEntry(struct inode &inode);
int addDataBlock(struct inode &inode);
int allocDataBlock(void);
int seekBlock(void);
bool systemInit(const char* filePath);
void genCheckSum(char *checkSum);
int get_current_dir_inode();
void get_current_dir_name(char *name);

bool login(const char *username, const char *password);
bool logout();

bool changeDir(const char* dir_name);
int seekDirItem(int inode_num, const char *name);

int createFile(const char *path);
int insertFileDir(struct inode &cur_dir_inode, const char* filename);

bool deleteFile(const char *path);
bool deleteDir(const char *path);

void listFile(void);
size_t writeFile(const char *filePath, const char *buf, size_t size, size_t pos);
size_t readFile(const char* filePath, char *buf, size_t size, size_t pos);
bool openFile(const char* filePath, char flag);
int analysisPath(const char *path);

bool ckPms(struct inode inode, char flag);
char getAclMaskCode(const char *flags);

int copyFile(const char *filepath, const char *dest);
#endif /* core_func_h */

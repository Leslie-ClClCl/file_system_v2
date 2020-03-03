#ifndef IO_operations_h
#define IO_operations_h

#include "core_func.h"
using std::vector;
extern struct winsize size;

int get_data_block_offset(int data_block_number);

void readSuperBlock(struct super_block &sup_blk);
void writeSuperBlock(struct super_block &sup_blk);

void readInode(int inode_num, struct inode &inode);
void writeInode(int inode_num, struct inode &inode);

void readGroupDesc(int group_num, struct group_desc &group_desc);
void writeGroupDesc(int group_num, struct group_desc &group_desc);

void readDirEntry(int block_num, int dir_entry_no, struct dir_entry &dir_entry);
void writeDirEntry(int block_num, int dir_entry_no, struct dir_entry &dir_entry);

void readUserInfo(int uid, struct user_info &user_info);
void writeUserInfo(int uid, struct user_info &user_info);

bool writeDataBlock(int block_num, const char* buf, size_t size);

bool cmpMD5(const char *code1, const char *code2);

size_t writeData(struct inode &inode, const char *buf, size_t size, size_t pos);
size_t readData(struct inode &inode, char *buf, size_t size, size_t pos);

void getWindowSize(struct winsize &size);
void ls_output(vector<std::string> file_v, vector<std::string> dir_v);

std::string center_aligned (std::string text);
#endif /* IO_operations_h */

#include "IO_operations.h"
#include<sys/ioctl.h>
#include<unistd.h>

struct winsize size;
using std::string;

int get_inode_offset(int inode_number) {
    int inode_table = inode_number / INODES_PER_BLOCK;
    int in_table = inode_number % INODES_PER_BLOCK;
    int group_number = inode_table / 128;
    int in_group = inode_table % 128;
    int inode_offset = GROUP_SIZE*group_number + (3+in_group)*BLOCK_SIZE + in_table*INODE_SIZE;
    return inode_offset;
}

int get_inode_table_offset(int inode_table_number) {
    int group_number = inode_table_number / 128;
    int in_group = inode_table_number % 128;
    int inode_table_offset = group_number*GROUP_SIZE + (in_group+3)*BLOCK_SIZE;
    return inode_table_offset;
}

int get_data_block_offset(int data_block_number) {
    int group_num = data_block_number / sup_blk.s_blocks_per_group;
    int in_group = data_block_number % sup_blk.s_blocks_per_group;
    int data_block_offset = group_num * GROUP_SIZE + (131+in_group) * BLOCK_SIZE;
    return data_block_offset;
}

int get_dir_entry_offset(int blk_num, int dir_num) {
    int blk_offset = get_data_block_offset(blk_num);
    int dir_offset = blk_offset + dir_num * 128;
    return dir_offset;
}

void readSuperBlock(struct super_block &sup_blk) {
    fseek(file, BLOCK_SIZE, SEEK_SET);
    fread(&sup_blk, sizeof(struct super_block), 1, file);
}

void writeSuperBlock(struct super_block &sup_blk) {
    int group_count = 0;
    while (group_count < 32) {
        fseek(file, BLOCK_SIZE + GROUP_SIZE * group_count, SEEK_SET);
        fwrite(&sup_blk, sizeof(struct super_block), 1, file);
        group_count++;
    }
}

void readInode(int inode_num, struct inode &inode) {
    fseek(file, get_inode_offset(inode_num), SEEK_SET);
    fread(&inode, sizeof(struct inode), 1, file);
    return;
}

void writeInode(int inode_num, struct inode &inode) {
    fseek(file, get_inode_offset(inode_num), SEEK_SET);
    fwrite(&inode, sizeof(struct inode), 1, file);
    return;
}

void readGroupDesc(int group_num, struct group_desc &group_desc) {
    fseek(file, group_num*GROUP_SIZE + 2*BLOCK_SIZE, SEEK_SET);
    fread(&group_desc, sizeof(struct group_desc), 1, file);
    return;
}

void writeGroupDesc(int group_num, struct group_desc &group_desc) {
    fseek(file, group_num*GROUP_SIZE + 2*BLOCK_SIZE, SEEK_SET);
    fwrite(&group_desc, sizeof(struct group_desc), 1, file);
    return;
}

void readDirEntry(int block_num, int dir_entry_no, struct dir_entry &dir_entry) {
    fseek(file, get_data_block_offset(block_num) + dir_entry_no*DIR_ENTRY_SIZE, SEEK_SET);
    fread(&dir_entry, sizeof(struct dir_entry), 1, file);
    return;
}

void writeDirEntry(int block_num, int dir_entry_no, struct dir_entry &dir_entry) {
    fseek(file, get_data_block_offset(block_num) + dir_entry_no*DIR_ENTRY_SIZE, SEEK_SET);
    fwrite(&dir_entry, sizeof(struct dir_entry), 1, file);
    return;
}

void readUserInfo(int uid, struct user_info &user_info) {
    fseek(file, 16+uid*82, SEEK_SET);
    fread(&user_info, sizeof(struct user_info), 1, file);
    return;
}

void writeUserInfo(int uid, struct user_info &user_info) {
    fseek(file, 16+uid*82, SEEK_SET);
    fwrite(&user_info, sizeof(struct user_info), 1, file);
    return;
}

bool readDataBlock(int block_num, char *buf, size_t size) {
    if (size > 1024)
        return false;
    fseek(file, get_data_block_offset(block_num), SEEK_SET);
    fread(buf, size, 1, file);
    return true;
}

bool writeDataBlock(int block_num, const char* buf, size_t size) {
    if (size > 1024) return false;
    fseek(file, get_data_block_offset(block_num), SEEK_SET);
    fwrite(buf, size, 1, file);
    return true;
}

bool cmpMD5(const char *code1, const char *code2) {
    int count = 0;
    while (count < 16) {
        if (code1[count] != code2[count]) {
            return false;
        }
        count++;
    }
    return true;
}

size_t writeData(struct inode &inode, const char *buf, size_t size, size_t pos) {
    const char *ptr = buf; size_t bytes_to_write;
    if (inode.i_block_count <= 14) {
        size_t bytes_ava = inode.i_block_count * BLOCK_SIZE - inode.i_size;
        if (bytes_ava > 0) {
            bytes_to_write = bytes_ava > size ? size : bytes_ava;
            writeDataBlock(inode.i_block[inode.i_block_count-1], buf, bytes_to_write);
            ptr += bytes_to_write;  size -= bytes_to_write;
            inode.i_size += bytes_to_write;
        }
    }
    while (size > 0) {
        if (inode.i_block_count <= 14) {    // direct index
            int blk_num = inode.i_block[inode.i_block_count] = allocDataBlock();
            inode.i_block_count++;
            bytes_to_write = size > 1024 ? 1024 : size;
            writeDataBlock(blk_num, ptr, bytes_to_write);
            ptr += bytes_to_write; size -= bytes_to_write;
            inode.i_size += bytes_to_write;
        }
        // indirect indexs are ignored
    }
    return true;
}

size_t readData(struct inode &inode, char *buf, size_t size, size_t pos) {
    char *ptr = buf;
    int offset = pos % BLOCK_SIZE, blk_count = (int)pos / BLOCK_SIZE;
    if (pos + size > inode.i_size)
        size = inode.i_size - blk_count * BLOCK_SIZE;
    else size += offset;
    size_t size_left = size;
    while (size_left >= BLOCK_SIZE && blk_count < 14) {
        readDataBlock(inode.i_block[blk_count], ptr, BLOCK_SIZE);
        size_left -= BLOCK_SIZE;   ptr += BLOCK_SIZE;
        blk_count++;
    }
    if (size_left > BLOCK_SIZE)
        return -1;   // indirect index
    if (size_left < BLOCK_SIZE && blk_count < 14) {
        readDataBlock(inode.i_block[blk_count], ptr, size_left);
    }
    ptr = buf + offset;
    while ((ptr - buf) < size) {
        *(ptr - offset) = *ptr;
        ptr ++;
    }
    *(ptr - offset) = 0;
    return size - offset;
}

void getWindowSize(struct winsize &size) {
    ioctl(STDIN_FILENO,TIOCGWINSZ,&size);
}

void ls_output(vector<std::string> file_v, vector<std::string> dir_v) {
    int byte_count = 0;
    std::cout << "\033[32m";
    for (auto i: dir_v) {
        byte_count += i.size() + 7;
        if (byte_count > size.ws_col) {
            std::cout << std::endl;
            byte_count = 0;
        }
        std::cout << i << '\t';
    }
    std::cout << "\033[30m";
    for (auto i: file_v) {
        byte_count += i.size();
        if (byte_count > size.ws_col) {
            std::cout << std::endl;
            byte_count = 0;
        }
        std::cout << i << '\t';
    }
    std::cout << "\033[0m" << std::endl;
}

string center_aligned (string text) {
    auto len = text.size(), black_count = (size.ws_col - len)/2;
    if (black_count < 1024) {
        string new_text = text;
        new_text.insert(0, black_count, ' ');
        return new_text;
    }
    else return text;
}

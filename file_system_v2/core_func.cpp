#include "core_func.h"
#include "openssl/md5.h"

FILE *file;
struct user_info logged_user = {0, "\0", "\0"};
struct super_block sup_blk = {0,0,0,0,0,0};
int current_dir[16] = {0};
int user_dir[3] = {0};

/*
 * fileFormat() create a 135MB file
 * initialise the super blocks and group description blocks
 * create a root user and the default password is 123456
 * initialise the directory including the root dir and usr dir
 */
bool fileFormat(const char *filename) {
    file = fopen(filename, "wb+");
    if (file == NULL) {
        errno = FILE_CREATE_ERR;
        return false;
    }
    unsigned char temp[1024] = {0};
    for (unsigned count = 0; count < FILE_SIZE; count++) {
        fwrite(temp, sizeof(unsigned char), 1024, file);
    }
    sup_blk = {4096, 131072, 4096, 131072, 1024, 4096};
    
    struct group_desc group_desc = {131072, 4096, 0,{0},{0}};
    // initialize super block and group description
    for (int group = 0; group < 32; group++) {
        writeSuperBlock(sup_blk);
        writeGroupDesc(group, group_desc);
    }
    initDir();
    current_dir[2] = user_dir[2] = createUser("root", "123456");
    
    fseek(file, 1000, SEEK_SET);
    fwrite("LESLIE_FS_V1.1", 24, 1, file);
    
    char checkSum[16];
    genCheckSum(checkSum);
    rewind(file);
    fwrite(&checkSum, 16, 1, file);
    fclose(file);
    return true;
}

/*
 * initDir() is used to initialise the directory
 * create a 'usr' directory in the root directory
 */
bool initDir(void) {
    struct inode inode; // struct group_desc group_desc;
    // new a inode for root dir
    int root_inode_num = allocInode(DIR);
    readInode(root_inode_num, inode);
    strcpy((char *)inode.i_acl, acl_root);
    inode.i_type = DIR;
    int usr_inode_num = makeDir(inode, "usr");
    writeInode(root_inode_num, inode);
    struct inode usr_inode;
    readInode(usr_inode_num, usr_inode);
    strcpy((char*)usr_inode.i_acl,acl_root);
    usr_inode.i_type = DIR;
    writeInode(usr_inode_num, usr_inode);
    current_dir[0] = user_dir[0] = root_inode_num;
    current_dir[1] = user_dir[1] = usr_inode_num;
    return true;
}

/*
 * createUser() is used to create a user with the given name and password
 * and create directory in 'usr' with the same name
 */
int createUser(const char *username, const char *password) {
    if (logged_user.user_id != 0) {
        errno = PERMISSION_DENIED;
        return -1;
    }
    struct user_info user_info;
    int user_count = 0;
    while (user_count < USER_NUM_MAX) {   // add user information in reserve block
        readUserInfo(user_count, user_info);
        if (user_info.user_name[0] == '\0') {
            user_info.user_id = user_count;
            strcpy(user_info.user_name, username);
            strcpy(user_info.password,
                   (const char*)MD5((const unsigned char*)password, strlen(password), NULL));
            writeUserInfo(user_count, user_info);
            break;
        }
        user_count++;
    }
    if (user_count == USER_NUM_MAX) {errno = USERS_FULL; return false;}
    struct inode usr_inode, new_inode;
    readInode(user_dir[1], usr_inode);
    int user_inode_num = makeDir(usr_inode, username);
    if(user_inode_num == -1) return -1;
    readInode(user_inode_num, new_inode);
    new_inode.i_acl[user_count] = acl_rwx;
    writeInode(user_inode_num, new_inode);
    writeInode(user_dir[1], usr_inode);
    return user_info.user_id;
}

/*
 * changePSW() is used to change a user's password
 * it must be the root user or the user itself to change the password
 */
bool changePSW(short uid, const char *newPSW) {
    if (uid != logged_user.user_id && logged_user.user_id != 0) {
        errno = PERMISSION_DENIED;
        return false;
    }
    struct user_info user_info;
    readUserInfo(uid, user_info);
    MD5((const unsigned char*)newPSW, strlen(newPSW), (unsigned char*)user_info.password);
    writeUserInfo(uid, user_info);
    return true;
}

/*
 * chmod() is used to change the file's acl table
 * the flag is a mask code of acl_rights
 */
bool chmod(short uid, char flag, char *filePath) {
    if (uid == 0 && logged_user.user_id != 0) {
        errno = PERMISSION_DENIED;
        return false;
    }
    int inode_num = analysisPath(filePath);
    struct inode inode;
    readInode(inode_num, inode);
    if ((logged_user.user_id != 0) || (logged_user.user_id != inode.i_uid)) {
        // no right to modify this file
        errno = PERMISSION_DENIED;
        return false;
    }
    inode.i_acl[uid] = flag;
    writeInode(inode_num, inode);
    return true;
}

/*
 * makeDir() is used to make a new directory in the current directory
 * insert the dir_entry in the current directory
 * and alloc a new inode for the new directory
 */
int makeDir(struct inode &cur_dir_inode, const char* dirName) {
    int dir_num = seekFreeDirEntry(cur_dir_inode);
    int blk_num = cur_dir_inode.i_block[dir_num / 8], dir_num_in_blk = dir_num % 8;
    if (dir_num == -1) {
        blk_num = addDataBlock(cur_dir_inode);
        if (blk_num == -1) {errno = ADD_BLOCK_ERR; return -1;}
        dir_num_in_blk = 0;
    }
    
    int new_dir_inode = allocInode(DIR);
    if (new_dir_inode == -1) return -1;
    struct dir_entry dir_entry = {new_dir_inode, DIR, };
    strcpy(dir_entry.name, dirName);
    writeDirEntry(blk_num, dir_num_in_blk, dir_entry);
    
    cur_dir_inode.i_size += DIR_ENTRY_SIZE;
    cur_dir_inode.i_mtime = cur_dir_inode.i_ctime = cur_dir_inode.i_atime = time(NULL);
    
    int group_num = new_dir_inode/1024;
    struct group_desc group_desc;
    readGroupDesc(group_num, group_desc);
    group_desc.bg_user_dirs_count ++;
    writeGroupDesc(group_num, group_desc);
    return new_dir_inode;
}

/*
 * allocInode() is used to allocate a free index node for a new file
 */
int allocInode(int fileType){
    int inode_num = seekInode();
    if (inode_num == -1) return 0;
    int inode_map = (inode_num % 1024) / 32, bit = inode_num % 32;
    
    struct inode inode;
    readInode(inode_num, inode);
    initInode(inode, fileType);
    writeInode(inode_num, inode);
    
    struct group_desc group_desc;
    readGroupDesc(inode_num / 1024, group_desc);
    group_desc.bg_free_inodes_count --;
    group_desc.inode_bit_map[inode_map] |= (0x80000000U >> bit);
    writeGroupDesc(inode_num / 1024, group_desc);
    
    sup_blk.s_free_inode_count--;
    return inode_num;
}

void freeInode(int inode_num) {
    struct inode inode;
    memset(&inode, 0, sizeof(struct inode));
    writeInode(inode_num, inode);
    struct group_desc group_desc;
    int group_num = inode_num / 1024;
    int map_num = (inode_num % 1024) / 32;
    int bit_num = inode_num % 32;
    int mask = ~(0x80000000U >> bit_num);
    readGroupDesc(group_num, group_desc);
    group_desc.inode_bit_map[map_num] &= mask;
    group_desc.bg_free_inodes_count ++;
    writeGroupDesc(group_num, group_desc);
    sup_blk.s_free_inode_count++;
}
/*
 * seekInode() is used to find a free Inode and return its number
 */
int seekInode(void) {
    if (sup_blk.s_free_inode_count <= 0)
        return -1;
    unsigned int inode_bit_map, group_num = 0;
    do {
        struct group_desc group_desc;   int map_count = 0;
        readGroupDesc(group_num, group_desc);
        do {
            inode_bit_map = group_desc.inode_bit_map[map_count];
            unsigned mask = 0x80000000, bit = 0;
            while (bit < 32) {
                if (!(inode_bit_map & mask))
                    return 1024*group_num+32*map_count+bit;
                mask = mask >> 1; bit++;
            }
            map_count++;
        } while (map_count < 32);
        group_num++;
    } while (group_num < 32);
    return -1;
}

/*
 * initInode() is used to initialise the Inode with some certain infomation
 */
void initInode(struct inode &inode, int fileType) {
    inode.i_uid = logged_user.user_id;
    inode.i_size = 0;
    inode.i_atime = inode.i_ctime = inode.i_mtime = time(NULL);
    inode.i_dtime = 0;
    inode.i_block_count = 0;
    inode.i_flag = 1;
    strcpy((char*)inode.i_acl, acl_private);
    inode.i_acl[logged_user.user_id] = acl_rwx;
    inode.i_type = fileType;
}

/*
 * seekFreeDirEntry() is used to find a DIR inode's first free room
 * and return it's number.
 * if the inode's blocks are totally full, the return value is -1
 */
int seekFreeDirEntry(struct inode &inode) {
    int blk_count = inode.i_block_count;
    struct dir_entry dir_entry;
    int count = 0;
    while (count < blk_count) {
        int dir_entry_count = 0;
        while (dir_entry_count < 8) {
            readDirEntry(inode.i_block[count], dir_entry_count, dir_entry);
            if (dir_entry.inode == 0)
                return dir_entry_count + count * 8;
            dir_entry_count++;
        }
        count++;
    }
    return -1;
}

/*
 * addDataBlock is used to add a data block to the end of the inode
 */
int addDataBlock(struct inode &inode) {
    if(inode.i_block_count < 14) {// blocks from 0 to 13 are direct index
        int blk_num = allocDataBlock();
        if (blk_num == -1) return 0;
        inode.i_block[inode.i_block_count] = blk_num;
        inode.i_block_count ++;
        inode.i_ctime = time(NULL);
        return blk_num;
    }
    return 0;
}

void freeDataBlock(int blk_num) {
    char buf[1024] = {0};
    writeDataBlock(blk_num, buf, 1024);
    struct group_desc group_desc;
    int group_num = blk_num / 4096;
    int map_num = (blk_num % 4096) / 32;
    int bit_num = blk_num % 32;
    int mask = ~(0x80000000U >> bit_num);
    readGroupDesc(group_num, group_desc);
    group_desc.bg_free_block_count--;
    group_desc.data_block_bit_map[map_num] &= mask;
    writeGroupDesc(group_num, group_desc);
    sup_blk.s_free_block_count++;
}

/*
 * allocDataBlock() is used to allocate a new data block
 * and return the block's number
 */
int allocDataBlock(void) {
    int block_num = seekBlock();
    if (block_num == -1) return 0;
    int block_map = (block_num % 4096) / 32, bit = block_num % 32;
    
    struct group_desc group_desc;
    readGroupDesc(block_num / 4096, group_desc);
    group_desc.bg_free_block_count --;
    group_desc.data_block_bit_map[block_map] |= (0x80000000U >> bit);
    writeGroupDesc(block_num / 4096, group_desc);
    
    sup_blk.s_free_block_count--;
    return block_num;
}

int seekBlock(void) {
    if (sup_blk.s_free_block_count <= 0)
        return -1;
    unsigned int block_bit_map, group_num = 0;
    do {
        struct group_desc group_desc;   int map_count = 0;
        readGroupDesc(group_num, group_desc);
        do {
            block_bit_map = group_desc.data_block_bit_map[map_count];
            unsigned mask = 0x80000000, bit = 0;
            while (bit < 32) {
                if (!(block_bit_map & mask))
                    return 4096*group_num+32*map_count+bit;
                mask = mask >> 1; bit++;
            }
            map_count++;
        } while (map_count < 128);
        group_num++;
    } while (group_num < 32);
    return -1;
}

/*
 * system_init() is used to initialise the file system
 * if the format file exists, create the FILE pointer to read the file
 * if not exists, you should first create a new file and format it;
 */
bool systemInit(const char* filePath) {
    file = fopen(filePath, "rb+");
    if (file ==  NULL)
        return 0;
    struct reserved_block rb;
    fseek(file, 0, SEEK_SET);
    fread(&rb, sizeof(struct reserved_block), 1, file);
    if (strcmp(rb.version, VERSION) != 0) {
        errno = INCOMPATIBLE_VERSION;
        return false;
    }
    char check_sum[16];
    genCheckSum(check_sum);
    if (cmpMD5(check_sum, rb.check_sum)) {
        fseek(file, 1024, SEEK_SET);
        fread(&sup_blk, sizeof(struct super_block), 1, file);
        current_dir[0] = user_dir[0] = 0;
        current_dir[1] = user_dir[1] = 1;
        current_dir[2] = user_dir[2] = 0;
        return true;
    }
    errno = CHECK_SUM_INCORRECT;
    return false;
}

/*
 * genCheckSum() is used to generate a md5 value for the file
 * skipping the version filed and checksum filed.
 */
void genCheckSum(char checkSum[]) {
    char buf[1100] = {0};
    fseek(file, 16, SEEK_SET);
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    size_t count = 0, bytes = 0;
    while(count < FILE_SIZE*1024 - 16) {
        bytes = fread(buf, 1, 1024, file);
        MD5_Update(&md5_ctx, buf, bytes);
        count += bytes;
    }
    MD5_Final((unsigned char*)checkSum, &md5_ctx);
}

/*
 * login() is used to verify users
 */
bool login(const char *username, const char *password) {
    for (int u_count = 0; u_count < USER_NUM_MAX; u_count++) {
        struct user_info user_info;
        readUserInfo(u_count, user_info);
        if (strcmp(user_info.user_name, username) == 0) {    // matched
            if (cmpMD5(user_info.password,
                       (const char*)MD5((unsigned char*)password, strlen(password), NULL))) {
                logged_user.user_id = user_info.user_id;
                strcpy(logged_user.user_name,username);
                current_dir[0] = user_dir[0] = 0;
                current_dir[1] = user_dir[1] = 1;
                current_dir[2] = user_dir[2] = seekDirItem(1, username);
                return true;
            }
        }
    }
    return false;
}

/*
 * logout() is used to log out current user
 * write back super block and write a check sum of the file
 */
bool logout(){
    writeSuperBlock(sup_blk);
    logged_user = {0, "\0", "\0"};
    current_dir[2] = user_dir[2] = 0;
    char checkSum[16];
    genCheckSum(checkSum);
    rewind(file);
    fwrite(&checkSum, 16, 1, file);
    return true;
}

int get_current_dir_level() {
    int count = 1;
    while(count < 16 && current_dir[count] != 0)
        count++;
    return count-1;
}
int get_current_dir_inode() {
    int count = 1;
    while(count < 16 && current_dir[count] != 0)
        count++;
    return current_dir[count-1];
}

void get_current_dir_name(char *name) {
    int level = get_current_dir_level();
    if (level == 0)
        strcpy(name, "/");
    else if ((level == 2) && current_dir[2] == user_dir[2])
        strcpy(name, "~");
    else {
        struct inode inode;
        readInode(current_dir[level-1], inode);
        int block_num = inode.i_block_count, blk_count = 0;
        while (blk_count < block_num) {
            struct dir_entry dir_entry;
            int de_count = 0;
            while (de_count < 8) {
                readDirEntry(inode.i_block[blk_count], de_count, dir_entry);
                if (dir_entry.inode == current_dir[level]) {
                    strcpy(name, dir_entry.name);
                    return;
                }
                de_count++;
            }
            blk_count++;
        }
        errno = NO_SUCH_DIRECTORY;
    }
}

/*
 * changeDir() is used to change current directory
 */
bool changeDir(const char* dir_name) {
    if (dir_name == NULL || *dir_name == 0){ // change to the user's dir
        current_dir[0] = user_dir[0];
        current_dir[1] = user_dir[1];
        current_dir[2] = user_dir[2];
        return true;
    }
    int cur_inode_num, dir_level; const char *ptr = dir_name;
    switch (dir_name[0]) {
        case '/':   cur_inode_num = 0; ptr++; dir_level = 0; break;   // Absolute path
        case '~':   cur_inode_num = user_dir[2]; ptr++; dir_level = 2;break; // Relative path
        default:    cur_inode_num = get_current_dir_inode();
            dir_level = get_current_dir_level(); break;
    }
    std::string buf;
    while (*ptr != '\0') {
        buf.clear();
        while (*ptr != '/' && *ptr != '\0') {
            buf += *ptr++;
        }
        if (buf == "..") {
            if (--dir_level < 0) {
                errno = NO_SUCH_DIRECTORY;
                return false;
            }
            current_dir[dir_level+1] = 0;
            cur_inode_num = current_dir[dir_level];
        }
        else if (buf == ".") ;
        else {
            // search dir item
            int item_inode_num;
            if((item_inode_num = seekDirItem(cur_inode_num, &buf[0])) == -1)
                return false;
            current_dir[++dir_level] = item_inode_num;
        }
        if (*ptr == '\0') break;
        ptr++;
    }
    return true;
}

/*
 * seekDirItem() is used to find whether the file exists
 * if exists, return its inode, else return -1
 */
int seekDirItem(int inode_num, const char *name) {
    struct inode inode; int block_num;
    readInode(inode_num, inode);
    if (!ckPms(inode, acl_r)) {
        errno = PERMISSION_DENIED;
        return -1;
    }
    if (inode.i_type != DIR) {
        errno = NO_SUCH_DIRECTORY;
        return -1;
    }
    block_num = inode.i_block_count;
    int blk_count = 0;
    while (blk_count < block_num) {
        struct dir_entry dir_entry;
        int de_count = 0;
        while (de_count < 8) {
            readDirEntry(inode.i_block[blk_count], de_count, dir_entry);
            if (dir_entry.inode != 0 && strcmp(dir_entry.name, name) == 0) {
                struct inode inode; readInode(dir_entry.inode, inode);
                if (!ckPms(inode, acl_r)) {errno = PERMISSION_DENIED; return -1;}
                return dir_entry.inode;
            }
            de_count++;
        }
        blk_count++;
    }
    errno = NO_SUCH_DIRECTORY;
    return -1;
}


/*
 * getFileDir() is used to get the inode of dir that the file exists
 */
int getFileDir(const char *filePath) {
    char path[128], *ptr = NULL; strcpy(path, filePath);
    ptr = path + strlen(path) - 1;
    while (*ptr != '/'&& *ptr != '~' && ptr > path)
        ptr--;
    int inode;
    if (ptr == path)
        inode = get_current_dir_inode();
    else {
        *ptr = '\0';
        inode = analysisPath(path);
    }
    return inode;
}

int createFile(const char *path) {
    int cur_dir_inode_num = getFileDir(path);
    char filePath[128]; strcpy(filePath, path);
    char *ptr = filePath; ptr += (strlen(filePath) - 1);
    while (*ptr != '/'&& *ptr != '~' && ptr > filePath)
        ptr--;
    if (ptr != filePath)
        ptr++;
    int isExist = seekDirItem(cur_dir_inode_num, ptr);
    if (isExist != -1) {
        errno = FILE_EXISTED;
        return -1;
    }
    struct inode cur_inode;
    readInode(cur_dir_inode_num, cur_inode);
    int ret = insertFileDir(cur_inode, ptr);
    writeInode(cur_dir_inode_num, cur_inode);
    return ret;
}

/*
 * insertFileDir() is used to insert a new file's information into the dir
 * this function call allocInode() to allocate a new inode for the new file
 * and update the file's group information
 */
int insertFileDir(struct inode &cur_dir_inode, const char* filename) {
    int dir_num = seekFreeDirEntry(cur_dir_inode);
    int blk_num = cur_dir_inode.i_block[dir_num / 8], dir_num_in_blk = dir_num % 8;
    if (dir_num == -1) {
        blk_num = addDataBlock(cur_dir_inode);
        if (blk_num == -1) {errno = ADD_BLOCK_ERR; return -1;}
        dir_num_in_blk = 0;
    }
    
    int new_dir_inode = allocInode(COMMON);
    if (new_dir_inode == -1) return -1;
    struct dir_entry dir_entry = {new_dir_inode, DIR, };
    strcpy(dir_entry.name, filename);
    writeDirEntry(blk_num, dir_num_in_blk, dir_entry);
    
    cur_dir_inode.i_size += DIR_ENTRY_SIZE;
    cur_dir_inode.i_mtime = cur_dir_inode.i_ctime = cur_dir_inode.i_atime = time(NULL);
    
    int group_num = new_dir_inode/1024;
    struct group_desc group_desc;
    readGroupDesc(group_num, group_desc);
    group_desc.bg_user_dirs_count ++;
    writeGroupDesc(group_num, group_desc);
    return new_dir_inode;
}

/*
 * removeFileDir is used to delete files(or a empty directory) from the file's dir
 */
void removeFileDir(struct inode &inode, int inode_num) {
    int blk_count = 0; int de_count = 0;struct dir_entry dir_entry;
    while (blk_count < inode.i_block_count) {
        de_count = 0;
        while (de_count < 8) {
            readDirEntry(inode.i_block[blk_count], de_count, dir_entry);
            if (dir_entry.inode == inode_num)
                break;
            de_count++;
        }
        if (de_count != 8)
            break;
    }
    if (blk_count == 8)
        errno = NO_SUCH_DIRECTORY;
    else {
        memset(&dir_entry, 0, sizeof(struct dir_entry));
        writeDirEntry(inode.i_block[blk_count], de_count, dir_entry);
    }
}

/*
 * listFile() is used to list all the files in current direction
 */
void listFile(void) {
    int cur_dir_inode = get_current_dir_inode();
    struct inode inode;
    readInode(cur_dir_inode, inode);
    int block_num = 0;
    vector<std::string> dir_v;
    vector<std::string> file_v;
    while (block_num < inode.i_block_count && block_num < 14) {
        struct dir_entry dir_entry;
        int de_count = 0;
        while (de_count < 8) {
            readDirEntry(inode.i_block[block_num], de_count, dir_entry);
            if (dir_entry.inode != 0) {
                if (dir_entry.file_type == DIR)
                    dir_v.push_back(dir_entry.name);
                else if (dir_entry.file_type == COMMON)
                    file_v.push_back(dir_entry.name);
            }
            de_count++;
        }
        block_num++;
    }
    ls_output(file_v, dir_v);
    return;
}

/*
 * writeFile() is used to write data to file
 */
size_t writeFile(const char* filePath, const char *buf, size_t size, size_t pos) {
    int inode_num = analysisPath(filePath);
    if (inode_num == -1) return false;
    struct inode inode;
    readInode(inode_num, inode);
    if (!ckPms(inode, acl_w)) {
        errno = PERMISSION_DENIED;
        return false;
    }
    writeData(inode, buf, size, pos);
    inode.i_atime = inode.i_mtime = inode.i_ctime = time(NULL);
    writeInode(inode_num, inode);
    return true;
}

/*
 * readFile() is used to read data from file
 */
size_t readFile(const char* filePath, char *buf, size_t size, size_t pos) {
    int inode_num = analysisPath(filePath);
    if (inode_num == -1) return -1;
    struct inode inode;
    readInode(inode_num, inode);
    if (!ckPms(inode, acl_r)) {
        errno = PERMISSION_DENIED;
        return -1;
    }
    size = size ? size:inode.i_size;
    size = readData(inode, buf, size, pos);
    inode.i_atime = time(NULL);
    writeInode(inode_num, inode);
    return size;
}

/*
 * openFile() is used to open a file
 */
bool openFile(const char* filePath, char flag) {
    int inode_num = analysisPath(filePath);
    struct inode inode; readInode(inode_num, inode);
    if (!ckPms(inode, flag)) {
        errno = PERMISSION_DENIED;
        return false;
    }
    size_t fileSize = inode.i_size, bytes = 0;
    FILE *tmp_f = fopen("temp_9475", "wb");
    char buf[1024] = {0};
    while (bytes < fileSize) {
        size_t byte_read = readFile(filePath, buf, 1024, bytes);
        bytes += byte_read;
        fwrite(buf, 1, byte_read, tmp_f);
        memset(buf, 0, 1024);
    }
    char cmd[64] = "vim ";
    if (!(flag & acl_w))
        strcat(cmd, "-M ");
    strcat(cmd, "temp_9475");
    fclose(tmp_f);
    system(cmd);
    if (flag & acl_w) { // if have the write right, write the modified file back
        tmp_f = fopen("temp_9475", "rb");
        deleteFile(filePath);
        createFile(filePath);
        fileSize = 0;
        while ((bytes = fread(buf, 1, 1024, tmp_f)) != 0) {
            writeFile(filePath, buf, bytes, fileSize);
            fileSize += bytes;
            memset(buf, 0, 1024);
        }
        fclose(tmp_f);
        system("rm temp_9475");
    }
    return true;
}

/*
 * deleteFile() is used to delete a file
 * free its inode and blocks and delete its dir_entry from dir
 */
bool deleteFile(const char *path) {
    char filePath[128]; strcpy(filePath, path);
    int inode_num = analysisPath(filePath);
    if(inode_num == -1) return false;
    struct inode inode;
    readInode(inode_num, inode);
    if (!ckPms(inode, acl_rwx)) {
        errno = PERMISSION_DENIED;
        return false;
    }
    int blk_num = inode.i_block_count;
    int count = 0;
    while (count < 14 && count < blk_num) {
        freeDataBlock(inode.i_block[count]);
        count++;
    }
    // block 14 and 15 are indirect index
    freeInode(inode_num);
    char *ptr = filePath; ptr += (strlen(filePath) - 1);
    while (*ptr != '/'&& *ptr != '~' && ptr > filePath)  ptr--;
    int dir_inode_num;
    if (ptr == filePath) dir_inode_num = get_current_dir_inode();
    else {
        *ptr = '\0';
        dir_inode_num = analysisPath(filePath);
    }
    if(dir_inode_num == -1) return false;
    readInode(dir_inode_num, inode);
    removeFileDir(inode, inode_num);
    writeInode(dir_inode_num, inode);
    return true;
}

/*
 * analysisPath() is used get inode number of the file that the path indicates
 * path begin with '/' means find the file from root directory
 *      begin with '~' means find the file from user's directory
 * otherwise, it's a relative path
 */
int analysisPath(const char *path) {
    if(path == NULL) return -1;
    int cur_inode_num, dir_level; const char *ptr = path;
    switch (path[0]) {
        case '/':   cur_inode_num = 0; ptr++; dir_level = 0; break;   // Absolute path from root path
        case '~':   cur_inode_num = user_dir[2]; ptr++; dir_level = 2;break;    // Absolute path from user's dir
        default:    cur_inode_num = get_current_dir_inode(); // Relative path
            dir_level = get_current_dir_level(); break;
    }
    std::string buf;
    while (*ptr != '\0') {
        buf.clear();
        while (*ptr != '/' && *ptr != '\0') {
            buf += *ptr++;
        }
        if (buf == "..") {
            if (--dir_level < 0) {
                errno = NO_SUCH_DIRECTORY;
                return -1;
            }
            cur_inode_num = current_dir[dir_level];
        }
        else if (buf == ".") ;
        else {
            // search dir item
            if((cur_inode_num = seekDirItem(cur_inode_num, &buf[0])) == 0)
                return -1;
        }
        if (*ptr == '\0') break;
        ptr++;
    }
    return cur_inode_num;
}

/*
 * ckPms() is used to check if the logged user have the right accessing to the file
 * flag is the mask code of acl_rights
 */
bool ckPms(struct inode inode, char flag) {
    return inode.i_acl[logged_user.user_id] & flag;
}

int copyFile(const char *filepath, const char *dest) {
    int dir_inode = getFileDir(filepath);
    char path[128]; strcpy(path, filepath);
    char *ptr = path + strlen(path);
    while (*ptr != '/'&& *ptr != '~' && ptr > path)
        ptr--;
    if (ptr != path)
        ptr++;
    int file_inode = seekDirItem(dir_inode, ptr);
    if (file_inode == -1) {
        errno = FILE_NOT_EXISTED;
        return false;
    }
    int new_inode = createFile(dest);
    struct inode inode;
    readInode(file_inode, inode);
    // copy the data block's content
    int blk_count = 0;
    while(blk_count < 14 && blk_count < inode.i_block_count) {
        char buf[BLOCK_SIZE];
        fseek(file, get_data_block_offset(inode.i_block[blk_count]), SEEK_SET);
        fread(buf, BLOCK_SIZE, 1, file);
        inode.i_block[blk_count] = allocDataBlock();
        fseek(file, get_data_block_offset(inode.i_block[blk_count]), SEEK_SET);
        fwrite(buf, BLOCK_SIZE, 1, file);
        blk_count ++;
    }
    // indirect index
    writeInode(new_inode, inode);
    return new_inode;
}

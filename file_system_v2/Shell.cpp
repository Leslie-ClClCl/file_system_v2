#include "Shell.h"
#include <sstream>

using namespace std;

bool shell(void) {
    while (true) {
        char dirName[120]; get_current_dir_name(dirName);
        cout << logged_user.user_name << "@Leslie's_File_System " << dirName << " % ";
        string cmd; vector<string> cmd_content;
        getline(cin, cmd); getCmdContent(cmd, cmd_content);
        if (cmd_content.size() == 0) continue;
        if (cmd_content[0] == "cd") {
            changeDir(cmd_content[1].c_str());
        }
        else if (cmd_content[0] == "ls") {
            listFile();
        }
        else if (cmd_content[0] == "logout") {
            return logout();
        }
        else if (cmd_content[0] == "exit") {
            logout(); fclose(file); return 0;
        }
        else if (cmd_content[0] == "adduser") {
            if (cmd_content.size() == 2) {
                short uid = createUser(cmd_content[1].c_str(), "\0");
                cout << "user \"" << cmd_content[1] << "\" has been created, the uid is " << uid << endl;
            }
            else cout << "\'adduser\' usage: \n   adduser \33[4musername\33[0m" << endl;
        }
        else if (cmd_content[0] == "passwd") {
            if (cmd_content.size() == 3) {
                changePSW(atoi(cmd_content[1].c_str()), cmd_content[2].c_str());
                cout << "the password of user(uid " << cmd_content[1] << ") has changed" << endl;
            }
            else cout << "\'passwd\' usage: \n   adduser \33[4muid\33[0m \33[4mnew_password\33[0m" << endl;
        }
        else if (cmd_content[0] == "create") {
            if (cmd_content.size() == 2) {
                createFile(cmd_content[1].c_str());
            }
            else cout << "\'create\' usage: \n   create \33[4mfilename\33[0m" << endl;
        }
        else if (cmd_content[0] == "read") {
            if (cmd_content.size() == 3) {
                int size = atoi(cmd_content[2].c_str());
                char buf[size];
                readFile(cmd_content[1].c_str(), buf, size, 1024);
                cout << buf << "\n\n";
            }
            else cout << "\'read\' usage:\n   read \33[4mfile_path\33[0m \33[4mbyte_num\33[0m" << endl;
        }
        else if (cmd_content[0] == "write") {
            if (cmd_content.size() == 2) {
                FILE *fp = fopen("/Users/cl/Desktop/text.txt", "r");
                char buf[1100] = {0};
                fread(buf, 1100, 1, fp);
                cout << buf << endl;
                writeFile(cmd_content[1].c_str(), buf, strlen(buf), 0);
            }
            else cout << "\'write\' usage:\n   write \33[4mfile_path\33[0m" << endl;
        }
        else if (cmd_content[0] == "open") {
            if (cmd_content.size() == 2) {
                openFile(cmd_content[1].c_str(), acl_rwx);
            }
            else cout << "\'open\' usage:\n   open \33[4mfile_path\33[0m" << endl;
        }
        else if (cmd_content[0] == "rm") {
            if (cmd_content.size() == 2) {
                deleteFile(cmd_content[1].c_str());
            }
            else cout << "\'rm\' usage:\n   rm \33[4mfile_path\33[0m" << endl;
        }
        else if (cmd_content[0] == "cp") {
            if (cmd_content.size() == 3) {
                copyFile(cmd_content[1].c_str(), cmd_content[2].c_str());
            }
            else cout << "\'cp\' usage:\n   cp \33[4msource file\33[0m \33[4mtarget file\33[0m" << endl;
        }
        else cout << "lsh: command not found: " << cmd_content[0] << endl;
    }
}

void getCmdContent(string cmd, vector<string> &cmd_content) {
    istringstream temp_stream(cmd); string content;
    while (temp_stream >> content) {
        cmd_content.push_back(content);
    }
}


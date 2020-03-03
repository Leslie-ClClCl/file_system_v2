#include "core_func.h"
using namespace std;
int main(int argc, const char * argv[]) {
    getWindowSize(size); cout << "\33[2J";
    cout << center_aligned("/*********************************************/\n")
    << center_aligned( "Leslie's Simple File System") << "\n"
    << center_aligned(VERSION) << "\n\n"
    << center_aligned("by Leslie Lee, U201714863\n")
    << center_aligned("This File System is an OS Course Design\n")
    << center_aligned("/*********************************************/\n\n\n");
    string filePath;
    cout << "Please enter the path of the file: " << endl;
    getline(cin, filePath);
    if (!systemInit(filePath.c_str())) {
        cout << "File initialization failed...\nFile does not exist or file version is incompatible\n";
        cout << "Do you want to create or format this file(Y/n): ";
        char option;
        cin >> option;
        if (option == 'Y' || option == 'y'){
            fileFormat(filePath.c_str());
            systemInit(filePath.c_str());
        }
        else {
            cout << "The system has exited" << endl;
            return 0;
        }
        cin.get();
    }
    while (true) {
        string username, password;
        cout << "------  LogIn  ------\n";
        cout << "username: "; getline(cin, username);
        cout << "password: "; getline(cin, password);
        if (login(username.c_str(), password.c_str())) {
            cout << "Hello " << username << endl;
            cout << "----------------------\n" << endl;
            if (!shell())
                return 0;
        }
        else cout << "username or password error!" << endl;
    }
}

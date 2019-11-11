#include <iostream>
#include <fstream>
#include <string>
using namespace std;
int main(int argc, char *argv[])
{
    string filename(argv[1]);
    string fsize(argv[1]);
    int size = stoi(fsize);
    ofstream fp(filename);
    if(fp)
    {
        for (int i=0; i<size-1; i++)
        {
            char c = (i%10) + '0';
            fp << c;
        }
	fp << '\0';
    } else
        cout << "文件打开失败";
    return 0;
}

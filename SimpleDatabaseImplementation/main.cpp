#include"QueryParser.h"
#include<iostream>
using namespace std;
int main()
{
	string tmp;
	QueryParser t;
	bool flag = true;//是否要输出插入成功的flag
	t.ExecuteSQL("help;", flag);
	while (getline(cin, tmp))
	{
		if (tmp.find(';') != string::npos)
		{
			t.ExecuteSQL(tmp, flag);
		}
		else
		{
			cout << "\'" + tmp + "\'" + "不是正确的命令。请输入help查看帮助。" << endl;
		}
	}
}
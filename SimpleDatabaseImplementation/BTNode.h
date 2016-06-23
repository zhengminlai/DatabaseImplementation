#pragma once
//简介：B+树节点类
//Implemented by Xu Jing
//作用：实现外存B+索引节点
#pragma once
#ifndef _BTNODE_H_
#define _BTNODE_H_

#include <string>
#include <vector>

#include "BPlusTree.h"
#include "CatalogManager.h"
#include "BufferManager.h"

using namespace std;

class BPlusTree;
class BTNode
{
public:
	BTNode(BPlusTree* tree, bool isnew, int blocknum, bool newleaf = false);
	~BTNode();

	int get_block_num();

	TKey get_keys(int index);			/*获取第index个key值*/
	int get_values(int index);			/*获取第index个value值*/
	int get_next_leaf();					/*获取下一个叶子节点*/
	int get_parent();					/*获取下一个父节点，得到的是该节点在buffer中的地址编号，是该节点所在的buffer的第8位数据*/
	int get_node_type();					/*获取节点类型，即该节点所在buffer中的第0位数据*/
	int get_count();						/*获取节点的数据个数，即该节点所在buffer中的第4位数据*/
	bool is_leaf();					/*判断是否是叶子节点*/

	void set_keys(int index, TKey key);	/*将key.key_放入该节点的第index个键中*/
	void set_values(int index, int val);	/*设置第index元素的值为val*/
	void set_next_leaf(int val);			/*设置下一个叶子节点的值*/
	void set_parent(int val);			/*设置父节点的值*/
	void set_node_type(int val);			/*节点类型：叶子节点为1，非叶子节点为0*/
	void set_count(int val);				/*设置节点存储的元素个数*/
	void set_is_leaf(bool val);			/*设置节点是否为叶子节点*/

	void get_buffer();					/*从file中获取当前B+树所在的db中当前树索引的文件块*/

	bool search(TKey key, int &index);	/*在B+树中搜索key所在的位置，并赋值到index中。返回值：true，index即为由key定位的节点地址；false，index即为指向下一层的指针。节点元素个数20以内，顺序查找；20以外，二分查找*/
	int add(TKey &key);					/*先查找b+树中是否存在该key，否：将key插入其对应的位置中*/
	int add(TKey &key, int &val);		/*插入KV对*/
	BTNode* split(TKey &key);	/*分裂*/

	bool isRoot();						/*判断是否为根节点*/
	bool remove(int index);			/*移除第index个元素*/
	void print();

private:
	BPlusTree* tree_;
	int block_num_;
	int rank_;
	char* buffer_;//一个缓存的block块
	bool is_leaf_;
	bool is_new_node_;
};
#endif
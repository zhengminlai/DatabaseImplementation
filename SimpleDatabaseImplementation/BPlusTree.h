//简介：B+树索引
//Implemented by Xu Jing
//作用：实现外存B+索引
#pragma once
#ifndef _BPLUSTREE_H_
#define _BPLUSTREE_H_

#include <string>
#include <vector>

#include "BTNode.h"
#include "CatalogManager.h"
#include "BufferManager.h"

using namespace std;

class BPlusTree;		/*B+树类*/
class BTNode;

typedef struct
{
	BTNode* pnode;
	int index;
	bool flag;
} FindNodeParam;				/*节点查找辅助结构*/

class BPlusTree
{
public:
	BPlusTree(Index* idx, BufferManager* bm, CatalogManager* cm, string dbname);
	~BPlusTree(void);

	Index* GetIndex();										/*获取本B+树的索引成员*/
	BufferManager* GetBufferManager();						/*获取本B+树的buffermanager*/
	CatalogManager* GetCatalogManager();					/*获取本B+树的目录管理器*/
	int get_degree();										/*获取度*/
	string get_db_name();									/*获取本B+树的数据库名*/

	bool add(TKey& key, int block_num, int offset);			/*在第block_num个块上加元素key 偏移offset*/
	bool spiltForAdd(int node);							/*加元素后调整B+树*/

	bool remove(TKey key);									/*移除key元素*/
	bool mergeForRemove(int node);						/*删元素后调整B+树*/

	FindNodeParam search(int node, TKey &key);				/*【用于直至叶子节点的查询】由node开始搜索到key所在的叶子节点。ans.flag：true，该key在B+树中存在；false，该key在B+树中不存在*/
	FindNodeParam search_pos(int node, TKey &key);		/*【用于删节点时，内部节点变化的查询】从node开始，查询key所在的pnode及其index。FindNodeParam中的flag：true，pnode为叶子节点；false，pnode为内部节点*/
	BTNode* get_node(int num);						/*获取第num个节点。实现方法：获取文件系统中第num个块，该块中的data_数据即为该节点*/

	int get_value(TKey key);									/*由key查询value值*/
	vector<int> get_value(TKey key, int op_type, string &searchType);		/*由key查询value值，op_type可用于判定是否为范围查询*/
	int get_new_blocknum();									/*idx_索引最大值增一*/

	void print();
	void print_node(int num);

private:
	Index *idx_;											/*索引指针，描述了当前B+树的基本信息（头节点地址，属性及长度等）。当一张table建立了一个B+树索引，则该table将该索引信息加入索引列表中，这里的索引指针与table中的索引列表的某一项。*/
	int degree_;											/*度*/
	BufferManager *buffer_m_;								/*缓存管理器指针*/
	CatalogManager *catalog_m_;								/*目录管理器指针*/
	string db_name_;										/*该B+树所属的db名字*/
	void InitTree();										/*初始化：创建根节点，初始idx属性*/
};

#endif
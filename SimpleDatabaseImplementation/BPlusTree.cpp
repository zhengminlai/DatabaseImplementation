//简介：B+树索引
//Implemented by Xu Jing
//作用：实现外存B+索引
#include <iomanip>
#include<iostream>
#include "BPlusTree.h"
#include "Exceptions.h"
#include "ConstValue.h"
using namespace std;

BPlusTree::BPlusTree(Index* idx, BufferManager* bm, CatalogManager* cm, string dbname)
{
	buffer_m_ = bm;
	catalog_m_ = cm;
	idx_ = idx;
	degree_ = 2 * idx_->get_rank() + 1;
	db_name_ = dbname;
}

BPlusTree::~BPlusTree(void) {}

void BPlusTree::InitTree()
{
	BTNode *root_node = new BTNode(this, true, get_new_blocknum(), true);
	idx_->set_root(0);
	idx_->set_leaf_head(idx_->get_root());
	idx_->set_key_count(0);
	idx_->set_node_count(1);
	idx_->set_level(1);
	root_node->set_next_leaf(-1);
}

Index* BPlusTree::GetIndex() { return idx_; }
BufferManager* BPlusTree::GetBufferManager() { return buffer_m_; }
CatalogManager* BPlusTree::GetCatalogManager() { return catalog_m_; }
int BPlusTree::get_degree() { return degree_; }
string BPlusTree::get_db_name() { return db_name_; }
/*在第block_num个块上加元素key 偏移offset*/
bool BPlusTree::add(TKey& key, int block_num, int offset)
{
	int value = (block_num << 16) | offset;/*第offset条数据记录。offset是一个16位二进制长度的数据:0000000…000|offset->获取offset的后16位数据*/
	if (idx_->get_root() == -1)
		InitTree();

	FindNodeParam fnp = search(idx_->get_root(), key);
	if (!fnp.flag)
	{
		fnp.pnode->add(key, value);
		idx_->IncreaseKeyCount();
		if (fnp.pnode->get_count() == degree_)
			return spiltForAdd(fnp.pnode->get_block_num());
		return true;
	}
	return false;
}
/*加元素后调整B+树:分裂*/
bool BPlusTree::spiltForAdd(int node)
{
	BTNode *pnode = get_node(node);
	TKey key(idx_->get_key_type(), idx_->get_key_len());
	BTNode *newnode = pnode->split(key);
	idx_->IncreaseNodeCount();
	int parent = pnode->get_parent();

	if (parent == -1)											/* 当前节点是根节点 */
	{
		BTNode *newroot = new BTNode(this, true, get_new_blocknum());
		if (newroot == NULL) return false;

		idx_->IncreaseNodeCount();
		idx_->set_root(newroot->get_block_num());

		newroot->add(key);
		newroot->set_values(0, pnode->get_block_num());
		newroot->set_values(1, newnode->get_block_num());

		pnode->set_parent(idx_->get_root());
		newnode->set_parent(idx_->get_root());
		newnode->set_next_leaf(-1);
		idx_->IncreaseLevel();
		return true;
	}
	else
	{
		BTNode *parentnode = get_node(parent);
		int index = parentnode->add(key);

		parentnode->set_values(index, pnode->get_block_num());
		parentnode->set_values(index + 1, newnode->get_block_num());

		if (parentnode->get_count() == degree_)
			return spiltForAdd(parentnode->get_block_num());
		return true;
	}
}
/*移除key元素*/
bool BPlusTree::remove(TKey key)
{
	if (idx_->get_root() == -1) return false;

	BTNode *rootnode = get_node(idx_->get_root());
	FindNodeParam fnp = search(idx_->get_root(), key);

	if (fnp.flag)
	{
		if (idx_->get_root() == fnp.pnode->get_block_num())
		{
			rootnode->remove(fnp.index);
			idx_->DecreaseKeyCount();
			mergeForRemove(fnp.pnode->get_block_num());
			return true;
		}

		if (fnp.index == fnp.pnode->get_count() - 1)
		{
			FindNodeParam fnpb = search_pos(idx_->get_root(), key);
			if (fnpb.flag)
				fnpb.pnode->set_keys(fnpb.index, fnp.pnode->get_keys(fnp.pnode->get_count() - 2));
		}

		fnp.pnode->remove(fnp.index);
		idx_->DecreaseKeyCount();
		mergeForRemove(fnp.pnode->get_block_num());
		return true;
	}
	return false;
}
/*删元素后调整B+树：合并*/
bool BPlusTree::mergeForRemove(int node)
{
	BTNode* pnode = get_node(node);
	if (pnode->get_count() >= idx_->get_rank()) return true;

	if (pnode->isRoot())
	{
		if (pnode->get_count() == 0)
		{
			if (!pnode->is_leaf())
			{
				idx_->set_root(pnode->get_values(0));
				get_node(pnode->get_values(0))->set_parent(-1);
			}
			else
			{
				idx_->set_root(-1);
				idx_->set_leaf_head(-1);
			}
			delete pnode;
			idx_->DecreaseNodeCount();
			idx_->DecreaseLevel();
		}
		return true;
	}

	BTNode *pbrother;
	BTNode *pparent;
	int pos;

	pparent = get_node(pnode->get_parent());
	pparent->search(pnode->get_keys(0), pos);/*在父节点中查询指向pnode的指针地址pos*/

	if (pos == pparent->get_count())/*若pnode是父节点的最后一个*/
	{
		pbrother = get_node(pparent->get_values(pos - 1));/*兄弟节点:父节点的前一个节点*/
		if (pbrother->get_count() > idx_->get_rank())/*如果兄弟节点个数大于索引规定的rank，则将pbrother的最后一个元素移至pnode*/
		{
			if (pnode->is_leaf())/*pnode为叶子节点*/
			{
				for (int i = pnode->get_count(); i > 0; i--)/*pnode中每一个元素后移*/
				{
					pnode->set_keys(i, pnode->get_keys(i - 1));/*指向下一个叶子节点的地址指针放于block头部，所以这里叶子节点的key数=value数*/
					pnode->set_values(i, pnode->get_values(i - 1));
				}
				pnode->set_keys(0, pbrother->get_keys(pbrother->get_count() - 1));/*将pbrother的最后一个元素移至pnode中首元素位置*/
				pnode->set_values(0, pbrother->get_values(pbrother->get_count() - 1));
				pnode->set_count(pnode->get_count() + 1);

				pbrother->set_count(pbrother->get_count() - 1);
				pparent->set_keys(pos - 1, pbrother->get_keys(pbrother->get_count() - 1));/*【叶子节点上一层对key的变化】设置父节点中区分pbrother与pnode的key为pbrother的最后一个元素的key。所以区分方式为：<= 与 > */
				return true;
			}
			else
			{
				/*pnode元素后移一位*/
				for (int i = pnode->get_count(); i > 0; i--)
					pnode->set_keys(i, pnode->get_keys(i - 1));
				for (int i = pnode->get_count() + 1; i > 0; i--)
					pnode->set_values(i, pnode->get_values(i - 1));
				//

				pnode->set_keys(0, pparent->get_keys(pos - 1));
				pparent->set_keys(pos - 1, pbrother->get_keys(pbrother->get_count() - 1));

				pnode->set_values(0, pbrother->get_values(pbrother->get_count()));
				pnode->set_count(pnode->get_count() + 1);

				if (pbrother->get_values(pbrother->get_count()) >= 0)
				{
					get_node(pbrother->get_values(pbrother->get_count()))->set_parent(pnode->get_block_num());
					pbrother->set_values(pbrother->get_count(), -1);
				}
				pbrother->set_count(pbrother->get_count() - 1);
				return true;
			}
		}
		else/*pnode元素过少，合并至pbrother*/
		{
			if (pnode->is_leaf())
			{
				pparent->remove(pos - 1);
				pparent->set_values(pos - 1, pbrother->get_block_num());

				for (int i = 0; i < pnode->get_count(); i++)
				{
					pbrother->set_keys(pbrother->get_count() + i, pnode->get_keys(i));
					pbrother->set_values(pbrother->get_count() + i, pnode->get_values(i));
					pnode->set_values(i, -1);
				}
				pbrother->set_count(pbrother->get_count() + pnode->get_count());
				pbrother->set_next_leaf(pnode->get_next_leaf());

				delete pnode;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
			else
			{
				pbrother->set_keys(pbrother->get_count(), pparent->get_keys(pos - 1));
				pbrother->set_count(pbrother->get_count() + 1);
				pparent->remove(pos - 1);
				pparent->set_values(pos - 1, pbrother->get_block_num());

				for (int i = 0; i < pnode->get_count(); i++)
					pbrother->set_keys(pbrother->get_count() + i, pnode->get_keys(i));

				for (int i = 0; i <= pnode->get_count(); i++)
				{
					pbrother->set_values(pbrother->get_count() + i, pnode->get_values(i));
					get_node(pnode->get_values(i))->set_parent(pbrother->get_block_num());
				}
				pbrother->set_count(2 * idx_->get_rank());

				delete pnode;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
		}
	}
	else/*pnode若不是父节点的最后一个节点，则pnode的兄弟节点是后一个节点*/
	{
		pbrother = get_node(pparent->get_values(pos + 1));
		if (pbrother->get_count() > idx_->get_rank())
		{
			if (pnode->is_leaf())
			{
				pparent->set_keys(pos, pbrother->get_keys(0));
				pnode->set_keys(pnode->get_count(), pbrother->get_keys(0));
				pnode->set_values(pnode->get_count(), pbrother->get_values(0));

				pbrother->set_values(0, -1);
				pnode->set_count(pnode->get_count() + 1);
				pbrother->remove(0);
				return true;
			}
			else
			{
				pnode->set_keys(pnode->get_count(), pparent->get_keys(pos));
				pnode->set_values(pnode->get_count() + 1, pbrother->get_values(0));
				pnode->set_count(pnode->get_count() + 1);
				pparent->set_keys(pos, pbrother->get_keys(0));
				get_node(pbrother->get_values(0))->set_parent(pnode->get_block_num());

				pbrother->remove(0);
				return true;
			}
		}
		else
		{
			if (pnode->is_leaf())
			{
				for (int i = 0; i <idx_->get_rank(); i++)
				{
					pnode->set_keys(pnode->get_count() + i, pbrother->get_keys(i));
					pnode->set_values(pnode->get_count() + i, pbrother->get_values(i));
					pbrother->set_values(i, -1);
				}

				pnode->set_count(pnode->get_count() + idx_->get_rank());
				delete pbrother;
				idx_->DecreaseNodeCount();

				pparent->remove(pos);
				pparent->set_values(pos, pnode->get_block_num());
				return mergeForRemove(pparent->get_block_num());
			}
			else
			{
				pnode->set_keys(pnode->get_count(), pparent->get_keys(pos));
				pparent->remove(pos);
				pparent->set_values(pos, pnode->get_block_num());
				pnode->set_count(pnode->get_count() + 1);

				for (int i = 0; i < idx_->get_rank(); i++)
					pnode->set_keys(pnode->get_count() + i, pbrother->get_keys(i));

				for (int i = 0; i <= idx_->get_rank(); i++)
				{
					pnode->set_values(pnode->get_count() + i, pbrother->get_values(i));
					get_node(pbrother->get_values(i))->set_parent(pnode->get_block_num());
				}

				pnode->set_count(pnode->get_count() + idx_->get_rank());
				delete pbrother;
				idx_->DecreaseNodeCount();
				return mergeForRemove(pparent->get_block_num());
			}
		}
	}
}

/**【用于直至叶子节点的查询】由node开始搜索到key所在的叶子节点。ans.flag：true，该key在B+树中存在；false，该key在B+树中不存在*/
FindNodeParam BPlusTree::search(int node, TKey &key)
{
	FindNodeParam ans;
	int index = 0;
	BTNode* pnode = get_node(node);
	if (pnode->get_count() == 0 && pnode->is_leaf() == false) {
		ans.flag = false;
		ans.index = index;
		ans.pnode = get_node(0);
		return ans;
	}
	if (pnode->search(key, index))
	{
		if (pnode->is_leaf())
		{
			ans.flag = true;
			ans.index = index;
			ans.pnode = pnode;
		}
		else
		{
			pnode = get_node(pnode->get_values(index));
			while (!pnode->is_leaf())
			{
				pnode = get_node(pnode->get_values(pnode->get_count()));
			}
			ans.flag = true;
			ans.index = pnode->get_count() - 1;
			ans.pnode = pnode;
		}
	}
	else
	{
		if (pnode->is_leaf())
		{
			ans.flag = false;
			ans.index = index;
			ans.pnode = pnode;
		}
		else
		{
			return search(pnode->get_values(index), key);
		}
	}
	return ans;
}
/*【用于删节点时，内部节点变化的查询】从node开始，查询key所在的pnode及其index。FindNodeParam中的flag：true，pnode为叶子节点；false，pnode为内部节点*/
FindNodeParam BPlusTree::search_pos(int node, TKey &key)
{
	FindNodeParam ans;
	int index = 0;
	BTNode* pnode = get_node(node);

	if (pnode->is_leaf()) throw BPlusTreeException();
	if (pnode->search(key, index))
	{
		ans.flag = true;
		ans.index = index;
		ans.pnode = pnode;
	}
	else
	{
		if (!get_node(pnode->get_values(index))->is_leaf())
			ans = search_pos(pnode->get_values(index), key);
		else
		{
			ans.flag = false;
			ans.index = index;
			ans.pnode = pnode;
		}
	}
	return ans;
}
/*获取第num个节点。实现方法：获取文件系统中第num个块，该块中的data_数据即为该节点*/
BTNode* BPlusTree::get_node(int num)
{
	BTNode* pnode = new BTNode(this, false, num);
	return pnode;
}
/*由key查询value值*/
int BPlusTree::get_value(TKey key)
{
	int ans = -1;
	if (idx_->get_root() != -1) {	/*索引不为空时。处理空表建索引后插值异常问题*/
		FindNodeParam fnp = search(idx_->get_root(), key);
		if (fnp.flag)
			ans = fnp.pnode->get_values(fnp.index);
	}
	return ans;
}

/*由key查询value值，支持单值查询与范围查询，op_type可用于判定是否为范围查询*/
vector<int> BPlusTree::get_value(TKey key, int op_type, string & searchType)
{
	vector<int> ans;
	if (idx_->get_root() != -1) {	/*索引不为空时。处理空表建索引后插值异常问题*/
		FindNodeParam fnp = search(idx_->get_root(), key);

		BTNode * temp = new BTNode(*fnp.pnode);
		BTNode * firstNode = new BTNode(*get_node(idx_->get_leaf_head()));//获取叶子头部节点信息
		int index = fnp.index;
		switch (op_type)
		{
		case SIGN_EQ:
			if (fnp.flag) {
				ans.push_back(fnp.pnode->get_values(fnp.index));
			}
			break;
		case SIGN_GT://>	
			if (!fnp.flag&&fnp.pnode->get_keys(fnp.pnode->get_count() - 1) <= key) {//防止这种情况被忽略：id(1)>0。所以该节点的最右值也<=key时，该节点才无效。
				break;
			}
			searchType = "基于B+树的范围查询";
			//读本节点的有效记录				
			while (index < (*temp).get_count()) {
				ans.push_back(temp->get_values(index));
				index++;
			}
			//读后续节点的有效记录
			while ((*temp).get_next_leaf() != -1) {
				temp = get_node((*temp).get_next_leaf());
				index = 0;
				while (index < (*temp).get_count()) {
					ans.push_back(temp->get_values(index));
					index++;
				}
			}
			break;
		case SIGN_LT://<	
			if (!fnp.flag&&fnp.pnode->get_keys(0) >= key) {
				break;
			}
			searchType = "基于B+树的范围查询";
			//读前节点到当前节点的当前位置的所有有效记录

			while ((*firstNode).get_block_num() != (*temp).get_next_leaf()) {
				int idx = 0;
				bool flag = true;
				while (idx < (*firstNode).get_count()) {
					if ((*firstNode).get_block_num() == (*temp).get_block_num() && idx == index)//读到当前块的当前KV对时，停止
					{
						flag = false;
						break;
					}
					ans.push_back(firstNode->get_values(idx));
					idx++;
				}
				if (!flag)
					break;
				firstNode = get_node((*firstNode).get_next_leaf());//下一个相邻叶子节点
			}
			break;
		case SIGN_GE://>=
			if (!fnp.flag&&fnp.pnode->get_keys(fnp.pnode->get_count() - 1) < key) {
				break;
			}
			searchType = "基于B+树的范围查询";
			ans.push_back(fnp.pnode->get_values(fnp.index));//=
															//读本节点的有效记录				
			while (++index < (*temp).get_count()) {
				ans.push_back(temp->get_values(index));
			}
			//读后续节点的有效记录
			while ((*temp).get_next_leaf() != -1) {
				temp = get_node((*temp).get_next_leaf());
				index = 0;
				while (index < (*temp).get_count()) {
					ans.push_back(temp->get_values(index));
					index++;
				}
			}
			break;
		case SIGN_LE://<=
			if (!fnp.flag&&fnp.pnode->get_keys(0) > key) {
				break;
			}
			searchType = "基于B+树的范围查询";
			//读前节点到当前节点的当前位置的所有有效记录

			while ((*firstNode).get_block_num() != (*temp).get_next_leaf()) {
				int idx = 0;
				bool flag = true;
				while (idx < (*firstNode).get_count()) {
					if ((*firstNode).get_block_num() == (*temp).get_block_num() && idx == index)
					{
						ans.push_back(fnp.pnode->get_values(fnp.index));//=
						flag = false;
						break;
					}
					ans.push_back(firstNode->get_values(idx));
					idx++;
				}
				if (!flag)
					break;
				firstNode = get_node((*firstNode).get_next_leaf());//下一个相邻叶子节点
			}
			break;
		default:
			break;
		}


	}
	return ans;
}
/*获取新block块号，idx_索引最大值增一*/
int BPlusTree::get_new_blocknum()
{
	return idx_->IncreaseMaxCount();
}

void BPlusTree::print()
{
	printf("*----------------------------------------------*\n");
	printf("键数: %d, 节点数: %d, 层级: %d, 根节点: %d \n", idx_->get_key_count(), idx_->get_node_count(), idx_->get_level(), idx_->get_root());

	if (idx_->get_root() != -1)
		print_node(idx_->get_root());
}
/**打印节点信息*/
void BPlusTree::print_node(int num)
{
	BTNode* pnode = get_node(num);
	pnode->print();
	if (!pnode->is_leaf())
	{
		for (int i = 0; i <= pnode->get_count(); ++i)
			print_node(pnode->get_values(i));
	}
}

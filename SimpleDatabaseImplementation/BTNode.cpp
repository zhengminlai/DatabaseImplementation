//简介：B+树索引
//Implemented by Xu Jing
//作用：实现外存B+索引
#include <iomanip>
#include<iostream>
#include "BTNode.h"
#include "Exceptions.h"
#include "ConstValue.h"

using namespace std;

BTNode::BTNode(BPlusTree* tree, bool isnew, int blocknum, bool newleaf)
{
	tree_ = tree;
	is_leaf_ = newleaf;
	rank_ = (tree_->get_degree() - 1) / 2;
	block_num_ = blocknum;
	get_buffer();
	if (isnew)
	{
		set_parent(-1);
		set_node_type(newleaf ? 1 : 0);
		set_count(0);
	}
}

BTNode::~BTNode() {}

int BTNode::get_block_num() { return block_num_; }
/*获取第index个key值*/
TKey BTNode::get_keys(int index)
{
	TKey k(tree_->GetIndex()->get_key_type(), tree_->GetIndex()->get_key_len());
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	memcpy(k.get_key(), &buffer_[base + index * lenr + 4], tree_->GetIndex()->get_key_len());
	return k;
}
/*获取第index个value值*/
int BTNode::get_values(int index)
{
	int base = 12;
	int lenR = 4 + tree_->GetIndex()->get_key_len();
	return *((int*)(&buffer_[base + index*lenR]));
}
/*获取下一个叶子节点*/
int BTNode::get_next_leaf()
{
	int base = 12;
	int lenR = 4 + tree_->GetIndex()->get_key_len();
	return *((int*)(&buffer_[base + tree_->get_degree()*lenR]));
}
/*获取下一个父节点，得到的是该节点在buffer中的地址编号，是该节点所在的buffer的第8-11字节数据*/
int BTNode::get_parent() { return *((int*)(&buffer_[8])); }
/*获取节点类型，即该节点所在buffer中的第0-3字节数据*/
int BTNode::get_node_type() { return *((int*)(&buffer_[0])); }
/*获取节点的数据个数，即该节点所在buffer中的第4-7字节数据*/
int BTNode::get_count() { return *((int*)(&buffer_[4])); }
/*判断是否是叶子节点*/
bool BTNode::is_leaf() { return get_node_type() == 1; }
/*将key.key_放入该节点的第index个键中*/
void BTNode::set_keys(int index, TKey key)
{
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	memcpy(&buffer_[base + index*lenr + 4], key.get_key(), tree_->GetIndex()->get_key_len());
}
/*设置第index元素的值为val*/
void BTNode::set_values(int index, int val)
{
	int base = 12;
	int lenr = 4 + tree_->GetIndex()->get_key_len();
	*((int*)(&buffer_[base + index*lenr])) = val;
}
/*设置下一个叶子节点的值*/
void BTNode::set_next_leaf(int val)
{
	int base = 12;
	int len = 4 + tree_->GetIndex()->get_key_len();
	*((int*)(&buffer_[base + tree_->get_degree() * len])) = val;/*设置该节点的最后一个value为指向下一叶子节点的指针。degree：节点的度，即该节点可以放的KV对最大个数*/
}
/*设置父节点的值*/
void BTNode::set_parent(int val) { *((int*)(&buffer_[8])) = val; }
/*节点类型：叶子节点为1，非叶子节点为0*/
void BTNode::set_node_type(int val) { *((int*)(&buffer_[0])) = val; }
/*设置节点存储的元素个数*/
void BTNode::set_count(int val) { *((int*)(&buffer_[4])) = val; }
/*设置节点是否为叶子节点*/
void BTNode::set_is_leaf(bool val) { set_node_type(val ? 1 : 0); }
/*从file中获取当前B+树所在的db中当前树索引的文件块*/
void BTNode::get_buffer()
{
	BlockInfo *bp = tree_->GetBufferManager()->GetFileBlock(tree_->get_db_name(), tree_->GetIndex()->get_name(), FORMAT_INDEX, block_num_);
	buffer_ = bp->get_data();
	bp->set_dirty(true);
}
/*在B+树中搜索key所在的位置，并赋值到index中。返回值：true，index即为由key定位的节点地址；false，index即为指向下一层的指针。节点元素个数20以内，顺序查找；20以外，二分查找*/
bool BTNode::search(TKey key, int &index)
{
	bool ans = false;
	if (get_count() == 0) { index = 0; return false; }
	if (get_keys(0) > key) { index = 0;  return false; }
	if (get_keys(get_count() - 1) < key) { index = get_count(); return false; }
	if (get_count() > 20)										/* 二分查找 */
	{
		int mid, start, end;
		start = 0;
		end = get_count() - 1;
		while (start < end)
		{
			mid = (start + end) / 2;
			if (key == get_keys(mid)) { index = mid; return true; }
			else if (key < get_keys(mid)) end = mid;
			else start = mid;

			if (start == end - 1)
			{
				if (key == get_keys(start)) { index = start; return true; }
				if (key == get_keys(end)) { index = end; return true; }
				if (key < get_keys(start)) { index = start; return false; }
				if (key < get_keys(end)) { index = end; return false; }
				if (key > get_keys(end)) { index = end + 1; return false; }
			}
		}
		return false;
	}
	else														/* 顺序查找 */
	{
		for (int i = 0; i < get_count(); i++)
		{
			if (key < get_keys(i))
			{
				index = i;
				ans = false;
				break;
			}
			else if (key == get_keys(i))
			{
				index = i;
				ans = true;
				break;
			}
		}
		return ans;
	}
}
/*先查找b+树中是否存在该key，否：将key插入其对应的位置中*/
int BTNode::add(TKey &key)
{
	int index = 0;
	if (get_count() == 0)
	{
		set_keys(0, key);
		set_count(1);
		return 0;
	}
	if (!search(key, index))
	{
		for (int i = get_count(); i > index; i--)
			set_keys(i, get_keys(i - 1));

		for (int i = get_count() + 1; i > index; i--)
			set_values(i, get_values(i - 1));

		set_keys(index, key);
		set_values(index, -1);
		set_count(get_count() + 1);
	}
	return index;
}
/*插入KV对*/
int BTNode::add(TKey &key, int &val)
{
	int index = 0;
	if (get_count() == 0)
	{
		set_keys(0, key);
		set_values(0, val);
		set_count(get_count() + 1);
		return 0;
	}
	if (!search(key, index)) {
		for (int i = get_count(); i > index; i--)
		{
			set_keys(i, get_keys(i - 1));
			set_values(i, get_values(i - 1));
		}

		set_keys(index, key);
		set_values(index, val);
		set_count(get_count() + 1);
	}
	return index;
}
/*节点分裂*/
BTNode* BTNode::split(TKey &key)
{
	BTNode* newnode = new BTNode(tree_, true, tree_->get_new_blocknum(), is_leaf());
	if (newnode == NULL)
	{
		throw BPlusTreeException();
		return NULL;
	}
	key = get_keys(rank_);
	if (is_leaf())
	{
		for (int i = rank_ + 1; i< tree_->get_degree(); i++)
		{
			newnode->set_keys(i - rank_ - 1, get_keys(i));
			newnode->set_values(i - rank_ - 1, get_values(i));
		}

		newnode->set_count(rank_);
		set_count(rank_ + 1);
		newnode->set_next_leaf(get_next_leaf());
		set_next_leaf(newnode->get_block_num());
		newnode->set_parent(get_parent());
	}
	else
	{
		for (int i = rank_ + 1; i< tree_->get_degree(); i++)
			newnode->set_keys(i - rank_ - 1, get_keys(i));

		for (int i = rank_ + 1; i <= tree_->get_degree(); i++)
			newnode->set_values(i - rank_ - 1, get_values(i));

		newnode->set_parent(get_parent());
		newnode->set_count(rank_);

		int childnode_num;
		for (int i = 0; i <= newnode->get_count(); i++)
		{
			childnode_num = newnode->get_values(i);
			BTNode* node = tree_->get_node(childnode_num);
			if (node) node->set_parent(newnode->get_block_num());
		}
		set_count(rank_);
	}
	return newnode;
}
/*判断是否为根节点*/
bool BTNode::isRoot()
{
	if (get_parent() == -1) return true;
	return false;
}
//从本节点中删除第index元素
bool BTNode::remove(int index)
{
	if (index > get_count() - 1) return false;
	if (is_leaf()) {
		for (int i = index; i < get_count() - 1; i++)
		{
			set_keys(i, get_keys(i + 1));
			set_values(i, get_values(i + 1));
		}
	}
	else
	{
		for (int i = index; i< get_count() - 1; i++)
			set_keys(i, get_keys(i + 1));

		for (int i = index; i < get_count(); i++)
			set_values(i, get_values(i + 1));
	}
	set_count(get_count() - 1);
	return true;
}
//B+树节点打印
void BTNode::print()
{
	printf("*----------------------------------------------*\n");
	printf("块号: %d 个数: %d, 父节点: %d  是叶子节点？:%d\n", block_num_, get_count(), get_parent(), is_leaf());
	printf("键K: { ");
	for (int i = 0; i < get_count(); i++)
		cout << setw(9) << left << get_keys(i);
	printf(" }\n");

	if (is_leaf())
	{
		printf("值V: { ");
		for (int i = 0; i < get_count(); i++)
		{
			if (get_values(i) == -1) printf("{NUL}");
			else
				cout << setw(9) << left << get_values(i);
		}
		printf(" }\n");
		printf("下一叶子: %5d\n", get_next_leaf());
	}
	else
	{
		printf("Ptrs: {");
		for (int i = 0; i <= get_count(); i++)
			cout << setw(9) << left << get_values(i);

		printf("}\n");
	}
}

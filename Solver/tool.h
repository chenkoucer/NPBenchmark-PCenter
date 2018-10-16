#include<vector>
#include<iostream>
#include<algorithm>
using namespace std;

//template <typename  T>
namespace ck{
	vector< int > sortIndexes(const vector< int > &v, int k) {
		//返回前k个最小值对应的索引值
		vector< int > idx(v.size());
		for (int i = 0; i != idx.size(); ++i) {
			idx[i] = i;
		}
		sort(idx.begin(), idx.end(),
			[&v](int i1, int i2) {return v[i1] < v[i2]; });
		idx.assign(idx.begin(), idx.begin() + k);
		return idx;
	}
	class Table{
	public: 
		vector<vector<int>> fTable, dTable;
		void addNodeToTable(){}
		void deleteNodeInTable(){}

	};//Table
	int selectSeveredNode() {
		return 0;
	}

}

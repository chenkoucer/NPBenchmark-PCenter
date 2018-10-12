#include <vector>
#include <iostream>

#include "CheckConstraints.h"

const int INF = 1000000000;

CheckConstraints::CheckConstraints(const pb::PCenter::Input &input, const pb::PCenter::Output &output) {
    numOfNodes = generateNum(input);
    vector<vector<int>> G = generateGraph(input);
    numOfCenters = input.centernum();
    vector<int> centers;
    vector<vector<int>> distance(numOfCenters, vector<int>(numOfNodes)); // 所有服务节点到其余节点的距离
    for (int i = 0; i < numOfCenters; ++i) {
        centers.push_back(output.centers(i));
        distance.push_back(Dijkstra(numOfNodes, output.centers(i) - 1, G));
    }
    vector<int> serveLength;
    for (int i = 0; i < numOfNodes; ++i) {
        if (find(centers.begin(), centers.end(), i + 1) == centers.end()) { // 只处理用户节点
            for (int j = 0; j < numOfCenters; ++j) {
                vector<int> length;
                length.push_back(distance[j][i]);
                auto minPosition = min_element(length.begin(), length.end());
                serveLength.push_back(*minPosition); // 服务边的长度
            }
        }
    }
    auto maxPosition = max_element(serveLength.begin(), serveLength.end()); // 服务边中的最大值
    maxLength = *maxPosition;
}

vector<int> CheckConstraints::Dijkstra(int n, int s, vector<vector<int>> G) { // 求服务节点到其余节点的距离
    vector<int> d(n, INF); // 初始化最短距离矩阵，全部为INF
    vector<bool> vis(n); // 标记节点是否被访问
    vector<int> pre(n); // 最短路径的上一个节点
    for (int i = 0; i < n; ++i)
        pre[i] = i;

    d[s] = 0;
    for (int i = 0; i < n; ++i) {
        int u = -1;
        int MIN = INF;
        for (int j = 0; j < n; ++j) {
            if (vis[j] == false && d[j] < MIN) {
                u = j;
                MIN = d[j];
            }
        }
        if (u == -1)
            return d;
        vis[u] = true; // 标记u已被访问
        for (int v = 0; v < n; ++v) {
            if (vis[v] == false && d[u] + G[u][v] < d[v]) {
                d[v] = d[u] + G[u][v];
                pre[v] = u;
            }
        }
    }
    return d;
}

int CheckConstraints::generateNum(const pb::PCenter::Input input) { // 获取节点数
    vector<int> vec;
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        vec.push_back(edge->source());
    }
    auto maxPosition = max_element(vec.begin(), vec.end());
    return *maxPosition;
}

vector<vector<int>> CheckConstraints::generateGraph(const pb::PCenter::Input &input) { // 生成临接表
    vector<vector<int>> G(numOfNodes, vector<int>(numOfNodes, INF));
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        int source = edge->source();
        int target = edge->target();
        int length = edge->length();
        G[source - 1][target - 1] = length;
        G[target - 1][source - 1] = length;
    }
    for (int i = 0; i < numOfNodes; ++i) {
        G[i][i] = 0;
    }
    return G;
}
#include <vector>
#include <iostream>

#include "CheckConstraints.h"



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
#include <vector>
#include <iostream>

#include "CheckConstraints.h"



CheckConstraints::CheckConstraints(const pb::PCenter::Input &input_s, const pb::PCenter::Output &output_s) {
    input = input_s;
    output = output_s;
}

int CheckConstraints::generateNum() { // ��ȡ�ڵ���
    vector<int> vec;
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        vec.push_back(edge->source());
        vec.push_back(edge->target());
    }
    auto maxPosition = max_element(vec.begin(), vec.end());
    numOfNodes = *maxPosition;
    return numOfNodes;
}

vector<vector<int>> CheckConstraints::generateGraph() { // �����ٽӱ�
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

int CheckConstraints::generateMaxLength() {
    vector<vector<int>> G = generateGraph();
    numOfCenters = input.centernum();
    vector<int> centers;
    vector<vector<int>> distance(numOfCenters, vector<int>(numOfNodes)); // ���з���ڵ㵽����ڵ�ľ���
    for (int i = 0; i < numOfCenters; ++i) {
        centers.push_back(output.centers(i));
        distance[i] = Dijkstra(numOfNodes, output.centers(i), G);
    }
    vector<int> serveLength;
    for (int i = 0; i < numOfNodes; ++i) {
        if (find(centers.begin(), centers.end(), i + 1) == centers.end()) { // ֻ�����û��ڵ�
            vector<int> length(numOfCenters);
            for (int j = 0; j < numOfCenters; ++j) {
                length[j] = distance[j][i];
            }
            auto minPosition = min_element(length.begin(), length.end());
            serveLength.push_back(*minPosition); // ����ߵĳ���
        }
    }
    auto maxPosition = max_element(serveLength.begin(), serveLength.end()); // ������е����ֵ
    maxLength = *maxPosition;
    return maxLength;
}

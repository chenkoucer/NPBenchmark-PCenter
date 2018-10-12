#ifndef CHECKCONSTRAINTS_H
#define CHECKCONSTRAINTS_H

#include <vector>
#include <iostream>
#include"../Solver/PCenter.pb.h"

using namespace std;

class CheckConstraints {
public:
    int numOfNodes;
    int numOfCenters;
    int maxLength = 0;

public:
    CheckConstraints(const pb::PCenter::Input &input, const pb::PCenter::Output &output);

public:
    vector<int> Dijkstra(int n, int s, vector<vector<int>> G);
    int generateNum(const pb::PCenter::Input input);
    vector<vector<int>> generateGraph(const pb::PCenter::Input &input);


};

#endif
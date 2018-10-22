#ifndef CHECKCONSTRAINTS_H
#define CHECKCONSTRAINTS_H

#include <vector>
#include <iostream>
#include"../Solver/PCenter.pb.h"

using namespace std;

const int INF = INT32_MAX;


class CheckConstraints {
public:
    int numOfNodes = 0;
    int numOfCenters = 0;
    int maxLength = 0;
    pb::PCenter::Input input;
    pb::PCenter::Output output;

public:
    CheckConstraints(const pb::PCenter::Input &input_s, const pb::PCenter::Output &output_s);

public:
    static vector<int> Dijkstra(int n, int s, vector<vector<int>> &G) {
        vector<int> d(n, INF); // ��ʼ����̾������ȫ��ΪINF
        vector<bool> vis(n); // ��ǽڵ��Ƿ񱻷���
        vector<int> pre(n); // ���·������һ���ڵ�
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
            if (u == -1) {
                return d;
            }
            vis[u] = true; // ���u�ѱ�����
            for (int v = 0; v < n; ++v) {
                if (vis[v] == false && d[u] + G[u][v] < d[v]) {
                    d[v] = d[u] + G[u][v];
                    pre[v] = u;
                }
            }
        }
        return d;
    }

    int generateNum();
    vector<vector<int>> generateGraph();
    int generateMaxLength();


};

#endif
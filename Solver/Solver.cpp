#include "Solver.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <cmath>
#include<map>
#include "../Checker/CheckConstraints.h"


using namespace std;

namespace szx {

#pragma region Solver::Cli
int Solver::Cli::run(int argc, char * argv[]) {
    Log(LogSwitch::Szx::Cli) << "parse command line arguments." << endl;
    Set<String> switchSet;
    Map<String, char*> optionMap({ // use string as key to compare string contents instead of pointers.
        { InstancePathOption(), nullptr },
        { SolutionPathOption(), nullptr },
        { RandSeedOption(), nullptr },
        { TimeoutOption(), nullptr },
        { MaxIterOption(), nullptr },
        { JobNumOption(), nullptr },
        { RunIdOption(), nullptr },
        { EnvironmentPathOption(), nullptr },
        { ConfigPathOption(), nullptr },
        { LogPathOption(), nullptr }
    });

    for (int i = 1; i < argc; ++i) { // skip executable name.
        auto mapIter = optionMap.find(argv[i]);
        if (mapIter != optionMap.end()) { // option argument.
            mapIter->second = argv[++i];
        } else { // switch argument.
            switchSet.insert(argv[i]);
        }
    }

    Log(LogSwitch::Szx::Cli) << "execute commands." << endl;
    if (switchSet.find(HelpSwitch()) != switchSet.end()) {
        cout << HelpInfo() << endl;
    }

    if (switchSet.find(AuthorNameSwitch()) != switchSet.end()) {
        cout << AuthorName() << endl;
    }

    Solver::Environment env;
    env.load(optionMap);
    if (env.instPath.empty() || env.slnPath.empty()) { return -1; }

    Solver::Configuration cfg;
    cfg.load(env.cfgPath);

    Log(LogSwitch::Szx::Input) << "load instance " << env.instPath << " (seed=" << env.randSeed << ")." << endl;
    Problem::Input input;
    if (!input.load(env.instPath)) { return -1; }

    Solver solver(input, env, cfg);
    solver.solve();

    pb::PCenter_Submission submission;
    submission.set_thread(to_string(env.jobNum));
    submission.set_instance(env.friendlyInstName());
    submission.set_duration(to_string(solver.timer.elapsedSeconds()) + "s");

    solver.output.save(env.slnPath, submission);
    #if JQ_DEBUG
    solver.output.save(env.solutionPathWithTime(), submission);
    solver.record();
    #endif // JQ_DEBUG

    return 0;
}
#pragma endregion Solver::Cli

#pragma region Solver::Environment
void Solver::Environment::load(const Map<String, char*> &optionMap) {
    char *str;

    str = optionMap.at(Cli::EnvironmentPathOption());
    if (str != nullptr) { loadWithoutCalibrate(str); }

    str = optionMap.at(Cli::InstancePathOption());
    if (str != nullptr) { instPath = str; }

    str = optionMap.at(Cli::SolutionPathOption());
    if (str != nullptr) { slnPath = str; }

    str = optionMap.at(Cli::RandSeedOption());
    if (str != nullptr) { randSeed = atoi(str); }

    str = optionMap.at(Cli::TimeoutOption());
    if (str != nullptr) { msTimeout = static_cast<Duration>(atof(str) * Timer::MillisecondsPerSecond); }

    str = optionMap.at(Cli::MaxIterOption());
    if (str != nullptr) { maxIter = atoi(str); }

    str = optionMap.at(Cli::JobNumOption());
    if (str != nullptr) { jobNum = atoi(str); }

    str = optionMap.at(Cli::RunIdOption());
    if (str != nullptr) { rid = str; }

    str = optionMap.at(Cli::ConfigPathOption());
    if (str != nullptr) { cfgPath = str; }

    str = optionMap.at(Cli::LogPathOption());
    if (str != nullptr) { logPath = str; }

    calibrate();
}

void Solver::Environment::load(const String &filePath) {
    loadWithoutCalibrate(filePath);
    calibrate();
}

void Solver::Environment::loadWithoutCalibrate(const String &filePath) {
    // EXTEND[szx][8]: load environment from file.
    // EXTEND[szx][8]: check file existence first.
}

void Solver::Environment::save(const String &filePath) const {
    // EXTEND[szx][8]: save environment to file.
}
void Solver::Environment::calibrate() {
    // adjust thread number.
    int threadNum = thread::hardware_concurrency();
    if ((jobNum <= 0) || (jobNum > threadNum)) { jobNum = threadNum; }

    // adjust timeout.
    msTimeout -= Environment::SaveSolutionTimeInMillisecond;
}
#pragma endregion Solver::Environment

#pragma region Solver::Configuration
void Solver::Configuration::load(const String &filePath) {
    // EXTEND[szx][5]: load configuration from file.
    // EXTEND[szx][8]: check file existence first.
}

void Solver::Configuration::save(const String &filePath) const {
    // EXTEND[szx][5]: save configuration to file.
}
#pragma endregion Solver::Configuration

#pragma region Solver
bool Solver::solve() {
    init();

    int workerNum = (max)(1, env.jobNum / cfg.threadNumPerWorker);
    cfg.threadNumPerWorker = env.jobNum / workerNum;
    List<Solution> solutions(workerNum, Solution(this));
    List<bool> success(workerNum);

    Log(LogSwitch::Szx::Framework) << "launch " << workerNum << " workers." << endl;
    List<thread> threadList;
    threadList.reserve(workerNum);
    for (int i = 0; i < workerNum; ++i) {
        // TODO[szx][2]: as *this is captured by ref, the solver should support concurrency itself, i.e., data members should be read-only or independent for each worker.
        // OPTIMIZE[szx][3]: add a list to specify a series of algorithm to be used by each threads in sequence.
        threadList.emplace_back([&, i]() { success[i] = optimize(solutions[i], i); });
    }
    for (int i = 0; i < workerNum; ++i) { threadList.at(i).join(); }

    Log(LogSwitch::Szx::Framework) << "collect best result among all workers." << endl;
    int bestIndex = -1;
    Length bestValue = INF;
    for (int i = 0; i < workerNum; ++i) {
        if (!success[i]) { continue; }
        Log(LogSwitch::Szx::Framework) << "worker " << i << " got " << solutions[i].maxLength << endl;
        //cout << solutions[i].centers().size();
        if (solutions[i].maxLength >= bestValue) { continue; }
        bestIndex = i;
        bestValue = solutions[i].maxLength;
    }
    env.rid = to_string(bestIndex);
    if (bestIndex < 0) { return false; }
    output = solutions[bestIndex];
    return true;
}

void Solver::record() const {
    #if JQ_DEBUG
    int generation = 0;

    ostringstream log;

    System::MemoryUsage mu = System::peakMemoryUsage();

    Length obj = output.maxLength;
    //cout << obj << endl;
    Length checkerObj = -1;
    bool feasible = check(checkerObj);

    // record basic information.
    log << env.friendlyLocalTime() << ","
        << env.rid << ","
        << env.instPath << ","
        << feasible << "," << (obj - checkerObj) << ","
        << output.maxLength << ","
        << timer.elapsedSeconds() << ","
        << mu.physicalMemory << "," << mu.virtualMemory << ","
        << env.randSeed << ","
        << cfg.toBriefStr() << ","
        << generation << "," << iteration << ",";
        

    // record solution vector.
    // EXTEND[szx][2]: save solution in log.
    log << endl;

    // append all text atomically.
    static mutex logFileMutex;
    lock_guard<mutex> logFileGuard(logFileMutex);

    ofstream logFile(env.logPath, ios::app);
    logFile.seekp(0, ios::end);
    if (logFile.tellp() <= 0) {
        logFile << "Time,ID,Instance,Feasible,ObjMatch,Width,Duration,PhysMem,VirtMem,RandSeed,Config,Generation,Iteration" << endl;
    }
    logFile << log.str();
    logFile.close();
    #endif // JQ_DEBUG
}

bool Solver::check(Length &checkerObj) const {
    #if JQ_DEBUG
    enum CheckerFlag {
        IoError = 0x0,
        FormatError = 0x1,
        CenterRepeatedError = 0x2,
    };

    checkerObj = System::exec("Checker.exe " + env.instPath + " " + env.solutionPathWithTime());
    if (checkerObj > 0) { return true; }
    checkerObj = ~checkerObj;
    if (checkerObj == CheckerFlag::IoError) { Log(LogSwitch::Checker) << "IoError." << endl; }
    if (checkerObj & CheckerFlag::FormatError) { Log(LogSwitch::Checker) << "FormatError." << endl; }
    if (checkerObj & CheckerFlag::CenterRepeatedError) { Log(LogSwitch::Checker) << "CenterRepeatedError." << endl; }
    return false;
    #else
    checkerObj = 0;
    return true;
    #endif // JQ_DEBUG
}

void Solver::init() {
    
}

bool Solver::optimize(Solution &sln, ID workerId) {
    Log(LogSwitch::Szx::Framework) << "worker " << workerId << " starts." << endl;
	vector<int> vec;
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        vec.push_back(edge->source());
        vec.push_back(edge->target());
    }
    auto maxPosition = max_element(vec.begin(), vec.end());
    nodeNum = *maxPosition;
    edgeNum = input.graph().edges().size();
    centerNum = input.centernum();
    bool status = true;
    sln.maxLength = 0;
	
    // TODO[0]: replace the following random assignment with your own algorithm.
	G.resize(nodeNum);
	for (int i = 0; i < nodeNum; i++) {
		G[i].resize(nodeNum);
	}
	for (int i = 0; i < nodeNum; ++i) {
		for (int j = 0; j < nodeNum; ++j) {
			G[i][j] = INT32_MAX;
		}
	}	
	for (int i = 0; i < nodeNum; ++i)
		G[i][i] = 0;
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        int source = edge->source();
        int target = edge->target();
        int length = edge->length();
        G[source - 1][target - 1] = length;
        G[target - 1][source - 1] = length;
    }
	for (int k = 0; k < nodeNum; ++k)
		for (int i = 0; i < nodeNum; ++i)
			for (int j = 0; j < nodeNum; ++j)
				if (G[i][k] != INT32_MAX && G[k][j] != INT32_MAX &&
					G[i][j] > G[i][k] + G[k][j])
					G[i][j] = G[i][k] + G[k][j];
	//初始化服务节点
	isServerdNode.assign(nodeNum, false);
	int index = rand.pick(nodeNum);
	dTable.assign(2, vector<int>(nodeNum, INF));
	fTable.assign(2, vector<int>(nodeNum, -1));
	centers.clear();
	centers.push_back(index);
	isServerdNode[index] = true;
	fTable[0].assign(nodeNum, index);
	dTable[0] = G[index];
	for (int f = 1; f < centerNum; ++f) {//从初始节点开始，依次构造服务节点
		int serverNode = selectNextSeveredNode();
		addNodeToTable(serverNode);
		cout << "inital maxLength : " << f <<"  "<< maxLength << endl;
	}
	//测试初始解
	/*
	for (int i = 0; i < centers.size(); ++i) {
		sln.add_centers(centers[i] + 1);
	}
	int maxServerLength = -1, serveredNode = -1;
	for (int v = 0; v < nodeNum; ++v) {
		if (dTable[0][v] > maxServerLength) {
			maxServerLength = dTable[0][v];
			serveredNode = v;
		}
	}
	sln.maxLength = maxServerLength;
	*/
	//交换服务节点
	cout << "inital maxLength : " << maxLength << endl;
	hist_maxLength = maxLength;
	tableTenure.assign(nodeNum, vector<int>(nodeNum, 0));
	vector<int> switchNodes;
	vector<vector<int>> switchNodePairs;
	vector<int> switchNodePair;
	for (int t = 0; t < 10000; ++t) {
        switchNodePair.clear();
        switchNodes.clear();
        switchNodePairs.clear();
		switchNodes = findSeveredNodeNeighbourhood();//待增添节点
        //for (int i = 0; i < switchNodes.size(); ++i) {
        //     cout << "witchnode:  "<<switchNodes[i] << "   length:  "<<dTable[0][switchNodes[i]] << endl;
        //}
		switchNodePairs = findPair(switchNodes, t);//交换节点对，若存在相同的Mf则全部返回
		if (switchNodePairs.size() == 0)
			continue;
		switchNodePair = switchNodePairs[rand.pick(switchNodePairs.size())];//交换节点对
		int f = switchNodePair[0], v = switchNodePair[1], minMaxlength = switchNodePair[2];
        //cout << "f :" << f << "   v :" << v << endl;
		addNodeToTable(f);
        //cout << "add f length: " << maxLength << endl;
		deleteNodeInTable(v);
        //cout << "delete v length: " << maxLength << endl;
		tableTenure[f][v] = t + step_tenure;//禁忌服务点在接下来的一段时间内被交换用户节点

	}
	for (int i = 0; i < centers.size(); ++i) {
		sln.add_centers(centers[i] + 1);
	}
	sln.maxLength = maxLength;
	cout << "maxLength after change: " << maxLength << endl;
    Log(LogSwitch::Szx::Framework) << "worker " << workerId << " ends." << endl;
    return status;
}

void Solver::addNodeToTable(int node)
{
    isServerdNode[node] = true;
	maxLength = 0;
	centers.push_back(node);
	isServerdNode[node] = true;
	for (int v = 0; v < nodeNum; ++v) {//更新f表和t表
		if (G[node][v] < dTable[0][v]) {
			dTable[1][v] = dTable[0][v];
			dTable[0][v] = G[node][v];
			fTable[1][v] = fTable[0][v];
			fTable[0][v] = node;
		}
		else if (G[node][v] < dTable[1][v]) {
			dTable[1][v] = G[node][v];
			fTable[1][v] = node;
		}
		if (dTable[0][v] > maxLength)
			maxLength = dTable[0][v];
	}
}

void Solver::deleteNodeInTable(int node)
{
    isServerdNode[node] = false;
    maxLength = 0;
	int i = 0;
	for (; i < centers.size() && centers[i] != node; ++i);//寻找要删除的服务节点
	for (; i < centers.size()-1; ++i) {
		centers[i] = centers[i + 1];
	}
	centers.pop_back();
	for (int v = 0; v < nodeNum; ++v) {
		if (fTable[0][v] == node) {
			fTable[0][v] = fTable[1][v];
			dTable[0][v] = dTable[1][v];
			findNext(v);
			
		}
		else if (fTable[1][v] == node) {
			findNext(v);
		}
		if (dTable[0][v] > maxLength)
			maxLength = dTable[0][v];
	}
}

void Solver::findNext(int v)
{	
	int nextNode = -1, secondLength = INF;
	for (int i = 0; i < centers.size(); ++i) {
		int f = centers[i];//寻找下一个次近服务节点
		if (f != fTable[0][v] && G[v][f] < secondLength) {
			secondLength = G[v][f]; 
			nextNode = f;
		}
	}
	dTable[1][v] = secondLength;
	fTable[1][v] = nextNode;
}

int Solver::selectNextSeveredNode()
{
	vector<int> kClosedNode = findSeveredNodeNeighbourhood();
	int serveredNode = kClosedNode[rand.pick(kClosedNode.size())];//从这k个近节点中随机选择一个作为服务节点
	return serveredNode;
}

vector<int> Solver::findSeveredNodeNeighbourhood()
{
	int maxServerLength = -1;
	vector<int> serveredNodes;
	for (int v = 0; v < nodeNum; ++v) {
		if (dTable[0][v] > maxServerLength) {
			serveredNodes.clear();
			maxServerLength = dTable[0][v];
			serveredNodes.push_back(v);
		}
		else if (dTable[0][v] == maxServerLength) {
			serveredNodes.push_back(v);
		}
	}
	int serveredNode = serveredNodes[rand.pick(serveredNodes.size())];
	vector<int> kClosedNode; //备选用户节点v的前k个最近节点
	kClosedNode = sortIndexes(G[serveredNode], kClosed, maxServerLength);
	return kClosedNode;
}

vector<int> Solver::sortIndexes(const vector<int>& v, int k,int length)
{
	//返回前k个最小值对应的索引值
	vector<int> idx(v.size());
	vector<int> res;
	for (int i = 0; i != idx.size(); ++i) {
		idx[i] = i;
	}
	sort(idx.begin(), idx.end(),
		[&v](int i1, int i2) {return v[i1] < v[i2]; });
	for (int i = 0; i < k; i++) {
		if (isServerdNode[idx[i]]) {
			++k;
			continue;
		}
		if (v[idx[i]] < length)
			res.push_back(idx[i]);
	}
	return res;
}

vector<vector<int>> Solver::findPair(const vector<int>& switchNode, int t)
{
	int minMaxLength = INF;//原始目标函数值
	vector<vector<int>> res;
    map<int, int> Mf; //t < serverTableTenure[v] && t < userTableTenure[f]
	for (int i : switchNode) {
		//isServerdNode[i] = true;
		Mf.clear();//存放删除某个服务节点f后产生的最长服务边maxlength，key为f value为maxlength
		for (int j = 0; j < centers.size(); ++j) {
			Mf[centers[j]] = 0;
		}
		for (int v = 0; v < dTable[0].size(); ++v) {
			if (min(G[i][v], dTable[1][v]) > Mf[fTable[0][v]])
				Mf[fTable[0][v]] = min(G[i][v], dTable[1][v]);
		}
		for (int f = 0; f < centers.size(); f++) {
			//选出删除f后所产生的最小最长服务边
			if (t < tableTenure[i][centers[f]] && Mf[centers[f]] >=  maxLength)
				continue;
			if (Mf[centers[f]] == minMaxLength) {
				vector<int> r;
				r.clear();
				r.push_back(i);
                r.push_back(centers[f]);
				r.push_back(minMaxLength);
				res.push_back(r);
			}
			else if ( Mf[centers[f]] < minMaxLength) {
				minMaxLength = Mf[centers[f]];
				res.clear();
				vector<int> r;
				r.clear();
                r.push_back(i);
                r.push_back(centers[f]);
				r.push_back(minMaxLength);
				res.push_back(r);
				
			}
		}
	}
	return  res;
}


#pragma endregion Solver

}

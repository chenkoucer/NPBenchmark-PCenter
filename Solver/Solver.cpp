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
    //vector<int> vec;
	vector<int> vec;
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        vec.push_back(edge->source());
        vec.push_back(edge->target());
    }
    auto maxPosition = max_element(vec.begin(), vec.end());
    ID nodeNum = *maxPosition;
    ID edgeNum = input.graph().edges().size();
    ID centerNum = input.centernum();
	//ID kClosed = 8; // ����û��ڵ��ǰk�����ڽڵ㣬�����ʼ��ʱ�õ�
    bool status = true;
    sln.maxLength = 0;
	
    // TODO[0]: replace the following random assignment with your own algorithm.
    vector<vector<int>> G(nodeNum, vector<int>(nodeNum, INF));
    for (auto edge = input.graph().edges().begin(); edge != input.graph().edges().end(); ++edge) {
        int source = edge->source();
        int target = edge->target();
        int length = edge->length();
        G[source - 1][target - 1] = length;
        G[target - 1][source - 1] = length;
    }
    for (int i = 0; i < nodeNum; ++i) {
        G[i] = CheckConstraints::Dijkstra(nodeNum, i, G);
    }

	//��ʼ������ڵ�
#pragma initialize solution{
	vector<int> centers;
	int index = rand.pick(nodeNum);
	//vector<vector<int>> fTable(2, vector<int>(nodeNum, -1));// ��ʾ�ڵ�v���������ڵ�fTable[0][v]�ʹν�����ڵ�fTable[1][v]
	//vector<vector<int>> dTable(2, vector<int>(nodeNum, INF)); //��ʾ�ڵ�v���������ھ���dTable[0][v]�ʹν�����ڵ����dTable[1][v]
	fTable.assign(2, vector<int>(nodeNum, -1));
	dTable.assign(2, vector<int>(nodeNum, INF));
	//sln.add_centers(index);
	centers.push_back(index);
	fTable[0].assign(nodeNum, index);
	dTable[0] = CheckConstraints::Dijkstra(nodeNum, index, G);
	for (int f = 1; f < centerNum; ++f) {//�ӳ�ʼ�ڵ㿪ʼ�����ι������ڵ�
		int serverNode = selectSeveredNode(nodeNum, G);
		addNodeToTable(centers, serverNode, nodeNum, G);
	}
	
#pragma initialize solution}
	for (int i = 0; i < centers.size(); ++i) {
		sln.add_centers(centers[i]);
	}
	int maxServerLength = -1, serveredNode = -1;
	for (int v = 0; v < nodeNum; ++v) {
		if (dTable[0][v] > maxServerLength) {
			maxServerLength = dTable[0][v];
			serveredNode = v;
		}
	}
	sln.maxLength = maxServerLength;
	//��������ڵ�
	/*
#pragma exchange serveredNode{
	int serverNode = selectSeveredNode(nodeNum, G);
	vector<int> kClosedNode; //��ѡ�û��ڵ�v��ǰk������ڵ�
	kClosedNode = sortIndexes(G[serverNode], kClosed);
	for (int i = kClosedNode.size() - 1; i >= 0 && kClosedNode[i] >= dTable[0][serverNode]; i--) {
		kClosedNode.pop_back();//ֻѡ�����߳���С�������ߵ��û��ڵ�
	}
	for (int f = 0; f < kClosedNode.size(); ++f) {
		addNodeToTable(centers, kClosedNode[f], nodeNum, G);
		for (int i = 0; i < centers.size() - 1; ++i) {//�����нڵ�������ɾȥһ���ڵ�
			deleteNodeInTable(centers, centers[i], nodeNum, G);
			//Ѱ�����ķ���߳���
		}
	
	}
#pragma exchange serveredNode}
	*/

	

	/*
    vector<int> centers;
    for (int e = 0; !timer.isTimeOut() && (e < centerNum); ++e) {
        int index = rand.pick(0, nodeNum);
        sln.add_centers(index);
        centers.push_back(index);
    }
    //cout << sln.centers().size() << endl;
    vector<vector<int>> distance(centerNum, vector<int>(nodeNum)); // ���з���ڵ㵽����ڵ�ľ���
    for (int i = 0; i < centerNum; ++i) {
        distance[i] = CheckConstraints::Dijkstra(nodeNum, sln.centers(i), G);
    }
    vector<int> serveLength;
    for (int i = 0; i < nodeNum; ++i) {
        if (find(centers.begin(), centers.end(), i + 1) == centers.end()) { // ֻ�����û��ڵ�
            vector<int> length(centerNum);
            for (int j = 0; j < centerNum; ++j) {
                length[j] = distance[j][i];
            }
            auto minPosition = min_element(length.begin(), length.end());
            serveLength.push_back(*minPosition); // ����ߵĳ���
        }
    }
    auto maxPosition_s = max_element(serveLength.begin(), serveLength.end()); // ������е����ֵ
    sln.maxLength = *maxPosition_s;

	*/


    Log(LogSwitch::Szx::Framework) << "worker " << workerId << " ends." << endl;
    return status;
}

void Solver::addNodeToTable(std::vector<int> &centers, int node, int nodeNum, vector<std::vector<int>> &G)
{

	centers.push_back(node);
	for (int i = 0; i < nodeNum; ++i) {//����f���t��
		if (G[node][i] < dTable[0][i]) {
			dTable[1][i] = dTable[0][i];
			dTable[0][i] = G[node][i];
			fTable[1][i] = fTable[0][i];
			fTable[0][i] = node;
		}
		else if (G[node][i] < dTable[1][i]) {
			dTable[1][i] = G[node][i];
			fTable[1][i] = node;
		}
		else;
	}
}

void Solver::deleteNodeInTable(std::vector<int> &centers, int node, int nodeNum, std::vector<std::vector<int>> &G)
{
	int i = 0;
	for (; i < centers.size() && centers[i] != node; ++i);//Ѱ��Ҫɾ���Ľڵ�
	for (; i < centers.size()-1; ++i) {
		centers[i] = centers[i + 1];
	}
	centers.pop_back();
	for (int i = 0; i < nodeNum; ++i) {
		if (fTable[0][i] == node) {
			fTable[0][i] = fTable[1][i];
			dTable[0][i] = dTable[1][i];
			int secondClosed = -1, secondeLength = INF;
			for (int j = 0; j < centers.size(); ++i) {
				if (centers[j] != fTable[0][i] && secondeLength < G[i][centers[j]]) {
					secondeLength = G[i][centers[j]];
					secondClosed = centers[j];
				}

			}
			dTable[1][i] = secondeLength;
			fTable[1][i] = secondClosed;
		}
	}
}

int Solver::selectSeveredNode(int nodeNum, vector<std::vector<int>> &G)
{
	int maxServerLength = -1, serveredNode = -1;
	for (int v = 0; v < nodeNum; ++v) {
		if (dTable[0][v] > maxServerLength) {
			maxServerLength = dTable[0][v];
			serveredNode = v;
		}
	}
	vector<int> vDistance(nodeNum);//��ѡ�û��ڵ��������ڵ���������
	vDistance = CheckConstraints::Dijkstra(nodeNum, serveredNode, G);
	vector<int> kClosedNode; //��ѡ�û��ڵ�v��ǰk������ڵ�
	kClosedNode = sortIndexes(vDistance, kClosed);
	serveredNode = kClosedNode[rand.pick(kClosedNode.size())];//����k�����ڵ������ѡ��һ����Ϊ����ڵ�
	return serveredNode;
}

vector<int> Solver::sortIndexes(const vector<int>& v, int k)
{
	//����ǰk����Сֵ��Ӧ������ֵ
	vector<int> idx(v.size());
	for (int i = 0; i != idx.size(); ++i) {
		idx[i] = i;
	}
	sort(idx.begin(), idx.end(),
		[&v](int i1, int i2) {return v[i1] < v[i2]; });
	idx.assign(idx.begin(), idx.begin() + k);
	return idx;
}


#pragma endregion Solver

}

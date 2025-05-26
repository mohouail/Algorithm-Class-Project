#include <bits/stdc++.h>
using namespace std;

/* ---------- Data types ---------- */
struct Lab {
    vector<int> p;           // processing times
};

struct Instance {
    string id;
    int L, C, T;
    vector<Lab> labs;        // size == L after parsing
};

/* ---------- CSV helpers ---------- */
vector<string> splitCSV(const string& line) {
    vector<string> out;
    string cur; bool inQuote = false;
    for (char ch : line) {
        if (ch == '"') inQuote = !inQuote;
        else if (ch == ',' && !inQuote) { out.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
    }
    out.push_back(cur);
    for (string& s : out) {            // trim spaces
        size_t l = s.find_first_not_of(" \t\r");
        size_t r = s.find_last_not_of(" \t\r");
        s = (l == string::npos) ? "" : s.substr(l, r - l + 1);
    }
    return out;
}

/* ---------- PRUNE   ---------- */
void pruneInstance(Instance& ins) {
    int C = ins.C;
    int T = ins.T;

    for (Lab& lab : ins.labs) {
        /* rule 1 – max C+1 students */
        if ((int)lab.p.size() > C + 1)
            lab.p.resize(C + 1);

        /* rule 2 – trim last period so sum == T */
        long long sum = accumulate(lab.p.begin(), lab.p.end(), 0LL);
        if (sum > T && !lab.p.empty()) {
            int excess = (int)(sum - T);
            lab.p.back() -= excess;
            if (lab.p.back() < 1) lab.p.back() = 1; // never 0/negative
        }
    }
}/* ---------- Helper DFS with *working* branch-and-bound ---------- */
void dfsRecursive(const vector<Lab>& labs,
                  const vector<int>& finishTimes,
                  vector<int>& inspections,
                  int nextIdx,
                  int C, int T, int L,
                  long long& bestUsage,
                  long long usedSoFar,
                  int lastCut,
                  vector<int>& idx,
                  vector<int>& busy,
                  vector<int>& avail)
{
    /* --- optimistic bound (now tight) --- */
    long long optimistic = usedSoFar + 1LL * (T - lastCut) * L;
    if (optimistic <= bestUsage) return;   // prune branch

    /* --- placed all C inspections -> simulate remaining block to T --- */
    if ((int)inspections.size() == C)
    {
        long long used = usedSoFar;
        int cut = T;

        for (int i = 0; i < L; ++i)
        {
            int t = max(avail[i], busy[i]);
            while (idx[i] < (int)labs[i].p.size() &&
                   t + labs[i].p[idx[i]] <= cut)
            {
                used += labs[i].p[idx[i]];
                t    += labs[i].p[idx[i]];
                busy[i] = t;
                idx[i]++;
            }
        }
        bestUsage = max(bestUsage, used);
        return;
    }

    /* --- recursive step: try next inspection time --- */
    for (int id = nextIdx; id < (int)finishTimes.size(); ++id)
    {
        int cut = finishTimes[id];
        if (cut >= T) break;

        /* simulate block [lastCut , cut) once */
        vector<int> idx2   = idx;
        vector<int> busy2  = busy;
        vector<int> avail2 = avail;
        long long   gain   = 0;

        for (int i = 0; i < L; ++i)
        {
            int t = max(avail2[i], busy2[i]);
            while (idx2[i] < (int)labs[i].p.size() &&
                   t + labs[i].p[idx2[i]] <= cut)
            {
                gain += labs[i].p[idx2[i]];
                t    += labs[i].p[idx2[i]];
                busy2[i] = t;
                idx2[i]++;
            }
            if (busy2[i] <= cut) avail2[i] = cut;   // clean only if idle
        }

        inspections.push_back(cut);
        dfsRecursive(labs, finishTimes, inspections,
                     id + 1, C, T, L,
                     bestUsage, usedSoFar + gain, cut,
                     idx2, busy2, avail2);
        inspections.pop_back();
    }
}


/* ---------- Exact solver: build finishTimes and call DFS ---------- */
pair<long long,long long> solveExact(const Instance& ins)
{
    int L = ins.L, C = ins.C, T = ins.T;
    const auto& labs = ins.labs;

    vector<int> finishTimes;
    for (int lab = 0; lab < L; ++lab)
    {
        int t = 0;
        for (int j = 0; j < (int)labs[lab].p.size() && j <= C; ++j)
        {
            t += labs[lab].p[j];
            if (t < T) finishTimes.push_back(t);
        }
    }
    sort(finishTimes.begin(), finishTimes.end());
    finishTimes.erase(unique(finishTimes.begin(), finishTimes.end()),
                      finishTimes.end());

    /* initial per-lab state */
    vector<int> idx(L, 1);            // first student already running
    vector<int> busy(L), avail(L);
    long long used0 = 0;
    for (int i = 0; i < L; ++i)
    {
        busy[i]  = labs[i].p.empty() ? 0 : labs[i].p[0];
        avail[i] = 0;                 // cleaned at t=0
        used0   += labs[i].p.empty() ? 0 : labs[i].p[0];
    }

    long long best = used0;
    vector<int> insp;
    dfsRecursive(labs, finishTimes, insp,
                 0, C, T, L,
                 best, used0, 0,
                 idx, busy, avail);

    long long idle = 1LL * T * L - best;
    return {best, idle};
}


/* ---------- MAIN ---------- */
int main() {
    string inputfile = "500_tight_instances.csv";
    string outputfile = "500_tight_instancesOutputOptimal.csv";
    ifstream fin(inputfile);
    ofstream fout(outputfile);
    if (!fin) { cerr << "Cannot open input file\n"; return 1; }
    if (!fout){ cerr << "Cannot open output file\n"; return 1; }

    vector<Instance> instances;
    string line;
    /* ------------ PARSE ------------ */
    while (getline(fin, line)) {
        if (line.empty() || line[0] == '#' || line.find("instance_id") != string::npos) continue;
        auto head = splitCSV(line);
        if (head.size() != 4) { cerr << "Bad header line\n"; return 1; }
        Instance ins;
        ins.id = head[0];
        ins.L  = stoi(head[1]);
        ins.C  = stoi(head[2]);
        ins.T  = stoi(head[3]);

        ins.labs.clear();
        for (int i = 0; i < ins.L; ++i) {
            if (!getline(fin, line)) { cerr << "Unexpected EOF\n"; return 1; }
            auto row = splitCSV(line);
            if (row.empty() || row[0] != "lab") { cerr << "Expected 'lab' row\n"; return 1; }
            int cnt = stoi(row[1]);
            if ((int)row.size() != cnt + 2) { cerr << "Bad lab row count\n"; return 1; }
            Lab lab;
            lab.p.reserve(cnt);
            for (int k = 0; k < cnt; ++k) lab.p.push_back(stoi(row[2 + k]));
            ins.labs.push_back(move(lab));
        }
        instances.push_back(move(ins));
    }

    /* ------------ PROCESS & OUTPUT ------------ */
    fout << "instance_id,best_usage,idle_time,labs,counted_students\n";
    for (Instance& ins : instances) {
        pruneInstance(ins);

        long long counted = 0;
        for (const Lab& lab : ins.labs) counted += lab.p.size();

        auto [used,idle] = solveExact(ins);      // plug in your brute-force later
        fout << ins.id << ',' << used << ',' << idle << ','
             << ins.L  << ',' << counted << '\n';
    }
    cout << "Done.  Wrote " << outputfile << "\n";
    return 0;
}

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
}
/*----------- Helper DFS ----------*/
void dfsRecursive(const Instance& ins, const vector<vector<int>>& finishPerLab,
                  const vector<int>& finishTimes, const vector<Lab>& labs,
                  vector<int>& inspections, int start, int C, int T, long long& bestUsage) {

    int L = labs.size();

    if ((int)inspections.size() == C) {
        long long used = 0;
        vector<int> idx(L, 0);
        vector<int> avail(L, 0);
        vector<int> cuts = inspections;
        cuts.push_back(T);

        for (int cut : cuts) {
            for (int i = 0; i < L; ++i) {
                int t = avail[i];
                while (idx[i] < (int)labs[i].p.size() && t + labs[i].p[idx[i]] <= cut) {
                    used += labs[i].p[idx[i]];
                    t += labs[i].p[idx[i]];
                    idx[i]++;
                }
                avail[i] = cut; // inspection refresh
            }
        }

        if (used > bestUsage)
            bestUsage = used;

        return;
    }

    for (int i = start; i < (int)finishTimes.size(); ++i) {
        int currentTime = finishTimes[i];
        bool helps = false;

        for (int lab = 0; lab < L && !helps; ++lab) {
            for (int ft : finishPerLab[lab]) {
                if (ft == currentTime) {
                    helps = true;
                    break;
                }
            }
        }

        if (helps) {
            inspections.push_back(currentTime);
            dfsRecursive(ins, finishPerLab, finishTimes, labs, inspections, i + 1, C, T, bestUsage);
            inspections.pop_back();
        }
    }
}

/* ---------- Stub solver  ---------- */

pair<long long, long long> solveExact(const Instance& ins) {
    int L = ins.L, C = ins.C, T = ins.T;
    const auto& labs = ins.labs;

    set<int> finishTimesSet;
    vector<vector<int>> finishPerLab(L);

    for (int i = 0; i < L; ++i) {
        int t = 0;
        for (int dur : labs[i].p) {
            t += dur;
            if (t < T) {
                finishTimesSet.insert(t);
                finishPerLab[i].push_back(t);
            }
        }
    }

    vector<int> finishTimes(finishTimesSet.begin(), finishTimesSet.end());

    long long bestUsage = 0;
    vector<int> inspections;
    dfsRecursive(ins, finishPerLab, finishTimes, labs, inspections, 0, C, T, bestUsage);

    long long idle = 1LL * T * ins.L - bestUsage;
    return {bestUsage, idle};
}


/* ---------- MAIN ---------- */
int main() {
    string inputfile = "validationInstances.csv";
    string outputfile = "validationOutput.csv";
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

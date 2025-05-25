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
/* ---------- Helper DFS with branch-and-bound (Opt 3) ---------- */
void dfsRecursive(const vector<Lab>& labs,
                  const vector<int>& finishTimes,
                  vector<int>& inspections,
                  int nextIdx,                 // first index still usable in finishTimes
                  int C, int T, int L,
                  long long& bestUsage,
                  long long usedSoFar,         // usage gained up to lastCut
                  int    lastCut)              // last inspection time (0 at root)
{
    /* -------- branch-and-bound upper bound -------- */
    long long optimistic = usedSoFar + 1LL * (T - lastCut) * L;   // every lab full till T
    if (optimistic <= bestUsage) return;                          // prune

    /* -------- base: placed all C inspections -------- */
    if ((int)inspections.size() == C)
    {
        /* simulate with current inspections */
        vector<int> cuts = inspections;
        cuts.push_back(T);

        vector<int> idx(L, 0);
        vector<int> avail(L, 0);
        vector<int> busy(L, 0);
        long long used = usedSoFar;          // continue from previous blocks
        int prev = lastCut;

        for (int cut : cuts)
        {
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
                if (busy[i] <= cut) avail[i] = cut;   // lab becomes clean only if idle
            }
            prev = cut;
        }
        bestUsage = max(bestUsage, used);
        return;
    }

    /* -------- recursive step: try remaining finish-times -------- */
    for (int idx = nextIdx; idx < (int)finishTimes.size(); ++idx)
    {
        int t = finishTimes[idx];
        if (t >= T) break;                  // Opt 5 already implicit through pruning

        inspections.push_back(t);
        dfsRecursive(labs, finishTimes, inspections,
                     idx + 1, C, T, L,
                     bestUsage, usedSoFar, t);
        inspections.pop_back();
    }
}

/* ---------- Exact solver with Opt 1 (finish-times) ---------- */
pair<long long,long long> solveExact(const Instance& ins)
{
    int L = ins.L, C = ins.C, T = ins.T;
    const auto& labs = ins.labs;

    /* ---- build unique finish-times, ≤ C+1 students per lab ---- */
    vector<int> finishTimes;
    finishTimes.reserve( (C+1) * L );

    for (int lab = 0; lab < L; ++lab)
    {
        int t = 0;
        for (int j = 0;
             j < (int)labs[lab].p.size() && j < C + 1; ++j)
        {
            t += labs[lab].p[j];
            if (t < T) finishTimes.push_back(t);
        }
    }
    sort(finishTimes.begin(), finishTimes.end());
    finishTimes.erase(unique(finishTimes.begin(), finishTimes.end()),
                      finishTimes.end());

    long long bestUsage = 0;
    vector<int> inspections;             // grows to size C
    dfsRecursive(labs, finishTimes, inspections,
                 0, C, T, L,
                 bestUsage, 0LL, 0);

    long long idle = 1LL * T * L - bestUsage;
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

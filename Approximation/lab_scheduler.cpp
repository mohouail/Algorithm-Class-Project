#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iomanip>
#include <fstream>
#include <sstream>
using namespace std;


// ----------  helpers + instance description ----------
static inline std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r\n");
    const auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> split_csv(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ',')) out.push_back(trim(tok));
    return out;
}

struct InstanceData
{
    std::string id;                 // “inst01” …
    int L{} , C{} , T{};            // #labs, #paid-visits, deadline (h)
    std::vector<std::vector<int>> labs;   // labs[i] = durations (min) of students in lab i
};


struct Student {
    string name;
    int duration; // in minutes
};

struct ScheduleEntry {
    string student;
    int start;
    int finish;
    int inspection_wait;
};

// Generate random students for a lab
vector<Student> generate_students(int num_students, int min_time, int max_time, mt19937 &rng) {
    vector<Student> students;
    uniform_int_distribution<int> dist(min_time, max_time);
    for (int i = 0; i < num_students; ++i) {
        string name = "Student_" + string(1, 'A' + i);
        int duration = dist(rng);
        students.push_back({name, duration});
    }
    sort(students.begin(), students.end(), [](const Student &a, const Student &b) {
        return a.name < b.name;
    });
    return students;
}

// Generate inspection times (every inspection_interval minutes)
vector<int> generate_inspection_times(int max_time, int num_students, int num_labs, int inspection_interval) {
    vector<int> times;
    int end_time = (max_time * num_students + 2) * num_labs;
    for (int t = 0; t <= end_time; t += inspection_interval) {
        times.push_back(t);
    }
    return times;
}

// Schedule students in a lab given inspection times
vector<ScheduleEntry> schedule_lab(const vector<Student> &students, const vector<int> &inspection_times) {
    vector<ScheduleEntry> schedule;
    int prev_finish = 0;
    for (size_t i = 0; i < students.size(); ++i) {
        int start = (i == 0) ? 0 : schedule.back().finish;

        /* find the first inspection not earlier than start */
        auto it = std::lower_bound(inspection_times.begin(),
                                   inspection_times.end(),
                                   start);

        /* if no inspection remains, the lab stays dirty → stop scheduling */
        if (it == inspection_times.end())
            break;                       // exits the student-loop for this lab

        int next_inspection = *it;
        int actual_start    = std::max(start, next_inspection);

        int finish = actual_start + students[i].duration;
        schedule.push_back({
            students[i].name,
            actual_start,
            finish,
            (i == 0) ? 0 : (actual_start - start)
        });
    }
    return schedule;
}

// Compute utilization
void compute_utilization(const vector<ScheduleEntry> &schedule) {
    int total_time = schedule.back().finish - schedule.front().start;
    int busy_time = 0;
    for (const auto &s : schedule) {
        busy_time += (s.finish - s.start);
    }
    double utilization = total_time > 0 ? (double)busy_time / total_time : 1.0;
    cout << "Total time: " << total_time << " min, Busy: " << busy_time << " min, Utilization: " << std::fixed << std::setprecision(2) << (utilization * 100) << "%\n";
}

// add two out-params: usage for every lab and total students actually scheduled
void blackbox(int L, int C, int T, int n[], int p[][100],
              std::vector<int>& usage_out, int& students_out)
{
    // n_max is assumed <= 100 for simplicity
    int n_max = 0;
    for (int i = 0; i < L; ++i) n_max = max(n_max, n[i]);
    int inspection_interval = T / C;        // hours, evenly spaced
    std::vector<int> all_usage;                 // ← collects each lab’s usage
    int total_scheduled = 0;                    // ← counts students actually scheduled

    for (int lab = 0; lab < L; ++lab) {
        cout << "\n--- Lab_" << (lab+1) << " ---\n";
        // Build students vector
        vector<Student> students;
        for (int j = 0; j < n[lab]; ++j) {
            string name = "Student_" + string(1, 'A' + j);
            students.push_back({name, p[lab][j]});
        }
        // Inspection times for this lab
        vector<int> inspection_times;
        for (int c = 0; c < C; ++c) {
            inspection_times.push_back(c * inspection_interval);
        }
        // Output inspection times
        cout << "Inspection times (min): ";
        for (auto t : inspection_times) cout << t << " ";
        cout << "\n";
        // Schedule
        auto schedule = schedule_lab(students, inspection_times);
        // Output schedule (in hours)
        for (const auto &s : schedule) {
            cout << s.student << ": Start " << s.start << " h, Finish " << s.finish << " h, Inspection wait: " << s.inspection_wait << " h\n";
        }
        // Occupied/unoccupied
        int occupied = 0;
        int unoccupied = 0;
        for (size_t i = 0; i < schedule.size(); ++i) {
            occupied += (schedule[i].finish - schedule[i].start);
            if (i > 0) unoccupied += schedule[i].inspection_wait;
        }
        int total_time = schedule.back().finish - schedule.front().start;
        cout << "Occupied time: " << occupied << " h\n";
        cout << "Unoccupied (idle) time: " << unoccupied << " h\n";
        cout << "Total time: " << total_time << " h\n";
        // Visual timeline (in hours)
        cout << "Timeline (| = inspection, [X] = student):\n ";
        int time = 0, idx = 0, next_insp = 0;
        for (int t = 0; t <= schedule.back().finish; t += 1) {
            bool is_insp = (next_insp < (int)inspection_times.size() && t >= inspection_times[next_insp]);
            bool is_stud = (idx < (int)schedule.size() && t >= schedule[idx].start && t < schedule[idx].finish);
            if (is_insp) {
                cout << "|";
                next_insp++;
            } else if (is_stud) {
                cout << "X";
            } else {
                cout << ".";
            }
            if ((t) % 12 == 11) cout << " "; // new hour (for long schedules)
            if (idx < (int)schedule.size() && t >= schedule[idx].finish) idx++;
        }
        cout << "\n";
        all_usage.push_back(occupied);          // track usage of this lab   (← occupied *is* defined)
        total_scheduled += schedule.size();     // add students of this lab

    }
    usage_out   = all_usage;
    students_out = total_scheduled;
}



// Reads input from CSV and calls blackbox, prints inspection schedule vector and total lab usage time
int main(int argc, char* argv[]) {
    // Default file paths
    string inpath = "500_tight_instances.csv";
    string outpath = "500_tight_instancesOutputApprox.csv";

    // Check for command-line arguments
    if (argc == 3) {
        inpath = argv[1];
        outpath = argv[2];
        cout << "Using input file: " << inpath << endl;
        cout << "Using output file: " << outpath << endl;
    } else if (argc != 1) { // argc == 1 means no arguments, use defaults
        cerr << "Usage: " << argv[0] << " <input_csv_path> <output_csv_path>" << endl;
        cerr << "Or run without arguments to use default paths: input/lab_scheduler_input.csv and output/lab_scheduler_output.csv" << endl;
        return 1; // Indicate an error
    }

    // ----------  read NEW csv format  ----------
    std::ifstream infile(inpath);
    if (!infile) { std::cerr << "Cannot open " << inpath << '\n'; return 1; }

    std::string line;
    std::getline(infile, line);                    // header  instance_id,L,C,T

    std::vector<InstanceData> instances;
    while (std::getline(infile, line))
    {
        line = trim(line);
        if (line.empty()) continue;                // blank → separator between instances

        auto f = split_csv(line);

        /* 1st non-blank line of an instance:  instXX,L,C,T */
        if (f[0].rfind("inst", 0) == 0) {
            instances.emplace_back();
            auto& cur = instances.back();
            cur.id = f[0];
            cur.L  = std::stoi(f[1]);
            cur.C  = std::stoi(f[2]);
            cur.T  = std::stoi(f[3]);
            continue;
        }

        /* “lab” rows that follow */
        if (f[0] == "lab") {
            if (instances.empty()) { std::cerr << "lab row before inst-header\n"; return 1; }
            const int nStu = std::stoi(f[1]);
            std::vector<int> durations;
            durations.reserve(nStu);
            for (int k = 0; k < nStu; ++k) durations.push_back(std::stoi(f[2 + k]));
            instances.back().labs.push_back(std::move(durations));
        }
    }

    // Call blackbox and capture outputs
    // Instead of printing, return vector and int as required


    // STEP 5: Output to CSV (inspection_times in hours)
    // ----------  schedule each instance & write summary  ----------
    std::ofstream outfile(outpath);
    outfile << "instance_id,best_usage,idle_time,labs,counted_students\n";

    for (const auto& inst : instances)
    {
        /*  build the fixed-size arrays blackbox expects  */
        int n[100] = {0};
        int p[100][100] = {0};
        for (size_t i = 0; i < inst.labs.size(); ++i) {
            n[i] = inst.labs[i].size();
            for (size_t j = 0; j < inst.labs[i].size(); ++j)
                p[i][j] = inst.labs[i][j];
        }

        std::vector<int> usage_per_lab;
        int counted_students = 0;
        blackbox(inst.L, inst.C, inst.T, n, p, usage_per_lab, counted_students);

        /*  derive the summary numbers the new format wants  */
        int overall_usage = std::accumulate(usage_per_lab.begin(),
                                    usage_per_lab.end(), 0);

        /* Total available time is T hours per lab */
        long long facility_time = 1LL * inst.T * inst.L;
        long long idle_time     = facility_time - overall_usage;
        if (idle_time < 0) idle_time = 0;   // guard against rounding / overshoot


        outfile << inst.id << ','
        << overall_usage   << ','
        << idle_time       << ','
        << inst.L          << ','
        << counted_students << '\n';

    }
    outfile.close();
    return 0;
}

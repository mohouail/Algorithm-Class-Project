#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iomanip>
#include <fstream>
#include <sstream>
using namespace std;

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
        // Find next inspection after 'start'
        int next_inspection = *std::lower_bound(inspection_times.begin(), inspection_times.end(), start);
        int actual_start = std::max(start, next_inspection);
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

void blackbox(int L, int C, int T, int n[], int p[][100]) {
    // n_max is assumed <= 100 for simplicity
    int n_max = 0;
    for (int i = 0; i < L; ++i) n_max = max(n_max, n[i]);
    int inspection_interval = (T * 60) / C; // minutes, evenly spaced

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
        // Output schedule
        for (const auto &s : schedule) {
            cout << s.student << ": Start " << s.start << " min, Finish " << s.finish << " min, Inspection wait: " << s.inspection_wait << " min\n";
        }
        // Occupied/unoccupied
        int occupied = 0;
        int unoccupied = 0;
        for (size_t i = 0; i < schedule.size(); ++i) {
            occupied += (schedule[i].finish - schedule[i].start);
            if (i > 0) unoccupied += schedule[i].inspection_wait;
        }
        int total_time = schedule.back().finish - schedule.front().start;
        cout << "Occupied time: " << occupied << " min\n";
        cout << "Unoccupied (idle) time: " << unoccupied << " min\n";
        cout << "Total time: " << total_time << " min\n";
        // Visual timeline
        cout << "Timeline (| = inspection, [X] = student):\n ";
        int time = 0, idx = 0, next_insp = 0;
        for (int t = 0; t <= schedule.back().finish; t += 10) {
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
            if ((t / 10) % 12 == 11) cout << " "; // new hour
            if (idx < (int)schedule.size() && t >= schedule[idx].finish) idx++;
        }
        cout << "\n";
    }
}


// Reads input from CSV and calls blackbox, prints inspection schedule vector and total lab usage time
int main() {
    ifstream file("lab_scheduler.csv");
    if (!file) {
        cerr << "Could not open lab_scheduler.csv\n";
        return 1;
    }
    string line;
    int L, C, T;
    // Read header and first line
    getline(file, line); // L,C,T
    getline(file, line); // values
    stringstream ss(line);
    char comma;
    ss >> L >> comma >> C >> comma >> T;
    // Read until 'n' line
    while (getline(file, line)) if (line == "n") break;
    // Read n's
    getline(file, line);
    vector<int> n_vec;
    ss.clear(); ss.str(line);
    int val;
    while (ss >> val) {
        n_vec.push_back(val);
        if (ss.peek() == ',') ss.ignore();
    }
    int n_max = 0;
    for (int ni : n_vec) n_max = max(n_max, ni);
    // Read until 'p' line
    while (getline(file, line)) if (line == "p") break;
    // Read p's
    int p[100][100] = {0}; // [L][n_max]
    for (int i = 0; i < L; ++i) {
        if (!getline(file, line)) break;
        ss.clear(); ss.str(line);
        int j = 0;
        while (ss >> val) {
            p[i][j++] = val;
            if (ss.peek() == ',') ss.ignore();
        }
    }
    int n[100];
    for (int i = 0; i < L; ++i) n[i] = n_vec[i];
    // Call blackbox and capture outputs
    // Instead of printing, return vector and int as required
    // STEP 1: Schedule all labs back-to-back (no inspections)
    vector<vector<pair<int, int>>> lab_periods(L); // [lab][{start, finish}]
    int T_minutes = T * 60;
    for (int lab = 0; lab < L; ++lab) {
        int t = 0;
        for (int j = 0; j < n[lab]; ++j) {
            int start = t;
            int finish = start + p[lab][j];
            if (finish > T_minutes) break; // don't exceed total time
            lab_periods[lab].push_back({start, finish});
            t = finish;
        }
    }

    // STEP 2: For each hour, count how many labs have a student in that hour
    vector<int> x(T, 0); // x[i] = number of labs with a student in hour i
    for (int hour = 0; hour < T; ++hour) {
        int h_start = hour * 60, h_end = (hour + 1) * 60;
        for (int lab = 0; lab < L; ++lab) {
            for (auto &seg : lab_periods[lab]) {
                if (seg.second > h_start && seg.first < h_end) { // overlaps hour
                    x[hour]++;
                    break;
                }
            }
        }
    }

    // STEP 3: Pick C hours with lowest x_i (least busy hours)
    vector<pair<int, int>> hour_x; // {x_i, hour}
    for (int i = 0; i < T; ++i) hour_x.push_back({x[i], i});
    sort(hour_x.begin(), hour_x.end());
    vector<int> inspection_hours;
    for (int i = 0; i < C && i < (int)hour_x.size(); ++i) inspection_hours.push_back(hour_x[i].second);
    sort(inspection_hours.begin(), inspection_hours.end());

    // Convert inspection hours to minutes
    vector<int> inspection_times;
    for (int h : inspection_hours) inspection_times.push_back(h * 60);

    // STEP 4: For each lab, reschedule with these inspections as hard barriers
    vector<int> all_usage;
    for (int lab = 0; lab < L; ++lab) {
        vector<Student> students;
        for (int j = 0; j < n[lab]; ++j) {
            string name = "Student_" + string(1, 'A' + j);
            students.push_back({name, p[lab][j]});
        }
        vector<ScheduleEntry> schedule;
        int t = 0, insp_idx = 0;
        for (int j = 0; j < (int)students.size(); ++j) {
            // If the current time is at or past an inspection, wait for the inspection to finish
            while (insp_idx < (int)inspection_times.size() && t >= inspection_times[insp_idx]) {
                // Move t forward to the inspection time if not already there
                t = inspection_times[insp_idx];
                insp_idx++;
            }
            int start = t;
            int finish = start + students[j].duration;
            // If the finish would go past T_minutes, stop scheduling
            if (finish > T_minutes) break;
            schedule.push_back({students[j].name, start, finish, 0});
            t = finish;
        }
        int usage = 0;
        for (auto &s : schedule) usage += (s.finish - s.start);
        all_usage.push_back(usage);
    }

    // STEP 5: Output to CSV (same inspection times for all labs)
    string outpath = "c:/Users/kouss/Documents/Analysis and Design of Algorithms/Project/Code/lab_scheduler_output.csv";
    ofstream outfile(outpath);
    if (outfile) {
        outfile << "Lab,inspection_times,total_usage_time\n";
        string insp_str;
        for (size_t i = 0; i < inspection_times.size(); ++i) {
            insp_str += to_string(inspection_times[i]);
            if (i + 1 < inspection_times.size()) insp_str += ";";
        }
        for (int lab = 0; lab < L; ++lab) {
            outfile << "Lab_" << (lab+1) << "," << insp_str << "," << all_usage[lab] << "\n";
        }
        outfile.flush();
        outfile.close();
    }
    return 0;
}


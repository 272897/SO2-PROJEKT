#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <set>
#include <chrono>
#include <random>
#include <string>

using namespace std;

// Custom counting semaphore to control access to resources
class my_counting_semaphore {
private:
    mutex mtx;
    condition_variable cv;
    int count;

public:
    my_counting_semaphore(int initial_count) : count(initial_count) {}

    void acquire() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&] { return count > 0; });
        --count;
    }

    void release() {
        unique_lock<mutex> lock(mtx);
        ++count;
        cv.notify_one();
    }
};

// Waiter class manages fork availability to prevent deadlock
class Waiter {
private:
    mutex mtx;
    mutex cout_mtx;
    condition_variable cv;
    vector<bool> forks;  // True = fork available, False = fork in use
    set<pair<chrono::steady_clock::time_point, int>> waiting_set;  // Queue of waiting philosophers
    my_counting_semaphore waiter;  // Semaphore to control the number of eating philosophers

public:
    bool stop_flag = false; // Flag to stop philosophers after a set time
    vector<int> eat_count;  // Counter to track how many times each philosopher has eaten

    Waiter(int n) : forks(n, true), waiter(n - 1), eat_count(n, 0) {}

    // Function to pick up forks
    void pick_up_forks(int id) {
        waiter.acquire();  // Ensure at most N-1 philosophers eat simultaneously

        unique_lock<mutex> lock(mtx);
        auto now = chrono::steady_clock::now();
        waiting_set.insert({now, id});

        // Wait until both forks are available and it's the philosopher's turn
        cv.wait(lock, [&] {
            auto first = *waiting_set.begin();
            return first.second == id && forks[id] && forks[(id + 1) % forks.size()];
        });

        waiting_set.erase(waiting_set.begin());
        forks[id] = false;
        forks[(id + 1) % forks.size()] = false;
    }

    // Function to put down forks after eating
    void put_down_forks(int id) {
        lock_guard<mutex> lock(mtx);
        forks[id] = true;
        forks[(id + 1) % forks.size()] = true;
        eat_count[id]++; // Increment the eat counter
        cv.notify_all();

        waiter.release();  // Allow another philosopher to start eating
    }

    // Print status messages safely with synchronization
    void print_status(const string &message, int id) {
        lock_guard<mutex> lock(cout_mtx);
        cout << "Philosopher " << id << " " << message << endl;
    }
};

// Function to simulate a philosopher's behavior
void philosopher(int id, Waiter &waiter, mt19937 &rng) {
    uniform_int_distribution<int> dist(500, 2000);  // Random wait time between 500-2000ms
    while (!waiter.stop_flag) { // Stop when flag is set
        waiter.print_status("is thinking.", id);
        this_thread::sleep_for(chrono::milliseconds(dist(rng)));

        waiter.print_status("is hungry.", id);
        waiter.pick_up_forks(id);

        waiter.print_status("is eating.", id);
        this_thread::sleep_for(chrono::milliseconds(dist(rng)));

        waiter.put_down_forks(id);
        waiter.print_status("has finished eating and put down the forks.", id);
    }
}

int main(int argc, char* argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number_of_philosophers>\n";
        return 1;
    }

    // Convert input argument to an integer
    int num_philosophers = stoi(argv[1]);

    // Ensure the number of philosophers is valid
    if (num_philosophers <= 1) {
        cerr << "Invalid number of philosophers. Exiting.\n";
        return 1;
    }

    // Create threads for philosophers
    vector<thread> philosophers;
    Waiter waiter(num_philosophers);
    random_device rd;
    mt19937 rng(rd());

    for (int i = 0; i < num_philosophers; ++i) {
        philosophers.emplace_back(philosopher, i, ref(waiter), ref(rng));
    }

    // Let the simulation run for 10 seconds
    this_thread::sleep_for(chrono::seconds(10));
    waiter.stop_flag = true; // Signal philosophers to stop

    // Join philosopher threads (they run indefinitely until stopped)
    for (auto &t : philosophers) {
        t.join();
    }

    // Print statistics after simulation ends
    cout << "\nStatistics:\n";
    for (int i = 0; i < num_philosophers; ++i) {
        cout << "Philosopher " << i << " ate " << waiter.eat_count[i] << " times.\n";
    }

    return 0;
}

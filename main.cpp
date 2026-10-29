#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace chrono;

queue<int> numbers1;
mutex mutex1;
bool dataReady1 = false;

condition_variable condition2;
mutex mutex2;
int value2 = 0;

condition_variable condition3;
mutex mutex3;
int value3 = 0;
bool stop3 = false;

queue<int> numbers4;
mutex mutex4;
condition_variable condition4;

bool isPrime(long long number) {
    if (number < 2) {
        return false;
    }

    for (long long divisor = 2; divisor * divisor <= number; ++divisor) {
        if (number % divisor == 0) {
            return false;
        }
    }

    return true;
}

long long nthPrime(int n) {
    if (n <= 0) {
        throw invalid_argument("Номер простого числа повинен бути додатним");
    }

    vector<long long> primes;
    primes.reserve(n);

    for (long long candidate = 2; static_cast<int>(primes.size()) < n; ++candidate) {
        bool prime = true;
        for (long long divisor : primes) {
            if (divisor * divisor > candidate) {
                break;
            }
            if (candidate % divisor == 0) {
                prime = false;
                break;
            }
        }
        if (prime) {
            primes.push_back(candidate);
        }
    }

    return primes.back();
}

void dataPreparation1() {
    cout << "Вводьте цілі числа. Для завершення введіть 0:\n";
    int number;
    while (cin >> number && number != 0) {
        lock_guard<mutex> lock(mutex1);
        numbers1.push(number);
    }

    lock_guard<mutex> lock(mutex1);
    dataReady1 = true;
}

void dataProcessing1() {
    unique_lock<mutex> lock(mutex1);
    while (!dataReady1) {
        lock.unlock();
        this_thread::sleep_for(milliseconds(100));
        lock.lock();
    }

    cout << "Прості числа:";
    while (!numbers1.empty()) {
        int number = numbers1.front();
        numbers1.pop();
        if (isPrime(number)) {
            cout << ' ' << number;
        }
    }
    cout << '\n';
}

void task1() {
    thread preparation(dataPreparation1);
    thread processing(dataProcessing1);
    preparation.detach();
    processing.join();
}

void waits2(int number) {
    unique_lock<mutex> lock(mutex2);
    cout << "Потік " << number << " входить у стан очікування\n";
    condition2.wait(lock, [] { return value2 == 1; });
    cout << "Очікування потоку " << number << " завершене\n";
}

void awake2() {
    this_thread::sleep_for(milliseconds(100));
    cout << "Перше повідомлення потоків\n";
    condition2.notify_all();

    this_thread::sleep_for(milliseconds(100));
    {
        lock_guard<mutex> lock(mutex2);
        value2 = 1;
    }
    cout << "Повторне повідомлення потоків\n";
    condition2.notify_all();
}

void task2() {
    vector<thread> waiting;
    for (int i = 1; i <= 3; ++i) {
        waiting.emplace_back(waits2, i);
    }
    thread awake(awake2);

    awake.join();
    for (thread& current : waiting) {
        current.join();
    }
}

void waits3(int number) {
    unique_lock<mutex> lock(mutex3);
    condition3.wait(lock, [] { return value3 == 1 || stop3; });
    if (!stop3) {
        cout << "Повідомлення з потоку " << number << '\n';
        value3 = 0;
    }
}

void notify3() {
    this_thread::sleep_for(milliseconds(100));
    {
        lock_guard<mutex> lock(mutex3);
        value3 = 1;
    }
    condition3.notify_one();
}

void task3() {
    vector<thread> waiting;
    for (int i = 1; i <= 3; ++i) {
        waiting.emplace_back(waits3, i);
    }
    thread notifier(notify3);
    notifier.join();

    this_thread::sleep_for(milliseconds(50));
    {
        lock_guard<mutex> lock(mutex3);
        value3 = 0;
        stop3 = true;
    }
    condition3.notify_all();

    for (thread& current : waiting) {
        current.join();
    }
}

void dataPreparation4() {
    cout << "Вводьте цілі числа. Для завершення введіть 0:\n";
    int number;
    while (cin >> number && number != 0) {
        lock_guard<mutex> lock(mutex4);
        numbers4.push(number);
    }
    condition4.notify_one();
}

void dataProcessing4() {
    unique_lock<mutex> lock(mutex4);
    condition4.wait(lock, [] { return !numbers4.empty(); });

    cout << "Прості числа:";
    while (!numbers4.empty()) {
        int number = numbers4.front();
        numbers4.pop();
        if (isPrime(number)) {
            cout << ' ' << number;
        }
    }
    cout << '\n';
}

void task4() {
    thread preparation(dataPreparation4);
    thread processing(dataProcessing4);
    preparation.detach();
    processing.join();
}

double calculateFunction(int n, int choice) {
    if (choice == 1) {
        return sqrt(n);
    }
    if (choice == 2) {
        return sin(n);
    }
    return log(n);
}

void runAsyncMode(int n, int choice, launch mode, const string& name) {
    future<long long> result = async(mode, nthPrime, n);
    double functionValue = calculateFunction(n, choice);
    cout << name << ": значення функції = " << functionValue << '\n';

    auto start = steady_clock::now();
    long long prime = result.get();
    auto finish = steady_clock::now();
    auto waitTime = duration_cast<milliseconds>(finish - start).count();

    cout << name << ": " << n << "-те просте число = " << prime << '\n';
    cout << name << ": очікування get() = " << waitTime << " мс\n";
}

void task5() {
    int n;
    int choice;
    cout << "Введіть номер простого числа: ";
    cin >> n;
    cout << "Оберіть функцію (1 - корінь, 2 - синус, 3 - логарифм): ";
    cin >> choice;

    runAsyncMode(n, choice, launch::deferred, "deferred");
    runAsyncMode(n, choice, launch::async, "async");
}

void task6() {
    deque<packaged_task<long long(int)>> tasks;
    deque<int> taskNumbers;
    vector<pair<int, future<long long>>> results;
    mutex tasksMutex;
    condition_variable tasksCondition;
    bool inputFinished = false;

    thread worker([&] {
        while (true) {
            packaged_task<long long(int)> currentTask;
            int number = 0;

            {
                unique_lock<mutex> lock(tasksMutex);
                tasksCondition.wait(lock, [&] { return !tasks.empty() || inputFinished; });
                if (tasks.empty() && inputFinished) {
                    break;
                }
                currentTask = std::move(tasks.front());
                tasks.pop_front();
                number = taskNumbers.front();
                taskNumbers.pop_front();
            }

            currentTask(number);
        }
    });

    cout << "Вводьте номери простих чисел. Для завершення введіть 0:\n";
    int n;
    while (cin >> n && n != 0) {
        packaged_task<long long(int)> task(nthPrime);
        future<long long> result = task.get_future();
        {
            lock_guard<mutex> lock(tasksMutex);
            tasks.push_back(std::move(task));
            taskNumbers.push_back(n);
        }
        results.emplace_back(n, std::move(result));
        tasksCondition.notify_one();
    }

    {
        lock_guard<mutex> lock(tasksMutex);
        inputFinished = true;
    }
    tasksCondition.notify_one();

    for (auto& item : results) {
        cout << item.first << "-те просте число = " << item.second.get() << '\n';
    }
    worker.join();
}

void calculatePrimes7(int n, promise<long long> firstPrime,
                      promise<bool> startSecond, promise<long long> secondPrime) {
    firstPrime.set_value(nthPrime(n));
    startSecond.set_value(true);
    secondPrime.set_value(nthPrime(n * 10));
}

void calculateRoot7(int n, future<bool> start) {
    bool canStart = start.get();
    if (!canStart) {
        return;
    }
    this_thread::sleep_for(seconds(2));
    cout << "Квадратний корінь з " << n << " = " << sqrt(n) << '\n';
}

void task7() {
    int n;
    cout << "Введіть номер простого числа: ";
    cin >> n;

    promise<long long> firstPromise;
    promise<long long> secondPromise;
    promise<bool> startPromise;
    future<long long> firstResult = firstPromise.get_future();
    future<long long> secondResult = secondPromise.get_future();
    future<bool> startResult = startPromise.get_future();

    thread first(calculatePrimes7, n, std::move(firstPromise),
                 std::move(startPromise), std::move(secondPromise));
    thread second(calculateRoot7, n, std::move(startResult));

    cout << n << "-те просте число = " << firstResult.get() << '\n';
    cout << n * 10 << "-те просте число = " << secondResult.get() << '\n';

    first.join();
    second.join();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Використання: ./lab2 <номер завдання 1-7>\n";
        return 1;
    }

    int task = stoi(argv[1]);
    switch (task) {
        case 1: task1(); break;
        case 2: task2(); break;
        case 3: task3(); break;
        case 4: task4(); break;
        case 5: task5(); break;
        case 6: task6(); break;
        case 7: task7(); break;
        default:
            cout << "Номер завдання повинен бути від 1 до 7\n";
            return 1;
    }

    return 0;
}

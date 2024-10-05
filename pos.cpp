#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <cstdlib>  // Для функции rand()
#include <vector>

using namespace std;

queue<int> buffer;
atomic<bool> data_ready(false);
atomic<bool> finished(false);
mutex mtx;
pthread_mutex_t posix_mtx = PTHREAD_MUTEX_INITIALIZER;

// Уникальные идентификаторы потоков в диапазоне 1 .. 3+N
int get_tid(int id = 0) {
    static thread_local unique_ptr<int> tid(new int);
    if (id > 0) {
        *tid = id;
    }
    return *tid;
}

void *producer_routine(void *arg) {
    (void)arg;
    get_tid(2);  // Присваиваем идентификатор для потока producer

    string line;
    getline(cin, line);
    istringstream iss(line);
    int number;

    while (iss >> number) {
        {
            lock_guard<mutex> lock(mtx);
            buffer.push(number);
        }
        data_ready = true;
    }

    finished = true;
    return nullptr;
}

void *consumer_routine(void *arg) {
    get_tid(3);  // Присваиваем идентификатор для потока consumer

    int *sleep_limit = static_cast<int *>(arg);
    int sum = 0;

    while (true) {
        int value;

        lock_guard<mutex> lock(mtx);
        if (!buffer.empty()) {
            value = buffer.front();
            buffer.pop();
        } else {
            if (finished) {
                break;
            }
            continue;
        }

        sum += value;

        if (*sleep_limit > 0) {
            usleep((rand() % (*sleep_limit + 1)));
        }
    }

    return (void *)(size_t)sum;
}

void *consumer_interrupter_routine(void *arg) {
    vector<pthread_t> *consumer_threads = static_cast<vector<pthread_t> *>(arg);
    get_tid(4);  // Присваиваем идентификатор для потока interrupter

    while (!finished) {
        usleep(100000);  // Ждем 100 мс перед попыткой прерывания

        // Выбираем случайный поток consumer
        int random_index = rand() % consumer_threads->size();

        // Прерываем выбранный поток consumer
        pthread_cancel((*consumer_threads)[random_index]);
    }

    return nullptr;
}

void run_threads(int consumer_count, int sleep_limit) {
    get_tid(1);  // Присваиваем идентификатор для основного потока

    pthread_t producer_thread;
    vector<pthread_t> consumer_threads(consumer_count);
    pthread_t interruptor_thread;

    // Запуск потока producer
    pthread_create(&producer_thread, nullptr, producer_routine, nullptr);

    // Запуск потоков consumers
    for (int i = 0; i < consumer_count; ++i) {
        pthread_create(&consumer_threads[i], nullptr, consumer_routine, (void *)&sleep_limit);
    }

    // Запуск потока interrupter
    pthread_create(&interruptor_thread, nullptr, consumer_interrupter_routine, (void *)&consumer_threads);

    // Ожидание завершения всех потоков consumers и получение суммы
    int total_sum = 0;
    for (auto &consumer_thread : consumer_threads) {
        void *result;
        pthread_join(consumer_thread, &result);
        total_sum += (size_t)result;
    }

    // Ожидание завершения потока producer
    pthread_join(producer_thread, nullptr);

    // Ожидание завершения потока interrupter
    pthread_join(interruptor_thread, nullptr);

    cout << total_sum << endl;
}

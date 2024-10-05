#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <sstream>
#include <unistd.h>
#include <vector>

using namespace std;

queue<int> buffer;
atomic<bool> data_ready(false);
atomic<bool> finished(false);
mutex mtx;
static pthread_mutex_t tid_mutex = PTHREAD_MUTEX_INITIALIZER;
static int *thread_id = nullptr;
vector<pthread_t> consumer_threads;

int get_tid() {
  if (thread_id == nullptr) {
    pthread_mutex_lock(&tid_mutex);
    static int next_id = 1;
    thread_id = new int(next_id++);
    pthread_mutex_unlock(&tid_mutex);
  }
  return *thread_id;
}

void *producer_routine(void *arg) {
  (void)arg;
  get_tid();

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
  get_tid();

  int *sleep_limit = static_cast<int *>(arg);
  int sum = 0;

  while (true) {
    int value;

    lock_guard<mutex> lock(mtx);
    if (buffer.empty() && finished) {
      break;
    }

    if (!buffer.empty()) {
      value = buffer.front();
      buffer.pop();
      data_ready = false;
    } else {
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
  (void)arg;
  get_tid();

  while (!finished) {
    if (data_ready) {
      int consumer_index = rand() % consumer_threads.size();
      cout << "Interruptor is trying to interrupt consumer #" << consumer_index
           << "..." << endl;
      pthread_cancel(consumer_threads[consumer_index]);
    }

    usleep(100000);
  }

  return nullptr;
}

void run_threads(int consumer_count, int sleep_limit) {
  get_tid();

  pthread_t producer_thread;
  consumer_threads.resize(consumer_count);
  pthread_t interruptor_thread;

  pthread_create(&producer_thread, nullptr, producer_routine, nullptr);

  for (int i = 0; i < consumer_count; ++i) {
    pthread_create(&consumer_threads[i], nullptr, consumer_routine,
                   (void *)&sleep_limit);
  }

  pthread_create(&interruptor_thread, nullptr, consumer_interrupter_routine,
                 nullptr);

  int total_sum = 0;
  for (auto &consumer_thread : consumer_threads) {
    void *result;
    pthread_join(consumer_thread, &result);
    total_sum += (size_t)result;
  }

  pthread_join(producer_thread, nullptr);

  pthread_join(interruptor_thread, nullptr);

  cout << total_sum << endl;
}

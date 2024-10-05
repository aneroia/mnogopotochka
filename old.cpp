#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>

using namespace std;

queue<int> buffer;
atomic<bool> data_ready(false);
atomic<bool> finished(false);
mutex mtx;
pthread_mutex_t posix_mtx = PTHREAD_MUTEX_INITIALIZER;

int get_tid(int id) {
  // 1 to 3+N thread ID
  static thread_local shared_ptr<int> tid(new int);
  if (id > 0) *tid = id;
  return *tid;
}

void *producer_routine(void *arg) {
  (void)arg;
  get_tid(2);

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
  get_tid(3);

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
  (void)arg;
  get_tid(4);

  while (!finished) {
    usleep(100000);

    pthread_mutex_lock(&posix_mtx);
    if (!buffer.empty()) {
      cout << "Interruptor is trying to interrupt the consumer..." << endl;
    }
    pthread_mutex_unlock(&posix_mtx);
  }

  // cout << "Interruptor thread finished." << endl;
  return nullptr;
}

void run_threads(int consumer_count, int sleep_limit) {
  get_tid(1);

  pthread_t producer_thread;
  vector<pthread_t> consumer_threads(consumer_count);
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

void *consumer_routine(void *arg) {
  get_tid(3);

  int *sleep_limit = static_cast<int *>(arg);
  int sum = 0;

  // Установка состояния отмены потока (разрешение отмены)
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);  // Отложенная отмена

  while (true) {
    int value;

    {
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
    }

    sum += value;

    // Проверяем возможность отмены потока (точка отмены)
    pthread_testcancel();

    if (*sleep_limit > 0) {
      usleep((rand() % (*sleep_limit + 1)));
    }
  }

  return (void *)(size_t)sum;
}

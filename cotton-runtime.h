/**
@file
@brief Header for COTTON runtime metohds.
**/

#include "cotton.h"


namespace cotton {

	const unsigned int MAX_WORKERS = 100;
	const unsigned int DEFAULT_NUM_WORKERS = 1;
	unsigned int NUM_WORKERS = 0;
	const unsigned int MAX_DEQUE_SIZE = 100;
	volatile bool SHUTDOWN;
	pthread_mutex_t FINISH_MUTEX;
	pthread_t thread[MAX_WORKERS];
	static pthread_key_t THREAD_KEY;
	volatile unsigned int FINISH_COUNTER;
	static pthread_mutex_t DEQUE_MUTEX[MAX_WORKERS];
	static pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;

	struct Deque {
		volatile unsigned int head;
		volatile unsigned int tail;
		std::function<void()> task_deque[MAX_WORKERS];

		Deque() {
			head = 0; tail = 0;
		}

		bool isEmpty();
		std::function<void()> pop_from_deque();
		std::function<void()> steal_from_deque();
		void push_to_deque(std::function<void()> &&lambda);
	};
	static Deque DEQUE_ARRAY[MAX_DEQUE_SIZE];

	void lib_key_init();
	unsigned int get_threadID();
	void find_and_execute_task();
	unsigned int thread_pool_size();
	void *worker_routine(void *args);
	std::function<void()> grab_task_from_runtime();
	void push_task_to_runtime(std::function<void()> &&lambda);
}

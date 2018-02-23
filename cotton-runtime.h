/**
@file
@brief Header for COTTON runtime metohds.
**/

#include "cotton.h"


namespace cotton {
	
	const unsigned int DEFAULT_NUM_WORKERS = 1;
	const unsigned int MAX_DEQUE_SIZE = 100;
	volatile bool SHUTDOWN;
	volatile unsigned int FINISH_COUNTER;
	unsigned int NUM_WORKERS = 0;
	pthread_key_t THREAD_KEY;
	pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;
	pthread_mutex_t FINISH_MUTEX = PTHREAD_MUTEX_INITIALIZER;
	pthread_t *thread = NULL;
	pthread_mutex_t *DEQUE_MUTEX = NULL;

	struct Deque {
		volatile unsigned int head;
		volatile unsigned int tail;
		std::function<void()> *task_deque;

		Deque() {
			head = 0; tail = 0;
			task_deque = (std::function<void()> *)malloc(sizeof(std::function<void()>) * MAX_DEQUE_SIZE);
		}

		~Deque() {
			free(task_deque);
		}

		bool isEmpty();
		std::function<void()> pop_from_deque();
		std::function<void()> steal_from_deque();
		void push_to_deque(std::function<void()> &&lambda);
	};

	Deque *DEQUE_ARRAY = NULL;
	
	void lib_key_init();
	unsigned int get_threadID();
	void find_and_execute_task();
	unsigned int thread_pool_size();
	void *worker_routine(void *args);
	std::function<void()> grab_task_from_runtime();
	void push_task_to_runtime(std::function<void()> &&lambda);
}

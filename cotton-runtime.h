/**
@file
@brief Header for COTTON runtime metohds.
**/

#include "cotton.h"


namespace cotton {
	
	#define CACHE_LINE_SIZE 64
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
		volatile void* task_deque[MAX_DEQUE_SIZE];
		int _padding[ (CACHE_LINE_SIZE - 16) >> 2 ];

		Deque() {
			memset( _padding, 0, sizeof(_padding) );
			head = 0; tail = 0;
			for(int i=0; i < MAX_DEQUE_SIZE; i++) {
				task_deque[i] = NULL;
			}
		}

		~Deque() {
			for(int i=0; i < MAX_DEQUE_SIZE; i++) {
				free((void *)task_deque[i]);
			}
		}

		bool isEmpty();
		void* pop_from_deque();
		void* steal_from_deque();
		void push_to_deque(void *task);
	};

	Deque *DEQUE_ARRAY = NULL;
	
	void free_all();
	void lib_key_init();
	unsigned int get_threadID();
	void find_and_execute_task();
	unsigned int thread_pool_size();
	void* worker_routine(void *args);
	void* grab_task_from_runtime();
	void push_task_to_runtime(void *task);
}

/**
@file
@brief Header for COTTON runtime metohds.
**/

#include "cotton.h"


namespace cotton {
	
	#define CACHE_LINE_SIZE 64
	
	volatile bool SHUTDOWN;
	pthread_t *thread = NULL;
	pthread_key_t THREAD_KEY;
	unsigned int NUM_WORKERS = 0;
	pthread_mutex_t *DEQUE_MUTEX = NULL;
	volatile unsigned int FINISH_COUNTER;
	const unsigned int MAX_DEQUE_SIZE = 100;
	const unsigned int DEFAULT_NUM_WORKERS = 1;
	pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;
	pthread_mutex_t FINISH_MUTEX = PTHREAD_MUTEX_INITIALIZER;

	struct Deque {
		volatile unsigned int head;
		volatile unsigned int tail;
		volatile void* task_deque[MAX_DEQUE_SIZE];

		// The size of object here is already momre than the CACHE_LINE_SIZE (64 bytes). How will padding help?
		// memset() padding with zeroes in constructor.
		// int _padding[ (CACHE_LINE_SIZE - 2*sizeof(unsigned int) - MAX_DEQUE_SIZE*sizeof(void *) ) >> 2 ];

		Deque() {
			// memset( _padding, 0, sizeof(_padding) );
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
	void* grab_task_from_runtime();
	unsigned int thread_pool_size();
	void* worker_routine(void *args);
	void push_task_to_runtime(void *task);
}

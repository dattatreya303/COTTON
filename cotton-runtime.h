/**
@file
@brief Header for COTTON runtime methods.
**/

#include "cotton.h"

namespace cotton {
	
	#define NUM_THRESHOLDS 4
	#define CACHE_LINE_SIZE 64
	
	bool EEFC_MODE = false;
	volatile bool SHUTDOWN;
	pthread_t* thread = NULL;
	pthread_key_t THREAD_KEY;
	unsigned int NUM_WORKERS = 0;
	pthread_mutex_t* DEQUE_MUTEX = NULL;
	volatile unsigned int FINISH_COUNTER;
	const unsigned int MAX_DEQUE_SIZE = 100;
	const unsigned int DEFAULT_NUM_WORKERS = 1;
	pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;
	pthread_mutex_t FINISH_MUTEX = PTHREAD_MUTEX_INITIALIZER;
	
	double CPU_FREQUENCIES_SUPPORTED[] = {0};

	struct Deque {
		volatile unsigned int head;
		volatile unsigned int tail;
		volatile void* task_deque[MAX_DEQUE_SIZE];

		// The size of object here is already more than the CACHE_LINE_SIZE (64 bytes). How will padding help?
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
		unsigned int sizeof_deque();
		void* pop_from_deque();
		void* steal_from_deque();
		void push_to_deque(void *task);
	};
	// Deque* DEQUE_ARRAY = NULL;

	struct Thief_node {
		int thief_worker_id;
		Thief_node* prev;
		Thief_node* next;

		Thief_node() {
			thief_worker_id = -1;
			prev = NULL;
			next = NULL;
		}

		Thief_node(int thief_worker_id) : Thief_node() {
			this->thief_worker_id = thief_worker_id;
		}
	};

	struct Worker {
		Deque* worker_deque;
		int size_thresholds[NUM_THRESHOLDS];
		int current_size_threshold_index;
		int current_frequency_index;
		Thief_node* thief_list_head;

		Worker() {
			worker_deque = new Deque();
			for (int i = 0; i < NUM_THRESHOLDS; i++) {
				size_thresholds[i] = (2 * MAX_DEQUE_SIZE * i) / (NUM_THRESHOLDS + 1);
			}
			current_frequency_index = 0;
			current_size_threshold_index = 0;
			thief_list_head = new Thief_node();
		}

		bool is_victim();
		void add_thief(int thief_worker_id);
		void free_thief_list();

		~Worker() {
			delete worker_deque;
			free_thief_list();
		}
	};
	Worker* WORKER_ARRAY;
	
	void free_all();
	void lib_key_init();
	void set_eefc_mode();
	void find_and_execute_task();
	void* grab_task_from_runtime();
	void* worker_routine(void *args);
	void set_supported_cpu_frequencies();
	void push_task_to_runtime(void *task);
	unsigned int bind_thread_to_core(pthread_t thread_to_map, int core_id);

	// Energy efficient runtime methods.
	void UP(int worker_id);	
	void DOWN(int worker_id);
	void DOWN(int victim_worker_id, int thief_worker_id);

	// Workpath sensitive
	void begin_victim_thief_relationship(int victim_worker_id, int thief_worker_id);
	void end_victim_thief_relationship(int victim_worker_id, int thief_worker_id);
	void end_victim(int victim_worker_id);

	// Workload sensitive
	void workload_up_check(int worker_id);
	void workload_down_check(int worker_id);
}

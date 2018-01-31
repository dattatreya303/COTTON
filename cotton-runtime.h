#include "cotton.h"

#define MAX_WORKERS 100
#define MAX_DEQUE_SIZE 100

namespace cotton_runtime {

	struct Deque {
		unsigned int head;
		unsigned int tail;
		std::function<void()> task_deque[MAX_DEQUE_SIZE];

		Deque() { head = 0; tail = 0; }

		bool isEmpty();
		std::function<void()> pop_from_deque();
		std::function<void()> steal_from_deque();
		void push_to_deque(std::function<void()> &&lambda);
	};

	void lib_key_init();
	unsigned int get_threadID();
	void find_and_execute_task();
	unsigned int thread_pool_size();
	void *worker_routine(void *args);
	std::function<void()> grab_task_from_runtime(); // figure the signature
	void push_task_to_runtime(std::function<void()> &&lambda); // figure the signature
}

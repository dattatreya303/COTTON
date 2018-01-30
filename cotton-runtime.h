#include "cotton.h"

#define MAX_WORKERS 4

namespace cotton_runtime {
	void lib_key_init();
	unsigned int get_threadID();
	void find_and_execute_task();
	unsigned int thread_pool_size();
	void *worker_routine(void *args);
	std::function<void()> grab_task_from_runtime(); // figure the signature
	void push_task_to_runtime(std::function<void()> &&lambda); // figure the signature
}

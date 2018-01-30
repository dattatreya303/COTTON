#include "cotton.h"

#define MAX_WORKERS 4

namespace cotton_runtime {
	unsigned int thread_pool_size();
	void *worker_routine(void *args);
	void find_and_execute_task();
	void grab_task_from_runtime(); // figure the signature
	void push_task_to_runtime(); // figure the signature
}

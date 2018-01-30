#include "cotton.h"
#include "cotton-runtime.h"

pthread_t thread[MAX_WORKERS];
volatile bool shutdown;
volatile unsigned int finish_counter;

unsigned int cotton_runtime::thread_pool_size() {
	return (unsigned int)atoi(std::getenv("COTTON_WORKERS"));
}

void *cotton_runtime::worker_routine(void *args) {
	while( !shutdown ) {
		cotton_runtime::find_and_execute_task();
	}
}

void cotton_runtime::find_and_execute_task() {
	return;
}

void cotton::init_runtime() {
	shutdown = false;
	for(int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		int status = pthread_create(&thread[i], NULL, cotton_runtime::worker_routine, NULL);
	}
}

void cotton::async(std::function<void()> &&lambda) {
	return;
}

void cotton::start_finish() {
	finish_counter = 0;
}

void cotton::end_finish() {
	while( finish_counter != 0 ) {
		cotton_runtime::find_and_execute_task();
	}
	return;
}

void cotton::finalize_runtime() {
	shutdown = true;
	for(int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		pthread_join(thread[i], NULL);
	}
	return;
}
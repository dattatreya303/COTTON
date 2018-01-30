#include "cotton.h"
#include "cotton-runtime.h"

volatile bool SHUTDOWN;
pthread_mutex_t FINISH_MUTEX;
pthread_t thread[MAX_WORKERS];
static pthread_key_t THREAD_KEY;
volatile unsigned int FINISH_COUNTER;
static pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;

unsigned int cotton_runtime::thread_pool_size() {
	return (unsigned int)atoi(std::getenv("COTTON_WORKERS"));
}

void cotton_runtime::lib_key_init(){
	if(pthread_key_create(&THREAD_KEY, NULL)){
		std::cout << "ERROR!! lib_key_init()" << std::endl;
	}
}

unsigned int cotton_runtime::get_threadID(){
	return *(int *)(pthread_getspecific(THREAD_KEY));
}

void *cotton_runtime::worker_routine(void *args) {
	int thread_id = *(int *)args;
	if( pthread_setspecific(THREAD_KEY, &thread_id) ) {
		std::cout << "ERROR!! pthread_setspecific() in worker_routine() " << std::endl;
	}

	while( !SHUTDOWN ) {
		cotton_runtime::find_and_execute_task();
	}
}

void cotton_runtime::find_and_execute_task() {
	auto task = cotton_runtime::grab_task_from_runtime();
	if( task != NULL ) {
		task();
		pthread_mutex_lock(&FINISH_MUTEX);
		FINISH_COUNTER--;
		pthread_mutex_unlock(&FINISH_MUTEX);
	}
}

void cotton_runtime::push_task_to_runtime(std::function<void()> &&lambda){
	
}

std::function<void()> cotton_runtime::grab_task_from_runtime(){
	
}

void cotton::init_runtime() {
	if(pthread_once(&THREAD_KEY_ONCE, cotton_runtime::lib_key_init)){
		std::cout << "ERROR!! init_runtime() key init" << std::endl;
	}

	int main_thread_id = 0;
	if( pthread_setspecific(THREAD_KEY, &main_thread_id) ) {
		std::cout << "ERROR!! pthread_setspecific() in init_runtime()" << std::endl;
	}

	SHUTDOWN = false;
	for(int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		int arg = i;
		int status = pthread_create(&thread[i], NULL, cotton_runtime::worker_routine, &arg);
	}
}

void cotton::async(std::function<void()> &&lambda) {
	cotton_runtime::push_task_to_runtime( std::forward< std::function<void()> >(lambda) );

	pthread_mutex_lock(&FINISH_MUTEX);
	FINISH_COUNTER++;
	pthread_mutex_unlock(&FINISH_MUTEX);
}

void cotton::start_finish() {
	FINISH_COUNTER = 0;
}

void cotton::end_finish() {
	while( FINISH_COUNTER != 0 ) {
		cotton_runtime::find_and_execute_task();
	}
	return;
}

void cotton::finalize_runtime() {
	SHUTDOWN = true;
	for(int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		pthread_join(thread[i], NULL);
	}
	return;
}
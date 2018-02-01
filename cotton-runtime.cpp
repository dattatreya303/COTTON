#include "cotton.h"
#include "cotton-runtime.h"

volatile bool SHUTDOWN;
pthread_mutex_t FINISH_MUTEX;
pthread_t thread[MAX_WORKERS];
static pthread_key_t THREAD_KEY;
volatile unsigned int FINISH_COUNTER;
static pthread_mutex_t DEQUE_MUTEX[MAX_WORKERS];
static cotton_runtime::Deque DEQUE_ARRAY[MAX_WORKERS];
static pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;

void cotton_runtime::Deque::push_to_deque(std::function<void()> &&lambda) {
	if( (tail + 1) % MAX_DEQUE_SIZE == head ) {
		throw std::out_of_range("Number of tasks exceeded deque size!");
	}

	task_deque[tail] = lambda;
	tail = (tail + 1) % MAX_DEQUE_SIZE;
}

bool cotton_runtime::Deque::isEmpty(){
	if( tail == head ){
		return true;
	}
	return false;
}

std::function<void()> cotton_runtime::Deque::pop_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	tail--;
	tail = (tail < 0) ? MAX_DEQUE_SIZE - 1 : tail;

	return task_deque[tail];
}

std::function<void()> cotton_runtime::Deque::steal_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	auto stolen_task = task_deque[head];
	head = ( head + 1 ) % MAX_DEQUE_SIZE;
	
	return stolen_task;
}


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
	unsigned int thread_id = *(unsigned int *)args;

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
	int current_thread_id = cotton_runtime::get_threadID();
	DEQUE_ARRAY[ current_thread_id ].push_to_deque( std::move(lambda) );
}

std::function<void()> cotton_runtime::grab_task_from_runtime(){
	int current_thread_id = cotton_runtime::get_threadID();
	
	
	pthread_mutex_lock( &DEQUE_MUTEX[ current_thread_id ] );
	auto grabbed_task = DEQUE_ARRAY[ current_thread_id ].pop_from_deque();
	pthread_mutex_unlock( &DEQUE_MUTEX[ current_thread_id ] );

	if( grabbed_task == NULL ) {
		int random_deque_id = current_thread_id;
		while( random_deque_id == current_thread_id ) {
			random_deque_id = rand() % MAX_DEQUE_SIZE;
		}
		pthread_mutex_lock( &DEQUE_MUTEX[ random_deque_id ] );
		grabbed_task = DEQUE_ARRAY[ random_deque_id ].steal_from_deque();
		pthread_mutex_unlock( &DEQUE_MUTEX[ random_deque_id ] );
	}

	return grabbed_task;
}

void cotton::init_runtime() {
	if(pthread_once(&THREAD_KEY_ONCE, cotton_runtime::lib_key_init)){
		std::cout << "ERROR!! init_runtime() key init" << std::endl;
	}

	unsigned int main_thread_id = 0;
	if( pthread_setspecific(THREAD_KEY, &main_thread_id) ) {
		std::cout << "ERROR!! pthread_setspecific() in init_runtime()" << std::endl;
	}

	SHUTDOWN = false;
	unsigned int *args = (unsigned int *)malloc( sizeof(unsigned int) * cotton_runtime::thread_pool_size() );
	for(unsigned int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		*(args+i) = i;
		int status = pthread_create(&thread[i], NULL, cotton_runtime::worker_routine, args+i);
	}
}

void cotton::async(std::function<void()> &&lambda) {
	cotton_runtime::push_task_to_runtime( std::move(lambda) );

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
}

void cotton::finalize_runtime() {
	SHUTDOWN = true;
	for(int i = 1; i < cotton_runtime::thread_pool_size(); i++) {
		pthread_join(thread[i], NULL);
	}
}
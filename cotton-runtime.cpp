/**
@file
@brief COTTON runtime implementation.
**/

#include "cotton.h"
#include "cotton-runtime.h"

volatile bool SHUTDOWN;
pthread_mutex_t FINISH_MUTEX;
pthread_t thread[MAX_WORKERS];
static pthread_key_t THREAD_KEY;
volatile unsigned int FINISH_COUNTER;
static pthread_mutex_t DEQUE_MUTEX[MAX_WORKERS];
static cotton::Deque DEQUE_ARRAY[MAX_DEQUE_SIZE];
static pthread_once_t THREAD_KEY_ONCE = PTHREAD_ONCE_INIT;


/**
Pushes the task to the deque data structure after checking boundary conditions

@param lambda Takes the task encapsulated in a lamda function
@return Void
**/
void cotton::Deque::push_to_deque(std::function<void()> &&lambda) {
	if( (tail + 1) % MAX_DEQUE_SIZE == head ) {
		throw std::out_of_range("Number of tasks exceeded deque size!");
	}

	task_deque[tail] = lambda;
	tail = (tail + 1) % MAX_DEQUE_SIZE;
}

/**
Wrapper to check if the deque data structure is empty

@return True if the task deque is empty and false otherwise
**/
bool cotton::Deque::isEmpty(){
	if( tail == head ){
		return true;
	}
	return false;
}

/**
Pops the task from the calling thread's deque data structure and checks for boundary conditions

@return Task encapsulated in a lambda function upon success otherwise NULL
**/
std::function<void()> cotton::Deque::pop_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	tail--;
	tail = (tail < 0) ? MAX_DEQUE_SIZE - 1 : tail;

	return task_deque[tail];
}

/**
Steals the task from the victim thread's deque data structure and checks for boundary conditions

@return Task encapsulated in a lambda function upon success otherwise NULL
**/
std::function<void()> cotton::Deque::steal_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	auto stolen_task = task_deque[head];
	head = ( head + 1 ) % MAX_DEQUE_SIZE;
	
	return stolen_task;
}

/**
Wrapper to get the value mentioned in the environment variable COTTON_WORKERS

@return Number of threads to use at max
**/
unsigned int cotton::thread_pool_size() {
	return (unsigned int)atoi(std::getenv("COTTON_WORKERS"));
}

/**
Wrapper to initialize the value of the pthread key variable with proper error checking

@return Void
**/
void cotton::lib_key_init() {
	if(pthread_key_create(&THREAD_KEY, NULL)) {
		std::cout << "ERROR!! lib_key_init()" << std::endl;
	}
}

/**
Gives the calling thread's ID used to access the per thread deque data structure

@return Thread ID of the calling thread 
**/
unsigned int cotton::get_threadID() {
	return *(int *)(pthread_getspecific(THREAD_KEY));
}

/**
Encapsulates the work that a thread does once spawing which includes setting the per thread ID and spinning until the thread finds a task to execute

@param args Contains the argument related to the ID of the calling thread
@return Void
**/
void *cotton::worker_routine(void *args) {
	unsigned int thread_id = *(unsigned int *)args;

	if( pthread_setspecific(THREAD_KEY, &thread_id) ) {
		std::cout << "ERROR!! pthread_setspecific() in worker_routine() " << std::endl;
	}

	while( !SHUTDOWN ) {
		cotton::find_and_execute_task();
	}
}

/**
Finds a task from the per thread deque data structures, executes it and finally decrements the volatile finish counter after acquiring the lock

@return Void
**/
void cotton::find_and_execute_task() {
	auto task = cotton::grab_task_from_runtime();
	if( task != NULL ) {
		task();
		pthread_mutex_lock(&FINISH_MUTEX);
		FINISH_COUNTER--;
		pthread_mutex_unlock(&FINISH_MUTEX);
	}
}

/**
Pushes a task encapsulated in a lambda function into the calling thread's data structure

@return Void
**/
void cotton::push_task_to_runtime(std::function<void()> &&lambda){
	int current_thread_id = cotton::get_threadID();
	DEQUE_ARRAY[ current_thread_id ].push_to_deque( std::move(lambda) );
}

/**
Pops or steals a task encapsulated as a lambda function from the caller or the victim thread's deque data structure.

It first checks in the calling thread deque data structure and if if does not find any then it tries to steal for other threads' deque data structure. All this is done after taking the necessary locks to avoid data races

@return Task encapsulated in a lambda function 
**/
std::function<void()> cotton::grab_task_from_runtime(){
	int current_thread_id = cotton::get_threadID();
	
	
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

/**
Spawns all the required threads after initializing the pthread key and setting the ID of the main thread

@return Void
**/
void cotton::init_runtime() {
	if(pthread_once(&THREAD_KEY_ONCE, cotton::lib_key_init)){
		std::cout << "ERROR!! init_runtime() key init" << std::endl;
	}

	// thread = (pthread_t *)malloc(sizeof(pthread_t)*cotton::thread_pool_size());

	// DEQUE_ARRAY = (cotton::Deque *)malloc(sizeof(cotton::Deque)*cotton::thread_pool_size());

	// DEQUE_MUTEX = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t)*cotton::thread_pool_size());

	unsigned int main_thread_id = 0;
	if( pthread_setspecific(THREAD_KEY, &main_thread_id) ) {
		std::cout << "ERROR!! pthread_setspecific() in init_runtime()" << std::endl;
	}


	SHUTDOWN = false;
	unsigned int *args = (unsigned int *)malloc( sizeof(unsigned int) * cotton::thread_pool_size() );
	for(unsigned int i = 1; i < cotton::thread_pool_size(); i++) {
		*(args+i) = i;
		int status = pthread_create(&thread[i], NULL, cotton::worker_routine, args+i);
	}
}

/**
Creates the task and pushes it to the calling thread's deque data structure and increments the finish counter after taking a lock

@param lambda Takes the task encapsulated in a lamda function
@return Void
**/
void cotton::async(std::function<void()> &&lambda) {
	pthread_mutex_lock(&FINISH_MUTEX);
	FINISH_COUNTER++;
	pthread_mutex_unlock(&FINISH_MUTEX);
	
	cotton::push_task_to_runtime( std::move(lambda) );
}

/**
Initializes the value of the finish counter

@return Void
**/
void cotton::start_finish() {
	FINISH_COUNTER = 0;
}

/**
Keeps spinning until the calling thread cannot find any task so that we can avoid it being idle

@return Void
**/
void cotton::end_finish() {
	while( FINISH_COUNTER != 0 ) {
		cotton::find_and_execute_task();
	}
}

/**
Waits till all the threads that have been spawed have finished executing the tasks

@return Void
**/
void cotton::finalize_runtime() {
	SHUTDOWN = true;
	for(int i = 1; i < cotton::thread_pool_size(); i++) {
		pthread_join(thread[i], NULL);
	}
}
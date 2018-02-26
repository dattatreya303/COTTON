/**
@file
@brief COTTON runtime implementation.
**/

#include "cotton.h"
#include "cotton-runtime.h"

/**
Pushes the task to the deque data structure after checking boundary conditions

@param lambda Takes the task encapsulated in a lamda function
@return Void
**/
void cotton::Deque::push_to_deque(volatile void *task) {
	int tail_next_pos = ( tail + 1 < cotton::MAX_DEQUE_SIZE ) ? ( tail + 1) : ( (tail + 1) % cotton::MAX_DEQUE_SIZE ); 
	if( tail_next_pos == head ) {
		cotton::free_all();
		throw std::out_of_range("Number of tasks exceeded deque size!");
	}
	task_deque[tail] = task;
	tail++;
	if( tail == cotton::MAX_DEQUE_SIZE )
		tail = tail % cotton::MAX_DEQUE_SIZE;
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
volatile void* cotton::Deque::pop_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	tail--;
	tail = (tail < 0) ? cotton::MAX_DEQUE_SIZE - 1 : tail;

	assert((task_deque[tail] != NULL));

	return task_deque[tail];
}

/**
Steals the task from the victim thread's deque data structure and checks for boundary conditions

@return Task encapsulated in a lambda function upon success otherwise NULL
**/
volatile void* cotton::Deque::steal_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	volatile void *stolen_task_ptr = task_deque[head];
	assert((stolen_task_ptr != NULL));

	head++;
	if( head == cotton::MAX_DEQUE_SIZE )
		head = head % cotton::MAX_DEQUE_SIZE;
	
	return stolen_task_ptr;
}

/**
Wrapper to get the value mentioned in the environment variable COTTON_WORKERS

@return Number of threads to use at max (default = 1)
**/
unsigned int cotton::thread_pool_size() {
	char * envData = std::getenv("COTTON_WORKERS");
	unsigned int numWorkers = cotton::DEFAULT_NUM_WORKERS;

	if(envData != '\0'){
		numWorkers = (unsigned int)atoi(std::getenv("COTTON_WORKERS"));
	}
	else{
		printf("COTTON_WORKERS environment variable not found.\n");
		printf("Using default number of workers (= %d).\n", cotton::DEFAULT_NUM_WORKERS);
	}
	
	return numWorkers;
}

/**
Wrapper to initialize the value of the pthread key variable with proper error checking

@return Void
**/
void cotton::lib_key_init() {
	assert((pthread_key_create(&cotton::THREAD_KEY, NULL) == 0));
}

/**
Gives the calling thread's ID used to access the per thread deque data structure

@return Thread ID of the calling thread 
**/
unsigned int cotton::get_threadID() {
	void *threadID = pthread_getspecific(cotton::THREAD_KEY);
	assert((threadID != NULL));
	return *(unsigned int *)threadID;
}

/**
Encapsulates the work that a thread does once spawing which includes setting the per thread ID and spinning until the thread finds a task to execute

@param args Contains the argument related to the ID of the calling thread
@return Void
**/
void* cotton::worker_routine(void *args) {
	unsigned int thread_id = *(unsigned int *)args;
	assert((pthread_setspecific(cotton::THREAD_KEY, &thread_id) == 0));
	while( !cotton::SHUTDOWN ) {
		cotton::find_and_execute_task();
	}
}

/**
Finds a task from the per thread deque data structures, executes it and finally decrements the volatile finish counter after acquiring the lock

@return Void
**/
void cotton::find_and_execute_task() {
	volatile void * task_ptr = cotton::grab_task_from_runtime();
	if( task_ptr != NULL ) {
		std::function<void()> task = *(std::function<void()> *)task_ptr;
		task();
		assert((pthread_mutex_lock(&cotton::FINISH_MUTEX) == 0));
		cotton::FINISH_COUNTER--;
		assert((pthread_mutex_unlock(&cotton::FINISH_MUTEX) == 0));
	}
}

/**
Pushes a task encapsulated in a lambda function into the calling thread's data structure

@return Void
**/
void cotton::push_task_to_runtime(volatile void *task) {
	unsigned int current_thread_id = cotton::get_threadID();
	cotton::DEQUE_ARRAY[ current_thread_id ].push_to_deque( task );
}

/**
Pops or steals a task encapsulated as a lambda function from the caller or the victim thread's deque data structure.

It first checks in the calling thread deque data structure and if if does not find any then it tries to steal for other threads' deque data structure. All this is done after taking the necessary locks to avoid data races

@return Task encapsulated in a lambda function 
**/
volatile void* cotton::grab_task_from_runtime() {
	unsigned int current_thread_id = cotton::get_threadID();
	
	assert((pthread_mutex_lock( &cotton::DEQUE_MUTEX[ current_thread_id ] ) == 0));
	volatile void *grabbed_task_ptr = cotton::DEQUE_ARRAY[ current_thread_id ].pop_from_deque();
	assert((pthread_mutex_unlock( &cotton::DEQUE_MUTEX[ current_thread_id ] ) == 0));
	
	if( grabbed_task_ptr == NULL ) {
		unsigned int random_deque_id = current_thread_id;
		while( random_deque_id == current_thread_id ) {
			random_deque_id = rand() % cotton::NUM_WORKERS;
		}
		assert((pthread_mutex_lock( &cotton::DEQUE_MUTEX[ random_deque_id ] ) == 0));
		grabbed_task_ptr = cotton::DEQUE_ARRAY[ random_deque_id ].steal_from_deque();
		assert((pthread_mutex_unlock( &cotton::DEQUE_MUTEX[ random_deque_id ] ) == 0));
	}

	return grabbed_task_ptr;
}

/**
Spawns all the required threads after initializing the pthread key and setting the ID of the main thread

@return Void
**/
void cotton::init_runtime() {

	cotton::NUM_WORKERS = cotton::thread_pool_size();

	assert((pthread_once(&cotton::THREAD_KEY_ONCE, cotton::lib_key_init) == 0));

	unsigned int *args = (unsigned int *)malloc( sizeof(unsigned int) * cotton::NUM_WORKERS );
	*(args) = 0;

	assert((pthread_setspecific(cotton::THREAD_KEY, args) == 0));

	cotton::DEQUE_ARRAY = new cotton::Deque[cotton::NUM_WORKERS];	
	cotton::thread = (pthread_t *)malloc(sizeof(pthread_t) * cotton::NUM_WORKERS);
	cotton::DEQUE_MUTEX = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * cotton::NUM_WORKERS);
	for(unsigned int i = 0; i < cotton::NUM_WORKERS; i++) {
		cotton::DEQUE_MUTEX[i] = PTHREAD_MUTEX_INITIALIZER;
	}
	
	cotton::SHUTDOWN = false;
	for(unsigned int i = 1; i < cotton::NUM_WORKERS; i++) {
		*(args + i) = i;
		int status = pthread_create(&cotton::thread[i], NULL, cotton::worker_routine, args+i);
	}

}

/**
Creates the task and pushes it to the calling thread's deque data structure and increments the finish counter after taking a lock

@param lambda Takes the task encapsulated in a lamda function
@return Void
**/
void cotton::async(std::function<void()> &&lambda) {
	volatile std::function<void()> *task_ptr = new std::function<void()>(lambda);
	assert((pthread_mutex_lock(&cotton::FINISH_MUTEX) == 0));
	cotton::FINISH_COUNTER++;
	assert((pthread_mutex_unlock(&cotton::FINISH_MUTEX) == 0));
	cotton::push_task_to_runtime( task_ptr );
}

/**
Initializes the value of the finish counter

@return Void
**/
void cotton::start_finish() {
	cotton::FINISH_COUNTER = 0;
}

/**
Keeps spinning until the calling thread cannot find any task so that we can avoid it being idle

@return Void
**/
void cotton::end_finish() {
	while( cotton::FINISH_COUNTER != 0 ) {		
		cotton::find_and_execute_task();
	}
}

/**
Call free() on everything allocated on heap.

@return Void
**/
void cotton::free_all() {
	free(cotton::thread);
	free(cotton::DEQUE_MUTEX);
	delete[] cotton::DEQUE_ARRAY;
} 

/**
Waits till all the threads that have been spawed have finished executing the tasks. Frees all the runtime allocated variables for better memory management.

@return Void
**/
void cotton::finalize_runtime() {
	cotton::SHUTDOWN = true;
	for(int i = 1; i < cotton::NUM_WORKERS; i++) {
		pthread_join(cotton::thread[i], NULL);
	}

	cotton::free_all();
}
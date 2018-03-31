/**
@file
@brief COTTON runtime implementation.
**/

#include "cotton.h"
#include "cotton-runtime.h"


unsigned int cotton::Deque::sizeof_deque() {
	return abs(tail - head);
}

/**
Pushes the task to the deque data structure after checking boundary conditions

@param task Takes the task encapsulated in a lamda function
@return Void
**/
void cotton::Deque::push_to_deque(void *task) {
	int tail_next_pos = ( tail + 1 < cotton::MAX_DEQUE_SIZE ) ? ( tail + 1) : ( (tail + 1) % cotton::MAX_DEQUE_SIZE ); 
	if( tail_next_pos == head ) {
		cotton::free_all();
		throw std::out_of_range("Number of tasks exceeded deque size!");
	}
	task_deque[tail] = (volatile void *)task;
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
void* cotton::Deque::pop_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	tail--;
	tail = (tail < 0) ? cotton::MAX_DEQUE_SIZE - 1 : tail;

	assert((task_deque[tail] != NULL));

	return (void *)task_deque[tail];
}

/**
Steals the task from the victim thread's deque data structure and checks for boundary conditions

@return Task encapsulated in a lambda function upon success otherwise NULL
**/
void* cotton::Deque::steal_from_deque() {
	if( isEmpty() ) {
		return NULL;
	}

	void *stolen_task_ptr = (void *)task_deque[head];
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
	
	if( envData != NULL ) {
		numWorkers = strtol(envData, NULL, 10);
		if( numWorkers == 0 ) {
			printf("COTTON_WORKERS only accepts positive integers greater.\n");
			printf("There has to be at least 1 worker.\n");
			printf("Using default number of workers (= %d) for this run ...\n", cotton::DEFAULT_NUM_WORKERS);
			numWorkers = cotton::DEFAULT_NUM_WORKERS;
		}
	}
	else {
		printf("COTTON_WORKERS environment variable not set.\n");
		printf("Using default number of workers (= %d) for this run ...\n", cotton::DEFAULT_NUM_WORKERS);
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
	void* task_ptr = cotton::grab_task_from_runtime();
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
void cotton::push_task_to_runtime(void* task) {
	unsigned int current_worker_id = cotton::get_threadID();
	cotton::WORKER_ARRAY[ current_worker_id ].worker_deque->push_to_deque( task );
	cotton::workload_up_check(current_worker_id);
}

/**
Pops or steals a task encapsulated as a lambda function from the caller or the victim thread's deque data structure.

It first checks in the calling thread deque data structure and if if does not find any then it tries to steal for other threads' deque data structure. All this is done after taking the necessary locks to avoid data races

@return Task encapsulated in a lambda function 
**/
void* cotton::grab_task_from_runtime() {
	unsigned int current_worker_id = cotton::get_threadID();
	
	assert((pthread_mutex_lock( &cotton::DEQUE_MUTEX[ current_worker_id ] ) == 0));
	void *grabbed_task_ptr = cotton::WORKER_ARRAY[ current_worker_id ].worker_deque->pop_from_deque();
	assert((pthread_mutex_unlock( &cotton::DEQUE_MUTEX[ current_worker_id ] ) == 0));
	
	if( grabbed_task_ptr == NULL ) {
		if (cotton::WORKER_ARRAY[current_worker_id].is_victim())
			cotton::end_victim(current_worker_id);
		unsigned int random_worker_id = current_worker_id;
		while( random_worker_id == current_worker_id ) {
			random_worker_id = rand() % cotton::NUM_WORKERS;
		}
		assert((pthread_mutex_lock( &cotton::DEQUE_MUTEX[ random_worker_id ] ) == 0));
		grabbed_task_ptr = cotton::WORKER_ARRAY[ random_worker_id ].worker_deque->steal_from_deque();
		if (grabbed_task_ptr != NULL) {
			cotton::begin_victim_thief_relationship(random_worker_id, current_worker_id);
			// random is victim right?
			cotton::workload_down_check(random_worker_id);
		}
		assert((pthread_mutex_unlock( &cotton::DEQUE_MUTEX[ random_worker_id ] ) == 0));
	}
	else {
		cotton::workload_down_check(current_worker_id);
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

	// cotton::DEQUE_ARRAY = new cotton::Deque[cotton::NUM_WORKERS];	
	cotton::WORKER_ARRAY = new cotton::Worker[cotton::NUM_WORKERS];	
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
	std::function<void()> *task_ptr = new std::function<void()>(lambda);
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
	delete[] cotton::WORKER_ARRAY;
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

/**
Checks if the worker is still a victim. A worker will remain a victim until its thief list points to NULL.

@return True, if worker is a victim. Else, false.
**/
bool cotton::Worker::is_victim() {
	return (thief_list_head->next != NULL);
}

/**
Add a thief node to the thief list of worker. This establishes a victim-thief relationship.

@param thief_worker_id Thief worker
@return Void
**/
void cotton::Worker::add_thief(int thief_worker_id) {
	cotton::Thief_node* new_thief_node = new cotton::Thief_node(thief_worker_id);
	new_thief_node->next = thief_list_head->next;
	if (new_thief_node->next != NULL)
		new_thief_node->next->prev = new_thief_node;
	new_thief_node->prev = thief_list_head;
	thief_list_head->next = new_thief_node;
}

/**
Iteratively free the memory occupied by the thief list of the worker.

@return Void
**/
void cotton::Worker::free_thief_list() {
	cotton::Thief_node* cur_node = thief_list_head;
	while (cur_node->next != NULL)
		cur_node = cur_node->next;
	while (cur_node != thief_list_head) {
		cotton::Thief_node* temp = cur_node;
		cur_node = cur_node->prev;
		delete temp;
	}
	thief_list_head->next = NULL;

	// printf("check if empty %d %d\n", thief_list_head->next == NULL, cur_node == thief_list_head);
}

/**
Begin a victim-thief relationship by:
- adding a new thief node to the worker's thief list.
- reducing the CPU frequency of thief worker using DVFS.

@param victim_worker_id Victim worker
@param thief_worker_id Thief worker, the one which stole the task.

@return Void
**/
void cotton::begin_victim_thief_relationship(int victim_worker_id, int thief_worker_id) {
	cotton::WORKER_ARRAY[victim_worker_id].add_thief(thief_worker_id);
	DOWN(thief_worker_id);
}

/**
Increasing the CPU frequency of all thiefs, and their thieves, recursively by one level.

@param victim_worker_id Victim worker
@param thief_worker_id Thief worker, the one which stole the task.

@return Void
**/
void cotton::end_victim_thief_relationship(int victim_worker_id, int thief_worker_id) {
	UP(thief_worker_id);
	cotton::Thief_node* cur_node = cotton::WORKER_ARRAY[thief_worker_id].thief_list_head;
	while (cur_node->next != NULL) {
		cur_node = cur_node->next;
		int thief_thief = cur_node->thief_worker_id;
		cotton::end_victim_thief_relationship(thief_worker_id, thief_thief);
	}
}

/**
End the victim status of a worker:
- increasing the CPU frequency of all its thiefs, and their thieves, recursively by one level.
- free the memory occupied by the thief list of the worker

@param victim_worker_id Victim worker

@return Void
**/
void cotton::end_victim(int victim_worker_id) {
	cotton::Thief_node* cur_node = cotton::WORKER_ARRAY[victim_worker_id].thief_list_head;
	while (cur_node->next != NULL) {
		cur_node = cur_node->next;
		int thief = cur_node->thief_worker_id;
		cotton::end_victim_thief_relationship(victim_worker_id, thief);
	}
	cotton::WORKER_ARRAY[victim_worker_id].free_thief_list();
}

void cotton::workload_up_check(int worker_id) {
	int current_threshold_index = cotton::WORKER_ARRAY[ worker_id ].current_size_threshold_index;
	int current_threshold = cotton::WORKER_ARRAY[ worker_id ].size_thresholds[current_threshold_index];

	if( cotton::WORKER_ARRAY[ worker_id ].worker_deque->sizeof_deque() > current_threshold ) {
		if( current_threshold_index < NUM_THRESHOLDS - 1 ) {
			cotton::WORKER_ARRAY[ worker_id ].current_size_threshold_index += 1;
			UP(worker_id);
		}
	}
}

void cotton::workload_down_check(int worker_id) {
	int current_threshold_index = cotton::WORKER_ARRAY[ worker_id ].current_size_threshold_index;
	int current_threshold = cotton::WORKER_ARRAY[ worker_id ].size_thresholds[current_threshold_index];

	if( cotton::WORKER_ARRAY[ worker_id ].worker_deque->sizeof_deque() < current_threshold ) {
		if( current_threshold_index > 0 ) {
			if( cotton::WORKER_ARRAY[ worker_id ].thief_list_head->prev != NULL ) {
				cotton::WORKER_ARRAY[ worker_id ].current_size_threshold_index -= 1;
				DOWN(worker_id);
			}
		}
	}
}

/**
Increase the CPU frequency of the worker by one level.

@param worker_id Worker

@return Void
**/
void cotton::UP(int worker_id) {
	return;
}

/**
Decrease the CPU frequency of the worker by one level.

@param worker_id Worker

@return Void
**/
void cotton::DOWN(int worker_id) {
	return;
}

/**
Decrease the CPU frequency of Worker 2 by one level below Worker 1.

@param worker_id_1 Worker 1
@param worker_id_2 Worker 2

@return Void
**/
void cotton::DOWN(int worker_id_1, int worker_id_2) {
	return;
}

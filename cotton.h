/**
@file
@brief Header for COTTON interface.
**/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <functional>
#include <fstream>

namespace cotton {
	void end_finish();
	void init_runtime();
	void start_finish();
	void finalize_runtime();
    unsigned int get_threadID();
    unsigned int thread_pool_size();
	void async(std::function<void()> &&lambda);
}
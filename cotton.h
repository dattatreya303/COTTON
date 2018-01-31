#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <functional>

namespace cotton {
	void init_runtime();
	void async(std::function<void()> &&lambda);
	void start_finish();
	void end_finish();
	void finalize_runtime();
}
#include "../cotton.h"
#include <inttypes.h>
#include <algorithm>
#include <sys/time.h>

static uint64_t *red_arr;

long get_usecs (void)
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return t.tv_sec*1000000+t.tv_usec;
}

uint64_t fib_seq(int n) {
  if (n < 2) {
    return n;
  }
  else {
    return fib_seq(n-1) + fib_seq(n-2);
  }
}

void fib(uint64_t n) {
  if (n < 18) { 
    uint64_t value = fib_seq(n);
    red_arr[cotton::get_threadID() * 32] += value;
  } 
  else {
    cotton::async([=]() {
      fib(n-1);
    });
    fib(n-2);
  }
}

int main(int argc, char **argv) {
  cotton::init_runtime();
  // hclib::launch([&]() {
    uint64_t n = 44;
    red_arr = new uint64_t[cotton::thread_pool_size() * 32];
    std::fill(red_arr, red_arr + cotton::thread_pool_size() * 32, 0);
    long start = get_usecs();
    cotton::start_finish();
    // hclib::finish([=]() {
      fib(n);
    // });
    cotton::end_finish();
    long end = get_usecs();
    uint64_t result = std::accumulate(red_arr, red_arr + cotton::thread_pool_size() * 32, 0);
    printf("Fibonacci of %" PRIu64 " is %" PRIu64 ".\n", n, result);
    if(result != 701408733) printf("TEST FAILED\n");
    else printf("TEST PASSED in %f\n", ((double)(end-start))/1000000);
    delete[] red_arr;
  // });
  cotton::finalize_runtime();
  return 0; 
}

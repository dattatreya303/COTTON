/**
@file
@brief Quicksort benchmark implementation.
**/

/*
 * Copyright 2017 Rice University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #include "hclib.hpp"
#include "../cotton.h"
#include <sys/time.h>
#include <inttypes.h>

using namespace std;

#define ELEMENT_T uint64_t

int partition(ELEMENT_T* data, int left, int right) {
	int i = left;
	int j = right;
	ELEMENT_T tmp;
	ELEMENT_T pivot = data[(left + right) / 2];

	while (i <= j) {
		while (data[i] < pivot) i++;
		while (data[j] > pivot) j--;
		if (i <= j) {
			tmp = data[i];
			data[i] = data[j];
		data[j] = tmp;
			i++;
			j--;
		}
	}

	return i;
}

int compare(const void * a, const void * b)
{
    // printf("%d \n", *(ELEMENT_T*)a);
	if ( *(ELEMENT_T*)a <  *(ELEMENT_T*)b ) return -1;
	else if ( *(ELEMENT_T*)a == *(ELEMENT_T*)b ) return 0;
	else return 1;
}

// int compare(const ELEMENT_T * a, const ELEMENT_T * b)
// {
//     printf("%d \n", *a);
//     if ( *a <  *b ) return -1;
//     else if ( *a == *b ) return 0;
//     else return 1;
// }

void sort(ELEMENT_T* data, int left, int right, ELEMENT_T threshold) {
	if (right - left + 1 > threshold) {
		int index = partition(data, left, right);
        // printf("%d\n", data[0]);
        // exit(0);
        // cotton::start_finish();
        // HCLIB_FINISH {
			if (left < index - 1) {
                cotton::async([=]{
                // hclib::async([=]() {
                    sort(data, left, (index - 1), threshold);
                // });
                });
            }

            if (index < right) {
                cotton::async([=]{
                // hclib::async([=]() {
                    sort(data, index, right, threshold);
                // });
                });
            }
        // }
        // cotton::end_finish();
    }
    else {
        //  quicksort in C++ library
        qsort(data+left, right - left + 1, sizeof(ELEMENT_T), compare);
        // printf("%" PRIu64 "\n", data[0]);
    }

}

long get_usecs (void)
{
   struct timeval t;
   gettimeofday(&t,NULL);
   return t.tv_sec*1000000+t.tv_usec;
}

int main(int argc, char **argv) {
    cotton::init_runtime();  
	// hclib::launch([=]() {
        int N = argc>1 ? atoi(argv[1]) : 10000000; // 1 million
            int threshold = argc>2 ? atoi(argv[2]) : (int)(0.001*N);
        printf("Sorting %d size array with threshold of %d\n",N,threshold);
        ELEMENT_T* data = new ELEMENT_T[N];

        srand(1);
        for(int i=0; i<N; i++) {
            data[i] = (ELEMENT_T)i;
            // printf("%d \n", data[i]);
        }
        
        long start = get_usecs();
        cotton::start_finish();
        sort(data, 0, N-1, threshold);
        cotton::end_finish();
        long end = get_usecs();
        double dur = ((double)(end-start))/1000000;
        
        ELEMENT_T a =0, b;
        bool ok= true;
        for (int k=0; k<N; k++) {
        b = data[k];
        ok &= (a <= b);
        a = b;
        }
        if(ok){
            printf("QuickSort passed, Time = %f\n",dur);
        }
        else{
            printf("QuickSort failed, Time = %f\n",dur);
        }
    // });
    cotton::finalize_runtime();
    return 0;
}
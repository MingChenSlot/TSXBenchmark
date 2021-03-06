/**
 * file:sbench.cpp
 * @author: Ming Chen 9068207811 <mchen67@wisc.edu>
 * @version 1.0
 * @created time: Mon 21 Oct 2013 10:19:47 PM CDT
 * 
 * @section LICENSE
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * @section DESCRIPTION
 * This benchmark is used to characterize Intel TSX (restricted transactional memory) with data structures like Array, LinkedList, Queue and Hash Table. 
 *
 */

#include <iostream>
#include <immintrin.h>
#include <sys/unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <iomanip>
#include <pthread.h>
#include <vector>
#include <list>
#include <set>
#include <time.h>
#include <sched.h>
//#include "stat.h"
#include "synclib.h"
#include "hwtimer.h"

using namespace std;

#define tr(container, it) \
	for(auto it = container.begin(); it != container.end(); it++)

#define SIZE 1000
#define TOTAL_WORKLOAD 1000
#define THREADS 8
#define REPEATS (TOTAL_WORKLOAD / THREADS)
// Global data structure
Lock l; // global lock
list<int> my_list(SIZE,1);
vector<Lock> locks(SIZE);
int access_pattern = 0;
int sync_mode = 0;
int trans_size;

// used to produce specific access pattern
vector<int> v_random;

void bind(int pproc) {
	cpu_set_t myProc;
	CPU_ZERO(&myProc);
	CPU_SET(pproc, &myProc);

	if (sched_setaffinity(0, sizeof(myProc), &myProc) < 0) {
		std::cerr << "Call to sched_setaffinity() failed for physical CPU "
				<< pproc << std::endl;
	}

}

/*************************************
 * Simple Transactional Access (HLE)
 *************************************/
template <class T>
void st_seq_access(T &data, long tid, int transac_size, int nit) {
	int seed = v_random[tid * REPEATS + nit];
	list<int>::iterator it_begin, it_mid, it_end, i;
	it_begin = my_list.begin();
	it_end = my_list.end();
	
	// get the start point
	for (int j = 0; j<seed; j++)
	{
	it_begin++;    
	}
	
	// get the stop point if can not reach the end of the list 
	it_mid = it_begin;
	for (int k = 0; k < transac_size; k++)
	{
		it_mid++;
	}

//	cout << seed << endl;
	if (_xbegin() == _XBEGIN_STARTED) {
		if (l.is_locked()) {
			_xabort(1);	
		}
		for (i = it_begin; i != it_mid && i != it_end ; i++) {
			// write after read
			*i = *i + 1;
		}
		_xend();
//		cout << "Succ" << endl;
	} else {
//		cout << "Fail" << endl;
		l.acquire();
		l.add_count();
		for (i = it_begin; i != it_mid && i != it_end ; i++) {
			// write after read
			*i = *i + 1;
		}
		l.release();
	}
}

template <class T>
void st_random_access(T &data, long tid, int transac_size, int nit) {
	int base = tid * REPEATS + nit;
//	cout << seed << endl;
	list<int>::iterator it;
	it = my_list.begin();

	if (_xbegin() == _XBEGIN_STARTED) {
		if (l.is_locked()) {
			_xabort(1);	
		}
		for (int i = 0; i < transac_size; i++) {
			for (int k = 0; k < v_random[base+i]; k++)
			{
				it++;
			}
			*it = *it + 1;
		}
		_xend();
	} else {
		l.acquire();
		l.add_count();
		for (int i = 0; i < transac_size; i++) {
			for (int k = 0; k < v_random[base+i]; k++)
			{
				it++;
			}
			*it = *it + 1;
		}
		l.release();
	}
}


/****************************************************
 * Abort Aware Transactional Access
 ****************************************************/
template <class T>
void aat_seq_access(T &data, long tid, int transac_size, int nit) {
	int seed = v_random[tid * REPEATS + nit];
	HTM htm;
	
	list<int>::iterator it_begin, it_mid, it_end, i;
	it_begin = my_list.begin();
	it_end = my_list.end();
	
	// get the start point
	for (int j = 0; j<seed; j++)
	{
	it_begin++;    
	}
	
	// get the stop point if can not reach the end of the list 
	it_mid = it_begin;
	for (int k = 0; k < transac_size; k++)
	{
		it_mid++;
	}
//	cout << seed << endl;
	
	if (htm.transaction_start()) {
		if (l.is_locked()) {
			htm.transaction_abort();	
		}
		for (i = it_begin; i != it_mid && i != it_end ; i++) {
			// write after read
			*i = *i + 1;
		}
		htm.transaction_commit();	
//		cout << "Succ" << endl;
	} else {
//		cout << "Fail" << endl;
		l.acquire();
		l.add_count();
		for (i = it_begin; i != it_mid && i != it_end ; i++) {
			// write after read
			*i = *i + 1;
		}
		l.release();
	}
//	htm.print_count();
}



template <class T>
void aat_random_access(T &data, long tid, int transac_size, int nit) {
	int base = tid * REPEATS + nit;
//	cout << seed << endl;
	HTM htm;
	list<int>::iterator it = my_list.begin();

	if (htm.transaction_start()) {
		if (l.is_locked()) {
			htm.transaction_abort();	
		}
		for (int i = 0; i < transac_size; i++) {
			for (int k = 0; k < v_random[base+i]; k++)
			{
				it++;
			}
			*it = *it + 1;
		}
		htm.transaction_commit();	
	} else {
		l.acquire();
		l.add_count();
		for (int i = 0; i < transac_size; i++) {
			for (int k = 0; k < v_random[base+i]; k++)
			{
				it++;
			}
			*it = *it + 1;
		}
		l.release();
	}
}


/*************************************
 * Coarse-Grained Lock Access
 *************************************/
template <class T>
void gl_seq_access(T &data, long tid, int transac_size, int nit) {
	int seed = v_random[tid * REPEATS + nit];
	
	list<int>::iterator it_begin, it_mid, it_end, i;
	it_begin = my_list.begin();
	it_end = my_list.end();
	
	// get the start point
	for (int j = 0; j<seed; j++)
	{
	it_begin++;    
	}
	// get the stop point if can not reach the end of the list 
	it_mid = it_begin;
	for (int k = 0; k < transac_size; k++)
	{
		it_mid++;
	}

	l.acquire();
		for (i = it_begin; i != it_mid && i != it_end ; i++) {
			// write after read
			*i = *i + 1;
		}
	l.release();
}


template <class T>
void gl_random_access(T &data, long tid, int transac_size, int nit) {
	int base = tid * REPEATS + nit;
	list<int>::iterator it = my_list.begin();

	l.acquire();
	for (int i = 0; i < transac_size; i++) {
			for (int k = 0; k < v_random[base+i]; k++)
			{
				it++;
			}
			*it = *it + 1;
	}
	l.release();
}

/*************************************
 * Fine-grained Lock Access
 * ***********************************/

template <class T>
void fl_seq_access(T &data, long tid, int transac_size, int nit) {
	int seed = v_random[tid * REPEATS + nit];
	list<int>::iterator it_begin, it_mid, it_end, k;
	it_begin = my_list.begin();
	it_end = my_list.end();
	
	// get the start point
	for (int j = 0; j<seed; j++)
	{
	it_begin++;    
	}
	// get the stop point if can not reach the end of the list 
	it_mid = it_begin;
	for (int k = 0; k < transac_size; k++)
	{
		it_mid++;
	}

	for (int i = seed; i < (seed + transac_size) && i < SIZE; i++) {
		locks[i].acquire();
	}
	for (k = it_begin; k != it_mid && k != it_end ; k++) {
		*k = *k + 1;
	}
	for (int i = seed; i < (seed + transac_size) && i < SIZE; i++) {
		locks[i].release();
	}
}


template <class T>
void fl_random_access(T &data, long tid, int transac_size, int nit) {
	int base = tid * REPEATS + nit;
	// sort accessing array entry by the their orders in the array, guaranteeing deadlock free
	set<int> sorted_set;
	list<int> sorted;
	list<int>::iterator it;
	list<int>::iterator it2 = my_list.begin();

	for (int i = 0; i < transac_size; i++) {
		sorted.push_back(v_random[base + i]);
		sorted_set.insert(v_random[base + i]);
	}
	sorted.sort();
	/*
	for (int i = 0; i < transac_size; i++) {
		locks[sorted[i]].acquire();
	}
	for (int i = 0; i < transac_size; i++) {
		data[sorted[i]] = data[sorted[i]] + 1;
	}
	*/
	//tr(sorted, it){
	//	cout << *it << endl;
	//}
	
	tr(sorted_set, it) {
	//	cout << *it << endl;
		locks[*it].acquire();
	}
	tr(sorted, it) {
			for (int k = 0; k < *it; k++)
			{
				it2++;
			}
		
			*it2= *it2 + 1;
	}
	tr(sorted_set, it) {
		locks[*it].release();
	}
}


void *target(void *threadid) {
	long tid = (long) threadid; 
	bind(tid);

	// 2 is simple transaction, 3 is coarse grained lock, 4 is fine-grained lock, 5 is abort code aware transaction
	if (sync_mode == 2) {
		for (int i = 0; i < REPEATS; i++) {
			switch(access_pattern) {
			case 1: st_seq_access(my_list, tid, trans_size, i); break;
			case 2: st_random_access(my_list, tid, trans_size, i); break;
			default: cout << "Error" << endl;
			}
		}	
	} else if (sync_mode == 3) {
		for (int i = 0; i < REPEATS; i++) {
			switch(access_pattern) {
			case 1: gl_seq_access(my_list, tid, trans_size, i); break;
			case 2: gl_random_access(my_list, tid, trans_size, i); break;
			default: cout << "Error" << endl;
			}
		}	
	} else if (sync_mode == 4) {
		for (int i = 0; i < REPEATS; i++) {
			switch(access_pattern) {
			case 1: fl_seq_access(my_list, tid, trans_size, i); break;
			case 2: fl_random_access(my_list, tid, trans_size, i); break;
			default: cout << "Error" << endl;
			}
		}	
	} else if (sync_mode == 5) {
		for (int i = 0; i < REPEATS; i++) {
			switch(access_pattern) {
			case 1: aat_seq_access(my_list, tid, trans_size, i); break;
			case 2: aat_random_access(my_list, tid, trans_size, i); break;
			default: cout << "Error" << endl;
			}
		}	
	}
	return 0;
}


int main(int argc, char **argv) {
	pthread_t threads[THREADS];
	int seed;	
	
	// init data structure
	vector<int>::iterator it;

	// timer init
	hwtimer_t timer;
	initTimer(&timer);
	
	if (argc != 4) {
		cout << "./arr access_pattern(-seq -rand) sync_mode(-st -gl -fl -aat) transactional_size" << endl;
		return 0;
	}
	trans_size = atoi(argv[3]);
	
	// mode selection
	if (strcmp(argv[1], "-seq") == 0 ) {
		access_pattern = 1;
		// initialize random queue
		for (int i = 0; i < (REPEATS * THREADS); i++) {
			srand(time(NULL) + i);
			seed = rand() % (SIZE - trans_size);
			v_random.push_back(seed);
		}
	} else if (strcmp(argv[1], "-rand") == 0) {
		access_pattern = 2;
		for (int i = 0; i < (REPEATS * THREADS * trans_size); i++) {
			srand(time(NULL) + i);
			seed = rand() % (SIZE - trans_size);// make sure operate on a entire transaction size
			v_random.push_back(seed);
		}
	} else {
		cout << "./arr access_pattern(-seq -rand) sync_mode(-st -gl -fl -aat) transactional_size" << endl;
		return 0;
	}
	
	if (strcmp(argv[2], "-st") == 0) {
		// htm
		sync_mode = 2;
	} else if(strcmp(argv[2], "-gl") == 0) {
		// global lock used
		sync_mode = 3;
	} else if(strcmp(argv[2], "-fl") == 0) {
		// fine-grained lock used
		sync_mode = 4;
	} else if(strcmp(argv[2], "-aat") == 0) {
		// fine-grained lock used
		sync_mode = 5;
	} else { 
		cout << "./arr access_pattern(-seq -rand) sync_mode(-st -gl -fl -aat) transactional_size" << endl;
		return 0;
	}

	startTimer(&timer);
	for (long t = 0; t < THREADS; t++) {
		if (pthread_create(&threads[t], NULL, &target, (void *)t)) { 
			cout << "create thread " << t << " failed\n";
			return -1;
		}
	}

	for (int i = 0; i < THREADS; i++) {
		if (pthread_join(threads[i], NULL)) {
			cout << "join thread " << i << " failed\n";
			return -1;
		}
		cout << "Thread " << i << " joined.\n";
	}
	stopTimer(&timer);
	cout << "Elapsed time for program: " << getTimerNs(&timer) << " ns" << endl;

/*	
	tr (arr, it) {
		cout << *it << " ";
	}
	cout << endl;
*/
/*
	tr (v_random, it) {
		cout << *it << " ";
	}
	cout << endl;
*/
	cout << "Lock count:" << l.get_count() << endl;
	return 0;
}

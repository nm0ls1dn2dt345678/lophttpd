#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "multicore.h"



#if defined __linux__ && !defined ANDROID

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <cstring>
#include <sched.h>
#include "misc.h"
#include "config.h"


namespace misc {

using namespace std;

int ncpus = 1;
int my_core = 0;

static int get_cores()
{
	int n = 1;
	char buf[256];

	FILE *f = fopen("/proc/cpuinfo", "r");
	if (!f) {
		err = "misc::get_cores:";
		err += strerror(errno);
		return -1;
	}
	for (;!feof(f);) {
		memset(buf, 0, sizeof(buf));
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;
		if (string(buf).find("processor") != string::npos)
			++n;
	}

	fclose(f);
	return n - 1;
}


int init_multicore()
{
	ncpus = get_cores();
	if (ncpus <= 0)
		ncpus = 1;
	return ncpus;
}


int setup_multicore(int n)
{
	// one core is this thread
	if (n == 1)
		return 0;

	// any invalid number of cores is treated as the
	// whole set
	if (n <= 0 || n > ncpus)
		n = ncpus;

	cpu_set_t *cpuset = CPU_ALLOC(n);
	if (!cpuset) {
		err = "misc::setup_multicore: OOM";
		return -1;
	}
	size_t size = CPU_ALLOC_SIZE(n);
	CPU_ZERO_S(size, cpuset);
	CPU_SET_S(0, size, cpuset);
	if (sched_setaffinity(getpid(), size, cpuset) < 0) {
		err = "misc::setup_multicore::sched_setaffinity:";
		err += strerror(errno);
		CPU_FREE(cpuset);
		return -1;
	}

	my_core = 0;
	pid_t pid = 0;
	// fork a child for each core
	for (int i = 1; i < n; ++i) {
		pid = fork();
		if (pid < 0) {
			err = "misc::setup_multicore::fork:";
			err += strerror(errno);
			CPU_FREE(cpuset);
			return -1;
		} else if (pid > 0)
			continue;
		CPU_ZERO_S(size, cpuset);
		CPU_SET_S(i, size, cpuset);
		if (sched_setaffinity(getpid(), size, cpuset) < 0) {
			err = "misc::setup_multicore::sched_setaffinity:";
			err += strerror(errno);
			CPU_FREE(cpuset);
			return -1;
		}
		my_core = i;
		httpd_config::master = 0;
		break;
	}

	CPU_FREE(cpuset);
	return 0;
}

}

#else

namespace misc {

int my_core = 0;

int init_multicore()
{
	return 0;
}

int setup_multicore(int n)
{
	return 0;
}

}

#endif


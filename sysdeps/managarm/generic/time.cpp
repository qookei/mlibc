
#include <bits/ensure.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

#include <hel.h>
#include <hel-syscalls.h>
#include <frigg/debug.hpp>
#include <mlibc/allocator.hpp>
#include <mlibc/posix-pipe.hpp>
#include <mlibc/sysdeps.hpp>

struct TrackerPage {
	uint64_t seqlock;
	int32_t state;
	int32_t padding;
	int64_t refClock;
	int64_t baseRealtime;
};

extern TrackerPage *__mlibc_clk_tracker_page;

namespace mlibc {

int sys_clock_get(int clock, time_t *secs, long *nanos) {
	if(clock == CLOCK_MONOTONIC) {
		uint64_t tick;
		HEL_CHECK(helGetClock(&tick));
		*secs = tick / 1000000000;
		*nanos = tick % 1000000000;
	}else if(clock == CLOCK_REALTIME) {
		cacheFileTable();

		// Start the seqlock read.
		auto seqlock = __atomic_load_n(&__mlibc_clk_tracker_page->seqlock, __ATOMIC_ACQUIRE);
		assert(!(seqlock & 1));

		// Perform the actual loads.
		auto ref = __atomic_load_n(&__mlibc_clk_tracker_page->refClock, __ATOMIC_RELAXED);
		auto base = __atomic_load_n(&__mlibc_clk_tracker_page->baseRealtime, __ATOMIC_RELAXED);

		// Finish the seqlock read.
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		assert(__atomic_load_n(&__mlibc_clk_tracker_page->seqlock, __ATOMIC_RELAXED) == seqlock);

		// Calculate the current time.
		uint64_t tick;
		HEL_CHECK(helGetClock(&tick));
		__ensure(tick >= __mlibc_clk_tracker_page->refClock); // TODO: Respect the seqlock!
		tick -= ref;
		tick += base;
		*secs = tick / 1000000000;
		*nanos = tick % 1000000000;
	}else if(clock == CLOCK_PROCESS_CPUTIME_ID) {
		frigg::infoLogger() << "\e[31mmlibc: clock_gettime does not support the CPU time clocks"
				"\e[39m" << frigg::endLog;
		*secs = 0;
		*nanos = 0;
	}else{
		frigg::panicLogger() << "mlibc: Unexpected clock " << clock << frigg::endLog;
	}
	return 0;
}

} //namespace mlibc


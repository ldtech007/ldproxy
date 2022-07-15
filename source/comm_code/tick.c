#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>

#include "tick.h"

/* 全局ticks */
struct timer_ticks g_ticks = {0, 0, 0};

/* 采样次数 */
#define TICKS_SAMPLE_TIMES 5
/* 采样间隔, ms单位 */
#define TICKS_SAMPLE_SLEEP 50

void timer_ticks_init(void)
{

#if defined(__x86_64__) || defined(__aarch64__)
	int i = 0;
	uint64_t ticks_pertime[TICKS_SAMPLE_TIMES];
	uint64_t tick_sum = 0;

	memset(&g_ticks, 0, sizeof(struct timer_ticks));

	/* 采样特定次数 */
	for (i = 0; i < TICKS_SAMPLE_TIMES; ++i) 
	{
		uint64_t start = timer_get_ticks();
		usleep(TICKS_SAMPLE_SLEEP * 1000);
		ticks_pertime[i] = timer_get_ticks() - start;
	}

	/* 根据采样结果初始化全局的ticks数 */
	for (i = 0; i < TICKS_SAMPLE_TIMES; ++i)
		tick_sum += ticks_pertime[i];
	g_ticks.ticks_per_us = tick_sum / TICKS_SAMPLE_TIMES / TICKS_SAMPLE_SLEEP / 1000;
	g_ticks.ticks_per_ms = tick_sum / TICKS_SAMPLE_TIMES / TICKS_SAMPLE_SLEEP;
	g_ticks.ticks_per_sec = g_ticks.ticks_per_ms * 1000;

	assert(g_ticks.ticks_per_us);
	assert(g_ticks.ticks_per_ms);
	assert(g_ticks.ticks_per_sec);

	printf("timer init ok -> ticks_per_us: %"PRIu64", ticks_per_ms: %"PRIu64", ticks_per_sec: %"PRIu64"\n", 
		   g_ticks.ticks_per_us, 
		   g_ticks.ticks_per_ms, 
		   g_ticks.ticks_per_sec);
#endif

}

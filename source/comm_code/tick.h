#ifndef	_TICKS_H_
#define	_TICKS_H_

#ifdef __cplusplus
extern "C" {
#endif
/*
 * ticks函数库, 实现ticks转s,ms,us, 提供给程序用于定时; 
 * 不一定精确, 但是快速
 * */

#include <time.h>
#include <stdint.h>
#include <assert.h>


struct timer_ticks
{
	uint64_t ticks_per_us; /* 每1us的ticks数 */
	uint64_t ticks_per_ms; /* 每1ms的ticks数 */
	uint64_t ticks_per_sec;/* 每1s的ticks数 */
};

extern struct timer_ticks g_ticks;

/* 获取当前ticks */
inline static uint64_t timer_get_ticks(void)
{
	uint64_t result;	
#if defined(__x86_64__)
	unsigned a, d; 
	__asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d)); 
	return ((uint64_t)a) | (((uint64_t)d) << 32); 
#elif defined(__aarch64__)
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(result)::);
#else
	result = 0;
#endif
	return result;
}

/* 每1s有多少ticks */
inline static uint64_t ticks_per_secs(void)
{
	assert(g_ticks.ticks_per_sec);
	return g_ticks.ticks_per_sec;
}
/* 每1ms有多少ticks */
inline static uint64_t ticks_per_ms(void)
{
	assert(g_ticks.ticks_per_ms);
	return g_ticks.ticks_per_ms;
}
/* 每1us有多少ticks */
inline static uint64_t ticks_per_us(void)
{
	assert(g_ticks.ticks_per_us);
	return g_ticks.ticks_per_us;
}

/* 获取当前时间, 以s为最小单位 */
inline static uint64_t timer_get_secs(void)
{
	assert(g_ticks.ticks_per_sec);
	return timer_get_ticks() / g_ticks.ticks_per_sec;
}
/* 获取当前时间, 以ms为最小单位 */
inline static uint64_t timer_get_ms(void)
{
	assert(g_ticks.ticks_per_ms);
	return timer_get_ticks() / g_ticks.ticks_per_ms;
}
/* 获取当前时间, 以us为最小单位 */
inline static uint64_t timer_get_us(void)
{
	assert(g_ticks.ticks_per_us);
	return timer_get_ticks() / g_ticks.ticks_per_us;
}

/* ticks转成s */
inline static uint64_t timer_ticks2s(uint64_t ticks)
{
	assert(g_ticks.ticks_per_sec);
	return ticks / g_ticks.ticks_per_sec;
}
/* ticks转成ms */
inline static uint64_t timer_ticks2ms(uint64_t ticks)
{
#if defined(__x86_64__) || defined(__aarch64__)
	assert(g_ticks.ticks_per_ms);
	return ticks / g_ticks.ticks_per_ms;
#else
    struct timespec tsnow = {0};
	uint64_t monotonic_sec = 0;
    uint64_t monotonic_nsec = 0;
    uint64_t monotonic_ms = 0;
    
    if (clock_gettime(CLOCK_MONOTONIC, &tsnow) == 0) {
        monotonic_sec = (uint64_t)tsnow.tv_sec;
        monotonic_nsec = (uint64_t)tsnow.tv_nsec;
    }
    monotonic_ms = monotonic_sec * 1000 + monotonic_nsec / 1000000;
    return monotonic_ms;
#endif
}
/* ticks转成us */
inline static uint64_t timer_ticks2us(uint64_t ticks)
{
	assert(g_ticks.ticks_per_us);
	return ticks / g_ticks.ticks_per_us;
}

/* 要使用ticks的程序需要先调用此函数初始化ticks */
extern void timer_ticks_init(void);

#ifdef __cplusplus
}
#endif

#endif

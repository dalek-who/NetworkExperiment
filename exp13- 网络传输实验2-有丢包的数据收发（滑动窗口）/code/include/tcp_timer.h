#ifndef __TCP_TIMER_H__
#define __TCP_TIMER_H__

#include "list.h"

#include <stddef.h>

struct tcp_timer {
	int type;	// now only support time-wait
	int timeout;	// in micro second
	int retrans_times;
	struct list_head list;
};

struct tcp_sock;
#define timewait_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, timewait))

#define TCP_TIMER_SCAN_INTERVAL 100000
#define TCP_MSL			1000000
#define TCP_TIMEWAIT_TIMEOUT	(2 * TCP_MSL)

// the thread that scans timer_list periodically
void *tcp_timer_thread(void *arg);
// add the timer of tcp sock to timer_list
void tcp_set_timewait_timer(struct tcp_sock *);

////////////////////////////////////////////////////////////////
//新添加的用来tcp重传的计时器tcp_retrans timer
void tcp_set_retrans_timewait_timer(struct tcp_sock *tsk);
void tcp_update_retrans_timewait_timer(struct tcp_sock *tsk);
void tcp_stop_retrans_timewait_timer(struct tcp_sock *tsk);
void tcp_scan_retrans_timer_list();
void *tcp_retrans_timer_thread(void *arg);



#endif

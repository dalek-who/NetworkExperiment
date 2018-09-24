#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <unistd.h>
#include <stdio.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *t, *q;
	list_for_each_entry_safe(t, q, &timer_list, list) {
		t->timeout -= TCP_TIMER_SCAN_INTERVAL;
		if (t->timeout <= 0) {
			list_delete_entry(&t->list);

			// only support time wait now
			tsk = timewait_to_tcp_sock(t);
			list_delete_entry(&tsk->retrans_timer.list);
			if (! tsk->parent)
				tcp_bind_unhash(tsk);
			tcp_set_state(tsk, TCP_CLOSED);
			free_tcp_sock(tsk);
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->timewait;

	timer->type = 0;
	timer->timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&timer->list, &timer_list);

	tcp_sock_inc_ref_cnt(tsk);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}


#define MAX_RETRANS 3
#define DEFAULT_RETRANS_TIME 200000
#define RETRANS_SCAN_TIMER 10000
#define retrans_timer_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, retrans_timer))

static struct list_head retrans_timer_list;
void tcp_scan_retrans_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *t, *q;
	list_for_each_entry_safe(t, q, &retrans_timer_list, list) {
		t->timeout -= RETRANS_SCAN_TIMER;
		tsk = retrans_timer_to_tcp_sock(t);
		if(t->timeout <=0){
			if(t->retrans_times>=MAX_RETRANS){
				list_delete_entry(&t->list);
				//list_delete_entry(&tsk->timewait.list);
				if (! tsk->parent)
					tcp_bind_unhash(tsk);
				wait_exit(tsk->wait_connect);
				wait_exit(tsk->wait_accept);
				wait_exit(tsk->wait_recv);
				wait_exit(tsk->wait_send);
				
				fprintf(stdout, "retrans_timer: ");
				tcp_set_state(tsk, TCP_CLOSED);
				free_tcp_sock(tsk);
				buf_remove_and_free_all_block(); //todo
				exit(0);
			}
			else{
				t->retrans_times += 1;
				t->timeout = DEFAULT_RETRANS_TIME * (2<<t->retrans_times);
				buf_retrans_first(tsk);//todo
			}
		}
	}
}

void tcp_set_retrans_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;

	timer->type = 0;
	timer->timeout = DEFAULT_RETRANS_TIME;
	timer->retrans_times = 0;
	init_list_head(&timer->list);
	list_add_tail(&timer->list, &retrans_timer_list);

	tcp_sock_inc_ref_cnt(tsk);
}

void tcp_update_retrans_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;

	timer->type = 0;
	timer->timeout = DEFAULT_RETRANS_TIME;
	timer->retrans_times = 0;
}

void tcp_stop_retrans_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;
	list_delete_entry(&timer->list);
	free_tcp_sock(tsk);
}


void *tcp_retrans_timer_thread(void *arg){
	init_list_head(&retrans_timer_list);
	while(1){
		usleep(RETRANS_SCAN_TIMER);
		tcp_scan_retrans_timer_list();
	}
}







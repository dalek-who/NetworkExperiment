#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

// handling incoming packet for TCP_LISTEN state
//
// 1. malloc a child tcp sock to serve this connection request; 
// 2. send TCP_SYN | TCP_ACK by child tcp sock;
// 3. hash the child tcp sock into established_table (because the 4-tuple 
//    is determined).
void tcp_state_listen(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	fprintf(stdout, ": implement this function please.\n");
	//0.判断是否异常
	//当本机没有监听端口，收到数据包时:回复RST数据包
	if(tsk==NULL){
		tcp_send_reset(cb);
		return;
	}
	//当监听端口，收到非SYN包时:如果是RST包，直接丢弃；否则，回复RST数据包
	else if((cb->flags & TCP_SYN) == 0){
		if(cb->flags & TCP_RST)
			return;
		else{
			tcp_send_control_packet(tsk, TCP_RST);
			return;
		}
	}

	//如果不异常：
	// 1. malloc a child tcp sock to serve this connection request; 
	struct tcp_sock *csk = alloc_tcp_sock();
	csk->sk_sip   = cb->daddr;
	csk->sk_sport = cb->dport;
	csk->sk_dip   = cb->saddr;
	csk->sk_dport = cb->sport;
	
	csk->iss = tcp_new_iss();
	csk->snd_una = csk->iss;
	csk->snd_nxt = csk->iss;
	csk->rcv_nxt = cb->seq + 1;
	csk->parent = tsk;
	list_add_tail(&csk->list, &csk->listen_queue);
	tcp_set_state(csk, TCP_SYN_RECV);
	
	// 2. send TCP_SYN | TCP_ACK by child tcp sock;
	tcp_set_state(tsk, TCP_SYN_RECV);
	tcp_send_control_packet(csk, TCP_SYN|TCP_ACK);

	// 3. hash the child tcp sock into established_table (because the 4-tuple is determined).
	tcp_hash(csk);

}

// handling incoming packet for TCP_CLOSED state, by replying TCP_RST
void tcp_state_closed(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	tcp_send_reset(cb);
}

// handling incoming packet for TCP_SYN_SENT state
//
// If everything goes well (the incoming packet is TCP_SYN|TCP_ACK), reply with 
// TCP_ACK, and enter TCP_ESTABLISHED state, notify tcp_sock_connect; otherwise, 
// reply with TCP_RST.
void tcp_state_syn_sent(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please.\n");
	if( (cb->flags & (TCP_SYN|TCP_ACK)) == (TCP_SYN|TCP_ACK) ){
		tsk->rcv_nxt = cb->seq + 1;
		tsk->snd_una = cb->ack;
		
		tcp_set_state(tsk, TCP_ESTABLISHED);
		tcp_send_control_packet(tsk, TCP_ACK);
		wake_up(tsk->wait_connect);
	}
	else
		tcp_send_reset(cb);
}

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0){
		wake_up(tsk->wait_send);
		//fprintf(stdout, "**tcp_update_window: [wake_up] wait_send\n");
	}
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

// handling incoming ack packet for tcp sock in TCP_SYN_RECV state
//
// 1. remove itself from parent's listen queue;
// 2. add itself to parent's accept queue;
// 3. wake up parent (wait_accept) since there is established connection in the
//    queue.
/* the arg tsk, is csk indeed. tcp_state_listen() create csk,and hash it into establish_hash.
   then when processing ACK in syn_recv, the "tsk" which handle_tcp() get from establish_hash using cb,
   is indeed the csk
*/
void tcp_state_syn_recv(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please.\n");
	
	if(cb->flags & TCP_ACK){
		struct tcp_sock *csk = tsk, *parent_tsk = csk->parent;
		// 1. remove itself from parent's listen queue;
		// 2. add itself to parent's accept queue;
		tcp_sock_accept_enqueue(csk);
		// 3. wake up parent (wait_accept) since there is established connection in the queue.
		
		//csk->rcv_nxt = cb->seq + 1;
		csk->rcv_nxt = cb->seq;
		tsk->snd_una = cb->ack;

		tcp_set_state(parent_tsk, TCP_ESTABLISHED);
		tcp_set_state(csk,        TCP_ESTABLISHED);
		wake_up(parent_tsk->wait_accept);
		//fprintf(stdout, "[tcp_state_syn_recv]:wake up on wait_accept.\n");
		//tcp_send_control_packet(tsk, TCP_ACK);
	}
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

void tcp_recv_data(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet){
	/*
	while( ring_buffer_full(tsk->rcv_buf) ){
		if(sleep_on(tsk->wait_recv)<0)
			return -1;
	}
	write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
	*/
	//fprintf(stdout, "**tcp_recv_data: into tcp recv data.\n");
	
	for(int i=0; i<cb->pl_len ; ){
		while(ring_buffer_full(tsk->rcv_buf)){
			//fprintf(stdout, "**tcp_recv_data: *[sleep_on] wait_recv, buf full\n");
			if(sleep_on(tsk->wait_recv)<0){
				//fprintf(stdout, "**tcp_recv_data: be *[wake_up] ERROR.\n");
				return ;
			}
			//fprintf(stdout, "**tcp_recv_data: be *[wake_up] from wait_recv.\n");
		}
		int wsize =  min(ring_buffer_free(tsk->rcv_buf), cb->pl_len-i);
		//fprintf(stdout, "**tcp_recv_data: write buf.\n");
		write_ring_buffer(tsk->rcv_buf, cb->payload+i, wsize);
		//fprintf(stdout, "**tcp_recv_data: write buf %d [finish].\n",wsize);
		i += wsize;
		wake_up(tsk->wait_recv);
		//fprintf(stdout, "**tcp_recv_data: *[wake_up] wait_recv.\n");
		
	}
	tsk->rcv_nxt = cb->seq + cb->pl_len;
	tsk->snd_una = cb->ack;
	tcp_send_control_packet(tsk, TCP_ACK);
}

// Process an incoming packet as follows:
// 	 1. if the state is TCP_CLOSED, hand the packet over to tcp_state_closed;
// 	 2. if the state is TCP_LISTEN, hand it over to tcp_state_listen;
// 	 3. if the state is TCP_SYN_SENT, hand it to tcp_state_syn_sent;
// 	 4. check whether the sequence number of the packet is valid, if not, drop
// 	    it;
// 	 5. if the TCP_RST bit of the packet is set, close this connection, and
// 	    release the resources of this tcp sock;
// 	 6. if the TCP_SYN bit is set, reply with TCP_RST and close this connection,
// 	    as valid TCP_SYN has been processed in step 2 & 3;
// 	 7. check if the TCP_ACK bit is set, since every packet (except the first 
//      SYN) should set this bit;
//   8. process the ack of the packet: if it ACKs the outgoing SYN packet, 
//      establish the connection; (if it ACKs new data, update the window;)
//      if it ACKs the outgoing FIN packet, switch to correpsonding state;
//   9. (process the payload of the packet: call tcp_recv_data to receive data;)
//  10. if the TCP_FIN bit is set, update the TCP_STATE accordingly;
//  11. at last, do not forget to reply with TCP_ACK if the connection is alive.
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	if(!tsk)
		return;
	// 	 1. if the state is TCP_CLOSED, hand the packet over to tcp_state_closed;
	if(tsk->state == TCP_CLOSED){
		tcp_state_closed(tsk, cb, packet);//tsk die,with RST insde it
		return;
	}
	// 	 2. if the state is TCP_LISTEN, hand it over to tcp_state_listen;
	if(tsk->state == TCP_LISTEN){
		tcp_state_listen(tsk, cb, packet);//has ack inside it
		return;
	}
	// 	 3. if the state is TCP_SYN_SENT, hand it to tcp_state_syn_sent;
	if(tsk->state == TCP_SYN_SENT){
		tcp_state_syn_sent(tsk, cb, packet);//has ack inside it
		return;
	}
	
	// 	 4. check whether the sequence number of the packet is valid, if not, drop it;
	if( !is_tcp_seq_valid(tsk, cb) ){
		return;
	}
	// 	 5. if the TCP_RST bit of the packet is set, close this connection, and 
	//      release the resources of this tcp sock;
	if(cb->flags & TCP_RST ){
		tcp_sock_close(tsk);
		free(tsk);
		return;
	}
	// 	 6. if the TCP_SYN bit is set, reply with TCP_RST and close this connection,
	// 	    as valid TCP_SYN has been processed in step 2 & 3;
	if(cb->flags & TCP_SYN){
		tcp_send_reset(cb);
		tcp_sock_close(tsk);
		return;
	}
	// 	 7. check if the TCP_ACK bit is set, since every packet (except the first 
	//      SYN) should set this bit;
	if((cb->flags & TCP_ACK) == 0){
		tcp_send_reset(cb);
		return;
	}
	else{
		//   8. process the ack of the packet: if it ACKs the outgoing SYN packet, 
		//      establish the connection; 
		if(tsk->state == TCP_SYN_RECV){
			tcp_state_syn_recv(tsk, cb, packet); //don't need send ack
			return;
		}
		//      (if it ACKs new data, update the window;)
		else if(tsk->state == TCP_ESTABLISHED && (cb->flags & TCP_FIN)==0 ){
			if(cb->pl_len==0){ //just an ACK packet
				tsk->snd_una = cb->ack;
				tsk->rcv_nxt = cb->seq +1;
				tcp_update_window_safe(tsk, cb);
				return;
			}
			else{ //packet with data
				tcp_recv_data(tsk, cb, packet);
				return;
			}
		}
		//      if it ACKs the outgoing FIN packet, switch to correpsonding state;
		else if((cb->flags & TCP_FIN) ==0){//all these don't need send ack
			switch(tsk->state){
				case TCP_FIN_WAIT_1:
					tcp_set_state(tsk, TCP_FIN_WAIT_2);
					return;
				case TCP_CLOSING:
					tcp_set_state(tsk, TCP_TIME_WAIT);
					tcp_set_timewait_timer(tsk);
					tcp_unhash(tsk);
					return;
				case TCP_LAST_ACK:
					tcp_set_state(tsk, TCP_CLOSED);
					return;
				default:
					break;
			}
		}
	}
	//   9. (process the payload of the packet: call tcp_recv_data to receive data;)
	//todo;

	//当收到的数据包没有TCP_ACK标志时:
	//	只有RST包和主动连接的SYN包可以不包含TCP_ACK标志位(这两种前面都已经处理了)
	//	对于其他数据包，回复RST数据包

	//  10. if the TCP_FIN bit is set, update the TCP_STATE accordingly;
	if(cb->flags & TCP_FIN){
		switch(tsk->state){
			case TCP_ESTABLISHED:
				tsk->rcv_nxt = cb->seq+1;
				wait_exit(tsk->wait_recv);
				wait_exit(tsk->wait_send);
				tcp_set_state(tsk, TCP_CLOSE_WAIT);
				tcp_send_control_packet(tsk, TCP_ACK);
				return;
			case TCP_FIN_WAIT_1:
				tcp_set_state(tsk, TCP_CLOSING);
				tcp_send_control_packet(tsk, TCP_ACK);
				return;
			case TCP_FIN_WAIT_2:
				tsk->rcv_nxt = cb->seq+1;
				tcp_set_state(tsk, TCP_TIME_WAIT);
				tcp_set_timewait_timer(tsk);
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_unhash(tsk);
				return;
			default:
				break;
		}
	}

	//  11. at last, do not forget to reply with TCP_ACK if the connection is alive.
	tcp_send_control_packet(tsk, TCP_ACK);
	return;
}

#include "tcp.h"
#include "tcp_hash.h"
#include "tcp_sock.h"
#include "tcp_timer.h"
#include "ip.h"
#include "rtable.h"
#include "log.h"

// TCP socks should be hashed into table for later lookup: Those which
// occupy a port (either by *bind* or *connect*) should be hashed into
// bind_table, those which listen for incoming connection request should be
// hashed into listen_table, and those of established connections should
// be hashed into established_table.

struct tcp_hash_table tcp_sock_table;
#define tcp_established_sock_table	tcp_sock_table.established_table
#define tcp_listen_sock_table		tcp_sock_table.listen_table
#define tcp_bind_sock_table			tcp_sock_table.bind_table

inline void tcp_set_state(struct tcp_sock *tsk, int state)
{
	log(DEBUG, IP_FMT":%hu switch state, from %s to %s.", \
			HOST_IP_FMT_STR(tsk->sk_sip), tsk->sk_sport, \
			tcp_state_str[tsk->state], tcp_state_str[state]);
	tsk->state = state;
}

// init tcp hash table and tcp timer
void init_tcp_stack()
{
	for (int i = 0; i < TCP_HASH_SIZE; i++)
		init_list_head(&tcp_established_sock_table[i]);

	for (int i = 0; i < TCP_HASH_SIZE; i++)
		init_list_head(&tcp_listen_sock_table[i]);

	for (int i = 0; i < TCP_HASH_SIZE; i++)
		init_list_head(&tcp_bind_sock_table[i]);

	pthread_t timer;
	pthread_create(&timer, NULL, tcp_timer_thread, NULL);

	init_tcp_trans_buf(); //new add 
}

// allocate tcp sock, and initialize all the variables that can be determined
// now
struct tcp_sock *alloc_tcp_sock()
{
	struct tcp_sock *tsk = malloc(sizeof(struct tcp_sock));

	memset(tsk, 0, sizeof(struct tcp_sock));

	tsk->state = TCP_CLOSED;
	tsk->rcv_wnd = TCP_DEFAULT_WINDOW;
	tsk->snd_wnd = TCP_DEFAULT_WINDOW;

	init_list_head(&tsk->list);
	init_list_head(&tsk->listen_queue);
	init_list_head(&tsk->accept_queue);

	tsk->rcv_buf = alloc_ring_buffer(tsk->rcv_wnd);
	init_list_head(&tsk->rcv_ofo_list); //new add

	tsk->wait_connect = alloc_wait_struct();
	tsk->wait_accept = alloc_wait_struct();
	tsk->wait_recv = alloc_wait_struct();
	tsk->wait_send = alloc_wait_struct();

	init_list_head(&tsk->timewait.list);
	init_list_head(&tsk->retrans_timer.list);


	return tsk;
}

// release all the resources of tcp sock
//
// To make the stack run safely, each time the tcp sock is refered (e.g. hashed), 
// the ref_cnt is increased by 1. each time free_tcp_sock is called, the ref_cnt
// is decreased by 1, and release the resources practically if ref_cnt is
// decreased to zero.
void free_tcp_sock(struct tcp_sock *tsk)
{
	tsk->ref_cnt -= 1;
	if (tsk->ref_cnt <= 0) {
		log(DEBUG, "free tcp sock: ["IP_FMT":%hu<->"IP_FMT":%hu].", \
				HOST_IP_FMT_STR(tsk->sk_sip), tsk->sk_sport,
				HOST_IP_FMT_STR(tsk->sk_dip), tsk->sk_dport);

		free_wait_struct(tsk->wait_connect);
		free_wait_struct(tsk->wait_accept);
		free_wait_struct(tsk->wait_recv);
		free_wait_struct(tsk->wait_send);

		free_ring_buffer(tsk->rcv_buf);

		free(tsk);
	}
}

// lookup tcp sock in established_table with key (saddr, daddr, sport, dport)
struct tcp_sock *tcp_sock_lookup_established(u32 saddr, u32 daddr, u16 sport, u16 dport)
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	int hash = tcp_hash_function(saddr, daddr, sport, dport);
	struct list_head *list = &tcp_established_sock_table[hash];
	struct tcp_sock *tmp;
	list_for_each_entry(tmp, list, hash_list) {
		if (    saddr == tmp->sk_sip &&
				daddr == tmp->sk_dip &&
				sport == tmp->sk_sport &&
				dport == tmp->sk_dport)
			return tmp;
	}
	return NULL;
}

// lookup tcp sock in listen_table with key (sport)
//
// In accordance with BSD socket, saddr is in the argument list, but never used.
struct tcp_sock *tcp_sock_lookup_listen(u32 saddr, u16 sport)
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	int hash = tcp_hash_function(0, 0, sport, 0);
	struct list_head *list = &tcp_listen_sock_table[hash];
	struct tcp_sock *tmp;
	list_for_each_entry(tmp, list, hash_list) {
		if (sport == tmp->sk_sport)
			return tmp;
	}
	return NULL;
}

// lookup tcp sock in both established_table and listen_table
struct tcp_sock *tcp_sock_lookup(struct tcp_cb *cb)
{
	u32 saddr = cb->daddr,
		daddr = cb->saddr;
	u16 sport = cb->dport,
		dport = cb->sport;

	struct tcp_sock *tsk = tcp_sock_lookup_established(saddr, daddr, sport, dport);
	if (!tsk)
		tsk = tcp_sock_lookup_listen(saddr, sport);

	return tsk;
}

// hash tcp sock into bind_table, using sport as the key
static int tcp_bind_hash(struct tcp_sock *tsk)
{
	int bind_hash_value = tcp_hash_function(0, 0, tsk->sk_sport, 0);
	struct list_head *list = &tcp_bind_sock_table[bind_hash_value];
	list_add_head(&tsk->bind_hash_list, list);

	tcp_sock_inc_ref_cnt(tsk);

	return 0;
}

// unhash the tcp sock from bind_table
void tcp_bind_unhash(struct tcp_sock *tsk)
{
	if (!list_empty(&tsk->bind_hash_list)) {
		list_delete_entry(&tsk->bind_hash_list);
		free_tcp_sock(tsk);
	}
}

// lookup bind_table to check whether sport is in use
static int tcp_port_in_use(u16 sport)
{
	int value = tcp_hash_function(0, 0, sport, 0);
	struct list_head *list = &tcp_bind_sock_table[value];
	struct tcp_sock *tsk;
	list_for_each_entry(tsk, list, hash_list) {
		if (tsk->sk_sport == sport)
			return 1;
	}

	return 0;
}

// find a free port by looking up bind_table
static u16 tcp_get_port()
{
	for (u16 port = PORT_MIN; port < PORT_MAX; port++) {
		if (!tcp_port_in_use(port))
			return port;
	}

	return 0;
}

// tcp sock tries to use port as its source port
static int tcp_sock_set_sport(struct tcp_sock *tsk, u16 port)
{
	if ((port && tcp_port_in_use(port)) ||
			(!port && !(port = tcp_get_port())))
		return -1;

	tsk->sk_sport = port;

	tcp_bind_hash(tsk);

	return 0;
}

// hash tcp sock into either established_table or listen_table according to its
// TCP_STATE
int tcp_hash(struct tcp_sock *tsk)
{
	struct list_head *list;
	int hash;

	if (tsk->state == TCP_CLOSED)
		return -1;

	if (tsk->state == TCP_LISTEN) {
		hash = tcp_hash_function(0, 0, tsk->sk_sport, 0);
		list = &tcp_listen_sock_table[hash];
	}
	else {
		int hash = tcp_hash_function(tsk->sk_sip, tsk->sk_dip, \
				tsk->sk_sport, tsk->sk_dport); 
		list = &tcp_established_sock_table[hash];

		struct tcp_sock *tmp;
		list_for_each_entry(tmp, list, hash_list) {
			if (tsk->sk_sip == tmp->sk_sip &&
					tsk->sk_dip == tmp->sk_dip &&
					tsk->sk_sport == tmp->sk_sport &&
					tsk->sk_dport == tmp->sk_dport)
				return -1;
		}
	}

	list_add_head(&tsk->hash_list, list);
	tcp_sock_inc_ref_cnt(tsk);

	return 0;
}

// unhash tcp sock from established_table or listen_table
void tcp_unhash(struct tcp_sock *tsk)
{
	if (!list_empty(&tsk->hash_list)) {
		list_delete_entry(&tsk->hash_list);
		free_tcp_sock(tsk);
	}
}

// XXX: skaddr here contains network-order variables
int tcp_sock_bind(struct tcp_sock *tsk, struct sock_addr *skaddr)
{
	int err = 0;

	// omit the ip address, and only bind the port
	err = tcp_sock_set_sport(tsk, ntohs(skaddr->port));

	return err;
}

// connect to the remote tcp sock specified by skaddr
//
// XXX: skaddr here contains network-order variables
// 1. initialize the four key tuple (sip, sport, dip, dport);
// 2. hash the tcp sock into bind_table;
// 3. send SYN packet, switch to TCP_SYN_SENT state, wait for the incoming
//    SYN packet by sleep on wait_connect;
// 4. if the SYN packet of the peer arrives, this function is notified, which
//    means the connection is established.
int tcp_sock_connect(struct tcp_sock *tsk, struct sock_addr *skaddr) //struct sock_addr *skaddr :server's skaddr
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	// 1. initialize the four key tuple (sip, sport, dip, dport);
	int sport=0;
	if( !(sport = tcp_get_port()) )
		return -1;
	tsk->sk_sport = sport;
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	tsk->sk_sip = iface->ip;
	tsk->sk_dport = ntohs(skaddr->port);
	tsk->sk_dip = ntohl(skaddr->ip);

	//2. hash the tcp sock into bind_table;
	tcp_bind_hash(tsk);

	// 3. send SYN packet, switch to TCP_SYN_SENT state, wait for the incoming 
	//    SYN packet by sleep on wait_connect;
	tcp_set_retrans_timewait_timer(tsk);
	tcp_send_control_packet(tsk, TCP_SYN);
	tcp_set_state(tsk, TCP_SYN_SENT);
	if( tcp_hash(tsk)<0 )
		return -1;
	
	// 4. if the SYN packet of the peer arrives, this function is notified, which
	//    means the connection is established.
	//谁来wake_ip? //void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
	if (sleep_on(tsk->wait_connect) >= 0){ //if success,return 0;else -1;
		return 0;
	}
	return -1;
}

// set backlog (the maximum number of pending connection requst), switch the
// TCP_STATE, and hash the tcp sock into listen_table
int tcp_sock_listen(struct tcp_sock *tsk, int backlog)
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	tsk->iss = tcp_new_iss();
	tsk->snd_una = tsk->iss;
	tsk->snd_nxt = tsk->iss;
	
	tsk->backlog = backlog;
	tcp_set_state(tsk, TCP_LISTEN);
	if( tcp_hash(tsk)>=0 )
		return 0;

	return -1;
}

// check whether the accept queue is full
inline int tcp_sock_accept_queue_full(struct tcp_sock *tsk)
{
	if (tsk->accept_backlog >= tsk->backlog) {
		log(ERROR, "tcp accept queue (%d) is full.", tsk->accept_backlog);
		return 1;
	}

	return 0;
}

// push the tcp sock into accept_queue
inline void tcp_sock_accept_enqueue(struct tcp_sock *tsk)
{
	if (!list_empty(&tsk->list))
		list_delete_entry(&tsk->list);
	list_add_tail(&tsk->list, &tsk->parent->accept_queue);
	tsk->parent->accept_backlog += 1;
}

// pop the first tcp sock of the accept_queue
inline struct tcp_sock *tcp_sock_accept_dequeue(struct tcp_sock *tsk)
{
	struct tcp_sock *new_tsk = list_entry(tsk->accept_queue.next, struct tcp_sock, list);
	list_delete_entry(&new_tsk->list);
	init_list_head(&new_tsk->list);
	tsk->accept_backlog -= 1;

	return new_tsk;
}

// check whether the accept queue is empty
int tcp_sock_accept_queue_empty(struct tcp_sock *tsk)
{
	if (tsk->accept_backlog == 0) {
		//log(ERROR, "tcp accept queue (%d) is empty.", tsk->accept_backlog);
		return 1;
	}

	return 0;
}

// if accept_queue is not emtpy, pop the first tcp sock and accept it,
// otherwise, sleep on the wait_accept for the incoming connection requests
struct tcp_sock *tcp_sock_accept(struct tcp_sock *tsk)
{
	//fprintf(stdout, "TODO: implement this function please.\n");

	if(tcp_sock_accept_queue_empty(tsk)){
		int sleep_result = sleep_on(tsk->wait_accept);
		if(sleep_result<0){
			return NULL;
		}
	}
	struct tcp_sock *csk = tcp_sock_accept_dequeue(tsk);
	if(tcp_hash(tsk)>=0)
		return csk;
	return NULL;
}

// clear the listen queue, which is carried out when *close* the tcp sock
static void tcp_sock_clear_listen_queue(struct tcp_sock *tsk)
{
	struct tcp_sock *lsn_tsk;
	while (!list_empty(&tsk->listen_queue)) {
		lsn_tsk = list_entry(tsk->listen_queue.next, struct tcp_sock, list);
		list_delete_entry(&lsn_tsk->list);

		if (lsn_tsk->state == TCP_SYN_RECV) {
			lsn_tsk->parent = NULL;
			tcp_unhash(lsn_tsk);
			free_tcp_sock(lsn_tsk);
		}
	}
}

// close the tcp sock, by releasing the resources, sending FIN/RST packet
// to the peer, switching TCP_STATE to closed
void tcp_sock_close(struct tcp_sock *tsk)
{
	switch (tsk->state) {
		case TCP_CLOSED:
			break;
		case TCP_LISTEN:
			tcp_sock_clear_listen_queue(tsk);
			tcp_unhash(tsk);
			tcp_set_state(tsk, TCP_CLOSED);
			break;
		case TCP_SYN_RECV:
			break;
		case TCP_SYN_SENT:
			break;
		case TCP_ESTABLISHED:
			tcp_set_retrans_timewait_timer(tsk);
			tcp_send_control_packet(tsk, TCP_FIN|TCP_ACK);
			tcp_set_state(tsk, TCP_FIN_WAIT_1);
			break;
		case TCP_CLOSE_WAIT:
			tcp_set_retrans_timewait_timer(tsk);
			tcp_send_control_packet(tsk, TCP_FIN|TCP_ACK);
			tcp_set_state(tsk, TCP_LAST_ACK);
			break;
	}
}

int tcp_sock_read(struct tcp_sock *tsk, char *buf, int len){
	static int times = 0;
	while( ring_buffer_empty(tsk->rcv_buf) ){
		if( sleep_on(tsk->wait_recv)<0 ){
			tcp_stop_retrans_timewait_timer(tsk);
			return -1;
		}
	}
	int rlen = read_ring_buffer(tsk->rcv_buf, buf, len); //with mutex inside it
	wake_up(tsk->wait_recv);
	tcp_stop_retrans_timewait_timer(tsk);
	return rlen;
}

#define MAX_TCP_DATA_LEN ((0x1<<16) -IP_BASE_HDR_SIZE -TCP_BASE_HDR_SIZE )
#define NORMAL_TCP_DATA_LEN (1024)

int tcp_sock_write(struct tcp_sock *tsk, char *buf, int len){
	int remain_len = len; //剩余未发送长度
	int offset = 0;
	tcp_set_retrans_timewait_timer(tsk); //打开重传计时器
	//发送数据
	while(remain_len>0){
		//int send_len = min(remain_len, MAX_TCP_DATA_LEN);
		int send_len = min(remain_len, NORMAL_TCP_DATA_LEN);
		while(trans_buf.size + send_len > tsk->snd_wnd){ //超过容量，先等待
			if(sleep_on(tsk->wait_send)<0){
				tcp_stop_retrans_timewait_timer(tsk);
				return -1;
			}
		}
		tcp_send_data(tsk, buf+offset, send_len );
		offset     += send_len;
		remain_len -= send_len;
	}
	while(trans_buf.size>0){ //还有剩余的数据未被ack
		if(sleep_on(tsk->wait_send)<0)
			return -1;
	}
	tcp_stop_retrans_timewait_timer(tsk); //关闭重传计时器
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// tcp send buf
//////////////////////////////////////////////////////////////////////////////

struct tcp_buf_block* new_tcp_buf_block(){
	struct tcp_buf_block* block = (struct tcp_buf_block*)malloc(sizeof(struct tcp_buf_block));
	if(block==NULL)
		return NULL;
	init_list_head(&block->list);
	block->len = 0;
	block->packet = NULL;
	return block;
}

void init_tcp_trans_buf(){
	init_list_head(&trans_buf.list);
	pthread_create(&trans_buf.thread_retrans_timer, NULL, tcp_retrans_timer_thread, NULL);
	pthread_mutex_init(&trans_buf.lock, NULL);
	trans_buf.size = 0;
}

//三次重发未被ack，连接超时，结束连接。把发送缓冲区的包释放掉
void buf_remove_and_free_all_block(){
	struct tcp_buf_block *block,*block_q;
	list_for_each_entry_safe(block, block_q, &trans_buf.list, list){
		pthread_mutex_lock(&trans_buf.lock);
		list_delete_entry(&block->list);
		pthread_mutex_unlock(&trans_buf.lock);
		free(block->packet);
		free(block);
	}
	trans_buf.size = 0;
}

//重传定时器触发时:重传snd_buffer中第一个数据包,(定时器数值翻倍)
void buf_retrans_first(struct tcp_sock *tsk){
	if(list_empty(&trans_buf.list))
		return;
	struct tcp_buf_block *first_block = list_entry(trans_buf.list.next,struct tcp_buf_block, list);
	char* packet = (char*)malloc(first_block->len);
	memcpy(packet, first_block->packet, first_block->len);
	//////////////////
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = (struct tcphdr *)((char *)ip + IP_BASE_HDR_SIZE);

	tcp->ack = htonl(tsk->rcv_nxt);
	tcp->checksum = tcp_checksum(ip, tcp);
	ip->checksum = ip_checksum(ip);	
	//////////////////
	ip_send_packet(packet, first_block->len);
}//todo

//发送新的数据时：放到snd_buffer队尾，(打开定时器)
void buf_append_packet(char* packet,int len){
	char* packet_copy = (char*)malloc(len);
	memcpy(packet_copy, packet, len);
	struct tcp_buf_block* block = new_tcp_buf_block();
	block->len = len;
	block->packet = packet_copy;
	int tcp_data_len = len- ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE; //控制包size为0
	//加入buf中
	pthread_mutex_lock(&trans_buf.lock);
	trans_buf.size += tcp_data_len;
	list_add_tail(&block->list, &trans_buf.list);
	pthread_mutex_unlock(&trans_buf.lock);
}

//收到新的ACK时：将snd_buffer中seq_end <= ack的数据包移除，并更新定时器
void buf_ack(struct tcp_sock *tsk, u32 ack){
	struct tcp_buf_block *block,*block_q;
	list_for_each_entry_safe(block, block_q, &trans_buf.list, list){
		struct iphdr *ip = packet_to_ip_hdr(block->packet);
		struct tcphdr *tcp = (struct tcphdr *)((char *)ip + IP_BASE_HDR_SIZE);
		int ip_tot_len = block->len - ETHER_HDR_SIZE;
		int tcp_data_len = ip_tot_len - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE;

		u32 seq = ntohl(tcp->seq);
		//if(less_or_equal_32b(seq, ack)){
		//if( (less_than_32b(seq, ack) && (tsk->retrans_timer.retrans_times==0)) || 
			//(less_or_equal_32b(seq, ack) && (tsk->retrans_timer.retrans_times>0)) ){
		if( (less_than_32b(seq, ack)) ){
			pthread_mutex_lock(&trans_buf.lock);
			trans_buf.size -= tcp_data_len;
			list_delete_entry(&block->list);
			pthread_mutex_unlock(&trans_buf.lock);

			free(block->packet);
			free(block);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// tcp recv out of order buf
//////////////////////////////////////////////////////////////////////////////

//乱序到达的packet有序进入优先级缓存队列。seq小的离头近
void ofo_packet_enqueue(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet){
	//create ofo block
	struct tcp_ofo_block* new_ofo_block = (struct tcp_ofo_block*)malloc(sizeof(struct tcp_ofo_block));
	new_ofo_block->seq = cb->seq;
	//new_ofo_block->ack = cb->ack;
	new_ofo_block->len = cb->pl_len;
	new_ofo_block->data = (char*)malloc(cb->pl_len);
	char* data_start = packet +ETHER_HDR_SIZE +IP_BASE_HDR_SIZE +TCP_BASE_HDR_SIZE;
	memcpy(new_ofo_block->data, data_start, cb->pl_len);
	//free(packet);

	//insert block into ofo_list
	int is_inserted = 0;
	struct tcp_ofo_block *block, *block_q;
	list_for_each_entry_safe(block, block_q, &tsk->rcv_ofo_list, list){
		if(less_than_32b(new_ofo_block->seq , block->seq)){
			list_add_tail(&new_ofo_block->list, &block->list);
			is_inserted = 1;
			break;
		}
	}
	if(!is_inserted){ //the greatest
		list_add_tail(&new_ofo_block->list, &tsk->rcv_ofo_list);
	}
}

//连续的数据块可以进入recv_buf等待read.返回连续确认的最大序列号
int ofo_packet_dequeue(struct tcp_sock *tsk){
	u32 seq = tsk->rcv_nxt;
	struct tcp_ofo_block *block, *block_q;
	list_for_each_entry_safe(block, block_q, &tsk->rcv_ofo_list, list){
		//a data block in order
		if((seq == block->seq)){
			//wait for read
			//while(block->len + ring_buffer_free(tsk->rcv_buf) > tsk->rcv_wnd){
			while(block->len > ring_buffer_free(tsk->rcv_buf) ){
				fprintf(stdout, "s ");
				if(sleep_on(tsk->wait_recv)<0){
					return -1;
				}
				fprintf(stdout, "w ");
			}
			write_ring_buffer(tsk->rcv_buf, block->data, block->len);
			wake_up(tsk->wait_recv);
			//free block
			seq += block->len;
			tsk->rcv_nxt = seq;
			//tsk->snd_una = block->ack;
			list_delete_entry(&block->list);
			free(block->data);
			free(block);
			continue;
		}
		else if(less_than_32b(seq, block->seq)){ //still lack some packet
			break;
		}
		else{ //packet seq error
			return -1;
		}
	}
	return 0;
}

//实现以下函数：
struct tcp_sock *alloc_tcp_sock();
int tcp_sock_bind(struct tcp_sock *, struct sock_addr *);
int tcp_sock_listen(struct tcp_sock *, int);
int tcp_sock_connect(struct tcp_sock *, struct sock_addr *);
struct tcp_sock *tcp_sock_accept(struct tcp_sock *);
void tcp_sock_close(struct tcp_sock *);


struct tcp_sock {
	// sk_ip, sk_sport, sk_sip, sk_dport are the 4-tuple that represents a connection
	struct sock_addr local;
	struct sock_addr peer;
#define sk_sip local.ip
#define sk_sport local.port
#define sk_dip peer.ip
#define sk_dport peer.port

	struct tcp_sock *parent; //指向父socket的指针。bind和listen端口的是父sock，accept时是子sock
    int ref_cnt; //sock被引用的次数。如果减到0就释放此sock

	struct list_head hash_list; //把sock hash进listen_table or established_table
	struct list_head bind_hash_list; //把sock hash进bind_table

	struct list_head listen_queue; //sock收到 SYN包时，malloc一个子sock用来处理后面的连接，子sock先放在listen_queue里
	struct list_head accept_queue; //三次握手收到到最后的ACK包时，listen_queue中的子sock被移入accept_queue
                                   //等待当前tcp（父tcp）调用accept

#define TCP_MAX_BACKLOG 128
	int accept_backlog; //accept_queue当前的子sock数量
	int backlog; //accept_queue最大许可容量

	struct list_head list; //在父sock的listen_queue 或 accept_queue中连接用的node
	struct tcp_timer timewait; //TCP_TIME_WAIT状态下计时用

	//各种需要用到的信号量
	struct synch_wait *wait_connect;
	struct synch_wait *wait_accept;
	struct synch_wait *wait_recv;
	struct synch_wait *wait_send;

	
	struct ring_buffer *rcv_buf;// receiving buffer
	
	int state;// tcp state, see enum tcp_state in tcp.h
	
	u32 iss;// initial sending sequence number
	
	u32 snd_una;// the highest byte that is ACKed by peer
	u32 snd_nxt;// the highest byte sent
	
	u32 rcv_nxt;// the highest byte ACKed by itself (i.e. the byte expected to receive next)

	u16 snd_wnd;// the size of sending window (i.e. the receiving window advertised by peer)
	u16 rcv_wnd;// the size of receiving window (advertised by tcp sock itself)
};

可以利用的函数：
1.用这个宏可以从sock.list得到sock的地址
// get the *real* node from the list node
#define list_entry(ptr, type, member)   (type *)((char *)ptr - offsetof(type, member))

2.计算hash值：
// tcp hash function: if hashed into bind_table or listen_table, only use sport; 
// otherwise, use all the 4 arguments
static inline int tcp_hash_function(u32 saddr, u32 daddr, u16 sport, u16 dport)
3.u32 tcp_new_iss()//随机产生一个新的初始序列号iss
4.//用来比较序列号的宏：
#define less_or_equal_32b(a, b) (((int32_t)(a)-(int32_t)(b)) <= 0)
#define less_than_32b(a, b) (((int32_t)(a)-(int32_t)(b)) < 0)
#define greater_or_equal_32b(a, b) (((int32_t)(a)-(int32_t)(b)) >= 0)
#define greater_than_32b(a, b) (((int32_t)(a)-(int32_t)(b)) > 0)

5.void tcp_copy_flags_to_str(u8 flags, char buf[32]) //将flag以人可读的形式转换为数组
6.
void tcp_sock_inc_ref_cnt(struct tcp_sock *tsk);//每次调用将tsk->ref_cnt +1
void free_tcp_sock(struct tcp_sock *tsk)//每次调用将tsk->ref_cnt -1.如果减为0，将这个tsk以及其内部的结构都释放
7.发tcp控制包：例：tcp_send_control_packet(tsk, TCP_FIN|TCP_ACK)













////////////////////////////////////////////////////////////
tcp发包函数（cp_send_packet,tcp_send_control_packet）的使用者，在调用发包函数前：
tcp_set_retrans_timewait_timer(tsk);
注：如果发的是ack或rst包，不set

发包函数里面：tcp_update_retrans_timewait_timer(tsk);

控制包收到ack时，以及write离开时： tcp_stop_retrans_timewait_timer(tsk);
////////////////////////////////////////////////////////////
#include "stp.h"

#include "base.h"
#include "ether.h"
#include "utils.h"
#include "types.h"
#include "packet.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <signal.h>

#define get_switch_id(switch_id) (int)(switch_id & 0xFFFF)
#define get_port_id(port_id) (int)(port_id & 0xFF)

stp_t *stp;

const u8 eth_stp_addr[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x01 };

static bool stp_is_root_switch(stp_t *stp)
{
	return stp->designated_root == stp->switch_id;
}

static bool stp_port_is_designated(stp_port_t *p)
{
	return p->designated_switch == p->stp->switch_id &&
		p->designated_port == p->port_id;
}

static const char *stp_port_state(stp_port_t *p)
{
	if (p->stp->root_port && \
			p->port_id == p->stp->root_port->port_id)
		return "ROOT";
	else if (p->designated_switch == p->stp->switch_id &&
		p->designated_port == p->port_id)
		return "DESIGNATED";
	else
		return "ALTERNATE";
}

static void stp_port_send_packet(stp_port_t *p, void *stp_msg, int msg_len)
{
	int pkt_len = ETHER_HDR_SIZE + LLC_HDR_SIZE + msg_len;
	char *pkt = malloc(pkt_len);

	// ethernet header
	struct ether_header *eth = (struct ether_header *)pkt;
	memcpy(eth->ether_dhost, eth_stp_addr, 6);
	memcpy(eth->ether_shost, p->iface->mac, 6);
	eth->ether_type = htons(pkt_len - ETHER_HDR_SIZE);

	// LLC header
	struct llc_header *llc = (struct llc_header *)(pkt + ETHER_HDR_SIZE);
	llc->llc_dsap = LLC_DSAP_SNAP;
	llc->llc_ssap = LLC_SSAP_SNAP;
	llc->llc_cntl = LLC_CNTL_SNAP;

	memcpy(pkt + ETHER_HDR_SIZE + LLC_HDR_SIZE, stp_msg, msg_len);

	iface_send_packet(p->iface, pkt, pkt_len);
}

static void stp_port_send_config(stp_port_t *p)
{
	stp_t *stp = p->stp;
	bool is_root = stp_is_root_switch(stp);
	if (!is_root && !stp->root_port) {
		return;
	}

	struct stp_config config;
	memset(&config, 0, sizeof(config));
	config.header.proto_id = htons(STP_PROTOCOL_ID);
	config.header.version = STP_PROTOCOL_VERSION;
	config.header.msg_type = STP_TYPE_CONFIG;
	config.flags = 0;
	config.root_id = htonll(stp->designated_root);
	config.root_path_cost = htonl(stp->root_path_cost);
	config.switch_id = htonll(stp->switch_id);
	config.port_id = htons(p->port_id);
	config.msg_age = htons(0);
	config.max_age = htons(STP_MAX_AGE);
	config.hello_time = htons(STP_HELLO_TIME);
	config.fwd_delay = htons(STP_FWD_DELAY);

	// log(DEBUG, "port %s send config packet.", p->port_name);
	stp_port_send_packet(p, &config, sizeof(config));
}

static void stp_send_config(stp_t *stp)
{
	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *p = &stp->ports[i];
		if (stp_port_is_designated(p)) {
			stp_port_send_config(p);
		}
	}
}

static void stp_handle_hello_timeout(void *arg)
{
	// log(DEBUG, "hello timer expired, now = %llx.", time_tick_now());

	stp_t *stp = arg;
	stp_send_config(stp);
	stp_start_timer(&stp->hello_timer, time_tick_now());
}

static void stp_port_init(stp_port_t *p)
{
	stp_t *stp = p->stp;

	p->designated_root = stp->designated_root;
	p->designated_switch = stp->switch_id;
	p->designated_port = p->port_id;
	p->designated_cost = stp->root_path_cost;
}

void *stp_timer_routine(void *arg)
{
	while (true) {
		long long int now = time_tick_now();

		pthread_mutex_lock(&stp->lock);

		stp_timer_run_once(now);

		pthread_mutex_unlock(&stp->lock);

		usleep(100);
	}

	return NULL;
}

int config_cmp_port_port(stp_port_t *p1,stp_port_t *p2){

	u64 p1_root = p1->designated_root,
		p2_root = p2->designated_root;
	u32 p1_cost = p1->designated_cost,
		p2_cost = p2->designated_cost;
	u64 p1_switch = get_switch_id(p1->designated_switch),
		p2_switch = get_switch_id(p2->designated_switch);
	u16 p1_port = get_port_id(p1->designated_port),
		p2_port = get_port_id(p2->designated_port);


	if(p1_root != p2_root)
		return (p1_root < p2_root)?1:-1;
	else if(p1_cost != p2_cost)
		return (p1_cost < p2_cost)?1:-1;
	else if(p1_switch != p2_switch)
		return (p1_switch < p2_switch)?1:-1;
	else if(p1_port != p2_port)
		return (p1_port < p2_port)?1:-1;
	else
		return 0;
}

/*
	config.root_id = htonll(stp->designated_root);
	config.root_path_cost = htonl(stp->root_path_cost);
	config.switch_id = htonll(stp->switch_id);
	config.port_id = htons(p->port_id);
	
*/
int config_cmp_port_msg(stp_port_t *p1,struct stp_config *config){
	u64 config_root_id = ntohll(config->root_id);
	u32 config_root_path_cost = ntohl(config->root_path_cost);
	u64 config_switch_id = get_switch_id(ntohll(config->switch_id));
	u16 config_port_id = get_port_id(ntohs(config->port_id));

	u64 p1_root = p1->designated_root;
	u32 p1_cost = p1->designated_cost;
	u64 p1_switch = get_switch_id(p1->designated_switch);
	u16 p1_port = get_port_id(p1->designated_port);

	if(p1_root != config_root_id)
		return (p1_root < config_root_id)?1:-1;
	else if(p1_cost != config_root_path_cost)
		return (p1_cost < config_root_path_cost)?1:-1;
	else if(p1_switch != config_switch_id)
		return (p1_switch < config_switch_id)?1:-1;
	else if(p1_port != config_port_id)
		return (p1_port < config_port_id)?1:-1;
	else
		return 0;
}


int config_cmp_port_stp(stp_port_t *p1,stp_t *stp){
	u64 stp_root_id = stp->designated_root;
	u32 stp_root_path_cost = stp->root_path_cost;
	u64 stp_switch_id = get_switch_id(stp->switch_id);
	u16 stp_port_id = get_port_id(p1->port_id);

	u64 p1_root = p1->designated_root;
	u32 p1_cost = p1->designated_cost;
	u64 p1_switch = get_switch_id(p1->designated_switch);
	u16 p1_port = get_port_id(p1->designated_port);

	if(p1_root != stp_root_id)
		return (p1_root < stp_root_id)?1:-1;
	else if(p1_cost != stp_root_path_cost)
		return (p1_cost < stp_root_path_cost)?1:-1;
	else if(p1_switch != stp_switch_id)
		return (p1_switch < stp_switch_id)?1:-1;
	else if(p1_port != stp_port_id)
		return (p1_port < stp_port_id)?1:-1;
	else
		return 0;
}

//把指定端口变为非指定端口
void undesigned_port(stp_port_t *p,struct stp_config *config){
	p->designated_root   = ntohll(config->root_id);
	p->designated_cost   = ntohl(config->root_path_cost);
	p->designated_switch = ntohll(config->switch_id);
	p->designated_port   = ntohs(config->port_id);
}


//节点收到优先级更高的config时，更新节点状态以及各个port
static void stp_update(stp_t *stp, struct stp_config *config){
	stp->root_port = NULL;
	stp_port_t *max_port = &stp->ports[0];
	
	//找到优先级最高端口
	/*for(int i=0; i<stp->nports ; ++i){
		if(config_cmp_port_port(&stp->ports[i],max_port) >0 ){
			max_port = &stp->ports[i];
		}
	}
	if( !stp_port_is_designated(max_port) ){
		stp->root_port = max_port;
	}
	*/

	for(int i=0; i<stp->nports ; ++i){
		if(		(stp->root_port!=NULL && !stp_port_is_designated(&stp->ports[i]) && config_cmp_port_port(&stp->ports[i],stp->root_port) >0) \
			||  (stp->root_port==NULL && !stp_port_is_designated(&stp->ports[i])) )
			stp->root_port = &stp->ports[i];
	}

	if(stp->root_port == NULL){ //意味着是根节点
		stp->designated_root = stp->switch_id;
		stp->root_path_cost  = 0;
	}
	else{
		stp->designated_root = stp->root_port->designated_root;
		stp->root_path_cost  = stp->root_port->designated_cost + stp->root_port->path_cost;
		//课件上是stp->root_path_cost = root_port+designated_cost + root->path_cost
	}
	//更新port状态
	
	for(int i=0; i<stp->nports ; ++i){
		if(stp_port_is_designated(&stp->ports[i])){
			stp->ports[i].designated_root = stp->designated_root;
			stp->ports[i].designated_cost = stp->root_path_cost;
		}
		
		else if(config_cmp_port_stp(&stp->ports[i],stp)<0){
			stp_port_t *p=&stp->ports[i];
			p->designated_root = stp->designated_root;
			p->designated_cost = stp->root_path_cost ;
			p->designated_switch = stp->switch_id;
			p->designated_port = p->port_id;
		}
		
	}
	/*
	for(int i=0; i<stp->nports ; ++i){
		stp->ports[i].designated_root = stp->designated_root;
		stp->ports[i].designated_cost = stp->root_path_cost;
	}*/
	
}
static void stp_handle_config_packet(stp_t *stp, stp_port_t *p,
		struct stp_config *config)
{
	// TODO: handle config packet here
	//fprintf(stdout, "TODO: handle config packet here.\n");
	//因为调用该函数的外层函数已经加锁了，所以此处不用再加锁
	if(config_cmp_port_msg(p,config)<0 ){
		int used_to_be_root = stp_is_root_switch(stp);
		undesigned_port(p,config);
		stp_update(stp,config);
		if(used_to_be_root)
			stp_stop_timer(&stp->hello_timer);
		stp_send_config(stp); //不知道是否需要
	}
	else{
		stp_port_send_config(p);
	}
}

static void stp_dump_state()
{
	pthread_mutex_lock(&stp->lock);

	bool is_root = stp_is_root_switch(stp);
	if (is_root) {
		log(INFO, "this switch is root."); 
	}
	else {
		log(INFO, "non-root switch, desinated root: %04x, root path cost: %d.", \
				get_switch_id(stp->designated_root), stp->root_path_cost);
	}

	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *p = &stp->ports[i];
		log(INFO, "port id: %02d, role: %s.", get_port_id(p->port_id), \
				stp_port_state(p));
		log(INFO, "\tdesignated ->root: %04x, ->switch: %04x, " \
				"->port: %02d, ->cost: %d.", \
				get_switch_id(p->designated_root), \
				get_switch_id(p->designated_switch), \
				get_port_id(p->designated_port), \
				p->designated_cost);
	}

	pthread_mutex_unlock(&stp->lock);
}

static void stp_handle_signal(int signal)
{
	if (signal == SIGTERM) {
		log(DEBUG, "received SIGTERM, terminate this program.");
		
		stp_dump_state();

		exit(0);
	}
}

void stp_init(struct list_head *iface_list)
{
	stp = malloc(sizeof(*stp));

	// set switch ID
	u64 mac_addr = 0;
	iface_info_t *iface = list_entry(iface_list->next, iface_info_t, list);
	for (int i = 0; i < sizeof(iface->mac); i++) {
		mac_addr <<= 8;
		mac_addr += iface->mac[i];
	}
	stp->switch_id = mac_addr | ((u64) STP_BRIDGE_PRIORITY << 48);

	stp->designated_root = stp->switch_id;
	stp->root_path_cost = 0;
	stp->root_port = NULL;

	stp_init_timer(&stp->hello_timer, STP_HELLO_TIME, \
			stp_handle_hello_timeout, (void *)stp);

	stp_start_timer(&stp->hello_timer, time_tick_now());

	stp->nports = 0;
	list_for_each_entry(iface, iface_list, list) {
		stp_port_t *p = &stp->ports[stp->nports];

		p->stp = stp;
		p->port_id = (STP_PORT_PRIORITY << 8) | (stp->nports + 1);
		p->port_name = strdup(iface->name);
		p->iface = iface;
		p->path_cost = 1;

		stp_port_init(p);

		// store stp port in iface for efficient access
		iface->port = p;

		stp->nports += 1;
	}

	pthread_mutex_init(&stp->lock, NULL);
	pthread_create(&stp->timer_thread, NULL, stp_timer_routine, NULL);

	signal(SIGTERM, stp_handle_signal);
}

void stp_destroy()
{
	pthread_kill(stp->timer_thread, SIGKILL);

	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *port = &stp->ports[i];
		port->iface->port = NULL;
		free(port->port_name);
	}

	free(stp);
}

void stp_port_handle_packet(stp_port_t *p, char *packet, int pkt_len)
{
	stp_t *stp = p->stp;

	pthread_mutex_lock(&stp->lock);
	
	// protocol insanity check is omitted
	struct stp_header *header = (struct stp_header *)(packet + ETHER_HDR_SIZE + LLC_HDR_SIZE);

	if (header->msg_type == STP_TYPE_CONFIG) {
		stp_handle_config_packet(stp, p, (struct stp_config *)header);
	}
	else if (header->msg_type == STP_TYPE_TCN) {
		log(ERROR, "TCN packet is not supported in this lab.\n");
	}
	else {
		log(ERROR, "received invalid STP packet.\n");
	}

	pthread_mutex_unlock(&stp->lock);
}


#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "packet.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the hash table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	//fprintf(stderr, "[arpcache_lookup] * TODO: lookup ip address in arp cache.\n");
	//return 0;
	pthread_mutex_lock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_lookup]  get lock.\n");
	int find = 0;
	for(int i=0; i<MAX_ARP_SIZE ; ++i){
		if(arpcache.entries[i].valid!=0 && arpcache.entries[i].ip4 == ip4){
			memcpy(mac,arpcache.entries[i].mac,ETH_ALEN);
			find = 1;
			break;
		}
		else
			continue;
	}
	pthread_mutex_unlock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_lookup]  release lock.\n\n");
	return find;
}

// append the packet to arpcache
//
// Lookup in the hash table which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	fprintf(stderr, "[arpcache_append_packet] * TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	pthread_mutex_lock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_append_packet]  get lock\n");
	struct arp_req *p_arp_req,*q_arp_req;
	struct cached_pkt *p_cached_pkt,*q_cached_pkt;
	//查找等待序列中是否有这个ip
	int find_ip = 0;
	list_for_each_entry_safe(p_arp_req, q_arp_req, &(arpcache.req_list), list){
		if( p_arp_req->ip4 == ip4){
			find_ip = 1;
			break;
		}
	}
	//如果没查到，添加这个ip的节点
	if(!find_ip){
		p_arp_req = (struct arp_req*)malloc(sizeof(struct arp_req));
		if(!p_arp_req){
			exit(0);
		}
		init_list_head(&p_arp_req->list);
		p_arp_req->iface = (iface_info_t *)malloc(sizeof(iface_info_t));
    	memcpy(p_arp_req->iface, iface, sizeof(iface_info_t));
    	p_arp_req->ip4 = ip4;
		p_arp_req->sent = 0;
		p_arp_req->retries = 0;
		init_list_head(&p_arp_req->cached_packets);

		list_add_tail(&p_arp_req->list, &arpcache.req_list);
	}

	//将packet插入到对应的ip下面
	p_cached_pkt = (struct cached_pkt*)malloc(sizeof(struct cached_pkt));
	if(!p_cached_pkt){
		exit(0);
	}
	init_list_head(&p_cached_pkt->list);
	p_cached_pkt->packet = packet;
	p_cached_pkt->len = len;
	list_add_tail(&p_cached_pkt->list,&p_arp_req->cached_packets);
	
	//发送arp request
	arp_send_request(iface, ip4);
	p_arp_req->sent = time(NULL);
	p_arp_req->retries += 1;

	pthread_mutex_unlock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_append_packet]  release lock\n\n");
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	fprintf(stderr, "[arpcache_insert] * TODO: insert ip->mac entry, and send all the pending packets.\n");
	pthread_mutex_lock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_insert]  get lock\n");
	//查找是否有无效项。如果没有，随机算一个
	int index = 0;
	for(; index<MAX_ARP_SIZE; ++index){
		if(!arpcache.entries[index].valid)
			break;
		else
			continue;
	}
	if(index == MAX_ARP_SIZE){
		index = rand() % MAX_ARP_SIZE;
	}
	//查找这个ip对应的缓存序列
	int find=0;
	struct arp_req *p_arp_req, *q_arp_req;
	list_for_each_entry_safe(p_arp_req,q_arp_req,&(arpcache.req_list),list){
		if(ip4 == p_arp_req->ip4){
			find = 1;
			break;
		}
	}
	if(find){
		memcpy(arpcache.entries[index].mac, mac, ETH_ALEN);
		arpcache.entries[index].ip4 = ip4;
		arpcache.entries[index].valid = 1;
		arpcache.entries[index].added = time(NULL);

		//将ip缓存队列下的packet发出去，并释放此缓存队列
		struct cached_pkt *pkt_entry, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(p_arp_req->cached_packets), list) {
			pthread_mutex_unlock(&arpcache.lock);
			//fprintf(stderr, "    [arpcache_insert]  release lock {send cache}\n");
			iface_send_packet_by_arp(p_arp_req->iface, ip4, pkt_entry->packet, pkt_entry->len);
			//ip_send_packet(pkt_entry->packet, pkt_entry->len);
			pthread_mutex_lock(&arpcache.lock);
			//fprintf(stderr, "    [arpcache_insert]  get lock {finish send cache}\n");
			list_delete_entry(&pkt_entry->list);
			free(pkt_entry);
		}
		//释放此缓存队列
		free(p_arp_req->iface);
		list_delete_entry(&(p_arp_req->list));
		free(p_arp_req);
	}
	pthread_mutex_unlock(&arpcache.lock);
	//fprintf(stderr, "    [arpcache_insert]  release lock\n\n");
	
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
	struct arp_req *req_entry, *req_q;
	struct cached_pkt *pkt_entry, *pkt_q;

	while (1) {
		sleep(1);
		//fprintf(stderr, "[arpcache_sweep] *  TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");
		pthread_mutex_lock(&arpcache.lock);
		//fprintf(stderr, "    [arpcache_sweep]  get lock {start sweep}\n");
		
		//删去超时的entry
		for(int index = 0; index < MAX_ARP_SIZE; index++){
			if( time(NULL) - arpcache.entries[index].added > ARP_ENTRY_TIMEOUT && arpcache.entries[index].valid){
				arpcache.entries[index].valid = 0;
				fprintf(stderr, "    [arpcache_sweep]  entry out of time .\n");
			}
			else{
				continue;
			}
		}
		//处理缓存packet序列
		list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
			//如果缓存packet序列超时未收到arp request，则删除packet并发出icmp
			if(req_entry->retries >= ARP_REQUEST_MAX_RETRIES){
				fprintf(stderr, "    [arpcache_sweep]  delete cache .\n");
				
				//pthread_mutex_unlock(&arpcache.lock);
				list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list){
					pthread_mutex_unlock(&arpcache.lock);
					//fprintf(stderr, "    [arpcache_sweep]  release lock {start icmp_send_packet}\n");
					fprintf(stderr, "    [arpcache_sweep]  icmp ICMP_HOST_UNREACH.\n");
					icmp_send_packet(pkt_entry->packet, pkt_entry->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
					pthread_mutex_lock(&arpcache.lock);
					//fprintf(stderr, "    [arpcache_sweep]  get lock {finish icmp_send_packet}\n");
					free(pkt_entry->packet);
					list_delete_entry(&pkt_entry->list);
					free(pkt_entry);
				}
				//pthread_mutex_lock(&arpcache.lock);
				
				free(req_entry->iface);
				list_delete_entry(&(req_entry->list));
				free(req_entry);
			}
			//如果未超时，继续发arp request
			else{
				fprintf(stderr, "    [arpcache_sweep]  cache arp request .\n");
				arp_send_request(req_entry->iface, req_entry->ip4);
				req_entry->sent = time(NULL);
				req_entry->retries += 1;
			}
			
		}
		pthread_mutex_unlock(&arpcache.lock);
		//fprintf(stderr, "    [arpcache_sweep]  release lock {finis sweep}\n\n");
		
	}

	return NULL;
}

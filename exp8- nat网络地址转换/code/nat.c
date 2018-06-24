#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
//当源地址为内部地址，且目的地址为外部地址时，方向为DIR_OUT
//当源地址为外部地址，且目的地址为external_iface地址时，方向为DIR_IN
static int get_packet_direction(char *packet)
{
	fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	u32 saddr = ntohl(ip_hdr->saddr),
		daddr = ntohl(ip_hdr->daddr);

	rt_entry_t *sentry = longest_prefix_match(saddr),
			   *dentry = longest_prefix_match(daddr);
	
	if(sentry && dentry){
		if( strcmp(sentry->iface->name,nat.internal_iface->name)==0 &&\
			strcmp(dentry->iface->name,nat.external_iface->name)==0 )
			return DIR_OUT;
		if( strcmp(sentry->iface->name,nat.external_iface->name)==0 &&\
			strcmp(dentry->iface->name,nat.external_iface->name)==0 )
			return DIR_IN;
	}
	return DIR_INVALID;
}

u16 assign_external_port(){
	static u16 port=1;
	int find=0;
	int i=0;
	for(i=0 ; i<65536 ; ++i){
		if((port+i)%65536 == 0)
			continue;
		if( nat.assigned_ports[(port+i)%65536]==0 ){
			find = 1;
			break;
		}
	}
	if(find){
		port = (port+i)%65536;
		nat.assigned_ports[port] = 1;
		return port;
	}
	else
		return -1;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
/*
如果为DIR_OUT方向数据包且没有对应连接映射（SYN）
	saddr = external_iface->ip;
	sport = assign_external_port();
	建立连接映射关系
	(internal_ip, internal_port) <-> (external_ip, external_port)
	前者为私网地址和Port，后者为映射后的公网地址和Port
其他合法数据包
	查找映射关系，进行(internal_ip, internal_port) <-> (external_ip, external_port)之间的转换
	更新IP/TCP数据包头部字段(包括校验和)
*/
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	pthread_mutex_lock(&nat.lock);
	fprintf(stdout, "TODO: do translation for this packet.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	struct tcphdr *tcp_hdr = packet_to_tcp_hdr(packet);

	int direction = get_packet_direction(packet);
	iface_info_t *iface_send=NULL;
	switch(direction){
		case DIR_OUT:{
			//hash对应的链表
				u32 daddr = ntohl(ip_hdr->daddr);
				u8 key = hash8((char*)&daddr,sizeof(daddr));
				struct list_head *head = &nat.nat_mapping_list[key];
				struct nat_mapping *mapping_entry, *q;
				int find=0;
				//查找是否有对应的映射
				list_for_each_entry_safe(mapping_entry, q, head, list) {
					if(mapping_entry->external_ip == ntohl(ip_hdr->daddr)){
						find = 1;
						break;
					}
				}
				//如果没有对应映射，要新建映射表项
				if(!find){
					struct nat_mapping *new_mapping = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
					new_mapping->internal_ip = ntohl(ip_hdr->saddr);
					new_mapping->external_ip = ntohl(ip_hdr->daddr);
					new_mapping->internal_port = ntohs(tcp_hdr->sport);
					new_mapping->external_port = assign_external_port();
					new_mapping->update_time = 0;
					memset(&new_mapping->conn,0,sizeof(struct nat_connection));
					list_add_tail(&new_mapping->list, &mapping_entry->list);
					mapping_entry = new_mapping;
				}
				//更新映射
				mapping_entry->update_time = 0;
				int fin = tcp_hdr->flags & 0x01; //TCP_FIN
				int ack = tcp_hdr->flags & 0x10; //TCP_ACK
				int rst = tcp_hdr->flags & 0x04; //TCP_RST
				if(fin)
					mapping_entry->conn.internal_fin = 1;
				if(ack)
					mapping_entry->conn.internal_ack = 1;
				if(rst){
					mapping_entry->conn.internal_fin = 1;
					mapping_entry->conn.internal_ack = 1;
					mapping_entry->conn.external_fin = 1;
					mapping_entry->conn.external_ack = 1;
				}
				//修改packet
				ip_hdr->saddr = htonl(nat.external_iface->ip);
				tcp_hdr->sport = htons(mapping_entry->external_port);
				tcp_hdr->checksum = tcp_checksum(ip_hdr,tcp_hdr);
				ip_hdr->checksum = ip_checksum(ip_hdr);
				ip_send_packet(packet,len);
			}
			break;
		case DIR_IN:{
			//hash对应的链表
				u32 saddr = ntohl(ip_hdr->saddr);
				u8 key = hash8((char*)&saddr,sizeof(saddr));
				struct list_head *head = &nat.nat_mapping_list[key];
				struct nat_mapping *mapping_entry, *q;
				int find=0;
				//查找是否有对应的映射
				list_for_each_entry_safe(mapping_entry, q, head, list) {
					if(mapping_entry->external_ip == ntohl(ip_hdr->saddr)){
						find = 1;
						break;
					}
				}
				
				if(!find){
					break;
				}
				//更新映射
				mapping_entry->update_time = 0;
				int fin = tcp_hdr->flags & 0x01; //TCP_FIN
				int ack = tcp_hdr->flags & 0x10; //TCP_ACK
				int rst = tcp_hdr->flags & 0x04; //TCP_RST
				if(fin)
					mapping_entry->conn.external_fin = 1;
				if(ack)
					mapping_entry->conn.external_ack = 1;
				if(rst){
					mapping_entry->conn.external_fin = 1;
					mapping_entry->conn.external_ack = 1;
					mapping_entry->conn.internal_fin = 1;
					mapping_entry->conn.internal_ack = 1;
				}
				//修改packet
				ip_hdr->daddr = htonl(mapping_entry->internal_ip);
				tcp_hdr->dport = htons(mapping_entry->internal_port);
				tcp_hdr->checksum = tcp_checksum(ip_hdr,tcp_hdr);
				ip_hdr->checksum = ip_checksum(ip_hdr);
				ip_send_packet(packet,len);
			}
			break;
		default:
			break;
		
	}
	pthread_mutex_unlock(&nat.lock);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
/*
Port号是NAT设备中的宝贵资源
	NAT设备的一个公网地址最多支持65536个并发连接
	对于已经结束的连接，可以收回已分配的Port号，释放连接映射资源

对认为已经结束的连接进行老化操作
	双方都已发送FIN且回复相应ACK的连接，一方发送RST包的连接，可以直接回收
	双方已经超过60秒未传输数据的连接，认为其已经传输结束，可以回收
*/
void *nat_timeout()
{
	while (1) {
		pthread_mutex_lock(&nat.lock);
		fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		
		for (int i = 0; i < HASH_8BITS; i++) {
			struct list_head *head = &nat.nat_mapping_list[i];
			struct nat_mapping *mapping_entry, *q;
			list_for_each_entry_safe(mapping_entry, q, head, list) {
				mapping_entry->update_time += 1;
				int conn_end = 	mapping_entry->conn.external_fin &&\
								mapping_entry->conn.internal_fin &&\
								mapping_entry->conn.external_ack &&\
								mapping_entry->conn.internal_ack ;
				if(mapping_entry->update_time >= 60 || conn_end){
					list_delete_entry(&mapping_entry->list);
				}
			}
		}

		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}

	return NULL;
}

// initialize nat table
void nat_table_init()
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	nat.internal_iface = if_name_to_iface("n1-eth0");
	nat.external_iface = if_name_to_iface("n1-eth1");
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

// destroy nat table
void nat_table_destroy()
{
	pthread_mutex_lock(&nat.lock);

	for (int i = 0; i < HASH_8BITS; i++) {
		struct list_head *head = &nat.nat_mapping_list[i];
		struct nat_mapping *mapping_entry, *q;
		list_for_each_entry_safe(mapping_entry, q, head, list) {
			list_delete_entry(&mapping_entry->list);
			free(mapping_entry);
		}
	}

	pthread_kill(nat.thread, SIGTERM);

	pthread_mutex_unlock(&nat.lock);
}

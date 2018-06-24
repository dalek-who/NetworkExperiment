#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

void send_arp(iface_info_t *iface, u32 dst_ip, struct ether_arp *req_hdr, u16 option){
	int len_packet = ETHER_HDR_SIZE + sizeof(struct ether_arp) ;
	char *packet=(char*)malloc(len_packet);
	if(packet == NULL){
		exit(0);
	}

	struct ether_header *eth_hdr = (struct ether_header*)(packet);
	struct ether_arp	*eth_arp = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	//ether头
	eth_hdr->ether_type = htons(ETH_P_ARP);
	memcpy((char*)eth_hdr->ether_shost, (char*)iface->mac,ETH_ALEN);
	
	//arp头
	eth_arp->arp_hrd = htons(0x01);
	eth_arp->arp_pro = htons(ETH_P_IP);
	eth_arp->arp_hln = ETH_ALEN; //6
	eth_arp->arp_pln = 4;
	eth_arp->arp_op  = htons(option);
	memcpy((char*)eth_arp->arp_sha, (char *)iface->mac, ETH_ALEN);
	eth_arp->arp_spa = htonl(iface->ip);
	//memset((char*)eth_arp->arp_tha, 0, ETH_ALEN); //todo ?
	
	switch(option){
		case ARPOP_REQUEST:
			memset(eth_hdr->ether_dhost, 0xff, ETH_ALEN);
			eth_arp->arp_tpa = htonl(dst_ip);
			memset((char*)eth_arp->arp_tha, 0, ETH_ALEN);
			break;

		case ARPOP_REPLY:
			memcpy(eth_hdr->ether_dhost, req_hdr->arp_sha, ETH_ALEN);
			eth_arp->arp_tpa = req_hdr->arp_spa;
			memcpy(eth_arp->arp_tha, req_hdr->arp_sha, ETH_ALEN);
			break;
	}
	//发送
	iface_send_packet(iface, packet, len_packet);
}

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	fprintf(stderr, "[arp_send_request] * TODO: send arp request when lookup failed in arpcache.\n");
	send_arp(iface, dst_ip, NULL, ARPOP_REQUEST);
	return;
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	fprintf(stderr, "[arp_send_reply] * TODO: send arp reply when receiving arp request.\n");
	send_arp(iface, 0, req_hdr, ARPOP_REPLY);
	return;
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	fprintf(stderr, "[handle_arp_packet] * TODO: process arp packet: arp request & arp reply.\n");
	struct ether_arp *eth_arp = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	u16 op = ntohs(eth_arp->arp_op);
	u32 ip = ntohl(eth_arp->arp_tpa);
	//arp请求
	if( op == ARPOP_REQUEST ){
		if(ip == iface->ip){
			arp_send_reply(iface, eth_arp);
		}
	}
	//arp答复
	else if(op == ARPOP_REPLY){
		if(ip == iface->ip){
			arpcache_insert(ntohl(eth_arp->arp_spa),eth_arp->arp_sha);
		}
	}
	else
		;
	return;
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}

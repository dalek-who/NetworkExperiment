#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
}

typedef struct hdr_t{
	struct ether_header *eth;
	struct iphdr *ip;
	struct mospf_hdr *mospf;
	struct mospf_hello *hello;
	struct mospf_lsu *lsu;
	struct mospf_lsa *lsa;
}hdr_t;

hdr_t packet_analize(char *packet,int type){
	hdr_t hdr;
	hdr.eth = (struct ether_header *)packet;
	hdr.ip  = packet_to_ip_hdr(packet);

	hdr.mospf=(struct mospf_hdr *)(packet+ ETHER_HDR_SIZE+ IP_HDR_SIZE(hdr.ip) );
	if(type == MOSPF_TYPE_HELLO){
		hdr.hello = (struct mospf_hello *)(packet+ ETHER_HDR_SIZE+ IP_HDR_SIZE(hdr.ip)+ sizeof(struct mospf_hdr) );
		hdr.lsu = NULL;
		hdr.lsa = NULL;
	}
	else if(type == MOSPF_TYPE_LSU){
		hdr.hello = NULL;
		hdr.lsu = (struct mospf_lsu *)(packet+ ETHER_HDR_SIZE+ IP_HDR_SIZE(hdr.ip)+ sizeof(struct mospf_hdr) );
		hdr.lsa = (struct mospf_lsa *)(packet+ ETHER_HDR_SIZE+ IP_HDR_SIZE(hdr.ip)+\
										sizeof(struct mospf_hdr)+ sizeof(struct mospf_lsu) );		
	}
	else{
		fprintf(stdout, "no such type %d.\n",type);
		hdr.hello = NULL;
		hdr.lsu = NULL;
		hdr.lsa = NULL;
	}
	return hdr;
}

void print_db(){
	mospf_db_entry_t *db;
	fprintf(stdout, "* * * * * *  ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(instance->router_id));
	fprintf(stdout, "\n");
	list_for_each_entry(db, &mospf_db, list){
		for(int i=0; i<db->nadv ; ++i){
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(db->rid));
			fprintf(stdout, "  ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(db->array[i].subnet));
			fprintf(stdout, "  ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(db->array[i].mask));
			fprintf(stdout, "  ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(db->array[i].rid));
			fprintf(stdout, "\n");
		}
		fprintf(stdout, "- - - - - - - - - - - - - - - - - - \n");
	}
	fprintf(stdout, "======================================\n");
}

void print_nbr(){
	mospf_nbr_t *nbr;
	iface_info_t *iface;
	list_for_each_entry(iface, &instance->iface_list, list){
		list_for_each_entry(nbr, &iface->nbr_list, list){
			fprintf(stdout, "iface->ip: ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(iface->ip));
			fprintf(stdout, "  nbr_id: ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(nbr->nbr_id));
			fprintf(stdout, "  nbr_ip: ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(nbr->nbr_ip));
			fprintf(stdout, "  nbr_mask: ");
			fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(nbr->nbr_mask));
			fprintf(stdout, "\n");
		}
		fprintf(stdout, "- - - - - - - - - - - - - - - - - - \n");
	}
	fprintf(stdout, "======================================\n");
}

void *sending_mospf_hello_thread(void *param)
{
	//fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	int pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + sizeof(struct mospf_hdr) + sizeof(struct mospf_hello);
	hdr_t hdr;
	iface_info_t *iface;
	char *packet;
	
	u8	mac_daddr[ETH_ALEN] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x05};

	while(1){
		sleep(MOSPF_DEFAULT_HELLOINT);//5
		pthread_mutex_lock(&mospf_lock);
		////fprintf(stdout, "*** sending_mospf_hello_thread [get] lock ***\n");

		//boradcast hello
		list_for_each_entry(iface, &instance->iface_list, list){
			packet = (char*)malloc(pkt_len);
			hdr = packet_analize(packet, MOSPF_TYPE_HELLO);
			//fill the packet
			//hello
			mospf_init_hello(hdr.hello, iface->mask);
			//mostf_hdr
			mospf_init_hdr(hdr.mospf,MOSPF_TYPE_HELLO, sizeof(struct mospf_hdr)+sizeof(struct mospf_hello), \
							instance->router_id,instance->area_id);
			hdr.mospf->checksum = mospf_checksum(hdr.mospf);
			//ip
			ip_init_hdr(hdr.ip, iface->ip, MOSPF_ALLSPFRouters, pkt_len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
			//eth
			memcpy(hdr.eth->ether_shost, iface->mac, ETH_ALEN);
			memcpy(hdr.eth->ether_dhost, mac_daddr, ETH_ALEN);
			hdr.eth->ether_type = htons(ETH_P_IP);
			//send packet
			iface_send_packet(iface, packet, pkt_len); //there is free() inside iface_send_packet()
		}
		pthread_mutex_unlock(&mospf_lock);	
		//fprintf(stdout, "*** sending_mospf_hello_thread ((release)) lock ***\n");

	}
	return NULL;
}

//todo
void broadcast_lsu(){
	u32 nadv = 0; //number of neignbors
	iface_info_t *iface;
	mospf_nbr_t *nbr;
	//count neignbor's number
	list_for_each_entry(iface, &instance->iface_list, list){
		nadv += iface->num_nbr;
	}
	
	struct mospf_lsa lsa_buf[nadv];
	int i=0;
	//fill all lsa buf
	list_for_each_entry(iface, &instance->iface_list, list){
		list_for_each_entry(nbr, &(iface->nbr_list), list){
			if(i>=nadv){
				fprintf(stdout, "lsa_buf out of range %d.\n",nadv);
			}
			lsa_buf[i].subnet = htonl(nbr->nbr_ip);
			lsa_buf[i].mask   = htonl(nbr->nbr_mask);
			lsa_buf[i].rid    = htonl(nbr->nbr_id);
			++i;
		}
	}

	int pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + sizeof(struct mospf_hdr)\
				 + sizeof(struct mospf_lsu) + nadv*sizeof(struct mospf_lsa);
	
	//boradcast: each iface send to each nbr
	list_for_each_entry(iface, &instance->iface_list, list){
		list_for_each_entry(nbr, &(iface->nbr_list), list){
			//malloc packet
			char *packet = (char*)malloc(pkt_len);
			hdr_t hdr = packet_analize(packet, MOSPF_TYPE_LSU);
			//fill packet
			//lsa:
			memcpy(hdr.lsa, lsa_buf, nadv*sizeof(struct mospf_lsa));
			//lsu:
			mospf_init_lsu(hdr.lsu, nadv);
			//mostf:
			mospf_init_hdr(hdr.mospf, MOSPF_TYPE_LSU, pkt_len-(ETHER_HDR_SIZE)-(IP_BASE_HDR_SIZE),\
							 instance->router_id, instance->area_id);
			hdr.mospf->checksum = mospf_checksum(hdr.mospf);
			//ip:
			ip_init_hdr(hdr.ip, iface->ip, nbr->nbr_ip, pkt_len-ETHER_HDR_SIZE, IPPROTO_MOSPF);
			//eth: ip_send_packet will fill it
			//send
			ip_send_packet(packet, pkt_len);
			//instance:
			++instance->sequence_num;
			//instance->lsuint = MOSPF_DEFAULT_LSUINT;
			instance->lsuint = 10;
		}
	}

	fprintf(stdout, "router: ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(instance->router_id));
	fprintf(stdout, "  boradcast lsu\n");
	fprintf(stdout, "========================================\n");
	//print_nbr();

}


void *checking_nbr_thread(void *param)
{
	//fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	iface_info_t *iface;
	mospf_nbr_t *nbr,*nbr_q;
	while(1){
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		//fprintf(stdout, "*** checking_nbr_thread [get] lock ***\n");
		//check each nbr under each iface
		list_for_each_entry(iface, &instance->iface_list, list){
			list_for_each_entry_safe(nbr, nbr_q, &(iface->nbr_list), list){
				if(--nbr->alive <=0 ){
					--iface->num_nbr;
					list_delete_entry(&nbr->list);
					broadcast_lsu();
					//todo:delete nbr in database?
					free(nbr);		
				}
			}
		}

		pthread_mutex_unlock(&mospf_lock);
		//fprintf(stdout, "*** checking_nbr_thread ((release)) lock ***\n");
		
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	mospf_nbr_t *nbr;
	hdr_t hdr = packet_analize(packet, MOSPF_TYPE_HELLO);
	u32 rid = ntohl(hdr.mospf->rid);
	
	//look for this neignbor
	int find=0;
	pthread_mutex_lock(&mospf_lock);
	//fprintf(stdout, "*** handle_mospf_hello [get] lock ***\n");
	list_for_each_entry(nbr, &iface->nbr_list, list){
		if(nbr->nbr_id == rid){
			find = 1;
			break;
		}
	}
	//not find:add a new nbr
	if(!find){
		nbr = (mospf_nbr_t*)malloc(sizeof(mospf_nbr_t));
		init_list_head(&nbr->list);
		nbr->nbr_id = ntohl(hdr.mospf->rid);
		nbr->nbr_ip = ntohl(hdr.ip->saddr);
		nbr->nbr_mask = ntohl(hdr.hello->mask);
		list_add_tail(&nbr->list, &iface->nbr_list);
		++iface->num_nbr;
	}
	//update alive
	nbr->alive = MOSPF_DEFAULT_HELLOINT * 3;

	//free(packet);
	pthread_mutex_unlock(&mospf_lock);
	//fprintf(stdout, "*** handle_mospf_hello ((release)) lock ***\n");
	//print_nbr();
}

void *sending_mospf_lsu_thread(void *param)
{
	//fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	
	while(1){
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		//fprintf(stdout, "*** sending_mospf_lsu_thread [get] lock ***\n");
		if(--instance->lsuint <= 0){
			broadcast_lsu();
			//instance->lsuint = MOSPF_DEFAULT_LSUINT;
		}
		pthread_mutex_unlock(&mospf_lock);
		//fprintf(stdout, "*** sending_mospf_lsu_thread ((release)) lock ***\n");
	}
	return NULL;
}

//todo

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	pthread_mutex_lock(&mospf_lock);
	//fprintf(stdout, "*** handle_mospf_lsu [get] lock ***\n");
	hdr_t hdr = packet_analize(packet, MOSPF_TYPE_LSU);
			            
		            
	//look for db
	u32 rid = ntohl(hdr.mospf->rid);
	u32 seq = ntohl(hdr.lsu->seq);
	u32 nadv= ntohl(hdr.lsu->nadv);

	u32 saddr = ntohl(hdr.ip->saddr);
	fprintf(stdout, "router: ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(instance->router_id));
	fprintf(stdout, "  iface: ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(iface->ip));
	fprintf(stdout, "  packet saddr: ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(saddr));
	fprintf(stdout, "  forward lsu from: ");
	fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(rid));		            
	fprintf(stdout, "\n");

	if(rid == instance->router_id){
		pthread_mutex_unlock(&mospf_lock);
		//fprintf(stdout, "*** handle_mospf_lsu [release] lock ***\n");
		return;
	}
	
	mospf_db_entry_t *db;
	int find=0, seq_larger=0;
	list_for_each_entry(db, &mospf_db, list){
		if(db->rid == rid){
			find = 1;
			seq_larger = (seq > db->seq)?1:0;
			break;
		}
	}
	//add new db_entry
	//fprintf(stdout, "find=%d\n",find);
	if(!find){
		db = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		init_list_head(&db->list);
		db->rid = rid;
		db->seq = 0;
		db->array = NULL;
		list_add_tail(&db->list, &mospf_db);
		//fprintf(stdout, "add db\n");
	}
	//copy lsa
	if(!find || seq_larger){
		db->nadv = nadv;
		if(db->array != NULL)
			free(db->array);
		db->array=(struct mospf_lsa*)malloc( nadv*sizeof(struct mospf_lsa) );
		for(int i=0; i<nadv; ++i){
			db->array[i].subnet = ntohl(hdr.lsa[i].subnet);
			db->array[i].mask   = ntohl(hdr.lsa[i].mask  );
			db->array[i].rid    = ntohl(hdr.lsa[i].rid   );
		}
		//fprintf(stdout, "update db\n");
	}
	//forward lsu
	char* fwd_packet;
	iface_info_t *fwd_iface;
	mospf_nbr_t *nbr;
	if(--hdr.lsu->ttl <=0 ){
		//free(packet);
		//return;
	}
	else{
		list_for_each_entry(fwd_iface, &instance->iface_list, list){
			if(fwd_iface == iface)
				continue;
			else{
				//forward lsu
				list_for_each_entry(nbr, &fwd_iface->nbr_list, list){
					fwd_packet = (char*)malloc(len);
					memcpy(fwd_packet, packet, len);
					hdr_t fwd_hdr = packet_analize(fwd_packet, MOSPF_TYPE_LSU);
					//mospf
					fwd_hdr.mospf->checksum = mospf_checksum(fwd_hdr.mospf);
					//ip
					ip_init_hdr(fwd_hdr.ip, fwd_iface->ip, nbr->nbr_ip, len-ETHER_HDR_SIZE, IPPROTO_MOSPF);
		            ip_send_packet(fwd_packet, len);
					fprintf(stdout, "  to: ");
		            fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(nbr->nbr_ip));
		            fprintf(stdout, "\n");
				}
				//fprintf(stdout, "forward lsu\n");
			}
		//free(packet);
		}
	}
	print_db();
	pthread_mutex_unlock(&mospf_lock);
	//fprintf(stdout, "*** handle_mospf_lsu [release] lock ***\n");
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	u32 rid = ntohl(mospf->rid);
	if(mospf->type!=MOSPF_TYPE_HELLO){
		fprintf(stdout, "handle_mospf_packet  rid: ");
		fprintf(stdout, IP_FMT, HOST_IP_FMT_STR(rid));
		fprintf(stdout, "    type: %s",(mospf->type==MOSPF_TYPE_HELLO)?"hello":"lsu");
		fprintf(stdout, "\n");
	}
	
	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}

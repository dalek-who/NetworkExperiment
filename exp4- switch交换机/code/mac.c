#include "mac.h"
#include "headers.h"
#include "log.h"

mac_port_map_t mac_port_map;

void init_mac_hash_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	pthread_mutexattr_init(&mac_port_map.attr);
	pthread_mutexattr_settype(&mac_port_map.attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mac_port_map.lock, &mac_port_map.attr);

	pthread_create(&mac_port_map.tid, NULL, sweeping_mac_port_thread, NULL);
}

void destory_mac_hash_table()
{
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *tmp, *entry;
	for (int i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		if (!entry) 
			continue;

		tmp = entry->next;
		while (tmp) {
			entry->next = tmp->next;
			free(tmp);
			tmp = entry->next;
		}
		free(entry);
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{
	pthread_mutex_lock(&mac_port_map.lock);
	// TODO: implement the lookup process here
	//fprintf(stdout, "lookup_port\n");
	mac_port_entry_t* entry = mac_port_map.hash_table[hash8(mac,ETH_ALEN)];
	if(entry!= NULL){
		for( ; entry!=NULL ; entry=entry->next){
			//查到
			if(memcmp(entry->mac,mac,ETH_ALEN*sizeof(u8))==0){
				entry->visited = time(NULL);
				break;
			}
		}		
	}
	pthread_mutex_unlock(&mac_port_map.lock);
	return (entry==NULL)?NULL:entry->iface;
}

void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	pthread_mutex_lock(&mac_port_map.lock);	
	// TODO: implement the insertion process here
	//fprintf(stdout, "insert_mac_port\n");
	mac_port_entry_t* entry = mac_port_map.hash_table[hash8(mac,ETH_ALEN)];
	if(entry == NULL){
		entry = (mac_port_entry_t*)malloc(sizeof(mac_port_entry_t));
		memcpy(entry->mac,mac,ETH_ALEN*sizeof(u8));
		entry->iface = iface;
		entry->visited = time(NULL);
		entry->next = NULL;
		mac_port_map.hash_table[hash8(mac,ETH_ALEN)] = entry;
	}
	else{
		//沿着链表查找。如果找到，更新时间。如果没找到（查到链表尾部），插入新表
		int find = 0;
		mac_port_entry_t* p=entry;
		mac_port_entry_t* tail = NULL; //链表最后一个非空node
		for(p=entry ; p!=NULL ; p=p->next){
			//查到
			if(memcmp(p->mac,mac,ETH_ALEN*sizeof(u8))==0){
				entry = p;
				entry->visited = time(NULL);
				find = 1;
				break;
			}
			tail = p;
		}
		if(!find){
			entry = (mac_port_entry_t*)malloc(sizeof(mac_port_entry_t));
			memcpy(entry->mac,mac,ETH_ALEN*sizeof(u8));
			entry->iface = iface;
			entry->visited = time(NULL);
			entry->next = NULL;
			tail->next = entry;
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	//fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		while (entry) {
			//fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac), \
					entry->iface->name, (int)(now - entry->visited));

			entry = entry->next;
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

int sweep_aged_mac_port_entry()
{
	// TODO: implement the sweeping process here
	static int i = 0;
	int sleep_time = MAC_PORT_TIMEOUT;
	while(1){
		//fprintf(stdout, "%d sweeping\n",++i);
		pthread_mutex_lock(&mac_port_map.lock);
		mac_port_entry_t* entry;
		for(int i=0; i<HASH_8BITS ; ++i){
			mac_port_entry_t *head,*tail;
			tail=head=mac_port_map.hash_table[i];
			for(entry=mac_port_map.hash_table[i]; entry!=NULL ; ){
				//时间超过30s没访问
				if(time(NULL)-entry->visited >= sleep_time){
					mac_port_entry_t* p;
					p=entry;
					//如果这个entry是头部：需要把hash表项换成下个entry
					if(head == entry){
						tail=head=mac_port_map.hash_table[i]=entry->next;
					}
					//如果不是头部，链表删掉这个节点
					else{
						tail->next = entry->next;
					}
					entry=entry->next;
					free(p);
				}
				//跳过
				else{
					tail = entry;
					entry=entry->next;
				}
			}
		}
		pthread_mutex_unlock(&mac_port_map.lock);
		sleep(sleep_time);
	}
	return 0;
}
/*
table[i]（指针）e1->e2->e3->
*/
void *sweeping_mac_port_thread(void *nil)
{
	while (1) {
		sleep(1);
		int n = sweep_aged_mac_port_entry();

		if (n > 0)
			log(DEBUG, "%d aged entries in mac_port table are removed.\n", n);
	}

	return NULL;
}

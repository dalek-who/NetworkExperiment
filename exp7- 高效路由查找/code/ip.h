#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;


typedef struct route_item_t {
	u32 ip;
	u32 mask_len;
	u32 port;
}route_item_t;

u32 get_ip(int ip[]){
	u32 num = 0;
	int i=0;
	for(i=0; i<4; ++i){
		//printf("%8x %2x %2d %8x %8x\n",num,ip[i],24-8*i,ip[i] << (24-8*i),num |( ip[i] << (24-8*i)));
		num |= ( ip[i] << (24-8*i));
	}
	return num;
}

u32 mask(int len){
	return 0xffffffff << (32-len);
}

route_item_t* fscanf_router(FILE *f){
	int ip[4];
	u32 mask_len;
	route_item_t *route_item = (route_item_t *)malloc(sizeof(route_item_t));
	if(route_item == NULL){
		exit(0);
	}
	if(fscanf(f,"%d.%d.%d.%d %d %d",&ip[0], &ip[1], &ip[2], &ip[3], \
									&mask_len, &route_item->port)!=EOF ){
		route_item->ip = get_ip(ip);
		route_item->mask_len = mask_len;
		return route_item;
	}
	else{
		free(route_item);
		return NULL;
	}
}

//0 <= loacte <=31  left 31 30 ... 2 1 0 right
u32 get_bit(u32 num,int locate){
	if(locate<0 || locate>31){
		printf("error bit locate %d\n",locate);
		return -1;
	}
	else 
		return (u32)( num & (0x1 << locate) ) >>locate ;
}

u32 get_bits(u32 num,int high, int low){
	if(low<0 || high>31){
		printf("error bit high %d, low %d\n",high,low);
		return -1;
	}
	else{ 
		//return (u32)( num & (0x1 << locate) ) >>locate ;
		u32 mask = 0xffffffff;
		u32 bits;

		mask = (signed int)mask >> low;
		mask = mask << low;
		mask = mask << (31-high);
		mask = (unsigned int)mask >> (31-high);
		
		bits = mask & num;
		bits = (unsigned int)bits >>low;
		return bits;
	}
}

u32 get_prefix(route_item_t *router){
	return router->ip & mask(router->mask_len);
}

char* get_ip_human(u32 ip,char* ip_str){
	char buf[5]={0};
	u32 mask[4]={0xff000000,0x00ff0000,0x0000ff00,0x000000ff};
	
	memset(ip_str, 0, 16); //xxx.xxx.xxx.xxx\0
	sprintf(buf,"%d",(u32)(mask[0] & ip)>>24); //gcc doesn't have itoa()
	strcat(ip_str, buf);
	strcat(ip_str, ".");	

	sprintf(buf,"%d",(u32)(mask[1] & ip)>>16);
	strcat(ip_str, buf);
	strcat(ip_str, ".");	

	sprintf(buf,"%d",(u32)(mask[2] & ip)>> 8);
	strcat(ip_str, buf);
	strcat(ip_str, ".");	

	sprintf(buf,"%d",(u32)(mask[3] & ip)>> 0);
	strcat(ip_str, buf);
	strcat(ip_str, "\0");
	
	return ip_str;
}

int route_cmp(route_item_t *router1, route_item_t *router2){
	u32 ip1 = router1->ip & mask(router1->mask_len);
	u32 ip2 = router2->ip & mask(router2->mask_len);

	if(ip1 > ip2){
		return 1;
	}
	else if(ip1 < ip2){
		return -1;
	}
	else{
		return router1->mask_len - router2->mask_len;
	}
}

int ceil_div(int m,int n){
	return (m+n-1)/n;
}
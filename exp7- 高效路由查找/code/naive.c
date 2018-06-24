#include "naive.h"

typedef unsigned char u8;
typedef unsigned int u32;

int main(int argc, char *argv[]){
	
	/*
	FILE *f = fopen("small.txt","r");
 	int ip[4];
	int mask;
	int port;
	
	route_item_t* r;
	char buf[20]={0};
	while((r=fscanf_router(f))){
		printf("ip %8x   mask_len %2d   port %3d\n",r->ip,r->mask_len,r->port);
		get_ip_human(r->ip,buf);
		printf("ip %s\n",buf);
	}
	*/
	/*
	char fi_name[50] = "small.txt",
		 ft_name[50] = "forwarding-table.txt",
		 fo_name[50] = "out.txt";

	for(int i=1; i<argc; ++i){
		if(strcmp(argv[i],"-i")==0){
			strcpy(fi_name,argv[++i]);
		}

		if(strcmp(argv[i],"-t")==0){
			strcpy(ft_name,argv[++i]);
		}

		if(strcmp(argv[i],"-o")==0){
			strcpy(fo_name,argv[++i]);
		}
	}

	tree_test(fi_name,ft_name,fo_name);
	*/
	/*
	unsigned int a;
	while(1){
		scanf("%x",&a);
		printf("%d %x\n",a,a);
	}
	*/
	char subtable[50] = "forwarding-table.txt",
		 match_i[50] = "match_i_3000.txt",
		 match_o[50] = "match_o_3000.txt",
		 notmatch_i[50] = "notmatch_i_3000.txt",
		 notmatch_o[50] = "notmatch_o_3000.txt";

	for(int i=1; i<argc; ++i){
		if(strcmp(argv[i],"-subtable")==0){
			strcpy(subtable,argv[++i]);
		}

		if(strcmp(argv[i],"-match_i")==0){
			strcpy(match_i,argv[++i]);
		}

		if(strcmp(argv[i],"-match_o")==0){
			strcpy(match_o,argv[++i]);
		}

		if(strcmp(argv[i],"-notmatch_i")==0){
			strcpy(notmatch_i,argv[++i]);
		}

		if(strcmp(argv[i],"-notmatch_o")==0){
			strcpy(notmatch_o,argv[++i]);
		}
	}

	tree_test(subtable, match_i, match_o, notmatch_i, notmatch_o);

}
#include <stdio.h>
#include <stdlib.h>
#include "ip.h"

#define CHILD 2 

typedef struct naive_ip_tree_t{
	struct naive_ip_tree_t *subt[CHILD];
	route_item_t* router;
}naive_ip_tree_t;

naive_ip_tree_t* create_node(route_item_t* router){
	naive_ip_tree_t* node = (naive_ip_tree_t*)malloc(sizeof(naive_ip_tree_t));
	if(node == NULL){
		exit(0);
	}
	memset(node, 0, sizeof(naive_ip_tree_t));
	node->router = router;
	return node;
}


naive_ip_tree_t* insert_route(naive_ip_tree_t* tree, route_item_t* router){
	naive_ip_tree_t* t;
	u32 bit=0;
	int i=0;
	for(t=tree,i=0; i< router->mask_len; ++i){
		bit = get_bit(get_prefix(router),31-i);
		if( t->subt[bit]==NULL ){
			t->subt[bit] = create_node(NULL);
		}
		t = t->subt[bit];
	}
	if(t->router == NULL){
		t->router = router;
	}
	return t;
}

/*
route_item_t* longest_prefix_match(naive_ip_tree_t* tree, u32 ip, int mask_len){
	naive_ip_tree_t* t;
	u32 bit=0;
	for(int i=0, t=tree; i< router->mask_len; ++i){
		bit = get_bit(ip & mask(mask_len),31-i);
		t = t->subt[bit];
		if(t==NULL){
			return NULL;
		}
	}
	return t->router;
}
*/
route_item_t* longest_prefix_match(naive_ip_tree_t* tree, u32 ip){
	naive_ip_tree_t *t;
	route_item_t *router=NULL;
	u32 bit=0;
	int i;
	for(i=0, t=tree; i< 32; ++i){
		bit = get_bit(ip,31-i);
		t = t->subt[bit];
		if(!t){
			break;
		}
		else{
			router = (t->router)? t->router: router;
		}
	}
	return router;
}

naive_ip_tree_t* create_tree_by_file(char* fi_name){
	FILE *fi = fopen(fi_name,"r");
	if(!fi){
		printf("no such file: %s\n",fi_name );
		exit(0);
	}
	naive_ip_tree_t* tree = create_node(NULL); //root node without value
	route_item_t* router = NULL;

	while( (router=fscanf_router(fi))!=NULL ){
		insert_route(tree, router);
	}
	fclose(fi);
	return tree;
}

void tree_test(char *table, char *match_i, char *match_o, char *notmatch_i, char *notmatch_o){
	FILE *mi=fopen(match_i,"r"), 
		 *mo=fopen(match_o,"w"), 
		 *nmi=fopen(notmatch_i,"r"), 
		 *nmo=fopen(notmatch_o,"w");

	if(!mi || !mo || !nmi || !nmo){
		printf("no such file: %s\n",(!mi)? match_i: "");
		printf("no such file: %s\n",(!mo)? match_o: "");
		printf("no such file: %s\n",(!nmi)? notmatch_i: "");
		printf("no such file: %s\n",(!nmo)? notmatch_o: "");
		exit(0);
	}

	printf("creating tree ......\n");
	naive_ip_tree_t* tree = create_tree_by_file(table);
	printf("creating tree success\n");
	
	route_item_t *router = NULL, *router_match = NULL;

	char ip_human[20]={0};
	u32 ip;

	printf("testing match ......\n");
	while( (fscanf(mi,"%x",&ip))!= EOF ){
		router_match = longest_prefix_match(tree,ip);
		if(router_match){
			fprintf(mo, "%x\n",ip);
		}
	}

	printf("testing notmatch ......\n");
	while( (fscanf(nmi,"%x",&ip))!= EOF ){
		router_match = longest_prefix_match(tree,ip);
		if(router_match){
			fprintf(nmo, "%x\n",ip);
		}
	}

	printf("testing tree success\n");
	
	fclose(mi);
	fclose(mo);
	fclose(nmi);
	fclose(nmo);
	return;
}

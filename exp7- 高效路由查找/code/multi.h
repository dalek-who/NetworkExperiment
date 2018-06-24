#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "ip.h"

#define BITS 4

typedef struct advance_ip_tree_t{
	struct advance_ip_tree_t *subt[ 0x1<<BITS ];
	route_item_t* router;
}advance_ip_tree_t;

advance_ip_tree_t* create_node(route_item_t* router){
	advance_ip_tree_t* node = (advance_ip_tree_t*)malloc(sizeof(advance_ip_tree_t));
	if(node == NULL){
		exit(0);
	}
	memset(node, 0, sizeof(advance_ip_tree_t));
	node->router = router;
	return node;
}

void insert_route(advance_ip_tree_t* tree, route_item_t* router){

	int insert_deep = ceil_div(router->mask_len,BITS);
	int keys[32/BITS] = {0};
	int i=0;
	advance_ip_tree_t *last_second = NULL, *t = NULL;
	int start_leaf ;
	int leaf_num ;

	//init all keys
	for(i=0; i<32; i+= BITS){
		keys[i/BITS] = get_bits(get_prefix(router),31-i,(31-i-BITS+1)>=0? 31-i-BITS+1: 0 );
	}
	
	//find the second last 
	for(i=0, t= tree; i<insert_deep; ++i){
		last_second = t;
		if(t->subt[keys[i]] == NULL)
			t->subt[keys[i]] = create_node(NULL);
		t = t->subt[keys[i]];
	}

	start_leaf = keys[insert_deep-1];
	leaf_num = 0x1 << (BITS*ceil_div(router->mask_len, BITS) - router->mask_len);

	for(i=start_leaf ; i< start_leaf+ leaf_num ; ++i){
		if(last_second->subt[i]==NULL)
			last_second->subt[i] = create_node(NULL);
		if( last_second->subt[i]->router == NULL || route_cmp(last_second->subt[i]->router,router)<0 )
			last_second->subt[i]->router = router;
	}

	return ;
}
/*
int leaf_push(advance_ip_tree_t *tree, route_item_t *router){
	int replace = 0;
	int is_replaced = 0;
	
	//judge if this is a leaf
	int is_leaf=1;
	int i=0;
	for(i=0 ; i< 0x1<<BITS ; ++i){
		if(tree->subt[i]!=NULL){
			is_leaf = 0;
			break;
		}
	}

	if(is_leaf){
		if(router==NULL || router!=NULL && tree->router!=NULL && route_cmp(router, tree->router)<=0 )
			replace = 0;
		else{//router!=NULL && (tree->router==NULL || tree->router!=NULL && route_cmp(router, tree->router)>0))
			replace = 1;
			tree->router = router;
		}
	}

	else{//not leaf
		is_replaced = 0; //this node is replaced by blow node?
		//int replace:this node(or nodes blow) replace a node passed from parent
		if(router==NULL || router!=NULL && tree->router!=NULL && route_cmp(router, tree->router)<=0){
			replace = 0;
			for(i=0 ; i< 0x1<<BITS ; ++i){
				if(tree->subt[i] == NULL)
					tree->subt[i] = create_node(NULL);
				is_replaced |= leaf_push(tree->subt[i],tree->router);
			}
		}
		else{//router!=NULL && (tree->router==NULL || tree->router!=NULL && route_cmp(router, tree->router)>0))
			is_replaced = 0;
			for(i=0 ; i< 0x1<<BITS ; ++i){
				if(tree->subt[i] == NULL)
					tree->subt[i] = create_node(NULL);
				replace |= leaf_push(tree->subt[i],router);
			}
		}
		if(!is_replaced && tree->router)
			free(tree->router);
	}
	return replace;
}
*/
route_item_t* longest_prefix_match(advance_ip_tree_t* tree, u32 ip){
	int keys[32/BITS] = {0};
	int i=0;
	advance_ip_tree_t *t=NULL, *match_t=NULL;
	route_item_t* router=NULL;

	for(i=0; i<32; i+=BITS){
		keys[i/BITS] = get_bits(ip, 31-i, 31-i-BITS+1);
	}
	
	for(i=0, t=tree ; i<=32 && t!=NULL ; i+=BITS){
		router = (t->router)? t->router: router;
		if(i==32)
			break;
		t = t->subt[keys[i/BITS]];
	}
	
	return router;//could be null
}

advance_ip_tree_t* create_tree_by_file(char* fi_name){
	FILE *fi = fopen(fi_name,"r");
	advance_ip_tree_t* tree = create_node(NULL); //root node without value
	route_item_t* router = NULL;

	if(!fi){
		printf("no such file: %s\n",fi_name );
		exit(0);
	}

	while( (router=fscanf_router(fi))!=NULL ){
		insert_route(tree, router);
	}
	//leaf_push(tree,NULL);

	fclose(fi);
	return tree;
}

int total_size(advance_ip_tree_t* tree){
	long long unsigned int size = 0;
	int i=0;
	if(tree == NULL)
		return 0;
	else{
		size += sizeof(*tree);
		for(i=0; i< 0x1<<BITS ; ++i){
			size += total_size(tree->subt[i]);
		}
		return size;
	}
}

void tree_test(char *table, char *match_i, char *match_o, char *notmatch_i, char *notmatch_o){
	char ip_human[20]={0};
	u32 ip;

	FILE *mi=fopen(match_i,"r"), 
		 *mo=fopen(match_o,"w"), 
		 *nmi=fopen(notmatch_i,"r"), 
		 *nmo=fopen(notmatch_o,"w");

	route_item_t *router = NULL, *router_match = NULL;
	advance_ip_tree_t* tree=NULL;

	struct timeval time_start,time_end;
	double time_all=0,time_avrg=0,timeuse=0;

	int ip_addrs=0;

	if(!mi || !nmi){
		printf("no such file: %s\n",(!mi)? match_i: "");
		//printf("no such file: %s\n",(!mo)? match_o: "");
		printf("no such file: %s\n",(!nmi)? notmatch_i: "");
		//printf("no such file: %s\n",(!nmo)? notmatch_o: "");
		exit(0);
	}

	printf("creating tree ......\n");
	tree = create_tree_by_file(table);
	printf("creating tree success\n");
	

	printf("testing match ......\n");
	while( (fscanf(mi,"%x",&ip))!= EOF ){
		gettimeofday(&time_start,NULL);
		router_match = longest_prefix_match(tree,ip);
		gettimeofday(&time_end,NULL);
		timeuse = 1000000*(time_end.tv_sec - time_start.tv_sec) + time_end.tv_usec -time_start.tv_usec;
		time_all += timeuse;
		ip_addrs += 1;
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
	
	printf("******************************************************************************\n");
	printf("space: %d MB\n", total_size(tree)/(1024*1024));
	printf("time:  total: %lf us, average: %lf us\n", time_all, (double)time_all/ip_addrs);
	printf("******************************************************************************\n");
	
	fclose(mi);
	fclose(mo);
	fclose(nmi);
	fclose(nmo);
	return;
}

#include "advance.h"

typedef unsigned char u8;
typedef unsigned int u32;

int main(int argc, char *argv[]){
	
	/*
	char subtable[50] = "./test_file/subtable_30.txt",
		 match_i[50] = "./test_file/match_i_30.txt",
		 match_o[50] = "./test_file/match_o_30.txt",
		 notmatch_i[50] = "./test_file/notmatch_i_30.txt",
		 notmatch_o[50] = "./test_file/notmatch_o_30.txt";
*/
	char subtable[50] = "./test_file/smalltable.txt",
		 match_i[50] = "./test_file/match_i_10.txt",
		 match_o[50] = "./test_file/match_o_10.txt",
		 notmatch_i[50] = "./test_file/notmatch_i_10.txt",
		 notmatch_o[50] = "./test_file/notmatch_o_10.txt";
	int i=1;
	for(i=1 ; i<argc; ++i){
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
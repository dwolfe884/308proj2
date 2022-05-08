#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#define MAX_CMD_LEN 100
#include "Bank.h"

void* dojob();

int numacc, numthread;
char outfile[100];
FILE *fp;
pthread_cond_t newjob;
//pthread_mutex_t *accountlocks;
pthread_mutex_t filelock;
pthread_mutex_t entire;

typedef struct {    // structure for a transaction pair
	int acc_id;    // account ID
	int amount;  // amount to be added, could be positive or negative
	}trans;
typedef struct request{
	struct request * next;  // pointer to the next request in the list
	int request_id;      // request ID assigned by the main thread
	int check_acc_id;  // account ID for a CHECK request
	trans * transactions; // array of transaction data
	int num_trans;     // number of accounts in this transaction
	struct timeval starttime, endtime; // starttime and endtime for TIME
	}request;
typedef struct queue{
	request * head, * tail; // head and tail of the list
	int num_jobs;               // number of jobs currently in queue
}queue;

//GLOBALS
queue q;
pthread_mutex_t  mut;
pthread_cond_t   producer_cv;
pthread_cond_t   consumer_cv;
request * head;
request * tail;
int job_count = 0;
int run = 1;
//GLOBALS

int main(int argc, char **argv){
	q.num_jobs = 0;
	if(argc != 4){
		printf("%s","Error: Incorrect arguments\n");
		printf("%s","server <# of worker threads> <# of accounts> <output file>\n");
		exit(1);
	}
	else{
		numthread = atoi(argv[1]);
		numacc = atoi(argv[2]);	
		strcpy(outfile,argv[3]);
	}
	
	int exited = 0;
	char cmd[MAX_CMD_LEN];
    fp = fopen(outfile,"w");
	
	//Init mutex array
	// TODO:
	pthread_mutex_init(&mut, NULL);
	pthread_cond_init(&producer_cv, NULL);
	//Init mutex array

	//Init threads array
	initialize_accounts(numacc);
	/*int dumb = 0;
	for(dumb=0;dumb<numacc;dumb++){
		printf("%d\n",read_account(dumb));
	}*/
	pthread_t threads[numthread];
	int t;
	for(t=0;t<numthread;t++){
		pthread_create(&threads[t], NULL, (void*)&dojob, NULL);
	}
	//Init threads array
	//Init array of mutexs for accounts
	/*
	accountlocks = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * numacc);
	for(t=0; t<numacc; t++){
		pthread_mutex_init(&accountlocks[t],0);
	}*/
	pthread_mutex_init(&entire, NULL);
	//Init array of mutexs for accounts
	pthread_mutex_init(&filelock,NULL);
	int id = 1;
	while(!exited){
		printf("%s","> ");
		fgets(cmd,MAX_CMD_LEN,stdin);
		cmd[strcspn(cmd,"\n\r")] = 0;
		char **arglist;
		char *ret;
		int tmpi = 0;	
		int argcount = 1;
		//Get string up to the first space
		ret = strchr(cmd, ' ');
		//While there is more data to process
		while(ret != NULL){
			//Count number of arguments
			argcount++;
			ret = strchr(ret+1, ' ');	
		}
		char *last = cmd;
		char *curr;
		arglist = malloc( (argcount * sizeof(char *))+2 );
		request* r = malloc(sizeof(request));
		gettimeofday(&r->starttime, NULL);
        for(tmpi=0; tmpi<argcount; tmpi++){
			int size;
			//Malloc a space in the array for the new string
			arglist[tmpi] = (char *)malloc(100);
			curr = strchr(last,' ');
			if(curr == NULL){
				size = strlen(last);
			}
			else{
				size = (int)(curr - last);
			}
			//Copy the string into the array
			strncpy(arglist[tmpi],last,size);
			last = curr+1;
		}
		arglist[argcount] = NULL;
		if(strcmp("END",arglist[0]) == 0){
			exited = 1;
		}
		else if(strcmp("CHECK",arglist[0]) == 0){
			//request* r = malloc(sizeof(request));
			if(argcount != 2){
				printf("Error\n");
			}
			else{
				int bal;
				int accountid = atoi(arglist[1]);
				//ADD TO QUEUE
				pthread_mutex_lock(&mut);
				r->check_acc_id = accountid;
				r->request_id = id;
				//gettimeofday(&r->starttime, NULL);
				// ## ENQUEUE ##
				if(q.num_jobs == 0){
					q.head = r;
					head = r;
					q.head->next = NULL;
					head->next = NULL;
					//printf("new head: %d\n",q.head->request_id);
					//q.tail = &r;
					//q.head->next = q.tail;
				}
				else if(q.num_jobs == 1){
					q.tail = r;
					//q.tail->next = &r;
					tail = r;
					head->next = tail;
					q.head->next = q.tail;
					//printf("new tail: %d\n",q.tail->request_id);
				}
				else{
					q.tail->next = r; // [h] -> [n1] -> [n2] -> [t] -> [r]
					q.tail = r;	   // [h] -> [n1] -> [n2] -> [n3] -> [t]
					tail->next = r;
					tail = r;/*
					printf("same head: %d\n",q.head->request_id);
					printf("same head no q: %d\n",head->request_id);
					printf("same next: %d\n",q.head->next->request_id);
					printf("new tail: %d\n",q.tail->request_id);*/
				}
				// ## ENQUEUE ##
				pthread_cond_signal(&newjob);
				pthread_mutex_unlock(&mut);
				//ADD TO QUEUE
				printf("< ID %d\n",id);
				q.num_jobs++;
				id++;
			}
		}
		else if(strcmp("TRANS",arglist[0]) == 0){
			//request* r = malloc(sizeof(request));
			r->num_trans = 0;
			r->check_acc_id = -1;
			// struct timeval stime;
			// struct timeval etime;
			// r->starttime = stime;
			// r->endtime = etime;
			//gettimeofday(&r->starttime, NULL);
			if(argcount < 3){
				printf("Error\n");
			}
			else{
				int m;
				int failed = 0;
				int accid;
				for(m=1; m<argcount-1;m+=2){
					r->num_trans++;
				}
				r->transactions = malloc(sizeof(trans)*r->num_trans);
				int fakem = 0;
				for(m=1; m<argcount-1;m+=2){
					accid = atoi(arglist[m]);
					int transbal = atoi(arglist[m+1]);
					//trans ts[] = malloc();
					trans t;
					t.acc_id = accid;
					t.amount = transbal;
					r->transactions[fakem] = t;
					fakem++;
				}
				//gettimeofday(&r->starttime, NULL);
				//ADD TO QUEUE
				pthread_mutex_lock(&mut);
				r->request_id = id;
				//r->starttime = stime;
				if(q.num_jobs == 0){
					q.head = r;
					head = r;
					q.head->next = NULL;
					head->next = NULL;
					//printf("new head: %d\n",q.head->request_id);
					//q.tail = &r;
					//q.head->next = q.tail;
				}
				else if(q.num_jobs == 1){
					q.tail = r;
					//q.tail->next = &r;
					tail = r;
					head->next = tail;
					q.head->next = q.tail;
					//printf("new tail: %d\n",q.tail->request_id);
				}
				else{
					q.tail->next = r; // [h] -> [n1] -> [n2] -> [t] -> [r]
					q.tail = r;	   // [h] -> [n1] -> [n2] -> [n3] -> [t]
					tail->next = r;
					tail = r;/*
					printf("same head: %d\n",q.head->request_id);
					printf("same head no q: %d\n",head->request_id);
					printf("same next: %d\n",q.head->next->request_id);
					printf("new tail: %d\n",q.tail->request_id);*/
				}
				q.num_jobs++;
				pthread_cond_broadcast(&newjob);
				pthread_mutex_unlock(&mut);
				printf("< ID %d\n",id);
				id++;
			}
		}
	}
	run = 0;
	pthread_mutex_unlock(&mut);
	printf("WORKING IS NOW DONE. END PROGRAM\n");
	while(q.num_jobs){
		usleep(1);
	}
	printf("ALL JOBS HAVE FINISHED\n");
	int fakei;
	for(fakei=0;fakei<numthread;fakei++){
		pthread_join(threads[fakei],NULL);
	}
	printf("THREAD HAVE ENDED");
	fclose(fp);
}

void* dojob(){
	//printf("%s","Get good kid\n");
	while(run || q.num_jobs){
		pthread_mutex_lock(&mut);
		while(q.num_jobs == 0){
			pthread_cond_wait(&newjob,&mut);
		}
		//printf("Out herhe\n");
		if(q.num_jobs > 0){
			request j;
			j = *q.head;
			q.head = q.head->next;
			q.num_jobs--;
			
			if (q.num_jobs == 0){
				//q.tail = NULL;
				//q.head =NULL;
			}
			//struct timeval stime;
			//struct timeval etime;
			//j.starttime = stime;
			//j.endtime = etime;
			//gettimeofday(&xttime, NULL);
			printf("job id: %d\n",j.request_id);
			pthread_mutex_unlock(&mut);
			//struct timeval etime;
			if(j.check_acc_id != -1){
				//pthread_mutex_lock(&accountlocks[j.check_acc_id-1]);
				pthread_mutex_lock(&entire);
				int bal = read_account(j.check_acc_id);
				pthread_mutex_unlock(&entire);
				//pthread_mutex_unlock(&accountlocks[j.check_acc_id-1]);
				//struct timeval etime;
				gettimeofday(&j.endtime, NULL);
				fprintf(fp, "%d BAL %d TIME %ld.%06ld %ld.%06ld\n",j.request_id,bal,j.starttime.tv_sec, j.starttime.tv_usec, j.endtime.tv_sec, j.endtime.tv_usec);
			}
			else{
				int m;
				int failed = 0;
				int accid;

				//SORT TRANSACTION ARRAY
				int minaccid = 99999;
				int toswap = -1;
				trans tmp;
				int unsorted = 1;
				int currlocation = 0;
				while(unsorted){
					for(m=currlocation; m<j.num_trans;m++){
						if(j.transactions[m].acc_id < minaccid){
							minaccid = j.transactions[m].acc_id;
							toswap = m;
							//printf("New min id: %d\n",minaccid);
						}
					}
					if(toswap <= 0 || minaccid == 99999){
						unsorted = 0;
					}
					else{
						//printf("min id: %d\n",minaccid);
						tmp = j.transactions[currlocation];
						j.transactions[currlocation] = j.transactions[toswap];
						j.transactions[toswap] = tmp;
						currlocation++;	
						minaccid = 99999;			
					}
				}
				int num_good = 0;
				trans* good_trans = malloc(sizeof(trans)*j.num_trans);
				pthread_mutex_lock(&entire);
				for(m=0; m<j.num_trans;m+=1){
					//pthread_mutex_lock(&accountlocks[j.transactions[m].acc_id-1]);
					int balsum = j.transactions[m].amount;
					while(j.transactions[m].acc_id == j.transactions[m+1].acc_id){
						balsum+=j.transactions[m+1].amount;
						m++;
					}
					int bal = read_account(j.transactions[m].acc_id);
					if(bal+balsum < 0){
						accid = j.transactions[m].acc_id;
						failed = 1;
						break;
					}
					trans* sumed = malloc(sizeof(trans));
					sumed->acc_id = j.transactions[m].acc_id;
					//printf("Trans init bal: %d\n",bal);
					sumed->amount = bal+balsum;
					//printf("Trans sum: %d\n",sumed->amount);
					good_trans[m] = *sumed;
					num_good++;

				}
				if(!failed){
					for(m=0; m<num_good;m+=1){
						write_account(good_trans[m].acc_id,good_trans[m].amount);
						//pthread_mutex_unlock(&accountlocks[accid-1]);
					}
				}
				if(failed){
					gettimeofday(&j.endtime, NULL);
					fprintf(fp,"%d ISF %d TIME %ld.%06ld %ld.%06ld\n",j.request_id,accid,j.starttime.tv_sec, j.starttime.tv_usec, j.endtime.tv_sec, j.endtime.tv_usec);
				}
				else{
					gettimeofday(&j.endtime, NULL);
					fprintf(fp,"%d OK TIME %ld.%06ld %ld.%06ld\n",j.request_id,j.starttime.tv_sec, j.starttime.tv_usec, j.endtime.tv_sec, j.endtime.tv_usec);
					// pthread_mutex_unlock(&filelock);
				}/*
				for(m=0;m<j.num_trans;m++){
					pthread_mutex_unlock(&accountlocks[j.transactions[m].acc_id-1]);
				}*/
				pthread_mutex_unlock(&entire);
			}	
		}
		else{
			pthread_mutex_unlock(&mut);
		}
	}
	//return;
}
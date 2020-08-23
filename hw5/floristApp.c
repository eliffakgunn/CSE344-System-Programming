#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h> 
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>

#define MAX(a,b)	(((a)>(b))?(a):(b))

char line[100];
int *florist_id = NULL;

int floristNum=0;
int clientNum=0;
int floristNumber=0;
int clientNumber=0;

char* file = NULL;
int fd_open=-2;
int exit_flag=0;

struct florist{
	char name[40];
	double x_coor;
	double y_coor;
	double click;
	int flowerNum;
	char **flowers; 
	int index;
	int *requests;
	int order;
};

struct client{
	double x_coor;
	double y_coor;
	char flower[40];
	int florist;
};

struct statistic
{
	char name[40];
	int requestNum;
	int totalTime;
};

struct florist *floristArr = NULL;
struct client *clientArr= NULL;
struct statistic *statsArr= NULL;

pthread_mutex_t *mutex= NULL; /*mutex*/
pthread_cond_t *condVar= NULL; /* condition variables for florists */
pthread_t *thread_florist= NULL; /* threads */

static void usageError();
void handler(int signal);
void floristInfos(char arr[]);
void clientInfos(char arr[]);
double calcDistance(double x1, double y1, double x2, double y2);
void* pool(void *id);
int closestFlorist(double x_coor, double y_coor, char flower[]);
void clean();

int main(int argc, char* argv[]){
    file = (char*) malloc(40*sizeof(char)); 
    int option, fd_read, i=0, temp=0, j;
    char ch[1];
  	struct sigaction sa_act;
	sigset_t blocked_signals;

	atexit(clean);

  	/* signal mask for SIGINT */
  	sigemptyset(&blocked_signals);
  	sigaddset(&blocked_signals, SIGINT);
  	/* signal handler for SIGINT */
  	sigemptyset(&sa_act.sa_mask);
  	sa_act.sa_handler = handler;
  	sa_act.sa_flags = 0;
  	if(sigaction (SIGINT, &sa_act, NULL)==-1){
  		perror("Sigaction: ");
		exit(EXIT_FAILURE);
	}

    if (argc != 3)
        usageError();        

    while((option = getopt(argc, argv, "i")) != -1){ /*get option from the getopt() method*/
        switch(option){
            case 'i':
                strcpy(file,argv[optind]);               
                break;            
            default: usageError();
        }
    }

    fd_open = open(file, O_RDONLY,0666);
    if(fd_open == -1){                      
        perror("open: ");
        exit(EXIT_FAILURE);
    } 

    /* read file and find florists's number and clients' number */
    while(1){
        fd_read=read(fd_open, ch, 1);
        if(fd_read==-1){
            perror("read1: ");
            exit(EXIT_FAILURE);
        } 
        
        if(fd_read == 0 || (clientNumber !=0 && line[i] == '\n'))
        	break;

        if(i !=0){
        	if(line[i-1] != '\n'){
        		line[i]=ch[0];
        		if(line[i] == '\n'){
		        	if(temp==0)
		        		++floristNumber;
		        	else
		        		++clientNumber;

        			for(j=0; j<100; ++j) /* flush the line */
        				line[j]='\0';
        			i=0;
        		}else
        			++i;
        	}
        	else{
        		i=0;
        		temp=1;
        	}
    	}
    	else
        	line[i++]=ch[0];  
    }  

    /* memory allocations */
    floristArr = (struct florist*) malloc(floristNumber*sizeof(struct florist)); 
    clientArr = (struct client*) malloc(clientNumber*sizeof(struct client));  
    statsArr = (struct statistic*) malloc(floristNumber*sizeof(struct statistic));  
    florist_id = (int*) malloc(floristNumber*sizeof(int));  

	for(i=0; i<floristNumber; ++i){    
    	floristArr[i].requests= (int*) malloc(clientNumber*sizeof(int)); 
    	floristArr[i].flowers= NULL;

    }

    for(i=0; i<floristNumber; ++i)
    	florist_id[i]=i;
    
    condVar = (pthread_cond_t*) malloc((floristNumber)*sizeof(pthread_cond_t)); 
    thread_florist = (pthread_t*) malloc(floristNumber*sizeof(pthread_t)); 	
    mutex = (pthread_mutex_t*) malloc((floristNumber)*sizeof(pthread_mutex_t)); 	

	/* sets ofset of the file to 0 index */
    if(lseek(fd_open,0,SEEK_SET) == -1){
        perror("lseek: ");
        exit(EXIT_FAILURE);
    }	

    i=0, temp=0; 

    while(1){
        fd_read=read(fd_open, ch, 1);
        if(fd_read==-1){
            perror("read2: ");
            exit(EXIT_FAILURE);
        } 
        
        if(fd_read == 0 || (clientNum !=0 && line[i] == '\n'))
        	break;

        if(i !=0){
        	if(line[i-1] != '\n'){
        		line[i]=ch[0];
        		if(line[i] == '\n'){
		        	if(temp==0){
		        		floristInfos(line);
		        		++floristNum;
		        	}
		        	else{
		        		clientInfos(line);
		        		++clientNum;
		        	}

        			for(j=0; j<100; ++j) /* flush the line */
        				line[j]='\0';

        			i=0;
        		}else
        			++i;
        	}
        	else{
        		i=0;
        		temp=1;
        	}
    	}
    	else
        	line[i++]=ch[0];  
    }    

    for(i=0; i<floristNumber; ++i){
	    floristArr[i].index=0;
	    floristArr[i].order=0;
	    strcpy(statsArr[i].name, floristArr[i].name);
	    statsArr[i].totalTime=0;
	    statsArr[i].requestNum=0;
		for(j=0; j<clientNumber; ++j) /* all requests are -1 at the beginning */
			floristArr[i].requests[j]=-1;
    }

    /* init the conditional variables and mutexs */
	for(i=0; i<floristNumber; ++i){
		if(pthread_cond_init(&condVar[i], NULL) != 0){
			perror("pthread_cond_init: ");
			exit(EXIT_FAILURE);
		}  
		if(pthread_mutex_init(&mutex[i], NULL) != 0){
			perror("pthread_mutex_init: ");
			exit(EXIT_FAILURE);
		}		
	}  

    /* block SIGINT */     
    if(sigprocmask (SIG_BLOCK, &blocked_signals, NULL)==-1){
      perror("Sigprocmask: ");
      exit(EXIT_FAILURE);
    }	 

    /* create threads */  
    for(i=0; i<floristNumber; ++i){
        if(pthread_create(&thread_florist[i], NULL, pool, &florist_id[i]) != 0){
            perror("pthread_create: ");
            exit(EXIT_FAILURE);
        }	
	}  

	printf("Florist application initializing from file: %s\n", file);
	printf("%d florists have been created\n", floristNumber);
	printf("Processing requests\n");

	/* processing the clients’ requests one by one */
    for(i=0; i<clientNumber; ++i){
		for(j=0; j<floristNumber; ++j){
			if(pthread_mutex_lock(&mutex[j]) !=0 ){
				perror("pthread_mutex_lock1: ");
				exit(EXIT_FAILURE);
			}	
			if(j == clientArr[i].florist){		
				floristArr[j].requests[floristArr[j].index]=i;
				++floristArr[j].index;
				++floristArr[j].order;

				if(i == clientNumber-1)
					exit_flag = 1;

				if (pthread_cond_signal(&condVar[j]) !=0 ) {
					if(pthread_mutex_unlock(&mutex[j]) !=0 ){
						perror("pthread_mutex_unlock1: ");
						exit(EXIT_FAILURE);
					} 	
					perror("pthread_cond_signal1: ");
					exit(EXIT_FAILURE);
				} 

				if(pthread_mutex_unlock(&mutex[j]) !=0 ){
					perror("pthread_mutex_unlock2: ");
					exit(EXIT_FAILURE);
				} 	
			}
			else{
				if(pthread_mutex_unlock(&mutex[j]) !=0 ){
					perror("pthread_mutex_unlock3: ");
					exit(EXIT_FAILURE);
				}				
			}
		}
    } 

	/* wake up the conditional waits to finish */
	for(i=0; i<floristNumber; ++i){
		if(pthread_mutex_lock(&mutex[i]) !=0 ){
			perror("pthread_mutex_lock3: ");
			exit(EXIT_FAILURE);
		}		
		if (pthread_cond_signal(&condVar[i]) !=0 ) {
			if(pthread_mutex_unlock(&mutex[i]) !=0 ){
				perror("pthread_mutex_unlock5: ");
				exit(EXIT_FAILURE);
			} 	
			perror("pthread_cond_signal2: ");
			exit(EXIT_FAILURE);
		} 	
		if(pthread_mutex_unlock(&mutex[i]) !=0 ){
			perror("pthread_mutex_unlock6: ");
			exit(EXIT_FAILURE);
		} 			
	}
   
    /* join threads */ 
    for(i=0; i<floristNumber ; i++){
        if(pthread_join(thread_florist[i],(void *) statsArr) != 0){
            perror("pthread_join: ");
            exit(EXIT_FAILURE);
        }
    } 

    for(i=0; i<floristNumber; ++i){
		if(pthread_cond_destroy(&condVar[i]) != 0){
	        perror("pthread_cond_destroy: ");
	        exit(EXIT_FAILURE);
	    }	
		if(pthread_mutex_destroy(&mutex[i]) != 0){
	        perror("pthread_mutex_destroy: ");
	        exit(EXIT_FAILURE);
	    }
    }

    /* unblock the SIGINT */
    if(sigprocmask (SIG_UNBLOCK, &blocked_signals, NULL)==-1){
    	perror("Sigprocmask2: ");
    	exit(EXIT_FAILURE);
    }   

	printf("All requests processed.\n");				
	for(i=0; i<floristNumber; ++i)
		printf("%s closing shop.\n",floristArr[i].name);    

	printf("Sale statistics for today:\n");
	printf("----------------------------------------------------\n");
	printf("Florist\t\t# of sales\t\tTotal time	 \n");
	printf("----------------------------------------------------\n");
	for(i=0; i<floristNumber; ++i)
		printf("%-10s\t%-10d\t\t%dms \n",floristArr[i].name, statsArr[i].requestNum, statsArr[i].totalTime);
	printf("----------------------------------------------------\n");         

    return 0;
}

void handler(int signal){
	if(signal == SIGINT){
		printf("SIGINT is caught. Program is exited.\n");
    	exit(0);
	}
}

void clean(){
	int i, j;
    if(fd_open != -2 && fd_open != -1){
		if(close(fd_open)==-1){
			perror("Close3: ");
			exit(EXIT_FAILURE);
		}
	}
	if(file != NULL)
		free(file);
	if(condVar != NULL)
		free(condVar);
	if(florist_id != NULL)
		free(florist_id);
	if(thread_florist != NULL)
		free(thread_florist);
	if(mutex != NULL)
		free(mutex);

	if(floristArr != NULL){
		for(i=0; i<floristNumber; ++i){
			if(floristArr[i].flowers != NULL){
				for(j=0; j<floristArr[i].flowerNum; ++j){
					if(floristArr[i].flowers[j] != NULL)
						free(floristArr[i].flowers[j]);
				}
				free(floristArr[i].flowers);   
			}
			if(floristArr[i].requests != NULL)
				free(floristArr[i].requests);
		}
		free(floristArr);
	}
	if(clientArr != NULL)
		free(clientArr);
	if(statsArr != NULL)
		free(statsArr);  	
}

/* finds closest florist to client */
int closestFlorist(double x_coor, double y_coor, char flower[]){
	int i, j, florist[floristNum], index;
	double distance[floristNum], min;

	for(i=0; i<floristNum; ++i)
		florist[i]=-1;

	/* which florists have clint's request */
	for(i=0; i<floristNum; ++i){
		for(j=0; j<floristArr[i].flowerNum; ++j)
			if(strcmp(floristArr[i].flowers[j], flower) ==0)
				florist[i]=1;
	}

	/* find distance between florists and client*/
	for(i=0; i<floristNum; ++i){
		if(florist[i]==1)
			distance[i] = calcDistance(floristArr[i].x_coor, floristArr[i].y_coor, x_coor, y_coor);
		else
			distance[i] = 999999999.0;
	}

	min = distance[0];
	index = 0; 

	/* find closest florist*/
	for(i=1; i<floristNum; ++i){
		if(distance[i]<min){
			min = distance[i];
			index = i;
		}
	}

	return index;
}

void* pool(void *id){
	srand(time(0));
	int ind=0, prepTime, deliveryTime, totalTime;

	while(1){
		if(pthread_mutex_lock(&mutex[*(int*)id]) !=0 ){
			perror("pthread_mutex_lock5: ");
			exit(EXIT_FAILURE);
		}	

		/* waits until new request arrive*/
		while(floristArr[*(int*)id].order == 0 && exit_flag == 0){
			if(pthread_cond_wait (&condVar[*(int*)id], &mutex[*(int*)id]) != 0){
				perror("pthread_cond_wait2: ");
				exit(EXIT_FAILURE);
			}				
		}

		/* if main thread gets exit signal */
		if(floristArr[*(int*)id].order == 0 && exit_flag == 1){
			if(pthread_mutex_unlock(&mutex[*(int*)id]) !=0 ){
				perror("pthread_mutex_unlock11: ");
				exit(EXIT_FAILURE);
			}					
			break;
		}

		--floristArr[*(int*)id].order;

		if(pthread_mutex_unlock(&mutex[*(int*)id]) !=0 ){
			perror("pthread_mutex_unlock12: ");
			exit(EXIT_FAILURE);
		}
		
		prepTime=rand() % 250 +1; /* ms */

		deliveryTime = calcDistance( clientArr[floristArr[*(int*)id].requests[ind]].x_coor,clientArr[floristArr[*(int*)id].requests[ind]].y_coor, floristArr[*(int*)id].x_coor, floristArr[*(int*)id].y_coor) / floristArr[*(int*)id].click;

		totalTime = prepTime + deliveryTime;

		statsArr[*(int*)id].totalTime += totalTime; 

		++statsArr[*(int*)id].requestNum;
	
		usleep(totalTime*1000);

		printf("Florist %s has delivered a %s to client%d in %dms\n",floristArr[*(int*)id].name, clientArr[floristArr[*(int*)id].requests[ind]].flower, (floristArr[*(int*)id].requests[ind])+1, totalTime);	

		++ind;

	}
	return (void *)statsArr;
}

static void usageError() {
    printf("Wrong command line arguments!\n");
    printf("Usage: ./program -i filename \n");
    printf(" -i represents the file that contains the list of florists, the type of flowers that they sell and the requests made by clients\n");
    exit(EXIT_FAILURE); 
}

/* gets florist infos */
void floristInfos(char arr[]){
	int i=0, j=0, k, temp, counter=0;
	char name[40];
	char x_coor[15];
	char y_coor[15];
	char click[15];
	char flower[40];

	while(arr[i] != '('){
		name[i]=arr[i];
		++i;
	}
	name[i-1]='\0';

	strcpy(floristArr[floristNum].name,name);

	++i;

	while(arr[i] != ','){
		x_coor[j]=arr[i];
		++i;
		++j;
	}
	x_coor[j]='\0';

	floristArr[floristNum].x_coor=atof(x_coor);

	++i;
	j=0;

	while(arr[i] != ';'){
		y_coor[j]=arr[i];
		++i;
		++j;
	}
	y_coor[j]='\0';

	floristArr[floristNum].y_coor=atof(y_coor);

	++i;
	j=0;

	while(arr[i] != ')'){
		click[j]=arr[i];
		++i;
		++j;
	}
	click[j]='\0';

	floristArr[floristNum].click=atof(click);

	if(floristArr[floristNum].click <= 0){
		printf("%s's speed is %.1lf. It is not possible. Program exiting.\n", floristArr[floristNum].name, floristArr[floristNum].click);
		exit(EXIT_FAILURE);
	}

	temp = i;

	/* finds flower count */
	while(arr[temp] != '\n'){
		if(arr[temp] == ',')
			++counter;
		++temp;
	}

	if(counter != 0){

		floristArr[floristNum].flowerNum=counter+1;
	
		floristArr[floristNum].flowers = (char**)malloc( (counter+1) * sizeof(char *)); 

  		for(j = 0; j < counter+1; ++j)
  		  floristArr[floristNum].flowers[j] = (char*)malloc( 40 * sizeof(char));

		i += 4;
		j=0;

		for(k=0; k<counter+1; ++k){
			if(k == counter){
				while(arr[i] != '\n'){
					flower[j]=arr[i];
					++i;
					++j;
				}
				flower[j]='\0';	

				strcpy(floristArr[floristNum].flowers[k],flower);
			}
			else{
				while(arr[i] != ','){
					flower[j]=arr[i];
					++i;
					++j;
				}
				flower[j]='\0';	

				strcpy(floristArr[floristNum].flowers[k],flower);

				for(j=0; j<40; ++j)
					flower[j]='\0';

				i += 2;
				j=0; 	
			}
		}
	}	
	else
		floristArr[floristNum].flowerNum = 0;   	
}

/* gets client informations */
void clientInfos(char arr[]){
	int i=0, j=0;
	char x_coor[15];
	char y_coor[15];
	char flower[40];

	while(arr[i] != '(')
		++i;

	++i;

	while(arr[i] != ','){
		x_coor[j]=arr[i];
		++i;
		++j;
	}
	x_coor[j]='\0';

	clientArr[clientNum].x_coor=atof(x_coor);

	++i;
	j=0;

	while(arr[i] != ')'){
		y_coor[j]=arr[i];
		++i;
		++j;
	}
	y_coor[j]='\0';

	clientArr[clientNum].y_coor=atof(y_coor);

	i += 3;
	j=0;

	while(arr[i] != '\n'){
		flower[j]=arr[i];
		++i;
		++j;
	}
	flower[j]='\0';	

	strcpy(clientArr[clientNum].flower,flower);	

	clientArr[clientNum].florist = closestFlorist(clientArr[clientNum].x_coor, clientArr[clientNum].y_coor, clientArr[clientNum].flower);
}

/* calculate distance using the Chebyshev Theorem*/
double calcDistance(double x1, double y1, double x2, double y2){
	return MAX( fabs(x1-x2), fabs(y1-y2) );
}

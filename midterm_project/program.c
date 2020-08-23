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

static void usageError(const char *progName);
void handler(int signal);

/* Shared memory buffer */
struct shmbuf {         		
    sem_t items_kitchen;  	// # of items at kitchen
    sem_t rooms_kitchen;  	// # of rooms at kitchen
    sem_t P_kitchen;      	// # of P plates at the kitchen
    sem_t C_kitchen;      	// # of C plates at the kitchen
    sem_t D_kitchen;      	// # of D plates at the kitchen
    sem_t rooms_counter;  	// # of rooms at counter
    sem_t P_counter;      	// # of P plates at the counter
    sem_t C_counter;      	// # of C plates at the counter
    sem_t D_counter;      	// # of D plates at the counter
    sem_t empty_table;  	// # of kitchen_room_count tables
    sem_t total_food;  	    // = 3*L*M
    sem_t kitchen_lock;  	// mutex 
    sem_t counter_lock;  	// mutex 
    sem_t cook_lock; 		// mutex
    sem_t P_counter2;       
    sem_t C_counter2;      	 
    sem_t D_counter2;      
    sem_t pusher_lock; 		//mutex
    sem_t P_counter3, C_counter3, D_counter3;
    sem_t lock1, lock2, lock3; //mutexes
    int kitchen_item_count, kitchen_room_count, kitchen_P_count, kitchen_C_count, kitchen_D_count; //number of each type of plates at kitchen
    int counter_P_count, counter_C_count, counter_D_count, counter_item_count; //number of each type of plates at counter
    int isSoup, isMainCourse, isDesert;
    int student_at_counter; //# of student at the counter
    int numP, numC, numD;
    int sinirp1, sinirp2, sinirp3;
    int P_counter2_count, C_counter2_count, D_counter2_count;
	int table_count;
};

struct shmbuf *shm;
int fd_shared, fd_file, N, M;
pid_t parent_pid = -1;
char* filePath;
int *table;

int main(int argc, char* argv[]){
	int  T, S, L, K, option, i, j; 
    const char* name = "shared_memory";	
   	filePath = (char*) malloc(9*sizeof(char)); //filePath
  	struct sigaction sa;

	//signal handler for SIGINT
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = handler;
	if(sigaction(SIGINT, &sa, NULL) == -1){
		perror("Sigaction");
		return -1;
  	}    

	if (argc != 13)
		usageError(argv[0]);		

	while((option = getopt(argc, argv, "NMTSLF")) != -1){ //get option from the getopt() method
		switch(option){
			case 'N':
				sscanf(argv[optind], "%d", &N);        	      
				break;
			case 'M':
          		sscanf(argv[optind], "%d", &M);        	      
			    break;
			case 'T':
				sscanf(argv[optind], "%d", &T);
				break;
			case 'S':
				sscanf(argv[optind], "%d", &S);
			    break;
			case 'L':
				sscanf(argv[optind], "%d", &L);  
				break;      
			case 'F':
				strcpy(filePath,argv[optind]);  
				break; 				
			default: usageError(argv[0]);
		}
	}

	if( (N<=2 || M<=N) || S<=3 || (T<1 || M<=T) || L<3){
		printf("Wrong command line arguments!");
		printf("Constraints\n");
		printf("M>N>2\n");
		printf("S>3\n");
		printf("M>T>=1\n");
		printf("L>=3\n");
		exit(EXIT_FAILURE); 
	}

	K=2*L*M+1; 

	/***open shared memory and memory allocation***/
    fd_shared = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd_shared == -1){
        perror("shm_open: ");
		exit(EXIT_FAILURE); 
    }

    if (ftruncate(fd_shared, sizeof(struct shmbuf)) == -1){
        perror("ftruncate: ");
		exit(EXIT_FAILURE); 
    }

   	shm = mmap(NULL, sizeof(struct shmbuf), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shared, 0);
   	if (shm == MAP_FAILED){
        perror("mmap1: ");
		exit(EXIT_FAILURE); 	
	}

	/***init the semaphore***/
	if (sem_init(&shm->items_kitchen, 1, 0) == -1){
        perror("sem_init1: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->rooms_kitchen, 1, K) == -1){
        perror("sem_init2: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->P_kitchen, 1, 0) == -1){
        perror("sem_init3: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->C_kitchen, 1, 0) == -1){
        perror("sem_init4: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->D_kitchen, 1, 0) == -1){
        perror("sem_init5: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->rooms_counter, 1, S) == -1){
        perror("sem_init6: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->P_counter, 1, 0) == -1){
        perror("sem_init7: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->C_counter, 1, 0) == -1){
        perror("sem_init8: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->D_counter, 1, 0) == -1){
        perror("sem_init9: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->empty_table, 1, T) == -1){
        perror("sem_init10: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->total_food, 1, 0) == -1){
        perror("sem_init11: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->kitchen_lock, 1, 1) == -1){
        perror("sem_init12: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->counter_lock, 1, 1) == -1){
        perror("sem_init13: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->cook_lock, 1, 1) == -1){
        perror("sem_init14: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->P_counter2, 1, 0) == -1){
        perror("sem_init15: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->C_counter2, 1, 0) == -1){
        perror("sem_init16: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->D_counter2, 1, 0) == -1){
        perror("sem_init17: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->pusher_lock, 1, 1) == -1){
        perror("sem_init18: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->P_counter3, 1, 0) == -1){
        perror("sem_init19: ");
		exit(EXIT_FAILURE); 	
	}
	if (sem_init(&shm->C_counter3, 1, 0) == -1){
        perror("sem_init20: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->D_counter3, 1, 0) == -1){
        perror("sem_init21: ");
		exit(EXIT_FAILURE); 	
	}			   
	if (sem_init(&shm->lock1, 1, 1) == -1){
        perror("sem_init22: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->lock2, 1, 1) == -1){
        perror("sem_init23: ");
		exit(EXIT_FAILURE); 	
	}	
	if (sem_init(&shm->lock3, 1, 1) == -1){
        perror("sem_init24: ");
		exit(EXIT_FAILURE); 	
	}	

	//empty tables
	table = mmap(0, T*sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (!table){
		perror("mmap2: ");
		exit(EXIT_FAILURE);
	}
	memset((void *)table, 0, T*sizeof(pid_t));	

	//0 is empty, 1 is full
  	for(j=0; j<T; ++j)
  		table[j]=0;				 	    					

	shm->kitchen_item_count = shm->counter_item_count =0;
	shm->kitchen_room_count=K;
	shm->kitchen_P_count = shm->kitchen_C_count = shm->kitchen_D_count = shm->counter_P_count = shm->counter_C_count = shm->counter_D_count = 0;
    shm->isSoup = shm->isMainCourse = shm->isDesert = 0;
    shm->student_at_counter  =0;
    shm->table_count=T;
    shm->C_counter2_count = shm->P_counter2_count = shm->D_counter2_count = 0;

    //agents push the semaphores numP, numC and numD times
    if(M%3 == 0)
    	shm->numP = shm->numC = shm->numD = M/3*L;
    else if(M%3 == 1){
    	shm->numP = (M/3 + 1)*L;
    	shm->numC = shm->numD = M/3*L;
    }else{
    	shm->numP = shm->numC = (M/3 + 1)*L;
    	shm->numD = M/3*L;
    }

    shm->sinirp1= L*M - shm->numP;
    shm->sinirp2= L*M - shm->numC;
    shm->sinirp3= L*M - shm->numD;
 
	for(i=0; i<M+N+1; ++i){
		switch(fork()){
			case -1:
        		perror("Fork1: ");
				exit(EXIT_FAILURE); 
      		case 0:
				if(i<M){ //student
  					int k;

      				if(i%3==0){
      					for(j=0; j<L; ++j){
      						printf("Student %d is going to the counter (round %d) - # of students at counter: %d and counter items P:%d,C:%d,D:%d=%d  \n",
      								i, j+1, shm->student_at_counter, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);

      						++shm->student_at_counter;

      						//Student looks wheter there is P plate
      						if(sem_wait(&shm->P_counter)==-1){
      							perror("sem_wait1: ");
      							exit(EXIT_FAILURE);
      						}
      						//If there is P plate now wait for C plate and D plate
      						if(sem_wait(&shm->P_counter2)==-1){
      							perror("sem_wait2: ");
      							exit(EXIT_FAILURE);
      						}
      						//When there are each type of plates at the counter, takes them
    						--shm->counter_P_count;
    						--shm->counter_C_count;
    						--shm->counter_D_count;
							shm->counter_item_count -= 3;  

							printf("Student %d got food and is going to get a table (round %d) - # of empty tables: %d\n", i, j+1, shm->table_count );

      						--shm->student_at_counter;							
							
							//waits for empty table, if they are full
							if(sem_wait(&shm->empty_table)==-1){
      							perror("sem_wait3: ");
      							exit(EXIT_FAILURE);
      						}
							--shm->table_count;

							//sits first empty table
							for(k=0; k<T; ++k){
								if(table[k] == 0){
									table[k]=1;
									break;
								}
							}							

							printf("Student %d sat at table %d to eat (round %d) - empty tables:%d\n",i, k, j+1, shm->table_count);

							//table is empty again
							if(sem_post(&shm->empty_table)==-1){
      							perror("sem_post1: ");
      							exit(EXIT_FAILURE);
      						}
							++shm->table_count;	

							table[k]=0;						

							if(j!=L-1)
								printf("Student %d left table %d to eat again (round %d) - empty tables:%d\n", i, k, j+1, shm->table_count);
							else
								printf("Student %d is done eating L=%d times - going home – GOODBYE!!!\n",i, L);
      					}
      				}
      				else if(i%3==1){
      					for(j=0; j<L; ++j){
      						printf("Student %d is going to the counter (round %d) - # of students at counter: %d and counter items P:%d,C:%d,D:%d=%d  \n",
      								i, j+1, shm->student_at_counter, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);

      						++shm->student_at_counter;

      						//Student looks wheter there is C plate
      						if(sem_wait(&shm->C_counter)==-1){
      							perror("sem_wait4: ");
      							exit(EXIT_FAILURE);
      						}
      						//If there is P plate now wait for P plate and D plate
      						if(sem_wait(&shm->C_counter2)==-1){
      							perror("sem_wait5: ");
      							exit(EXIT_FAILURE);
      						}
      						//When there are each type of plates at the counter, takes them
    						--shm->counter_P_count;
    						--shm->counter_C_count;
    						--shm->counter_D_count;
							shm->counter_item_count -= 3; 

							printf("Student %d got food and is going to get a table (round %d) - # of empty tables: %d\n", i, j+1, shm->table_count );

      						--shm->student_at_counter;														
							
							//waits for empty table, if they are full
							if(sem_wait(&shm->empty_table)==-1){
      							perror("sem_wait6: ");
      							exit(EXIT_FAILURE);
      						}
							--shm->table_count;

							//sits first empty table
							for(k=0; k<T; ++k){
								if(table[k] == 0){
									table[k]=1;
									break;
								}
							}

							printf("Student %d sat at table %d to eat (round %d) - empty tables:%d\n", i, k, j+1, shm->table_count);

							//table is empty again
							if(sem_post(&shm->empty_table)==-1){
      							perror("sem_post2: ");
      							exit(EXIT_FAILURE);
      						}
							++shm->table_count;	

							table[k]=0;						

							if(j!=L-1)
								printf("Student %d left table %d to eat again (round %d) - empty tables:%d\n", i, k, j+1, shm->table_count);
							else
								printf("Student %d is done eating L=%d times - going home – GOODBYE!!!\n",i, L);
      					}      					
      				}
      				else{
      					for(j=0; j<L; ++j){
      						printf("Student %d is going to the counter (round %d) - # of students at counter: %d and counter items P:%d,C:%d,D:%d=%d  \n",
      								i, j+1, shm->student_at_counter, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);

      						++shm->student_at_counter;

      						//Student looks wheter there is D plate
      						if(sem_wait(&shm->D_counter)==-1){
      							perror("sem_wait7: ");
      							exit(EXIT_FAILURE);
      						}
      						//If there is P plate now wait for P plate and C plate
      						if(sem_wait(&shm->D_counter2)==-1){
      							perror("sem_wait8: ");
      							exit(EXIT_FAILURE);
      						}
      						//When there are each type of plates at the counter, takes them
    						--shm->counter_P_count;
    						--shm->counter_C_count;
    						--shm->counter_D_count;
							shm->counter_item_count -= 3;  

							printf("Student %d got food and is going to get a table (round %d) - # of empty tables: %d\n", i, j+1, shm->table_count );

      						--shm->student_at_counter;	

      						//waits for empty table, if they are full						
							if(sem_wait(&shm->empty_table)==-1){
      							perror("sem_wait9: ");
      							exit(EXIT_FAILURE);
      						}
							--shm->table_count;

							//sits first empty table
							for(k=0; k<T; ++k){
								if(table[k] == 0){
									table[k]=1;
									break;
								}
							}							

							printf("Student %d sat at table %d to eat (round %d) - empty tables:%d\n",i, k, j+1, shm->table_count);

							//table is empty again
							if(sem_post(&shm->empty_table)==-1){
      							perror("sem_post3: ");
      							exit(EXIT_FAILURE);
      						}
							++shm->table_count;	

							table[k]=0;																			

							if(j!=L-1)
								printf("Student %d left table %d to eat again (round %d) - empty tables:%d\n", i, k, j+1, shm->table_count);
							else
								printf("Student %d is done eating L=%d times - going home – GOODBYE!!!\n",i, L);
      					}      					
      				}	
    			}else if(i==M){ //suplier
      				int counter1=0, counter2=0, counter3=0;
      				char plate[1];

      				fd_file = open(filePath, O_RDONLY,0666);
      				if(fd_file == -1){      				
      					perror("open: ");
      					exit(EXIT_FAILURE);
      				}
					
					//each supplier waits for room at the kitchen, if there are no room left then takes the kitchen_lock to block cooks
      				for(j=0; j<3*L*M; ++j){ 
      					if(sem_wait(&shm->rooms_kitchen)==-1){
      							perror("sem_wait10: ");
      							exit(EXIT_FAILURE);
      					}
      					if(sem_wait(&shm->kitchen_lock)==-1){
      							perror("sem_wait11: ");
      							exit(EXIT_FAILURE);
      					}

      					//reads bytes from the file then delivers them to kitchen
      					if(read(fd_file, plate,1)==-1){
      							perror("read: ");
      							exit(EXIT_FAILURE);
      					}
						
     					if(plate[0] == 'P'){  
      						printf("The supplier is going to the kitchen to deliver soup: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);

	     					++(shm->kitchen_item_count);
      						--(shm->kitchen_room_count);	
      						++shm->kitchen_P_count;
    						if(sem_post(&shm->P_kitchen)==-1){
      							perror("sem_post4: ");
      							exit(EXIT_FAILURE);
      						}

      						printf("The supplier delivered soup – after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);
     						++counter1;
      					}
      					else if(plate[0] == 'C'){
      						printf("The supplier is going to the kitchen to deliver main course: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);      						
      						
	     					++(shm->kitchen_item_count);
      						--(shm->kitchen_room_count);	      						
      						++shm->kitchen_C_count;
      						if(sem_post(&shm->C_kitchen)==-1){
      							perror("sem_post5: ");
      							exit(EXIT_FAILURE);
      						}

      						printf("The supplier delivered main course – after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);   						
      						++counter2;
      					}
      					else{
      						printf("The supplier is going to the kitchen to deliver desert: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);      						
      						
	     					++(shm->kitchen_item_count);
      						--(shm->kitchen_room_count);	      						
      						++shm->kitchen_D_count;
      						if(sem_post(&shm->D_kitchen)==-1){
      							perror("sem_post6: ");
      							exit(EXIT_FAILURE);
      						}

      						printf("The supplier delivered desert – after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",
      								shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);      						     						      						
      						++counter3;
      					}

     					if(j == (3*L*M-1))
      						printf("Supplier finished supplying – GOODBYE!\n");

      					if(sem_post(&shm->kitchen_lock)==-1){
      							perror("sem_post7: ");
      							exit(EXIT_FAILURE);
      						}
      					if(sem_post(&shm->items_kitchen)==-1){
      							perror("sem_post8: ");
      							exit(EXIT_FAILURE);
      						}  					      					
      				}
      			}else{ //cooks
      				int food, flag;
      				//cooks take plates from the kitchen then delivers them to counter. they choose the type of plate acording to counter plates
      				while(1){
      					//to avoid cooks enter critical section at the same time
      					if(sem_wait(&shm->cook_lock)==-1){
      						perror("sem_wait12: ");
      						exit(EXIT_FAILURE);
      					}      					
      					if(sem_getvalue(&shm->total_food, &food)==-1){
      						perror("sem_getvalue: ");
      						exit(EXIT_FAILURE);
      					}

      					if(food!=3*L*M){ 
      						//cooks wait for supplier if there is no plate at the kitchen
      						if(sem_wait(&shm->items_kitchen)==-1){
      							perror("sem_wait13: ");
      							exit(EXIT_FAILURE);
      						}

      						//to choose which plate should be choose, cook checks which type of plates less than other then it choose this table
	      					if(shm->counter_P_count<=shm->counter_C_count){
	      						if(shm->counter_P_count<=shm->counter_D_count){
      								if(sem_wait(&shm->P_kitchen)==-1){
      									perror("sem_wait14: ");
      									exit(EXIT_FAILURE);
      								}
      								flag=0;
	      						}
	      						else{
      								if(sem_wait(&shm->D_kitchen)==-1){
      									perror("sem_wait15: ");
      									exit(EXIT_FAILURE);
      								}
      								flag=2;   							 
	      						}
	      					}else{
	      						if(shm->counter_C_count<=shm->counter_D_count){
      								if(sem_wait(&shm->C_kitchen)==-1){
      									perror("sem_wait16: ");
      									exit(EXIT_FAILURE);
      								}	  
      								flag=1;   							
	      						}
	      						else{
      								if(sem_wait(&shm->D_kitchen)==-1){
      									perror("sem_wait17: ");
      									exit(EXIT_FAILURE);
      								}
      								flag=2;	      							
	      						}
	      					} 

      						if(sem_wait(&shm->kitchen_lock)==-1){
      							perror("sem_wait18: ");
      							exit(EXIT_FAILURE);
      						} 

      						printf("Cook %d is going to the kitchen to wait for/get a plate - kitchen items P:%d,C:%d,D:%d=%d\n",	      						
									i-1-M, shm->kitchen_P_count, shm->kitchen_C_count, shm->kitchen_D_count, shm->kitchen_item_count);      	

      						++(shm->kitchen_room_count);
      						--(shm->kitchen_item_count);
		
	      					if(flag == 0){
      							printf("Cook %d is going to the counter to deliver soup – counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);     							

      							--shm->kitchen_P_count;
      							++shm->counter_P_count;
      							if(sem_post(&shm->P_counter)==-1){
      								perror("sem_post8: ");
      								exit(EXIT_FAILURE);
      							}
      							++shm->counter_item_count;

      							printf("Cook %d placed soup on the counter - counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);
	      					}
	      					else if(flag == 1){
      							printf("Cook %d is going to the counter to deliver main course – counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);

      							--shm->kitchen_C_count;
      							++shm->counter_C_count; 
      							if(sem_post(&shm->C_counter)==-1){
      								perror("sem_post9: ");
      								exit(EXIT_FAILURE);
      							}     							  
      							++shm->counter_item_count;

      							printf("Cook %d placed main course on the counter - counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);
	      					}
	      					else{
      							printf("Cook %d is going to the counter to deliver desert – counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);

      							--shm->kitchen_D_count;
      							++shm->counter_D_count;
      							if(sem_post(&shm->D_counter)==-1){
      								perror("sem_post10: ");
      								exit(EXIT_FAILURE);
      							}    							
      							++shm->counter_item_count;

      							printf("Cook %d placed desert on the counter - counter items P:%d,C:%d,D:%d=%d\n",
      									i-1-M, shm->counter_P_count, shm->counter_C_count, shm->counter_D_count, shm->counter_item_count);
	      					}
	      					   
      						if(food == (3*L*M-1))
      							printf("Cook %d finished serving - items at kitchen: %d – going home – GOODBYE!!! \n", i-1-M, shm->kitchen_item_count);	 
      						
      						if(sem_post(&shm->total_food)==-1){
  								perror("sem_post11: ");
  								exit(EXIT_FAILURE);
  							}      					
	      					if(sem_post(&shm->cook_lock)==-1){
  								perror("sem_post12: ");
  								exit(EXIT_FAILURE);
  							}    	 
      						if(sem_post(&shm->kitchen_lock)==-1){
  								perror("sem_post13: ");
  								exit(EXIT_FAILURE);
  							}    
	      					if(sem_post(&shm->rooms_kitchen)==-1){
  								perror("sem_post14: ");
  								exit(EXIT_FAILURE);
  							}    
      					}else{
	      					if(sem_post(&shm->cook_lock)==-1){
  								perror("sem_post15: ");
  								exit(EXIT_FAILURE);
  							}    					
      						break;
      					}
      				}
      			}			
      			
      			exit(EXIT_SUCCESS);
      		default: break;
      	}
	}

	//pushers push the available plates
	for(i=0; i<3; ++i){//pushers
		switch(fork()){
			case -1:
        		perror("Fork1: ");
				exit(EXIT_FAILURE); 
      		case 0:
      			if(i==0){ //pusher1
      				int j=0;
      				while(1){
      					if(j<shm->sinirp1){
      						//wait for P_counter3
							if(sem_wait(&shm->P_counter3)==-1){
      							perror("sem_wait19: ");
      							exit(EXIT_FAILURE);
      						}	
      						//takes the lock, lock avoids p plates go on
							if(sem_wait(&shm->lock1)==-1){
      							perror("sem_wait20: ");
      							exit(EXIT_FAILURE);
      						}	
      						//takes pusher lock to block other pushers
							if(sem_wait(&shm->pusher_lock)==-1){
      							perror("sem_wait21: ");
      							exit(EXIT_FAILURE);
      						}	
      						//if there is main course or desert at the counter it means that you have P, C and D so one child can eat
							if(shm->isMainCourse==1){
								if(shm->D_counter2_count == shm->numD)
									shm->isSoup=1;
								else{
									shm->isMainCourse=0;
									if(sem_post(&shm->lock2)==-1){
  										perror("sem_post16: ");
  										exit(EXIT_FAILURE);
  									}    
									if(sem_post(&shm->lock1)==-1){
  										perror("sem_post17: ");
  										exit(EXIT_FAILURE);
  									}    
									++shm->D_counter2_count;
									if(sem_post(&shm->D_counter2)==-1){
  										perror("sem_post18: ");
  										exit(EXIT_FAILURE);
  									}    
								}
							}else if(shm->isDesert==1){ 
								if(shm->C_counter2_count == shm->numC)
									shm->isSoup=1;
								else{
									shm->isDesert=0;
									if(sem_post(&shm->lock1)==-1){
  										perror("sem_post19: ");
  										exit(EXIT_FAILURE);
  									}    
									if(sem_post(&shm->lock3)==-1){
  										perror("sem_post20: ");
  										exit(EXIT_FAILURE);
  									}    
									++shm->C_counter2_count;
									if(sem_post(&shm->C_counter2)==-1){
  										perror("sem_post21: ");
  										exit(EXIT_FAILURE);
  									}    
								}
							}else	//if there is no P plate at the counter, now it has							
								shm->isSoup=1;
							if(sem_post(&shm->pusher_lock)==-1){
  								perror("sem_post22: ");
  								exit(EXIT_FAILURE);
  							}    
							++j;
						}
						else
							break;  
					}    				
      			}else if(i==1){//pusher 2
      				for(j=0;j<shm->sinirp2; ++j){
      					//wait for C_counter3
						if(sem_wait(&shm->C_counter3)==-1){
      						perror("sem_wait22: ");
      						exit(EXIT_FAILURE);
      					}	
      					//takes the lock2, lock2 avoids p plates go on
						if(sem_wait(&shm->lock2)==-1){
      						perror("sem_wait23: ");
      						exit(EXIT_FAILURE);
      					}	
      					//takes pusher lock to block other pushers
						if(sem_wait(&shm->pusher_lock)==-1){
      						perror("sem_wait23: ");
      						exit(EXIT_FAILURE);
      					}	
      					//if there is spup or desert at the counter it means that you have P, C and D so one child can eat
						if(shm->isSoup==1){
							if(shm->D_counter2_count == shm->numD)
								shm->isMainCourse=1;
							else{
								shm->isSoup=0;
								if(sem_post(&shm->lock1)==-1){
  									perror("sem_post23: ");
  									exit(EXIT_FAILURE);
  								}    
								if(sem_post(&shm->lock2)==-1){
  									perror("sem_post24: ");
  									exit(EXIT_FAILURE);
  								}    
								++shm->D_counter2_count;
								if(sem_post(&shm->D_counter2)==-1){
  									perror("sem_post25: ");
  									exit(EXIT_FAILURE);
  								}    
							}
						}else if(shm->isDesert==1){
							if(shm->P_counter2_count == shm->numP)
								shm->isMainCourse=1;
							else{								
								shm->isDesert=0;
								if(sem_post(&shm->lock2)==-1){
  									perror("sem_post26: ");
  									exit(EXIT_FAILURE);
  								}    
								if(sem_post(&shm->lock3)==-1){
  									perror("sem_post27: ");
  									exit(EXIT_FAILURE);
  								}    
								++shm->P_counter2_count;
								if(sem_post(&shm->P_counter2)==-1){
  									perror("sem_post28: ");
  									exit(EXIT_FAILURE);
  								}    
							}
						}else	//if there is no C plate at the counter, now it has						
							shm->isMainCourse=1;
						if(sem_post(&shm->pusher_lock)==-1){
  							perror("sem_post29: ");
  							exit(EXIT_FAILURE);
  						}    
					}
      			}
      			else{ //pusher3
      				for(j=0; j<shm->sinirp3; ++j){
      					//wait for D_counter3
						if(sem_wait(&shm->D_counter3)==-1){
      						perror("sem_wait24: ");
      						exit(EXIT_FAILURE);
      					}	
      					//takes the lock3, lock3 avoids p plates go on
						if(sem_wait(&shm->lock3)==-1){
      						perror("sem_wait25: ");
      						exit(EXIT_FAILURE);
      					}	
      					//takes pusher lock to block other pushers
						if(sem_wait(&shm->pusher_lock)==-1){
      						perror("sem_wait26: ");
      						exit(EXIT_FAILURE);
      					}	
      					//if there is spup or main course at the counter it means that you have P, C and D so one child can eat
						if(shm->isSoup == 1){
							if(shm->C_counter2_count == shm->numC)
								shm->isDesert=1;
							else{								
								shm->isSoup=0;
								if(sem_post(&shm->lock1)==-1){
  									perror("sem_post30: ");
  									exit(EXIT_FAILURE);
  								}    
								if(sem_post(&shm->lock3)==-1){
  									perror("sem_post31: ");
  									exit(EXIT_FAILURE);
  								}    
								++shm->C_counter2_count;
								if(sem_post(&shm->C_counter2)==-1){
  									perror("sem_post32: ");
  									exit(EXIT_FAILURE);
  								}    
							}
						}else if(shm->isMainCourse == 1){
							if(shm->P_counter2_count == shm->numP)
								shm->isDesert=1;
							else{								
								shm->isMainCourse=0;
								if(sem_post(&shm->lock3)==-1){
  									perror("sem_post33: ");
  									exit(EXIT_FAILURE);
  								}    
								if(sem_post(&shm->lock2)==-1){
  									perror("sem_post33: ");
  									exit(EXIT_FAILURE);
  								}    
								++shm->P_counter2_count;
								if(sem_post(&shm->P_counter2)==-1){
  									perror("sem_post34: ");
  									exit(EXIT_FAILURE);
  								}    
							}
						}else  	//if there is no D plate at the counter, now it has	
							shm->isDesert=1;
						if(sem_post(&shm->pusher_lock)==-1){
  							perror("sem_post35: ");
  							exit(EXIT_FAILURE);
  						}    
					} 				
      			}
      			exit(EXIT_SUCCESS);
      		default: break;		
      	}
	}

	//agent process look for P-C , P-D and C-D plates then post them for pushers
	for(i=0; i<3; ++i){
		switch(fork()){
			case -1:
	    		perror("Fork1: ");
				exit(EXIT_FAILURE); 
	  		case 0:
	  			if(i==0){//agent1
	  				for(j=0; j<shm->numD; ++j){
	  					if(sem_wait(&shm->P_counter)==-1){
      						perror("sem_wait27: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->P_counter3)==-1){
  							perror("sem_post37: ");
  							exit(EXIT_FAILURE);
  						}    
	  					if(sem_wait(&shm->C_counter)==-1){
      						perror("sem_wait28: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->C_counter3)==-1){
  							perror("sem_post38: ");
  							exit(EXIT_FAILURE);
  						}
	  				}
	  			}else if(i==1){ //agent2
					for(j=0; j<shm->numC; ++j){	  				
	  					if(sem_wait(&shm->P_counter)==-1){
      						perror("sem_wait29: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->P_counter3)==-1){
  							perror("sem_post39: ");
  							exit(EXIT_FAILURE);
  						}
	  					if(sem_wait(&shm->D_counter)==-1){
      						perror("sem_wait30: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->D_counter3)==-1){
  							perror("sem_post40: ");
  							exit(EXIT_FAILURE);
  						}
	  				}	  			
	  			}else{ //agent3
	  				for(j=0; j<shm->numP; ++j){  						
	  					if(sem_wait(&shm->C_counter)==-1){
      						perror("sem_wait31: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->C_counter3)==-1){
  							perror("sem_post41: ");
  							exit(EXIT_FAILURE);
  						}
	  					if(sem_wait(&shm->D_counter)==-1){
      						perror("sem_wait32: ");
      						exit(EXIT_FAILURE);
      					}	
	  					if(sem_post(&shm->D_counter3)==-1){
  							perror("sem_post42: ");
  							exit(EXIT_FAILURE);
  						}
	  				} 				
	  			}
	  			exit(EXIT_SUCCESS);
	  		default: break;
	  	}
	}

	parent_pid=getpid();	

	/***close files and destroy semaphore, free resources***/
	if(close(fd_shared)==-1){
		perror("Close1: ");
		exit(EXIT_FAILURE);
	}
	if(close(fd_file)==-1){
		perror("Close2: ");
		exit(EXIT_FAILURE);
	}
	if(shm_unlink(name)==-1){
        perror("shm_unlink: ");
		exit(EXIT_FAILURE); 		
	}	
	if(sem_destroy(&shm->items_kitchen)==-1){
		perror("sem_destroy1: ");
		exit(EXIT_FAILURE);		
	}
	if(sem_destroy(&shm->rooms_kitchen)==-1){
		perror("sem_destroy2: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->P_kitchen)==-1){
		perror("sem_destroy3: ");
		exit(EXIT_FAILURE);		
	}
	if(sem_destroy(&shm->C_kitchen)==-1){
		perror("sem_destroy4: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->D_kitchen)==-1){
		perror("sem_destroy5: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->rooms_counter)==-1){
		perror("sem_destroy6: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->P_counter)==-1){
		perror("sem_destroy7: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->C_counter)==-1){
		perror("sem_destroy8: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->D_counter)==-1){
		perror("sem_destroy9: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->empty_table)==-1){
		perror("sem_destroy10: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->kitchen_lock)==-1){
		perror("sem_destroy11: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->counter_lock)==-1){
		perror("sem_destroy12: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->cook_lock)==-1){
		perror("sem_destroy13: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->P_counter2)==-1){
		perror("sem_destroy14: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->C_counter2)==-1){
		perror("sem_destroy15: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->D_counter2)==-1){
		perror("sem_destroy16: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->pusher_lock)==-1){
		perror("sem_destroy17: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->P_counter3)==-1){
		perror("sem_destroy18: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->C_counter3)==-1){
		perror("sem_destroy19: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->D_counter3)==-1){
		perror("sem_destroy20: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->lock1)==-1){
		perror("sem_destroy21: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->lock2)==-1){
		perror("sem_destroy22: ");
		exit(EXIT_FAILURE);		
	}		
	if(sem_destroy(&shm->lock3)==-1){
		perror("sem_destroy23: ");
		exit(EXIT_FAILURE);		
	}	
	if(munmap(shm, sizeof(struct shmbuf)) == -1){
		perror("munmap1: ");
		exit(EXIT_FAILURE);
	}
	free(filePath);

	for(i=0; i<(N+M+7); ++i)
		wait(NULL);	

	exit(EXIT_SUCCESS);

  return 0;
}

static void usageError(const char *progName) {
	printf("Wrong command line arguments!\n");
	printf("Usage: %s [-NMTSL] \n", progName);
	printf(" -N # of cooks\n");
	printf(" -M # of students \n");
	printf(" -T # of tables \n");
	printf(" -S A counter of size \n");
	printf(" -L Every student get food from the counter, a total of L times \n");
	printf(" -F filePath which supplier will read that must contains exactly 3LM characters \n");
	exit(EXIT_FAILURE); 
}
//in case of ctrl+c, program closes files and destroy semaphores, free resources then exits gravefully
void handler(int signal){
	if(parent_pid != -1){
		if(close(fd_shared)==-1){
			perror("Close3: ");
			exit(EXIT_FAILURE);
		}
		if(close(fd_file)==-1){
			perror("Close4: ");
			exit(EXIT_FAILURE);
		}			
		if(sem_destroy(&shm->items_kitchen)==-1){
			perror("sem_destroy24: ");
			exit(EXIT_FAILURE);		
		}
		if(sem_destroy(&shm->rooms_kitchen)==-1){
			perror("sem_destroy25: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->P_kitchen)==-1){
			perror("sem_destroy26: ");
			exit(EXIT_FAILURE);		
		}
		if(sem_destroy(&shm->C_kitchen)==-1){
			perror("sem_destroy27: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->D_kitchen)==-1){
			perror("sem_destroy28: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->rooms_counter)==-1){
			perror("sem_destroy29: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->P_counter)==-1){
			perror("sem_destroy30: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->C_counter)==-1){
			perror("sem_destroy31: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->D_counter)==-1){
			perror("sem_destroy32: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->empty_table)==-1){
			perror("sem_destroy33: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->kitchen_lock)==-1){
			perror("sem_destroy34: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->counter_lock)==-1){
			perror("sem_destroy35: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->cook_lock)==-1){
			perror("sem_destroy36: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->P_counter2)==-1){
			perror("sem_destroy37: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->C_counter2)==-1){
			perror("sem_destroy38: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->D_counter2)==-1){
			perror("sem_destroy39: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->pusher_lock)==-1){
			perror("sem_destroy40: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->P_counter3)==-1){
			perror("sem_destroy41: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->C_counter3)==-1){
			perror("sem_destroy42: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->D_counter3)==-1){
			perror("sem_destroy43: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->lock1)==-1){
			perror("sem_destroy44: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->lock2)==-1){
			perror("sem_destroy45: ");
			exit(EXIT_FAILURE);		
		}		
		if(sem_destroy(&shm->lock3)==-1){
			perror("sem_destroy46: ");
			exit(EXIT_FAILURE);		
		}
		if(munmap(shm, sizeof(struct shmbuf)) == -1){
			perror("munmap2: ");
			exit(EXIT_FAILURE);
		}
		free(filePath);		
		int i;	
		for(i=0; i<(N+M+7); ++i)
			wait(NULL);		
	}
	exit(EXIT_SUCCESS);
}

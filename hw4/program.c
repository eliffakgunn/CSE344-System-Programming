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

static void usageError();
void * chef(void *id);
void * pusher(void *id);
void * agent(void *id);
void post();

sem_t mutex;    /* for pushers */
sem_t gullac;   /* when gullac is ready chefs post this semaphore */
sem_t milk;     
sem_t flour;
sem_t walnuts;
sem_t sugar;
sem_t MF;       /* milk+flour    */
sem_t MW;       /* milk+walnuts  */
sem_t MS;       /* milk+sugar    */
sem_t FW;       /* flour+walnuts */
sem_t FS;       /* flour+sugar   */
sem_t WS;       /* walnuts+sugar */
sem_t semAgent;   
sem_t semChef;    
sem_t semPusher;  
                  
char *ingredients;
int isMilk = 0;
int isFlour = 0;
int isWalnuts = 0;
int isSugar = 0;
int flag=0;

int main(int argc, char* argv[]){
    int i, option, fd_file, fd_read, line_count=0;
    int chef_id[6] = {0,1,2,3,4,5};
    int pusher_id[6] = {0,1,2,3}; 
    int agent_id = 0;   
    char new_line[1];
    pthread_t thread_chef[6];
    pthread_t thread_pusher[4];
    pthread_t thread_agent;
    char* filePath = (char*) malloc(20*sizeof(char));  /*filePath*/
    ingredients = (char*) malloc(2*sizeof(char));      /*ingredients*/

    if (argc != 3)
        usageError();        

    while((option = getopt(argc, argv, "i")) != -1){ /* get option from the getopt() method */
        switch(option){
            case 'i':
                strcpy(filePath,argv[optind]);               
                break;            
            default: usageError();
        }
    }   

    /***init the semaphores***/
    if (sem_init(&mutex, 0, 1) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }
    if (sem_init(&gullac, 0, 1) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }  
    if (sem_init(&milk, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }      
    if (sem_init(&flour, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }      
    if (sem_init(&walnuts, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }      
    if (sem_init(&sugar, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    } 
    if (sem_init(&MF, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }  
    if (sem_init(&MW, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }       
    if (sem_init(&MS, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }
    if (sem_init(&FW, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }             
    if (sem_init(&FS, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }     
    if (sem_init(&WS, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    } 
    if (sem_init(&semChef, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }
    if (sem_init(&semPusher, 0, 0) == -1){
        perror("sem_init1: ");
        exit(EXIT_FAILURE);     
    }        

    /* open filePath to get ingredients */
    fd_file = open(filePath, O_RDONLY,0666);
    if(fd_file == -1){                      
        perror("open: ");
        exit(EXIT_FAILURE);
    }     

    /* check if the file is less than 10 lines */
    while(1){
        fd_read=read(fd_file, new_line,1);
        if(fd_read==-1){
            perror("read: ");
            exit(EXIT_FAILURE);
        }     

        if(fd_read == 1 && new_line[0] == '\n')
            ++line_count;

        if(line_count==10)
            break;

        if(fd_read == 0 && line_count<10){
            printf("Invalid input file. The file must have at least 10 rows!\n");
            exit(EXIT_FAILURE);
        }   
    }

    /* sets ofset of the file to 0 index */
    if(lseek(fd_file,0,SEEK_SET) == -1){
        perror("lseek: ");
        exit(EXIT_FAILURE);
    }    
    
    /**creates threads***/  
    for(i=0; i<6; ++i)
        if(pthread_create(&thread_chef[i], NULL, chef, &chef_id[i]) != 0){
            perror("pthread_create1: ");
            exit(EXIT_FAILURE);
        }
    for(i=0; i<4; ++i)
        if(pthread_create(&thread_pusher[i], NULL, pusher, &pusher_id[i]) != 0){
            perror("pthread_create2: ");
            exit(EXIT_FAILURE);
        }
    if(pthread_create(&thread_agent, NULL, agent, &agent_id) != 0){
        perror("pthread_create2: ");
        exit(EXIT_FAILURE);
    }  

    /**read the file to EOF***/
    i=0;
    while (1){
        if(sem_wait(&gullac)==-1){
            perror("sem_wait1: ");
            exit(EXIT_FAILURE);
        } 

        fd_read=read(fd_file, ingredients,2);
        if(fd_read==-1){
            perror("read: ");
            exit(EXIT_FAILURE);
        }  

        /* if there are no more elements in the file notify this to threads */
        if(i!=0){
            if(fd_read==0)
                flag=1;

            if(sem_post(&semChef)==-1){
                perror("sem_post1: ");
                exit(EXIT_FAILURE);
            } 
            if(sem_post(&semPusher)==-1){
                perror("sem_post2: ");
                exit(EXIT_FAILURE);
            }
        }        

        if(fd_read>0){

            if((ingredients[0] == 'M' && ingredients[1] == 'F') || (ingredients[0] == 'F' && ingredients[1] == 'M'))
                printf("the wholesaler delivers milk and flour\n");
            else if((ingredients[0] == 'M' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'M'))
                printf("the wholesaler delivers milk and walnuts\n");
            else if((ingredients[0] == 'M' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'M'))
                printf("the wholesaler delivers milk and sugar\n");
            else if((ingredients[0] == 'F' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'F'))
                printf("the wholesaler delivers flour and walnuts\n");
            else if((ingredients[0] == 'F' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'F'))
                printf("the wholesaler delivers flour and sugar\n");
            else
                printf("the wholesaler delivers walnuts and sugar\n");
    
            /* there are some ingredients so agent can post these ingredients now */
            if(sem_post(&semAgent)==-1){
                perror("sem_post3: ");
                exit(EXIT_FAILURE);
            }
            
            printf("the wholesaler is waiting for the dessert\n");

            if(read(fd_file, new_line,1)==-1){
                perror("read: ");
                exit(EXIT_FAILURE);
            }  

            ++i;                                                      
        }  
        else{ 
            flag=1;
            if(sem_post(&semAgent)==-1){
                perror("sem_post4: ");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }  

    /***join threads***/ 
    for(i=0; i<6 ; i++)
        if(pthread_join(thread_chef[i], NULL) != 0){
            perror("pthread_join1: ");
            exit(EXIT_FAILURE);
        }
    for(i=0; i<4 ; i++)
        if(pthread_join(thread_pusher[i], NULL) != 0){
            perror("pthread_join2: ");
            exit(EXIT_FAILURE);
        }
    if(pthread_join(thread_agent, NULL) != 0){
        perror("pthread_join3: ");
        exit(EXIT_FAILURE);
    }

    /***close file, desttroy semaphores deallocate momory***/
    if(close(fd_file)==-1){
        perror("Close: ");
        exit(EXIT_FAILURE);
    }
    if(sem_destroy(&mutex)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&gullac)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&milk)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&flour)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&walnuts)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&sugar)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&MF)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&MW)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    }
    if(sem_destroy(&MS)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&FW)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&FS)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&WS)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&semAgent)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&semChef)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    } 
    if(sem_destroy(&semPusher)==-1){
        perror("sem_destroy1: ");
        exit(EXIT_FAILURE);     
    }
    free(filePath);
    free(ingredients);

    return 0;        
}

static void usageError() {
    printf("Wrong command line arguments!\n");
    printf("Usage: ./program -i filePath \n");
    printf(" -i represents the file that contains ingredients to deliver\n");
    exit(EXIT_FAILURE); 
}

/* agents check the ingredients array and post the relevant ingredients */
void * agent(void *id){
    if(*(int*) id==0){
        while(1){
            if(sem_wait(&semAgent)==-1){
                perror("sem_wait2: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==0){    
                if((ingredients[0] == 'M' && ingredients[1] == 'F') || (ingredients[0] == 'F' && ingredients[1] == 'M')){
                    if(sem_post(&milk)==-1){
                        perror("sem_post5: ");
                        exit(EXIT_FAILURE);
                    }
                    if(sem_post(&flour)==-1){
                        perror("sem_post6: ");
                        exit(EXIT_FAILURE);
                    }
                }
                else if((ingredients[0] == 'M' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'M')){
                    if(sem_post(&milk)==-1){
                        perror("sem_post7: ");
                        exit(EXIT_FAILURE);
                    }
                    if(sem_post(&walnuts)==-1){
                        perror("sem_post8: ");
                        exit(EXIT_FAILURE);
                    }            
                }
                else if((ingredients[0] == 'M' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'M')){
                    if(sem_post(&milk)==-1){
                        perror("sem_post9: ");
                        exit(EXIT_FAILURE);
                    }  
                    if(sem_post(&sugar)==-1){
                        perror("sem_post10: ");
                        exit(EXIT_FAILURE);
                    }             
                }
                else if((ingredients[0] == 'F' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'F')){
                    if(sem_post(&flour)==-1){
                        perror("sem_post11: ");
                        exit(EXIT_FAILURE);
                    }   
                    if(sem_post(&walnuts)==-1){
                        perror("sem_post12: ");
                        exit(EXIT_FAILURE);
                    }              
                }
                else if((ingredients[0] == 'F' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'F')){
                    if(sem_post(&flour)==-1){
                        perror("sem_post13: ");
                        exit(EXIT_FAILURE);
                    }   
                    if(sem_post(&sugar)==-1){
                        perror("sem_post14: ");
                        exit(EXIT_FAILURE);
                    }               
                }
                else{
                    if(sem_post(&walnuts)==-1){
                        perror("sem_post15: ");
                        exit(EXIT_FAILURE);
                    }   
                    if(sem_post(&sugar)==-1){
                        perror("sem_post16: ");
                        exit(EXIT_FAILURE);
                    }               
                }
            }else
                break;
        }
    }   

    return 0;
}

/*pushers post chefs semaphores according to the ingredients
 *after pushing, pushers wait for wholesaler to read file again, if file does not contains any ingredients anymore, break the loop and exit 
 */
void * pusher(void *id){
    while(1){
        if(*(int*) id==0){ /* pusher1 */
            if(sem_wait(&milk)==-1){
                perror("sem_wait3: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;

            if(sem_wait(&mutex)==-1){
                perror("sem_wait4: ");
                exit(EXIT_FAILURE);
            } 

            if(isFlour){
                isFlour=0;
                if(sem_post(&MF)==-1){
                    perror("sem_post17: ");
                    exit(EXIT_FAILURE);
                }   

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait5: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else if(isWalnuts){
                isWalnuts=0;
                if(sem_post(&MW)==-1){
                    perror("sem_post18: ");
                    exit(EXIT_FAILURE);
                }   

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait6: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else if(isSugar){
                isSugar=0;
                if(sem_post(&MS)==-1){
                    perror("sem_post19: ");
                    exit(EXIT_FAILURE);
                }   

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait7: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else
                isMilk=1;
            
            if(sem_post(&mutex)==-1){
                perror("sem_post20: ");
                exit(EXIT_FAILURE);
            }   
        }
        else if(*(int*) id==1){ /* pusher2 */
            if(sem_wait(&flour)==-1){
                perror("sem_wait8: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;

            if(sem_wait(&mutex)==-1){
                perror("sem_wait9: ");
                exit(EXIT_FAILURE);
            }   
        
            if(isMilk){
                isMilk=0;
                if(sem_post(&MF)==-1){
                    perror("sem_post21: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait10: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();                
            }
            else if(isWalnuts){
                isWalnuts=0;
                if(sem_post(&FW)==-1){
                    perror("sem_post22: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait11: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else if(isSugar){
                isSugar=0;
                if(sem_post(&FS)==-1){
                    perror("sem_post23: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait12: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else
                isFlour=1;

            if(sem_post(&mutex)==-1){
                perror("sem_post24: ");
                exit(EXIT_FAILURE);
            }                    
        }
        else if(*(int*) id==2){ /* pusher3 */
            if(sem_wait(&walnuts)==-1){
                perror("sem_wait13: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;

            if(sem_wait(&mutex)==-1){
                perror("sem_wait14: ");
                exit(EXIT_FAILURE);
            }   
        
            if(isMilk){
                isMilk=0;
                if(sem_post(&MW)==-1){
                    perror("sem_post25: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait14: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();              
            }
            else if(isFlour){
                isFlour=0;
                if(sem_post(&FW)==-1){
                    perror("sem_post26: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait15: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else if(isSugar){
                isSugar=0;
                if(sem_post(&WS)==-1){
                    perror("sem_post27: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait16: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();                
            }
            else
                isWalnuts=1;

            if(sem_post(&mutex)==-1){
                perror("sem_post28: ");
                exit(EXIT_FAILURE);
            }
        }
        else{ /* pusher4 */
            if(sem_wait(&sugar)==-1){
                perror("sem_wait17: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;

            if(sem_wait(&mutex)==-1){
                perror("sem_wait18: ");
                exit(EXIT_FAILURE);
            }   
            
            if(isMilk){
                isMilk=0;
                if(sem_post(&MS)==-1){
                    perror("sem_post29: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait19: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();               
            }
            else if(isFlour){
                isFlour=0;
                if(sem_post(&FS)==-1){
                    perror("sem_post30: ");
                    exit(EXIT_FAILURE);
                }
                
                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait20: ");
                    exit(EXIT_FAILURE);
                } 
                if(flag==1)
                    post();             
            }
            else if(isWalnuts){
                isWalnuts=0;
                if(sem_post(&WS)==-1){
                    perror("sem_post31: ");
                    exit(EXIT_FAILURE);
                }

                if(sem_wait(&semPusher)==-1){
                    perror("sem_wait21: ");
                    exit(EXIT_FAILURE);
                } 

                if(flag==1)
                    post();               
            }
            else
                isSugar=1;
            
            if(sem_post(&mutex)==-1){
                perror("sem_post32: ");
                exit(EXIT_FAILURE);
            }            
        } 
    }  

    return 0;     
}

/*chefs wait for relevant ingrediens
*chef1 waits for milk and flour
*chef2 waits for milk and walnuts
*chef3 waits for milk and sugar
*chef4 waits for flour and walnuts
*chef5 waits for flour and sugar
*chef6 waits for walnuts and sugar
*after preparing gullac chefs wait for wholesaler to read file again, if file does not contains any ingredients anymore, break the loop and exit
*/
void * chef(void *id){
    srand(time(0));
    while(1){
        if(*(int*) id==0){ /* chef1 */
            printf("chef1 is waiting for milk and flour\n");
            if(sem_wait(&MF)==-1){
                perror("sem_wait22: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;
            
            printf("chef1 has taken the milk\n");   
            printf("chef1 has taken the flour\n");
            printf("chef1 is preparing the dessert\n");

            sleep(rand()%5 +1); 
            
            printf("chef1 has delivered the dessert to the wholesaler\n");               
        }
        else if(*(int*) id==1){ /* chef2 */
            printf("chef2 is waiting for milk and walnuts\n");
            if(sem_wait(&MW)==-1){
                perror("sem_wait23: ");
                exit(EXIT_FAILURE);
            } 
            
            if(flag==1)
                break;                
            
            printf("chef2 has taken the milk\n");
            printf("chef2 has taken the walnuts\n");
            printf("chef2 is preparing the dessert\n");

            sleep(rand()%5 +1);            

            printf("chef2 has delivered the dessert to the wholesaler\n");                               
        }
        else if(*(int*) id==2){ /* chef3 */
            printf("chef3 is waiting for milk and sugar\n");
            if(sem_wait(&MS)==-1){
                perror("sem_wait24: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;                
            
            printf("chef3 has taken the milk\n");
            printf("chef3 has taken the sugar\n");
            printf("chef3 is preparing the dessert\n");

            sleep(rand()%5 +1);            

            printf("chef3 has delivered the dessert to the wholesaler\n");               
        }               
        else if(*(int*) id==3){ /* chef4 */
            printf("chef4 is waiting for flour and walnuts\n");
            if(sem_wait(&FW)==-1){
                perror("sem_wait25: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;                
            
            printf("chef4 has taken the flour\n");
            printf("chef4 has taken the walnuts\n");
            printf("chef4 is preparing the dessert\n");
           
            sleep(rand()%5 +1);            
               
            printf("chef4 has delivered the dessert to the wholesaler\n"); 
        }
        else if(*(int*) id==4){ /* chef5 */
            printf("chef5 is waiting for flour and sugar\n");
            if(sem_wait(&FS)==-1){
                perror("sem_wait26: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;                
            
            printf("chef5 has taken the flour\n");
            printf("chef5 has taken the sugar\n");
            printf("chef5 is preparing the dessert\n");

            sleep(rand()%5 +1);            
               
                printf("chef5 has delivered the dessert to the wholesaler\n");               
        }
        else{ /* chef6 */
            printf("chef6 is waiting for walnuts and sugar\n");
            if(sem_wait(&WS)==-1){
                perror("sem_wait27: ");
                exit(EXIT_FAILURE);
            } 

            if(flag==1)
                break;                
            
            printf("chef6 has taken the walnuts\n");
            printf("chef6 has taken the sugar\n");
            printf("chef6 is preparing the dessert\n");
            
            sleep(rand()%5 +1);            
        
            printf("chef6 has delivered the dessert to the wholesaler\n");               
        }

        printf("the wholesaler has obtained the dessert and left to sell it\n");            
        if(sem_post(&gullac)==-1){
            perror("sem_post33: ");
            exit(EXIT_FAILURE);
        }

        if(sem_wait(&semChef)==-1){
            perror("sem_wait27: ");
            exit(EXIT_FAILURE);
        } 

        if(flag==1){
            if(sem_post(&MF)==-1){
                perror("sem_post34: ");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&MW)==-1){
                perror("sem_post35: ");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&MS)==-1){
                perror("sem_post36: ");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&FW)==-1){
                perror("sem_post37: ");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&FS)==-1){
                perror("sem_post38: ");
                exit(EXIT_FAILURE);
            }
            if(sem_post(&WS)==-1){
                perror("sem_post39: ");
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;
}

void post(){
    if(sem_post(&milk)==-1){
        perror("sem_post40: ");
        exit(EXIT_FAILURE);
    }
    if(sem_post(&flour)==-1){
        perror("sem_post41: ");
        exit(EXIT_FAILURE);
    }
    if(sem_post(&walnuts)==-1){
        perror("sem_post42: ");
        exit(EXIT_FAILURE);
    }
    if(sem_post(&sugar)==-1){
        perror("sem_post43: ");
        exit(EXIT_FAILURE);
    }    
}

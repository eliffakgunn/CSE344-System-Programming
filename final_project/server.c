#include <time.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <net/if.h>
#include <stdint.h>
#include <stdbool.h>
#include <ifaddrs.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>


/*******GRAPH*******/

struct Graph {
    int node;
    int edge;
    struct Node** adjArr;
    int* visited;
};

struct Node{
    int node;
    struct Node* next;
};

struct Queue {
    int front;
    int rear;
    int *qu; 
};

struct Queue* createQueue();
void addToQueue(struct Queue* queue, int index);
int removeFromQueue(struct Queue* queue);
int isEmpty(struct Queue* queue);
int *BFS(struct Graph* graph, int src, int dest);
struct Node* createNode(int v);
struct Graph* createGraph(int node);
void addEdge(struct Graph* graph, int src, int dest);
int hasNode(int node, int node_count);
int *findPath(int *path);
int hasAdj(int ind, int dest_ind);
struct Graph* graph = NULL;
struct Node** new = NULL;
int counter=0, fd_log=-2;
int *nodes=NULL;

/******************/

static void usageError();
void selectAdrr();
void loadGraph(int fd_input);
void* threadPoolW(void *id); //writer prioritization
void* threadPoolR(void *id); //reader prioritization
void* threadPoolRW(void *id); // writer and reader has same prioritization
void* tracker();
int createSocket();
int *readCache(int src, int dest);
void writeCache(int *path);
void handler(int signal);
void createLockFile();
int daemon_();

int r = -1;
int portNum;
struct sockaddr_in clientSock;
int **cache = NULL;
int fd_sock=-1;
int flag=0;
int busyThrNum=0;
int thrNum, maxThrNum;
int fd_clnt=-1;
int isRunning=1;
int exit_flag=0;
char* logPrint=NULL;
int firstThrNum=0;
int row=1;
int column=1;
int which_row=0;
char *inputFile=NULL;
char *logFile = NULL;

pthread_t *poolThrs = NULL;
pthread_t trackerThr;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condVar = PTHREAD_COND_INITIALIZER;
pthread_cond_t condVar2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t condVar3 = PTHREAD_COND_INITIALIZER;
pthread_cond_t okToRead = PTHREAD_COND_INITIALIZER;
pthread_cond_t okToWrite = PTHREAD_COND_INITIALIZER;

int AR=0;
int AW=0;
int WR=0;
int WW=0;

int main(int argc, char* argv[]){
    int option, fd_input=-2, i=0, thrNum_;
    inputFile=(char*) malloc(50*sizeof(char)); 
    logFile=(char*) malloc(50*sizeof(char));
    logPrint = (char*) malloc(10000*sizeof(char)); // for logFile
    char dir[1024], result[1000]; 
    socklen_t  clientLen = sizeof(struct sockaddr_in); 
    struct timeval t1, t2;
    double t;   
    int *thrID=NULL;
    time_t tt;
    char *time_;

    struct sigaction sa_act;
    sigset_t blocked_signals;

    /* signal mask for SIGINT */
    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGINT);
    sigemptyset(&sa_act.sa_mask);
    sa_act.sa_handler = handler;
    sa_act.sa_flags = 0;
    if(sigaction (SIGINT, &sa_act, NULL)==-1){
        perror("Sigaction: ");
        exit(EXIT_FAILURE);
    }    

    if(argc != 11 && !(argc == 13 && ( strcmp(argv[12], "0") == 0 || strcmp(argv[12] , "1") == 0 || strcmp(argv[12] , "2") == 0 )))
        usageError();     

    new = NULL; 
    memset(result, '\0', 1000);
    memset(logPrint, '\0', 10000);

    while((option = getopt(argc, argv, "iposxr")) != -1){ /*get option from the getopt() method*/
        switch(option){
            case 'i':
                strcpy(inputFile,argv[optind]);               
                break;
            case 'p':
                sscanf(argv[optind], "%d", &portNum);               
                break;
            case 'o':
                strcpy(logFile ,argv[optind]);              
                break;
            case 's':
                sscanf(argv[optind], "%d", &thrNum);               
                break;                                   
            case 'x':
                sscanf(argv[optind], "%d", &maxThrNum); 
                break;
            case 'r':
                sscanf(argv[optind], "%d", &r); 
                break;                                                            
            default: usageError();
        }
    }

    thrNum_ = thrNum;
    firstThrNum = thrNum;

    fd_log=open(logFile, O_WRONLY | O_CREAT,0666);    
    if(fd_log == -1){
        perror("open: ");
        free(inputFile);
        free(logFile);
        if(logPrint != NULL)
            free(logPrint);          
        exit(EXIT_FAILURE);
    }   

    if(thrNum<2 || maxThrNum<2){
        strcat(logPrint,"The number of threads in the pool at startup must be at least 2.\n");
        write(fd_log, logPrint, strlen(logPrint));
        //printf("%s",logPrint);
        free(inputFile);
        free(logFile);
        if(logPrint != NULL)
            free(logPrint);         
        exit(EXIT_FAILURE);
    }


    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_));
    strcat(logPrint," Executing with parameters:\n");
    //printf("%s", logPrint);
    write(fd_log, logPrint, strlen(logPrint));
    memset(logPrint, '\0', strlen(logPrint));
   
    getcwd(dir, sizeof(dir));

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    write(fd_log, logPrint, strlen(logPrint));
    memset(time_, '\0', strlen(time_));    
    write(fd_log, " -i ", 3);
    write(fd_log, dir, strlen(dir));
    write(fd_log, "/", 1);
    write(fd_log, inputFile, strlen(inputFile));
    write(fd_log, "\n", 1);

   /* tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_));  */ 
    strcat(logPrint," -p ");
    sprintf(result, "%d", portNum); 
    strcat(logPrint, result);   
    write(fd_log, logPrint, strlen(logPrint));
    write(fd_log, "\n", 1);
    memset(logPrint, '\0', strlen(logPrint));    

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    write(fd_log, logPrint, strlen(logPrint));
    memset(time_, '\0', strlen(time_));    
    write(fd_log, " -o ", 3);
    write(fd_log, dir, strlen(dir));
    write(fd_log, "/", 1);
    write(fd_log, logFile, strlen(logFile));
    write(fd_log, "\n", 1);     

    /*tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_));   */
    strcat(logPrint," -s ");
    sprintf(result, "%d", thrNum); 
    strcat(logPrint, result);   
    write(fd_log, logPrint, strlen(logPrint));
    write(fd_log, "\n", 1);
    memset(logPrint, '\0', strlen(logPrint)); 

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_));
    strcat(logPrint," -x ");
    sprintf(result, "%d", maxThrNum); 
    strcat(logPrint, result);   
    write(fd_log, logPrint, strlen(logPrint));
    write(fd_log, "\n", 1);
    memset(logPrint, '\0', strlen(logPrint));

    if( r != -1){
        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint," -r ");
        sprintf(result, "%d", r); 
        strcat(logPrint, result);   
        write(fd_log, logPrint, strlen(logPrint));
        write(fd_log, "\n", 1);
        memset(logPrint, '\0', strlen(logPrint));        
    }      

    //printf("-i %s/%s\n", dir, inputFile);
    //printf("-p %d\n", portNum);
    //printf("-o %s/%s\n", dir, logFile);
    //printf("-s %d\n", thrNum);
    //printf("-x %d\n", maxThrNum);

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_));
    strcat(logPrint," Loading graph…\n");
    write(fd_log, logPrint, strlen(logPrint));
    memset(logPrint, '\0', strlen(logPrint));

    //printf("Loading graph…\n");

    gettimeofday(&t1, NULL);

    fd_input = open(inputFile, O_RDONLY);
    if(fd_input == -1){
        //perror("open: ");
        strcat(logPrint,"Input file could not be opened.\n");
        write(fd_log, logPrint, strlen(logPrint));
        isRunning = 0;   
        free(inputFile);
        free(logFile);
        if(logPrint != NULL)
            free(logPrint);              
        exit(EXIT_FAILURE);
    }

    loadGraph(fd_input);  

    daemon_();    
    
    createSocket(); 

    gettimeofday(&t2, NULL);

    t = ((t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec))*1.0 / 1000000.0;    

    if(isRunning){

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint," Graph loaded in ");
        sprintf(result, "%.1lf", t); 
        strcat(logPrint, result);  
        strcat(logPrint," seconds with ");
        sprintf(result, "%d", graph->node); 
        strcat(logPrint, result);      
        strcat(logPrint," nodes and ");
        sprintf(result, "%d", graph->edge); 
        strcat(logPrint, result); 
        strcat(logPrint," edges. \n");       
        write(fd_log, logPrint, strlen(logPrint));
        memset(logPrint, '\0', strlen(logPrint));    

        //printf("Graph loaded in %.1lf seconds with %d nodes and %d edges. \n", t, graph->node, graph->edge);
    
    }

    if(isRunning){

        flag = 1;

        poolThrs = (pthread_t*) malloc(thrNum * sizeof(pthread_t));  
        thrID = (int*) malloc(thrNum * sizeof(int));  

        //printf("A pool of %d threads has been created\n", thrNum); 

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint," A pool of ");
        sprintf(result, "%d", thrNum); 
        strcat(logPrint, result);  
        strcat(logPrint," threads has been created\n");  
        write(fd_log, logPrint, strlen(logPrint));
        memset(logPrint, '\0', strlen(logPrint));   


        for (i=0; i<thrNum; ++i){
            thrID[i] = i;
            if(r == -1 || r == 1){
                if(pthread_create(&poolThrs[i], NULL, threadPoolW, (void *) &thrID[i]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint,"Thread could not be created.\n");
                    write(fd_log, logPrint, strlen(logPrint));             
                    exit(EXIT_FAILURE);
                }
            }
            else if(r == 0){
                if(pthread_create(&poolThrs[i], NULL, threadPoolR, (void *) &thrID[i]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint,"Thread could not be created.\n");
                    write(fd_log, logPrint, strlen(logPrint));             
                    exit(EXIT_FAILURE);
                }
            }  
            else{
                if(pthread_create(&poolThrs[i], NULL, threadPoolRW, (void *) &thrID[i]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint,"Thread could not be created.\n");
                    write(fd_log, logPrint, strlen(logPrint));             
                    exit(EXIT_FAILURE);
                }
            }                     
        }

        if(pthread_create(&trackerThr, NULL, tracker, (void *) NULL) != 0 ){
            perror("pthread_create: ");
            strcat(logPrint,"Thread could not be created.\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }  
    }  

    //while program is running, wait for connection then send signal to thread pool
    while(isRunning){  

        if(pthread_mutex_lock(&mutex) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint,"Mutex could not be locked.\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }  

        if(!isRunning ){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint,"Mutex could not be locked.\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 
            if (pthread_cond_signal(&condVar) !=0 ) {  
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 
            if (pthread_cond_signal(&condVar3) !=0 ) {  
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }                         

            break;
        }    

        fd_clnt = accept(fd_sock, (struct sockaddr *) &clientSock, &clientLen);
        if(fd_clnt == -1 && errno != EINTR) {
            perror("accept:");
            strcat(logPrint,"Accept error!\n");
            write(fd_log, logPrint, strlen(logPrint));            
            isRunning=0;
        }  

        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint,"Mutex could not be locked.\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }  
            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }
            if (pthread_cond_signal(&condVar3) !=0 ) {  
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }                                     
            break;
        }    

        if (pthread_cond_signal(&condVar) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }            

        if(pthread_cond_wait (&condVar2, &mutex) != 0){ 
            perror("pthread_cond_wait2: ");
            strcat(logPrint,"pthread_cond_wait error!\n");
            write(fd_log, logPrint, strlen(logPrint));                  
            exit(EXIT_FAILURE);
        }     

        if(!isRunning){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint,"Mutex could not be locked.\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }              

            break;
        }                    

        if(pthread_mutex_unlock(&mutex) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint,"Mutex could not be locked.\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        } 
   
    }

    if(flag == 1){
        int fd_join;

        for(i=0; i<thrNum_ ; i++){
            while ((fd_join = pthread_join(poolThrs[i], NULL)) != 0 && errno == EINTR)
            if(fd_join == -1){
                perror("pthread_join: ");
                strcat(logPrint,"pthread_join error.\n");
                write(fd_log, logPrint, strlen(logPrint));                 
                exit(EXIT_FAILURE);
            }
        }     
        while ((fd_join = pthread_join(trackerThr, NULL)) != 0 && errno == EINTR)
        if(fd_join == -1){
            perror("pthread_join: ");
            strcat(logPrint,"pthread_join error.\n");
            write(fd_log, logPrint, strlen(logPrint));            
            exit(EXIT_FAILURE);
        }        
    }

    //printf("All threads have terminated, server shutting down.\n");

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    strcat(logPrint, time_);
    memset(time_, '\0', strlen(time_)); 
    strcat(logPrint," All threads have terminated, server shutting down.\n"); 
    write(fd_log, logPrint, strlen(logPrint));
    memset(logPrint, '\0', strlen(logPrint));       

    free(inputFile);
    free(logFile);
    if(logPrint != NULL)
        free(logPrint);
    if(thrID != NULL)
        free(thrID);
    if(poolThrs != NULL)
        free(poolThrs);
    if(nodes != NULL)
        free(nodes);
    if(graph != NULL){
        if(new != NULL){
            for(i=0; i<graph->edge; ++i){
                if(new[i] != NULL)
                    free(new[i]);
            }
            free(new);
        }
        if(cache != NULL){
            for(i=0; i<row; ++i){
                free(cache[i]);
            }
            free(cache);
        }
        free(graph->adjArr);
        free(graph->visited);  
        free(graph);
    }
    if(fd_sock != -1)
        close(fd_sock);
    if(fd_log != -1)
        close(fd_log);
    if(fd_input != -1)
        close(fd_input);
    return 0;
}

static void usageError(){
    printf("Usage: ./server -i <pathToFile> -p <port_num> -o <pathToLogFile> -s <thread_num> -x <max_thread_num>\n");
    printf("-i: denotes the relative or absolute path to an input file containing a directed unweighted\n");
    printf("graph from the Stanford Large Network Dataset Collection\n");
    printf("-p: this is the port number the server will use for incoming connections.\n");
    printf("-o: is the relative or absolute path of the log file to which the server daemon_ will\n");
    printf("write all of its output (normal output & errors).\n");
    printf("-s: this is the number of threads in the pool at startup (at least 2)\n");
    printf("-x : this is the maximum allowed number of threads, the pool must not grow beyond this number\n");
    printf("**BONUS**\n");
    printf("-r 0 : reader prioritization\n");
    printf("-r 1 : writer prioritization\n");
    printf("-r 2 : equal priorities to readers and writers\n");    

    free(inputFile);
    free(logFile);
    if(logPrint != NULL)
        free(logPrint);     
    
    exit(EXIT_FAILURE);
}

// signal handler handles SIGINT then program terminates
void handler(int signal){
    if(signal == SIGINT){
        char *logPrint2 = (char*) malloc(10000*sizeof(char)); // for logFile
        strcpy(logPrint2, logPrint);
        memset(logPrint2, '\0', strlen(logPrint2)); 
        time_t tt;   
        char *time_;                  

        //printf("Termination signal received, waiting for ongoing threads to complete.\n");
        isRunning=0;

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," Termination signal received, waiting for ongoing threads to complete.\n");         
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));         

        if (pthread_cond_signal(&condVar3) !=0 ) {
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        } 

        free(logPrint2);
    }
}

//all threads wait signal from main thread, then finds path and respomse to client
//it implemented by readers/writers paradigm, by prioritizing writers.
void* threadPoolW(void *id){
    char result[100]="c";
    int arr[2]={0,0}, i=0, nodeNum=0, fd=-1;
    double load=0.0;
    int *path = NULL;
    int *absPath = NULL;
    char *logPrint2 = (char*) malloc(10000*sizeof(char)); // for logFile
    int busyThrNum_=0, thrNum_=0;
    time_t tt;
    char *time_;    

    strcpy(logPrint2, logPrint);
    memset(logPrint2, '\0', strlen(logPrint2));    

    //while program is running, threads wait for request
    while(isRunning){
        //printf("Thread #%d: waiting for connection\n", *(int*)id);

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));       
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": waiting for connection\n");  
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));    

        if(pthread_mutex_lock(&mutex) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        } 

        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }             

            break;
        } 

        while(fd_clnt < 0){

            if(pthread_cond_wait (&condVar, &mutex) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                  
                exit(EXIT_FAILURE);
            }          

            if(!isRunning){
                if(fd_clnt != -1){               
                    fd= fd_clnt;
                    fd_clnt = -1;
                    exit_flag = 1;
                }

                break;                     
            } 
        }   
   
        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }  

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }                                         
            
            break;        
        }

        if(exit_flag == 0){
            fd=fd_clnt;
            fd_clnt = -1; 
        }              

        //notify main thread that took request
        if (pthread_cond_signal(&condVar2) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }   

        if(pthread_mutex_unlock(&mutex) !=0 ){ 
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }

        //printf("A connection has been delegated to thread id #%d, system load %.1lf %%\n", *(int*)id, load);  

        //sleep(10);

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }                  

        ++busyThrNum;
        load = 100*busyThrNum/thrNum;

        busyThrNum_ = busyThrNum;
        thrNum_ = thrNum;        

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," A connection has been delegated to thread id #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,", system load "); 
        sprintf(result, "%.1lf", load); 
        strcat(logPrint2, result);      
        strcat(logPrint2,"%%\n");           
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));

        //send signal to tracker thread to check system load
        if (pthread_cond_signal(&condVar3) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        } 

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }           

        if(busyThrNum_ == thrNum_){
            //printf("No thread is available! Waiting for one.\n");
            
            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));            
            strcat(logPrint2," No thread is available! Waiting for one.\n");         
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));             
        }          

        //read src and desr nodes
        if(read(fd, arr, sizeof(arr)) == -1){
            perror("read1: ");
            strcat(logPrint2,"read error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);            
        }             
    
        //read and write to db according to reader/writer paradigm by prioritizing writers
        if(pthread_mutex_lock(&m) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        }      

        while( (AW + WW) > 0){
            ++WR;

            if(pthread_cond_wait (&okToRead, &m) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                  
                exit(EXIT_FAILURE);
            } 

            --WR;            
        }

        ++AR;

        if(pthread_mutex_unlock(&m) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }        

        //printf("Thread #%d: searching database for a path from node %d to node %d\n", *(int*)id, arr[0], arr[1]);          

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": searching database for a path from node "); 
        sprintf(result, "%d", arr[0]); 
        strcat(logPrint2, result);   
        strcat(logPrint2," to node "); 
        sprintf(result, "%d", arr[1]); 
        strcat(logPrint2, result);           
        strcat(logPrint2,"\n");       
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));  

        //read cache
        path = readCache(arr[0], arr[1]);

        if(pthread_mutex_lock(&m) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        } 
  
        --AR;

        if(AR == 0 && WW > 0){

            if (pthread_cond_signal(&okToWrite) !=0 ) {
                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_unlock9: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                     
                    exit(EXIT_FAILURE);
                }   
                perror("pthread_cond_signal1: ");
                strcat(logPrint2,"pthread_cond_signal error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                
                exit(EXIT_FAILURE);
            }            
        }

        if(pthread_mutex_unlock(&m) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        } 

        //if cache has this path
        if(path != NULL){
            //printf("Thread #%d: path found in database: ", *(int*)id);

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));            
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": path found in database: ");     

            for(i=0; i<path[0]; ++i){
                if(i != path[0]-1){
                    //printf("%d->", path[i+1]);
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"->");  
                }                        
                else{
                    //printf("%d\n", path[i+1]);  
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"\n");                                            
                }
            }

            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2)); 

            //printf("Thread #%d: responding to client\n", *(int*)id); ///////////////////8 kendim yzdım           

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));            
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": responding to client\n"); 
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));            

            //response to client
            if(write(fd, &path[0], sizeof(nodeNum)) == -1){
                perror("server write1: ");
                strcat(logPrint2,"write error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));
                exit(EXIT_FAILURE);                   
            }

            for(i=0; i<path[0]; ++i){
                if(write(fd, &path[i+1], sizeof(int)) < 0){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));    
                    exit(EXIT_FAILURE);                         
                }
            }

            free(path);
            close(fd);

        }
        else{ //path is not in db
            //printf("Thread #%d: no path in database, calculating %d->%d\n", *(int*)id, arr[0], arr[1]);          
            
            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));             
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": no path in database, calculating "); 
            sprintf(result, "%d", arr[0]); 
            strcat(logPrint2, result);   
            strcat(logPrint2,"->"); 
            sprintf(result, "%d", arr[1]); 
            strcat(logPrint2, result);           
            strcat(logPrint2,"\n");           
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));   

            path = BFS(graph, arr[0], arr[1]); 

            if(path != NULL)      
                absPath = findPath(path);

            //if graph has this path
            if(path != NULL){
                //printf("Thread #%d: path calculated: ", *(int*)id);                

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));                
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path calculated: ");                

                for(i=0; i<absPath[0]; ++i){
                    if(i != absPath[0]-1){
                        //printf("%d->", absPath[i+1]);    
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"->");                         
                    }
                    else{
                        //printf("%d\n", absPath[i+1]);                        
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"\n");                        
                    }
                } 

                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                                                 

                //printf("Thread #%d: responding to client and adding path to database\n", *(int*)id); 

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));                 
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": responding to client and adding path to database\n"); 
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                  

                //response to client
                if(write(fd, &absPath[0], sizeof(int)) == -1){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2)); 
                    exit(EXIT_FAILURE);                    
                }

                for(i=0; i<absPath[0]; ++i){

                    if(write(fd, &absPath[i+1], sizeof(int)) == -1){
                        perror("server write1: "); 
                        strcat(logPrint2,"write error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2)); 
                        exit(EXIT_FAILURE);                              
                    }
                }

                close(fd);

                if(pthread_mutex_lock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_lock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                } 

                while( (AW + AR) > 0){

                    ++WW;                   

                    if(pthread_cond_wait (&okToWrite, &m) != 0){
                        perror("pthread_cond_wait2: ");
                        strcat(logPrint2,"pthread_cond_wait error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                        
                        exit(EXIT_FAILURE);
                    }   

                    --WW;                 
                }                 

                ++AW;

                //sleep(5);

                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                } 

                //write cache
                writeCache(absPath);
                free(path);  
                free(absPath);  

                if(pthread_mutex_lock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_lock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                }

                --AW;

                if(WW > 0){                 

                    if(pthread_cond_signal(&okToWrite) !=0 ) {
                        if(pthread_mutex_unlock(&m) !=0 ){
                            perror("pthread_mutex_unlock9: ");
                            strcat(logPrint2,"pthread_mutex_unlock error!\n");
                            write(fd_log, logPrint2, strlen(logPrint2));                            
                            exit(EXIT_FAILURE);
                        }   
                        perror("pthread_cond_signal1: ");
                        strcat(logPrint2,"pthread_cond_signal error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                         
                        exit(EXIT_FAILURE);
                    }                     
                }
                else if( WR > 0){

                    if(pthread_cond_broadcast(&okToRead) != 0){
                        perror("pthread_cond_broadcast: ");
                        strcat(logPrint2,"pthread_cond_broadcast error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                        
                        exit(EXIT_FAILURE);                        
                    }
                }

                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                }                 

            }
            else{ //if graph does not include this path
                //printf("Thread #%d: path not possible from node %d to %d\n", *(int*)id, arr[0], arr[1]);            

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));               
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path not possible from node "); 
                sprintf(result, "%d", arr[0]); 
                strcat(logPrint2, result);   
                strcat(logPrint2," to "); 
                sprintf(result, "%d", arr[1]); 
                strcat(logPrint2, result);           
                strcat(logPrint2,"\n");           
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                

                nodeNum=-1;

                if (write(fd, &nodeNum, sizeof(nodeNum)) == -1){
                    perror("server write1: "); 
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                                    
                }

                close(fd);
            }           
        }     

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        }

        --busyThrNum;

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        } 
    }

    free(logPrint2);

    return NULL;
}

//all threads wait signal from main thread, then finds path and respomse to client
//it implemented by readers/writers paradigm, by prioritizing readers
void* threadPoolR(void *id){
    char result[100]="c";
    int arr[2]={0,0}, i=0, nodeNum=0, fd=-1;
    double load=0.0;
    int *path = NULL;
    int *absPath = NULL;
    char *logPrint2 = (char*) malloc(10000*sizeof(char)); // for logFile
    int busyThrNum_=0, thrNum_=0;
    time_t tt;    
    char *time_;    

    strcpy(logPrint2, logPrint);
    memset(logPrint2, '\0', strlen(logPrint2));    

    //while program is running, threads wait for request
    while(isRunning){
        //printf("Thread #%d: waiting for connection\n", *(int*)id);

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": waiting for connection\n");  
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));    

        if(pthread_mutex_lock(&mutex) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        } 

        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }             

            break;
        } 

        while(fd_clnt < 0){

            if(pthread_cond_wait (&condVar, &mutex) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                  
                exit(EXIT_FAILURE);
            }          

            if(!isRunning){
                if(fd_clnt != -1){                
                    fd= fd_clnt;
                    fd_clnt = -1;
                    exit_flag = 1;
                }

                break;                     
            } 
        }   
   
        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }  

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }                                         
            
            break;        
        }

        if(exit_flag == 0){
            fd=fd_clnt;
            fd_clnt = -1; 
        }              

        //notify main thread that took request
        if (pthread_cond_signal(&condVar2) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }   

        if(pthread_mutex_unlock(&mutex) !=0 ){ 
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }

        //printf("A connection has been delegated to thread id #%d, system load %.1lf %%\n", *(int*)id, load);  

        //sleep(10);

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }                  

        ++busyThrNum;
        load = 100*busyThrNum/thrNum;

        busyThrNum_ = busyThrNum;
        thrNum_ = thrNum;        

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));         
        strcat(logPrint2," A connection has been delegated to thread id #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,", system load "); 
        sprintf(result, "%.1lf", load); 
        strcat(logPrint2, result);      
        strcat(logPrint2,"%%\n");           
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));

        //send signal to tracker thread to check system load
        if (pthread_cond_signal(&condVar3) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        } 

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }           

        if(busyThrNum_ == thrNum_){
            //printf("No thread is available! Waiting for one.\n");
            
            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));  
            strcat(logPrint2, ctime(&tt));             
            strcat(logPrint2," No thread is available! Waiting for one.\n");         
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));             
        }          

        //read and write to db according to reader/writer paradigm by prioritizing readers
        if(read(fd, arr, sizeof(arr)) == -1){
            perror("read1: ");
            strcat(logPrint2,"read error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);            
        }             
   
        if(pthread_mutex_lock(&m) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        }      

        while(AW > 0){
            ++WR;

            if(pthread_cond_wait (&okToRead, &m) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                  
                exit(EXIT_FAILURE);
            } 

            --WR;            
        }

        ++AR;

        if(pthread_mutex_unlock(&m) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }        

        //printf("Thread #%d: searching database for a path from node %d to node %d\n", *(int*)id, arr[0], arr[1]);          

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));         
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": searching database for a path from node "); 
        sprintf(result, "%d", arr[0]); 
        strcat(logPrint2, result);   
        strcat(logPrint2," to node "); 
        sprintf(result, "%d", arr[1]); 
        strcat(logPrint2, result);           
        strcat(logPrint2,"\n");       
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));  

        //read cache
        path = readCache(arr[0], arr[1]);

        if(pthread_mutex_lock(&m) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        } 
  
        --AR;

        if(AR == 0 && WR == 0 && WW > 0){

            if (pthread_cond_signal(&okToWrite) !=0 ) {
                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_unlock9: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                     
                    exit(EXIT_FAILURE);
                }   
                perror("pthread_cond_signal1: ");
                strcat(logPrint2,"pthread_cond_signal error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                
                exit(EXIT_FAILURE);
            }            
        }

        if(pthread_mutex_unlock(&m) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        } 

        //if cache has this path
        if(path != NULL){
            //printf("Thread #%d: path found in database: ", *(int*)id);

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));           
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": path found in database: ");     

            for(i=0; i<path[0]; ++i){
                if(i != path[0]-1){
                    //printf("%d->", path[i+1]);
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"->");  
                }                        
                else{
                    //printf("%d\n", path[i+1]);  
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"\n");                                            
                }
            }

            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2)); 

            //printf("Thread #%d: responding to client\n", *(int*)id); ///////////////////8 kendim yzdım           
          
            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));            
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": responding to client\n"); 
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));     
    
            //response to client
            if(write(fd, &path[0], sizeof(nodeNum)) == -1){
                perror("server write1: ");
                strcat(logPrint2,"write error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));
                exit(EXIT_FAILURE);                   
            }

            for(i=0; i<path[0]; ++i){
                if(write(fd, &path[i+1], sizeof(int)) < 0){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));    
                    exit(EXIT_FAILURE);                         
                }
            }

            free(path);
            close(fd);

        }
        else{ //path is not in db
            //printf("Thread #%d: no path in database, calculating %d->%d\n", *(int*)id, arr[0], arr[1]);          

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));           
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": no path in database, calculating "); 
            sprintf(result, "%d", arr[0]); 
            strcat(logPrint2, result);   
            strcat(logPrint2," to node "); 
            sprintf(result, "%d", arr[1]); 
            strcat(logPrint2, result);           
            strcat(logPrint2,"\n");           
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));   

            path = BFS(graph, arr[0], arr[1]); 

            if(path != NULL)      
                absPath = findPath(path);

            //if graph has this path
            if(path != NULL){
                //printf("Thread #%d: path calculated: ", *(int*)id);                

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));                
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path calculated: ");                

                for(i=0; i<absPath[0]; ++i){
                    if(i != absPath[0]-1){
                        //printf("%d->", absPath[i+1]);    
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"->");                         
                    }
                    else{
                        //printf("%d\n", absPath[i+1]);                        
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"\n");                        
                    }
                } 

                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                                                 

                //printf("Thread #%d: responding to client and adding path to database\n", *(int*)id); 

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));                
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": responding to client and adding path to database\n"); 
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                   

                //response to client
                if(write(fd, &absPath[0], sizeof(int)) == -1){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2)); 
                    exit(EXIT_FAILURE);                    
                }

                for(i=0; i<absPath[0]; ++i){

                    if(write(fd, &absPath[i+1], sizeof(int)) == -1){
                        perror("server write1: "); 
                        strcat(logPrint2,"write error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2)); 
                        exit(EXIT_FAILURE);                              
                    }
                }

                close(fd);

                if(pthread_mutex_lock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_lock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                } 

                while( (AW + AR + WR) > 0){

                    ++WW;                   

                    if(pthread_cond_wait (&okToWrite, &m) != 0){
                        perror("pthread_cond_wait2: ");
                        strcat(logPrint2,"pthread_cond_wait error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                        
                        exit(EXIT_FAILURE);
                    }   

                    --WW;                 
                }                 

                ++AW;

                //sleep(5);

                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                } 

                //write cache
                writeCache(absPath);
                free(path);  
                free(absPath);  

                if(pthread_mutex_lock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_lock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                }

                --AW;

                if( WR > 0){

                    if(pthread_cond_broadcast(&okToRead) != 0){
                        perror("pthread_cond_broadcast: ");
                        strcat(logPrint2,"pthread_cond_broadcast error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                        
                        exit(EXIT_FAILURE);                        
                    }
                }                
                else if(WW > 0){                 

                    if(pthread_cond_signal(&okToWrite) !=0 ) {
                        if(pthread_mutex_unlock(&m) !=0 ){
                            perror("pthread_mutex_unlock9: ");
                            strcat(logPrint2,"pthread_mutex_unlock error!\n");
                            write(fd_log, logPrint2, strlen(logPrint2));                            
                            exit(EXIT_FAILURE);
                        }   
                        perror("pthread_cond_signal1: ");
                        strcat(logPrint2,"pthread_cond_signal error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2));                         
                        exit(EXIT_FAILURE);
                    }                     
                }

                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                }                 

            }
            else{ //if graph does not include this path
                //printf("Thread #%d: path not possible from node %d to %d\n", *(int*)id, arr[0], arr[1]);            

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));               
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path not possible from node "); 
                sprintf(result, "%d", arr[0]); 
                strcat(logPrint2, result);   
                strcat(logPrint2," to "); 
                sprintf(result, "%d", arr[1]); 
                strcat(logPrint2, result);           
                strcat(logPrint2,"\n");           
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                

                nodeNum=-1;

                if (write(fd, &nodeNum, sizeof(nodeNum)) == -1){
                    perror("server write1: "); 
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                                    
                }

                close(fd);
            }           
        }     

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        }

        --busyThrNum;

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        } 
    }

    free(logPrint2);

    return NULL;
}

//all threads wait signal from main thread, then finds path and respomse to client
//it implemented by readers/writers paradigm, their priority is equal 
void* threadPoolRW(void *id){
    char result[100]="c";
    int arr[2]={0,0}, i=0, nodeNum=0, fd=-1;
    double load=0.0;
    int *path = NULL;
    int *absPath = NULL;
    char *logPrint2 = (char*) malloc(10000*sizeof(char)); // for logFile
    int busyThrNum_=0, thrNum_=0;
    time_t tt;    
    char *time_;    

    strcpy(logPrint2, logPrint);
    memset(logPrint2, '\0', strlen(logPrint2));    

    //while program is running, threads wait for request
    while(isRunning){
        //printf("Thread #%d: waiting for connection\n", *(int*)id);

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));         
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": waiting for connection\n");  
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));    

        if(pthread_mutex_lock(&mutex) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        } 

        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }             

            break;
        } 

        while(fd_clnt < 0){

            if(pthread_cond_wait (&condVar, &mutex) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                  
                exit(EXIT_FAILURE);
            }    

            if(!isRunning){
                if(fd_clnt != -1){                  
                    fd= fd_clnt;
                    fd_clnt = -1;
                    exit_flag = 1;
                }

                break;                     
            }         
        }   
   
        if(!isRunning && fd_clnt == -1){
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }  

            if (pthread_cond_signal(&condVar) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar2) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            } 

            if (pthread_cond_signal(&condVar3) !=0 ) {
                perror("pthread_cond_signal1: ");
                strcat(logPrint,"pthread_cond_signal error!\n");
                write(fd_log, logPrint, strlen(logPrint));             
                exit(EXIT_FAILURE);
            }                                         
            
            break;        
        }

        if(exit_flag == 0){
            fd=fd_clnt;
            fd_clnt = -1; 
        }              

        //notify main thread that took request
        if (pthread_cond_signal(&condVar2) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }   

        if(pthread_mutex_unlock(&mutex) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }

        //printf("A connection has been delegated to thread id #%d, system load %.1lf %%\n", *(int*)id, load);  

        //sleep(10);

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }                  

        ++busyThrNum;
        load = 100*busyThrNum/thrNum;

        busyThrNum_ = busyThrNum;
        thrNum_ = thrNum;        

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," A connection has been delegated to thread id #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,", system load "); 
        sprintf(result, "%.1lf", load); 
        strcat(logPrint2, result);      
        strcat(logPrint2,"%%\n");           
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));

        //send signal to tracker thread to check system load
        if (pthread_cond_signal(&condVar3) !=0 ) {
            if(pthread_mutex_unlock(&mutex) !=0 ){
                perror("pthread_mutex_unlock9: ");
                strcat(logPrint,"pthread_mutex_unlock error!\n");
                write(fd_log, logPrint, strlen(logPrint));                
                exit(EXIT_FAILURE);
            }  
            perror("pthread_cond_signal1: ");
            strcat(logPrint,"pthread_cond_signal error!\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        } 

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"Mutex could not be locked.\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);
        }           

        if(busyThrNum_ == thrNum_){
            //printf("No thread is available! Waiting for one.\n");
            
            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));             
            strcat(logPrint2," No thread is available! Waiting for one.\n");         
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));             
        }          

        //read and write to db. writers and readers priority are equal 
        if(read(fd, arr, sizeof(arr)) == -1){
            perror("read1: ");
            strcat(logPrint2,"read error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));             
            exit(EXIT_FAILURE);            
        }             
   
        if(pthread_mutex_lock(&m) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        }          

        //printf("Thread #%d: searching database for a path from node %d to node %d\n", *(int*)id, arr[0], arr[1]);          

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," Thread #");
        sprintf(result, "%d", *(int*)id); 
        strcat(logPrint2, result);  
        strcat(logPrint2,": searching database for a path from node "); 
        sprintf(result, "%d", arr[0]); 
        strcat(logPrint2, result);   
        strcat(logPrint2," to node "); 
        sprintf(result, "%d", arr[1]); 
        strcat(logPrint2, result);           
        strcat(logPrint2,"\n");       
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));  

        //read cache
        path = readCache(arr[0], arr[1]);

        if(pthread_mutex_unlock(&m) !=0 ){
            perror("pthread_mutex_unlock12: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));              
            exit(EXIT_FAILURE);
        } 

        //if cache has this path
        if(path != NULL){
            //printf("Thread #%d: path found in database: ", *(int*)id);

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));             
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": path found in database: ");     

            for(i=0; i<path[0]; ++i){
                if(i != path[0]-1){
                    //printf("%d->", path[i+1]);
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"->");  
                }                        
                else{
                    //printf("%d\n", path[i+1]);  
                    sprintf(result, "%d", path[i+1]); 
                    strcat(logPrint2, result); 
                    strcat(logPrint2,"\n");                                            
                }
            }

            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2)); 

            //printf("Thread #%d: responding to client\n", *(int*)id); ///////////////////8 kendim yzdım           

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));             
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": responding to client\n"); 
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));          

            //response to client
            if(write(fd, &path[0], sizeof(nodeNum)) == -1){
                perror("server write1: ");
                strcat(logPrint2,"write error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));
                exit(EXIT_FAILURE);                   
            }

            for(i=0; i<path[0]; ++i){
                if(write(fd, &path[i+1], sizeof(int)) < 0){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));    
                    exit(EXIT_FAILURE);                         
                }
            }

            free(path);
            close(fd);

        }
        else{ //path is not in db
            //printf("Thread #%d: no path in database, calculating %d->%d\n", *(int*)id, arr[0], arr[1]);          

            tt=time(NULL);
            time_ = ctime(&tt);
            time_[strlen(time_) -1] = '\0'; 
            strcat(logPrint2, time_);
            memset(time_, '\0', strlen(time_));             
            strcat(logPrint2," Thread #");
            sprintf(result, "%d", *(int*)id); 
            strcat(logPrint2, result);  
            strcat(logPrint2,": no path in database, calculating "); 
            sprintf(result, "%d", arr[0]); 
            strcat(logPrint2, result);   
            strcat(logPrint2," to node "); 
            sprintf(result, "%d", arr[1]); 
            strcat(logPrint2, result);           
            strcat(logPrint2,"\n");           
            write(fd_log, logPrint2, strlen(logPrint2));
            memset(logPrint2, '\0', strlen(logPrint2));   

            path = BFS(graph, arr[0], arr[1]); 

            if(path != NULL)      
                absPath = findPath(path);

            //if graph has this path
            if(path != NULL){
                //printf("Thread #%d: path calculated: ", *(int*)id);                

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));                 
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path calculated: ");                

                for(i=0; i<absPath[0]; ++i){
                    if(i != absPath[0]-1){
                        //printf("%d->", absPath[i+1]);    
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"->");                         
                    }
                    else{
                        //printf("%d\n", absPath[i+1]);                        
                        sprintf(result, "%d", absPath[i+1]); 
                        strcat(logPrint2, result); 
                        strcat(logPrint2,"\n");                        
                    }
                } 

                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                                                 

                //printf("Thread #%d: responding to client and adding path to database\n", *(int*)id); 

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));               
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": responding to client and adding path to database\n"); 
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                       

                //response to client
                if(write(fd, &absPath[0], sizeof(int)) == -1){
                    perror("server write1: ");
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2)); 
                    exit(EXIT_FAILURE);                    
                }

                for(i=0; i<absPath[0]; ++i){

                    if(write(fd, &absPath[i+1], sizeof(int)) == -1){
                        perror("server write1: "); 
                        strcat(logPrint2,"write error!\n");
                        write(fd_log, logPrint2, strlen(logPrint2)); 
                        exit(EXIT_FAILURE);                              
                    }
                }

                close(fd);

                if(pthread_mutex_lock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_lock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                } 

                //write cache
                writeCache(absPath);
                free(path);  
                free(absPath);  

                if(pthread_mutex_unlock(&m) !=0 ){
                    perror("pthread_mutex_lock6: ");
                    strcat(logPrint2,"pthread_mutex_unlock error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                    
                    exit(EXIT_FAILURE);
                }                 

            }
            else{ //if graph does not include this path
                //printf("Thread #%d: path not possible from node %d to %d\n", *(int*)id, arr[0], arr[1]);            

                tt=time(NULL);
                time_ = ctime(&tt);
                time_[strlen(time_) -1] = '\0'; 
                strcat(logPrint2, time_);
                memset(time_, '\0', strlen(time_));               
                strcat(logPrint2," Thread #");
                sprintf(result, "%d", *(int*)id); 
                strcat(logPrint2, result);  
                strcat(logPrint2,": path not possible from node "); 
                sprintf(result, "%d", arr[0]); 
                strcat(logPrint2, result);   
                strcat(logPrint2," to "); 
                sprintf(result, "%d", arr[1]); 
                strcat(logPrint2, result);           
                strcat(logPrint2,"\n");           
                write(fd_log, logPrint2, strlen(logPrint2));
                memset(logPrint2, '\0', strlen(logPrint2));                

                nodeNum=-1;

                if (write(fd, &nodeNum, sizeof(nodeNum)) == -1){
                    perror("server write1: "); 
                    strcat(logPrint2,"write error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));                                    
                }

                close(fd);
            }           
        }     

        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        }

        --busyThrNum;

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_unlock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));                    
            exit(EXIT_FAILURE);
        } 
    }

    free(logPrint2);

    return NULL;
}

//tracks the load of pool, if it increase 75% then enlarges it by 25%.
void* tracker(){
    int i, num, percent, counter=0, k=0;
    char *logPrint2 = (char*) malloc(10000*sizeof(char)); // for logFile 
    strcpy(logPrint2, logPrint);
    memset(logPrint2, '\0', strlen(logPrint2));  
    char result[10000];  
    time_t tt;
    char *time_;    

    pthread_t *newThrs = (pthread_t*) malloc((maxThrNum- firstThrNum)*sizeof(pthread_t)); 
    int *newIDs = (int*) malloc((maxThrNum- firstThrNum)*sizeof(int)); 
  

    while(isRunning){
        if(pthread_mutex_lock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error!\n");
            write(fd_log, logPrint2, strlen(logPrint2));            
            exit(EXIT_FAILURE);
        }  

        percent=100*busyThrNum/thrNum;
        num=thrNum*25/100;

        while(percent < 75 || (thrNum+num) > maxThrNum || num == 0){
            if(pthread_cond_wait (&condVar3, &mtx) != 0){
                perror("pthread_cond_wait2: ");
                strcat(logPrint2,"pthread_cond_wait error!\n");
                write(fd_log, logPrint2, strlen(logPrint2));                 
                exit(EXIT_FAILURE);
            } 

            if(!isRunning){
                if (pthread_cond_signal(&condVar) !=0 ) {
                    perror("pthread_cond_signal1: ");
                    strcat(logPrint2,"pthread_cond_signal error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));             
                    exit(EXIT_FAILURE);
                } 

                if (pthread_cond_signal(&condVar2) !=0 ) {
                    perror("pthread_cond_signal1: ");
                    strcat(logPrint2,"pthread_cond_signal error!\n");
                    write(fd_log, logPrint2, strlen(logPrint2));             
                    exit(EXIT_FAILURE);
                }                

                break;
            }

            percent=100*busyThrNum/thrNum;
            num=thrNum*25/100;            
        }

        if(!isRunning){
            if(pthread_mutex_unlock(&mtx) !=0 ){
                perror("pthread_mutex_lock6: ");
                strcat(logPrint2,"Mutex could not be locked.\n");
                write(fd_log, logPrint2, strlen(logPrint2));             
                exit(EXIT_FAILURE);
            }    
            
            break;        
        }  

        //printf("System load %d%%, pool extended to %d threads\n", percent, num+thrNum);   

        tt=time(NULL);
        time_ = ctime(&tt);
        time_[strlen(time_) -1] = '\0'; 
        strcat(logPrint2, time_);
        memset(time_, '\0', strlen(time_));        
        strcat(logPrint2," System load ");
        sprintf(result, "%d", percent); 
        strcat(logPrint2, result);  
        strcat(logPrint2,"%%, pool extended to "); 
        sprintf(result, "%d",  num+thrNum); 
        strcat(logPrint2, result);      
        strcat(logPrint2," threads\n");
        write(fd_log, logPrint2, strlen(logPrint2));
        memset(logPrint2, '\0', strlen(logPrint2));                    

        for(i=0; i<num; ++i){
            newIDs[k]=thrNum+i;

            if(r == -1 || r == 1){
                if(pthread_create(&newThrs[k], NULL, threadPoolW, (void *) &newIDs[k]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint2,"Thread could not be created.\n");
                    write(fd_log, logPrint2, strlen(logPrint2));             
                    exit(EXIT_FAILURE);
                }   
            }
            else if(r == 0){
                if(pthread_create(&newThrs[k], NULL, threadPoolR, (void *) &newIDs[k]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint2,"Thread could not be created.\n");
                    write(fd_log, logPrint2, strlen(logPrint2));             
                    exit(EXIT_FAILURE);
                }   
            }    
            else{
                if(pthread_create(&newThrs[k], NULL, threadPoolRW, (void *) &newIDs[k]) != 0 ){
                    perror("pthread_create: ");
                    strcat(logPrint2,"Thread could not be created.\n");
                    write(fd_log, logPrint2, strlen(logPrint2));             
                    exit(EXIT_FAILURE);
                }   
            }                    
            ++k;  
            ++counter;   
        }

        thrNum += num;

        if(pthread_mutex_unlock(&mtx) !=0 ){
            perror("pthread_mutex_lock6: ");
            strcat(logPrint2,"pthread_mutex_lock error.\n");
            write(fd_log, logPrint2, strlen(logPrint2));            
            exit(EXIT_FAILURE);
        } 
    }

    int fd_join;
    for(i=0; i<counter ; i++){
        while ((fd_join = pthread_join(newThrs[i], NULL)) != 0 && errno == EINTR)
        if(fd_join == -1){
            perror("pthread_join: ");
            strcat(logPrint,"pthread_join error.\n");
            write(fd_log, logPrint, strlen(logPrint));            
            exit(EXIT_FAILURE);
        }
    }  

    if(newIDs != NULL)
        free(newIDs);
    if(newThrs != NULL)
        free(newThrs);
    free(logPrint2);

    pthread_exit(NULL);

    return NULL;
}

//creates socket and lissten
int createSocket(){
    struct sockaddr_in serverSock;
    memset(&serverSock, 0, sizeof(serverSock));

    serverSock.sin_family = AF_INET;
    serverSock.sin_port = htons(portNum);

    fd_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_sock == -1){
        perror("socket: ");
        strcat(logPrint,"Socket error.\n");
        write(fd_log, logPrint, strlen(logPrint));         
        memset(logPrint, '\0', strlen(logPrint));

        return -1;
    }

    if(bind(fd_sock, (struct sockaddr *) &serverSock, sizeof(serverSock)) == -1){
        perror("bind: ");
        strcat(logPrint,"Bind error.\n");
        write(fd_log, logPrint, strlen(logPrint));         
        memset(logPrint, '\0', strlen(logPrint));

        return -1;
    }

    if(listen(fd_sock, 10) == -1){
        perror("listen: ");
        strcat(logPrint,"Listen error.\n");
        write(fd_log, logPrint, strlen(logPrint));         
        memset(logPrint, '\0', strlen(logPrint));

        return -1;
    }

    return 0;
 
}

//reades cache, if it has path, it returns path; if it has not, it returns NULL
int *readCache(int src, int dest){
    int i, j, index1=-1, index2= -1, temp=0;
    int *path;

    for(i=0; i<row; ++i){
        if(cache[i][0] == src){
            index1 = i;
            for(j=0; j<column; ++j){
                if(j!= column -1 && cache[i][j] == dest && cache[i][j+1] == -1 ){
                    index2 = j;
                    temp = 1;
                    break;
                }
                else if(j == column -1 && cache[i][j] == dest){
                    index2 = j;
                    temp =1;
                    break;
                }
            }

            if( temp == 1)
                break;
        }
    }  

    if( temp == 1){
        path = (int *) malloc((index2+2) * sizeof(int));

        path[0]=index2+1;         
        for(i=0; i<index2+1; ++i)
            path[i+1] = cache[index1][i];
        
        return path;
    }

    return NULL; 
}

//writes cache
void writeCache(int *path){
    int i, j;

    if( which_row == row){
        cache = (int **)realloc(cache, (row+1)* sizeof(int **));
        cache[row] = (int *) malloc(column * sizeof(int));

        for(i=row; i<row+1; ++i)
            for(j=0; j<column; ++j)
                cache[i][j] = -1;

        row +=1 ;                 
    }

    if(path[0] > column){
        for(i=0; i<row; ++i)
            cache[i] = (int *)realloc(cache[i], sizeof(int)* path[0]);        

        for(i=0; i<row-1; ++i){
            for(j=column; j<path[0]; ++j){
                cache[i][j]=-1;
            }
        }

        for(i=0; i<path[0]; ++i)
            cache[row-1][i]=-1 ;       

        column = path[0];         
    }    

    for(i=0; i<row; ++i){
        if(cache[i][0] == -1){ 
            for(j=0; j<path[0]; ++j)
                cache[i][j] = path[j+1];
        }
    }
    ++which_row;
}

//read the file then load the grapf to memory
void loadGraph(int fd_input){
    int i=0, j=0, node=0, edge=0, flag=0, vrtx1=0, vrtx2=0, fd_read, ind=0, size=100;
    char nod[1000], edg[1000], ch[1], v1[1000], v2[1000];
    char b='b';

    nodes = (int *) malloc(100 * sizeof(int));

    memset(nod, '\0', 1000);
    memset(edg, '\0', 1000);    
    memset(v1, '\0', 1000);    
    memset(v2, '\0', 1000);  
    memset(ch, '\0', 1);  

    while(isRunning){
        if(read(fd_input,ch,1) == -1){
            perror("read input: ");
            strcat(logPrint,"Reading error.\n");
            write(fd_log, logPrint, strlen(logPrint));            
            exit(EXIT_FAILURE);
        }

        if(ch[0] != '#' && ( b == '\n'||  b == 'b')){
            v1[i++]=ch[0];
            break;
        }

        b = ch[0];
    }

    while(isRunning){
        fd_read = read(fd_input,ch,1);
        if(fd_read == -1){
            perror("read input: ");
            strcat(logPrint,"Reading error.\n");
            write(fd_log, logPrint, strlen(logPrint));            
            exit(EXIT_FAILURE);
        }

        if(fd_read == 0)
            break;

        if(ch[0] == '\t' || ch[0] == '\n'){
            if(ch[0]== '\t')
                ++edge;

            vrtx1 = atoi(v1);   
            memset(v1, '\0', 1000);  

            if(node != 0){
                if(!hasNode(vrtx1, node)){
                    nodes[ind++]=vrtx1;
                    ++node;           
                }      
            }
            else{
                nodes[ind++]=vrtx1;
                ++node;                 
            }

            i = 0;
        }
        else
            v1[i++]=ch[0];
        
        if(node == size){
            size += 100;
            nodes = (int *) realloc(nodes, size*sizeof(int));
        }  
    }

    if(isRunning){

        new=(struct Node**)malloc( edge * sizeof(struct Node *));
        cache = (int **) malloc(row* sizeof(int *));

        for(i=0; i<row; ++i){
            cache[i] = (int *) malloc(column * sizeof(int));

            for(j=0; j<column; ++j)
                cache[i][j]=-1;
        }

        for(i=0; i<edge; ++i)
            new[i] = NULL;

        graph = createGraph(node);

        graph->node=node;
        graph->edge=edge;
    }

    if(lseek(fd_input,0,SEEK_SET) == -1){
        perror("lseek: ");
        strcat(logPrint,"lseek error.\n");
        write(fd_log, logPrint, strlen(logPrint));        
        exit(EXIT_FAILURE);
    }    

    i=0, j=0;
    memset(v1, '\0', 1000);
    while(isRunning){
        if(read(fd_input,ch,1) == -1){
            perror("read input: ");
            strcat(logPrint,"Reading error.\n");
            write(fd_log, logPrint, strlen(logPrint));            
            exit(EXIT_FAILURE);
        }

        if(ch[0] != '#' && ( b == '\n'||  b == 'b')){
            v1[i++]=ch[0];
            break;
        }

        b = ch[0];
    }    

    while(isRunning){
        fd_read = read(fd_input,ch,1);
        if(fd_read == -1){
            perror("read input6: ");
            strcat(logPrint,"Reading error.\n");
            write(fd_log, logPrint, strlen(logPrint));             
            exit(EXIT_FAILURE);
        }

        if(fd_read == 0)
            break;
        
        if(ch[0] == '\t')
            flag=1;
        
        if(ch[0] == '\n'){
            vrtx1 = atoi(v1);   
            vrtx2 = atoi(v2); 

            memset(v1, '\0', 1000);    
            memset(v2, '\0', 1000);               

            i = j = 0;
            flag=0;   

            addEdge(graph, vrtx1, vrtx2); 
        }

        if(flag==0){
            v1[i]=ch[0];
            ++i;
        }
        else{
            v2[j]=ch[0];
            ++j;
        }
    }    
}

/********* GRAPH FUNCTIONS ***********/

//find visited nodes with bfs
int *BFS(struct Graph* graph, int src, int dest){
    int i;
    if(hasNode(src, graph->node) == 0)
        return NULL;
    
    if(hasNode(dest, graph->node) == 0)
        return NULL;
    
    for(i=0; i<graph->node; ++i){
        if(nodes[i]==src){
            src = i;
            break;
        }
    }

    if(nodes[src] == dest){
        if(hasAdj(src, dest)){
            int *path = malloc(2* sizeof(int));
            
            path[0]=1;
            path[1]=dest;
            return path;
        }
        else
            return NULL;
    }

    for(i=0; i<graph->node; ++i){
        if(nodes[i]==dest){
            dest = i;
            break;
        }
    } 

    int curr, adj, counter=0, p_size=2;
    struct Node* node=NULL;     
    struct Queue* queue = createQueue();
    queue->qu = malloc((graph->node) * sizeof(int));
    int *path = malloc(p_size * sizeof(int));     

    for(i=0; i<graph->node; ++i)
        graph->visited[i]=0;

    graph->visited[src] = 1;
    addToQueue(queue, src);

    i=1;

    while (isEmpty(queue) == 0){
        curr = removeFromQueue(queue);

        path[i++]=nodes[curr];
        if(p_size == i){
            p_size += 1;
            path = (int *)realloc(path, sizeof(int)* p_size);
        }
        ++counter;

        if(nodes[curr] == nodes[dest]){
            path[0] = counter;
            free(queue->qu);
            free(queue);   
            if(node != NULL)
                free(node);         
            return path;
        }

        node = graph->adjArr[curr];

        while(node){
            adj = node->node;

            if(graph->visited[adj] == 0){
                graph->visited[adj] = 1;
                addToQueue(queue, adj);
            }
            node = node->next;
        }
    }

    free(queue->qu);
    free(queue);
    free(path);
    if(node != NULL)
        free(node);     

    return NULL;
}

//creates node 
struct Node* createNode(int node){
    new[counter] = malloc(1*sizeof(struct Node));    
    new[counter]->node = node;
    new[counter]->next = NULL;

    return new[counter];
}

//creates graph 
struct Graph* createGraph(int node){
    int i;
    struct Graph* graph;

    graph = malloc(1*sizeof(struct Graph));
    graph->adjArr = malloc(node * sizeof(struct Node*));
    graph->visited = malloc(node * sizeof(int));
    
    for(i=0; i<node; ++i){
        graph->adjArr[i]=NULL;
        graph->visited[i]=0;
    }

    return graph;
}

//sdds an edge to graph 
void addEdge(struct Graph* graph, int src, int dest){
    int source, destination, i;

    for(i=0; i<graph->node; ++i){
        if(nodes[i] == src){
            source = i;
            break;
        }
    }

    for(i=0; i<graph->node; ++i){
        if(nodes[i] == dest){
            destination = i;
            break;
        }
    }

    struct Node* new;
    new=createNode(destination);
    new->next=graph->adjArr[source];
    graph->adjArr[source]=new;

    ++counter;
}

//creates queue 
struct Queue* createQueue(){
    struct Queue* queue;

    queue=malloc(1*sizeof(struct Queue));

    queue->rear=-1;
    queue->front=-1;

    return queue;
}

//checks queue 
int isEmpty(struct Queue* queue){
    if(queue->rear == -1)
      return 1;
    else
      return 0;
}

//this function adds new node index to queue 
void addToQueue(struct Queue* queue, int index){
    if(queue->front == -1)
        queue->front = 0;
    
    queue->rear++;
    queue->qu[queue->rear]=index;
}

//this function removes a node index from queue 
int removeFromQueue(struct Queue* queue){
    int temp;
    if(isEmpty(queue) == 0){
        temp = queue->qu[queue->front];
        queue->front++;

        if(queue->front > queue->rear){ 
            queue->front=-1;
            queue->rear=-1;        
        }
    }
    else
        temp=-1;

    return temp;
}

//this function checks if graph has this node 
int hasNode(int node, int node_count){
    int i;

    for(i=0; i<node_count; ++i){
        if(nodes[i] == node)
            return 1;
    }
    
    return 0;
}

//this function fins absolute path from bfs 
int *findPath(int *path){
    int size, dest, i, j, ind, path_ind=1;
    int *path2, *absPath;

    size = path[0];
    dest = path[size];

    path2 = (int*) malloc((size+1)*sizeof(int));     

    for(i=0; i<size+1; ++i)
        path2[i] = -1;

    path2[0] = dest;

    for(i=size-1; i>0; --i){
        if(i == 1){
            path2[path_ind++] = path[1];
            break;
        }

        for(j=0; j<graph->node; ++j){
            if(nodes[j] == path[i])
                ind = j;
        }

        if(hasAdj(ind, dest)){
            path2[path_ind++] = nodes[ind];
            dest = nodes[ind];
        }
    }

    size = path_ind;
    absPath = (int*) malloc((size+1)*sizeof(int)); 

    absPath[0]=size;
    j=1;

    for(i=size-1; i>=0; --i){
        absPath[j]= path2[i];
        ++j;
    }

    free(path2);

    return absPath;
    
}

//this function chechs graph has tihs edge  
int hasAdj(int ind, int dest){
    struct Node* node;
    int adj;
    node = graph->adjArr[ind];

    while(node){
        adj = node->node;      
        if(nodes[adj] == dest)
            return 1;
        
        node = node->next;
    }  
    return 0;
}

/** following three functions locks lockFile that created or opened from same directory **/
 static int lockFlock(int fd, int cmd, int type, int whence, int start, off_t len)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_whence = whence;
    lock.l_start = start;
    lock.l_len = len;

    return fcntl(fd, cmd, &lock);
}

int lockFile(int fd, int type, int whence, int start, int len)
{
    return lockFlock(fd, F_SETLK, type, whence, start, len);
}

void createLockFile(){
    char fileDir[1024];
    int fd_lock, flags, i;

    getcwd(fileDir, sizeof(fileDir));   

    strcat(fileDir,"/lockFile\n");     

    fd_lock=open(fileDir, O_WRONLY | O_CREAT,0666);    
    if(fd_lock == -1){
        perror("open: ");
        exit(EXIT_FAILURE);
    } 

    flags = fcntl(fd_lock, F_GETFD); /* Fetch flags */
    if (flags == -1){
        perror("fcntl1");
        exit(EXIT_FAILURE);
    }
 
    flags |= FD_CLOEXEC; /* Turn on FD_CLOEXEC */
 
    if (fcntl(fd_lock, F_SETFD, flags) == -1){ /* Update flags */
        perror("fcntl2");
        exit(EXIT_FAILURE);
    }

    int x=lockFile(fd_lock, F_WRLCK, SEEK_SET, 0, 0) ;
    if (x== -1) {
        if (errno  == EAGAIN || errno == EACCES){
            printf("It is not possible double instantiation! Exiting. \n");   
            free(inputFile);
            free(logFile);
            if(logPrint != NULL)
                free(logPrint);
            if(poolThrs != NULL)
                free(poolThrs);
            if(nodes != NULL)
                free(nodes);
            if(graph != NULL){
                if(new != NULL){
                    for(i=0; i<graph->edge; ++i){
                        if(new[i] != NULL)
                            free(new[i]);                        
                    }
                    free(new);
                }
                if(cache != NULL){
                    for(i=0; i<row; ++i){
                        free(cache[i]);
                    }
                    free(cache);
                }
                free(graph->adjArr);
                free(graph->visited);  
                free(graph);
                if(fd_sock != -1){
                    close(fd_sock);
                }
            }                     
            exit(EXIT_FAILURE);
        }
    }
 
    if (ftruncate(fd_lock, 0) == -1){
        perror("ftruncate"); 
        exit(EXIT_FAILURE);        
    }
}

//creates daemon
int daemon_(){
    pid_t pid, sid; // prpcess id and session id
    int i;
        
    //Fork and exit from parent process
    pid = fork();
    if (pid < 0) {
        perror("pid:");          
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        free(inputFile);
        free(logFile);
        if(logPrint != NULL)
            free(logPrint);
        if(poolThrs != NULL)
            free(poolThrs);
        if(nodes != NULL)
            free(nodes);
        if(graph != NULL){
            if(new != NULL){
                for(i=0; i<graph->edge; ++i){
                    if(new[i] != NULL)
                        free(new[i]);
                }
                free(new);
            }
            if(cache != NULL){
                for(i=0; i<row; ++i){
                    free(cache[i]);
                }
                free(cache);
            }
            free(graph->adjArr);
            free(graph->visited);  
            free(graph);
            if(fd_sock != -1){
                close(fd_sock);
            }
        }              
        exit(EXIT_SUCCESS);
    }

    //create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        perror("setsid:");
        exit(EXIT_FAILURE);
    }

    //Fork and exit from parent process
    pid = fork();
    if (pid < 0) {
        perror("pid:");          
        exit(EXIT_FAILURE);
    }

    // for again to ensure we are not session leader    
    if (pid > 0) {
        free(inputFile);
        free(logFile);
        if(logPrint != NULL)
            free(logPrint);
        if(poolThrs != NULL)
            free(poolThrs);
        if(nodes != NULL)
            free(nodes);
        if(graph != NULL){
            if(new != NULL){
                for(i=0; i<graph->edge; ++i){
                    if(new[i] != NULL)
                        free(new[i]);
                }
                free(new);
            }
            if(cache != NULL){
                for(i=0; i<row; ++i){
                    free(cache[i]);
                }
                free(cache);
            }
            free(graph->adjArr);
            free(graph->visited);  
            free(graph);
            if(fd_sock != -1){
                close(fd_sock);
            }
        }              
        exit(EXIT_SUCCESS);
    }    

    createLockFile();

    //change the file mode mask 
    umask(0);
                 
    // change the current working directory 
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
        
    //close file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}
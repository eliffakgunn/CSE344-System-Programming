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

static void usageError();
int createSocket();
void connectToServer(int addr);

struct sockaddr_in sockAddr;
char *serverIP;
int portNum;

int main(int argc, char* argv[]){
    int option, srcNode, destNode, sockfd, rd, size=0, i, num=0, arr[2];
    struct timeval t1, t2;
    double t;
    time_t tt;
    char *time_;

    if (argc != 9)
        usageError();        
    
    serverIP = (char*) malloc(30*sizeof(char)); 
    memset(serverIP, '\0', 30);

    while((option = getopt(argc, argv, "apsd")) != -1){ /*get option from the getopt() method*/
        switch(option){
            case 'a':
                strcpy(serverIP,argv[optind]);               
                break;
            case 'p':
                sscanf(argv[optind], "%d", &portNum);               
                break;
            case 's':
                sscanf(argv[optind], "%d", &srcNode);               
                break;
            case 'd':
                sscanf(argv[optind], "%d", &destNode);               
                break;                                                            
            default: usageError();
        }
    }

    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    printf("%s",time_);
    memset(time_, '\0', strlen(time_));	
    printf(" Client (%d) connecting to %s:%d\n", getpid(), serverIP, portNum);
    
    tt=time(NULL);
    time_ = ctime(&tt);
    time_[strlen(time_) -1] = '\0'; 
    printf("%s",time_);
    memset(time_, '\0', strlen(time_));
    printf(" Client (%d) connected and requesting a path from node %d to %d\n", getpid(), srcNode, destNode);

    arr[0]=srcNode;
    arr[1]=destNode;

    sockfd = createSocket();

	gettimeofday(&t1, NULL);

	connectToServer(sockfd);

	if (write(sockfd, arr, sizeof(arr)) < 0)
        perror("writing on stream socket");  

    rd=read(sockfd, &size, sizeof(size)); 

   // printf("rd=%d      size=%d\n", rd, size);

	gettimeofday(&t2, NULL);    

    //if size is not equal to -1, then path is existing. Then writes path
    if(size != -1){
		gettimeofday(&t2, NULL);  

    	t = ((t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec))*1.0 / 1000000.0;    

    	tt=time(NULL);
    	time_ = ctime(&tt);
    	time_[strlen(time_) -1] = '\0'; 
    	printf("%s",time_);
    	memset(time_, '\0', strlen(time_));	
    	printf(" Server’s response to (%d): ", getpid());

    	for(i=0; i<size; ++i){
    		rd=read(sockfd, &num, sizeof(num));   		
    		if(rd == -1)
    			perror("read:");
    		if(i != size-1)
    			printf("%d->", num);
    		else
    			printf("%d, arrived in %.1lf seconds.\n", num, t);
    	}
    }
    else{
    	t=t2.tv_sec-t1.tv_sec;
    	tt=time(NULL);
    	time_ = ctime(&tt);
    	time_[strlen(time_) -1] = '\0'; 
    	printf("%s",time_);
    	memset(time_, '\0', strlen(time_));
    	printf(" Server’s response to (%d): NO PATH, arrived in %.1lf seconds, shutting down\n", getpid(), t);
    }

    close(sockfd);

    return 0;
}

static void usageError(){
    printf("Usage: ./client -a <ip_addr> -p <port_num> -s <src_node> -d <dest_node>\n");
    printf("-a: IP address of the machine running the server\n");
    printf("-p: port number at which the server waits for connections\n");
    printf("-s: source node of the requested path\n");
    printf("-d: destination node of the requested path\n");

    exit(EXIT_FAILURE); 
}

int createSocket(){
	int sockfd;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket: ");
		exit(EXIT_FAILURE);
	}

	memset(&sockAddr, 0, sizeof(struct sockaddr_in));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(serverIP);
	sockAddr.sin_port = htons(portNum);

	return sockfd;
}

void connectToServer(int addr){
	if (connect(addr, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) == -1) {
		perror("connect: ");
		exit(EXIT_FAILURE);
	}	
}

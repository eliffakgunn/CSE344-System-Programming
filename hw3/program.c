#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h> 
#include <time.h>

#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR); //to avoid EINTR errors
#define Abs(x)    ((x) > 0.0 ?  (x) : (-(x)) ) 
#define SIGN(u,v) ((v) >= 0.0 ? Abs(u) : -Abs(u) )
#define MAX(a,b)  (((a) > (b)) ? (a) : (b) )

void handler(int sig);
int *calculate_c_matrix_elements(int *arr, int size); 
//following two functions for svd calculation
static double PYTHAG(double a, double b);
int svd(float **a, int m, int n, float *w, float **v);

sig_atomic_t numLiveChildren = 4;
sig_atomic_t parentPid=-1;

int main(int argc, char* argv[]){
  pid_t pid;
  char byteA[1], byteB[1], result[3]; //result for castint int to string 
  int option, i, j, k, l, n, temp, m;
  int fd_open_inputA, fd_open_inputB, fd_read_inputA, fd_read_inputB, write_ret, read_ret, close_ret;
  char *power = (char*) malloc(3*sizeof(char)); //power
  char *inputPathA = (char*) malloc(11*sizeof(char)); //inputPathA
  char *inputPathB = (char*) malloc(11*sizeof(char)); //inputPathB
  int parent_to_child_p2[2], child_to_parent_p2[2], parent_to_child_p3[2], child_to_parent_p3[2]; //pipe fds
  int parent_to_child_p4[2], child_to_parent_p4[2], parent_to_child_p5[2], child_to_parent_p5[2]; //pipe fds 
  sigset_t blockMask, emptyMask;
  struct sigaction sa, act;

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handler;
  if(sigaction(SIGCHLD, &sa, NULL) == -1){
    fprintf(stderr, "%s%s\n","Sigaction1: ", strerror(errno));
    return -1;
  }
  if(sigaction(SIGINT, &sa, NULL) == -1){
    fprintf(stderr, "%s%s\n","Sigaction2: ", strerror(errno));
    return -1;
  }
  //checking for command line arguments
  if(argc!=7){
    char message[] = "Command line arguments must be this format to program runs correctly:\n./program -i inputPathA -j inputPathB -n 8 (n must be positive)\n";
    NO_EINTR(write_ret = write(1, message, strlen(message)));
    if(write_ret ==-1){
      fprintf(stderr, "%s%s\n","Write11: ", strerror(errno));
      return -1;     
    } 
    return -1;
  }
  //checking for n, n must be positive
  if(argv[6][0]=='-' || argv[6][0]=='0' ){
    char message[] = "Command line arguments must be this format to program runs correctly:\n./program -i inputPathA -j inputPathB -n 8 (n must be positive)\n";
    NO_EINTR(write_ret = write(1, message, strlen(message)));
    if(write_ret ==-1){
      fprintf(stderr, "%s%s\n","Write12: ", strerror(errno));
      return -1;     
    } 
    return -1;      
  }

  while((option = getopt(argc, argv, "ijn")) != -1){ //get option from the getopt() method
    switch(option){
      case 'i':
        strcpy(inputPathA, argv[optind]);
        break;
      case 'j':
        strcpy(inputPathB, argv[optind]);
        break;
      case 'n':
        strcpy(power, argv[optind]);
        sscanf(power, "%d", &n);
        break;
      default:
        break;
    }
  }

  int input_size=(int)(pow(2,n)); //row and column size of matrix
  int matrix_elements_size=(int)(pow(2,n)*pow(2,n)/2); //size of matrices that will be written pipes

  int **inputA = /*(int**)*/malloc( input_size * sizeof(int *)); //matrix A
  int **inputB = /*(int**)*/malloc( input_size * sizeof(int *)); //matrix B
  int **matrix_elements_p2 = malloc( 2 * sizeof(int *));  //first row is matrix A's elements, second row is matrix B's elements for p2
  int **matrix_elements_p3 = malloc( 2 * sizeof(int *));  //first row is matrix A's elements, second row is matrix B's elements for p3
  int **matrix_elements_p4 = malloc( 2 * sizeof(int *));  //first row is matrix A's elements, second row is matrix B's elements for p4
  int **matrix_elements_p5 = malloc( 2 * sizeof(int *));  //first row is matrix A's elements, second row is matrix B's elements for p5

  for(i = 0; i < input_size; i++){ //memory allocation for matrices 
    inputA[i] = malloc( input_size * sizeof(int));
    inputB[i] = malloc( input_size * sizeof(int));
  }

  for(i = 0; i < 2; i++){ //memory allocation for children's matrices
    matrix_elements_p2[i] = malloc( matrix_elements_size * sizeof(int)); //there are n*n/2 elements each row
    matrix_elements_p3[i] = malloc( matrix_elements_size * sizeof(int)); //there are n*n/2 elements each row
    matrix_elements_p4[i] = malloc( matrix_elements_size * sizeof(int)); //there are n*n/2 elements each row
    matrix_elements_p5[i] = malloc( matrix_elements_size * sizeof(int)); //there are n*n/2 elements each row
  }

  /******************open inputPathA then read******************/
  NO_EINTR(fd_open_inputA=open(inputPathA, O_RDONLY));
  if (fd_open_inputA == -1){
    fprintf(stderr, "%s%s\n","Open1: ", strerror(errno));
    return -1;
  }

  i=0, j=0;
  while(1){
    NO_EINTR(fd_read_inputA=read(fd_open_inputA,byteA,1));
    if (fd_read_inputA == -1){
      fprintf(stderr, "%s%s\n","Read1: ", strerror(errno));
      return -1;
    }
    if(fd_read_inputA == 0)
      break;

    inputA[i][j] = (unsigned char)byteA[0];   
    ++j;

    if(j == pow(2,n)){
      j=0;
      ++i;
    }
    if(i == pow(2,n))
      break;
  }
  if(i != pow(2,n) || j!=0 ){
    NO_EINTR(write_ret = write(1, "There are not sufficient characters in the inputPathA, exited the program.\n", 75));
    if(write_ret ==-1){
      fprintf(stderr, "%s%s\n","Write13: ", strerror(errno));
      return -1;     
    } 
    exit(1);
  }

  /******************open inputPathB then read******************/
  i=0, j=0;
  NO_EINTR(fd_open_inputB=open(inputPathB, O_RDONLY,0666));
  if (fd_open_inputB == -1){
    fprintf(stderr, "%s%s\n","Open2: ", strerror(errno));
    return -1;
  }

  while(1){
    NO_EINTR(fd_read_inputB=read(fd_open_inputB, byteB,1));
    if (fd_read_inputB == -1){
      fprintf(stderr, "%s%s\n","Read2: ", strerror(errno));
      return -1;
    }
    if(fd_read_inputB==0)
      break;

    if(i==pow(2,n))
      break;
    else{
      inputB[i][j]=(unsigned char)byteB[0];    
      ++j;
    }
    if(j==pow(2,n)){
      j=0;
      ++i;
    }
  }
  if(i != pow(2,n) || j!=0 ){
    NO_EINTR(write_ret = write(1, "There are not sufficient characters in the inputPathB, exited the program.\n", 75));
    if(write_ret ==-1){
      fprintf(stderr, "%s%s\n","Write2: ", strerror(errno));
      return -1;     
    } 
    exit(1);
  }

  /***close input files***/
  NO_EINTR(close_ret = close(fd_open_inputA)); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close33: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(fd_open_inputB)); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close34: ", strerror(errno));
    return -1;     
  }   

  /*fills matrices taht will be written to pipe for p2, p3, p4, p5*/
  k=0, l=0;
  for(i=0; i<pow(2,n); ++i){
    for (j = 0; j < pow(2,n); ++j){
      if(i<pow(2,n)/2){
        matrix_elements_p2[0][k] = inputA[i][j];
        matrix_elements_p3[0][k] = inputA[i][j];
        ++k;
      }else{
        matrix_elements_p4[0][l] = inputA[i][j];
        matrix_elements_p5[0][l] = inputA[i][j];
        ++l;
      }
    }
  }

  k=0, l=0;
  for(j=0; j<pow(2,n); ++j){
    for (i = 0; i < pow(2,n); ++i){
      if(j<pow(2,n)/2){
        matrix_elements_p2[1][k] = inputB[i][j];
        matrix_elements_p4[1][k] = inputB[i][j];
        ++k;
      }else{
        matrix_elements_p3[1][l] = inputB[i][j];
        matrix_elements_p5[1][l] = inputB[i][j];  
        ++l;      
      }
    }
  }
  int where=(int)(pow(2,n)*pow(2,n)/2);
  int size=(int)(pow(2,n)*pow(2,n));

  //also these matrices will be written the pipe
  int* rows_and_columns_p2 = (int*) malloc(size*sizeof(int)); 
  int* rows_and_columns_p3 = (int*) malloc(size*sizeof(int)); 
  int* rows_and_columns_p4 = (int*) malloc(size*sizeof(int)); 
  int* rows_and_columns_p5 = (int*) malloc(size*sizeof(int)); 

  k=0;
  for(i=0; i<2; ++i){
    for(j=0; j<where; ++j){
      rows_and_columns_p2[k]=matrix_elements_p2[i][j];
      rows_and_columns_p3[k]=matrix_elements_p3[i][j];
      rows_and_columns_p4[k]=matrix_elements_p4[i][j];
      rows_and_columns_p5[k]=matrix_elements_p5[i][j];
      ++k;
    }
  }
  
  /************************pipes************************/
  if(pipe(parent_to_child_p2)){
    fprintf(stderr, "%s%s\n","Pipe1: ", strerror(errno));
    return -1;  
  }
  if(pipe(child_to_parent_p2)){
    fprintf(stderr, "%s%s\n","Pipe2: ", strerror(errno));
    return -1;  
  }
  if(pipe(parent_to_child_p3)){
    fprintf(stderr, "%s%s\n","Pipe3: ", strerror(errno));
    return -1;  
  }
  if(pipe(child_to_parent_p3)){
    fprintf(stderr, "%s%s\n","Pipe4: ", strerror(errno));
    return -1;  
  }
  if(pipe(parent_to_child_p4)){
    fprintf(stderr, "%s%s\n","Pipe5: ", strerror(errno));
    return -1;  
  }
  if(pipe(child_to_parent_p4)){
    fprintf(stderr, "%s%s\n","Pipe6: ", strerror(errno));
    return -1;  
  }
  if(pipe(parent_to_child_p5)){
    fprintf(stderr, "%s%s\n","Pipe7: ", strerror(errno));
    return -1;  
  }
  if(pipe(child_to_parent_p5)){
    fprintf(stderr, "%s%s\n","Pipe8: ", strerror(errno));
    return -1;  
  }

  //sets mask not to miss SIGCHLD signals
  sigemptyset(&blockMask);
  sigaddset(&blockMask, SIGCHLD);
  if (sigprocmask(SIG_SETMASK, &blockMask, NULL) == -1){
    fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
    return -1;
  }
  
  //creates child process with for loop
  for(i=0; i<4; ++i){
    switch(fork()){
      case -1:
        fprintf(stderr, "%s%s\n","Fork: ", strerror(errno));
      
      case 0:
        if(i==0){ //p2
          int *arr_p2=(int*) malloc(size*sizeof(int)); //array that will be readed form pipe
          int *c_matrix_elements_p2=(int*) malloc((size/4)*sizeof(int)); //calculated c matrix part

          NO_EINTR(close_ret = close(child_to_parent_p2[0])); // close the unwanted child_to_parent_p2 read side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close1: ", strerror(errno));
            return -1;     
          }          
          NO_EINTR(close_ret = close(parent_to_child_p2[1])); // close the unwanted parent_to_child_p2 write side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close2: ", strerror(errno));
            return -1;     
          }  

          NO_EINTR(read_ret = read(parent_to_child_p2[0], arr_p2, size*sizeof(int))); //reads from the pipe
          if(read_ret ==-1){
            fprintf(stderr, "%s%s\n","Read3: ", strerror(errno));
            return -1;     
          } 

          c_matrix_elements_p2=calculate_c_matrix_elements(arr_p2, size);

          NO_EINTR(write_ret = write(child_to_parent_p2[1], c_matrix_elements_p2, (size/4)*sizeof(int))); //writes to pipe for the parent to read
          if(write_ret ==-1){
            fprintf(stderr, "%s%s\n","Write3: ", strerror(errno));
            return -1;     
          } 

          /**closes the pipes**/
          NO_EINTR(close_ret = close(parent_to_child_p2[0])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close3: ", strerror(errno));
            return -1;     
          }            
          NO_EINTR(close_ret = close(child_to_parent_p2[1])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close4: ", strerror(errno));
            return -1;     
          }    
          free(arr_p2);
          free(c_matrix_elements_p2);        
        }
        else if(i==1){ //p3
          int *arr_p3=(int*) malloc(size*sizeof(int)); //array that will be readed form pipe           
          int *c_matrix_elements_p3=(int*) malloc(size/4*sizeof(int)); //calculated c matrix part

          NO_EINTR(close_ret = close(child_to_parent_p3[0])); // close the unwanted child_to_parent_p3 read side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close5: ", strerror(errno));
            return -1;     
          }            
          NO_EINTR(close_ret = close(parent_to_child_p3[1])); // close the unwanted parent_to_child_p3 write side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close6: ", strerror(errno));
            return -1;     
          }            
          
          NO_EINTR(read_ret = read(parent_to_child_p3[0], arr_p3, size*sizeof(int))); //reads from the pipe
          if(read_ret ==-1){
            fprintf(stderr, "%s%s\n","Read4: ", strerror(errno));
            return -1;     
          }           
          c_matrix_elements_p3=calculate_c_matrix_elements(arr_p3, size);

          NO_EINTR(write_ret = write(child_to_parent_p3[1], c_matrix_elements_p3, (size/4)*sizeof(int))); //writes to pipe for the parent to read
          if(write_ret ==-1){
            fprintf(stderr, "%s%s\n","Write4: ", strerror(errno));
            return -1;     
          } 

          /**closes the pipes**/
          NO_EINTR(close_ret = close(parent_to_child_p3[0])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close7: ", strerror(errno));
            return -1;     
          }            
          NO_EINTR(close_ret = close(child_to_parent_p3[1])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close8: ", strerror(errno));
            return -1;     
          }   
          free(arr_p3);
          free(c_matrix_elements_p3);                    
        }
        else if(i==2){ //p4          
          int *arr_p4=(int*) malloc(size*sizeof(int)); //array that will be readed form pipe          
          int *c_matrix_elements_p4=(int*) malloc(size/4*sizeof(int)); //calculated c matrix part

          NO_EINTR(close_ret = close(child_to_parent_p4[0])); // close the unwanted child_to_parent_p4 read side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close9: ", strerror(errno));
            return -1;     
          }  
          NO_EINTR(close_ret = close(parent_to_child_p4[1])); // close the unwanted parent_to_child_p4 write side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close10: ", strerror(errno));
            return -1;     
          }            
          
          NO_EINTR(read_ret = read(parent_to_child_p4[0], arr_p4, size*sizeof(int))); //reads from the pipe
          if(read_ret ==-1){
            fprintf(stderr, "%s%s\n","Read5: ", strerror(errno));
            return -1;     
          } 

          c_matrix_elements_p4=calculate_c_matrix_elements(arr_p4, size);

          NO_EINTR(write_ret = write(child_to_parent_p4[1], c_matrix_elements_p4, (size/4)*sizeof(int))); //writes to pipe for the parent to read
          if(write_ret ==-1){
            fprintf(stderr, "%s%s\n","Write5: ", strerror(errno));
            return -1;     
          } 

          /**closes the pipes**/
          NO_EINTR(close_ret = close(parent_to_child_p4[0])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close11: ", strerror(errno));
            return -1;     
          }  
          NO_EINTR(close_ret = close(child_to_parent_p4[1])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close12: ", strerror(errno));
            return -1;     
          }            
          free(arr_p4);
          free(c_matrix_elements_p4); 
        }else{ //p5
          int *arr_p5=(int*) malloc(size*sizeof(int)); //array that will be readed form pipe          
          int *c_matrix_elements_p5=(int*) malloc(size/4*sizeof(int)); //calculated c matrix part

          NO_EINTR(close_ret = close(child_to_parent_p5[0])); // close the unwanted child_to_parent_p5 read side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close13: ", strerror(errno));
            return -1;     
          }  
          NO_EINTR(close_ret = close(parent_to_child_p5[1])); // close the unwanted parent_to_child_p5 write side
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close14: ", strerror(errno));
            return -1;     
          }            
          
          NO_EINTR(read_ret = read(parent_to_child_p5[0], arr_p5, size*sizeof(int))); //reads from the pipe
          if(read_ret ==-1){
            fprintf(stderr, "%s%s\n","Read6: ", strerror(errno));
            return -1;     
          } 

          c_matrix_elements_p5=calculate_c_matrix_elements(arr_p5, size);

          NO_EINTR(write_ret = write(child_to_parent_p5[1], c_matrix_elements_p5, (size/4)*sizeof(int))); //writes to pipe for the parent to read
          if(write_ret ==-1){
            fprintf(stderr, "%s%s\n","Write6: ", strerror(errno));
            return -1;     
          } 

          /**closes the pipes**/
          NO_EINTR(close_ret = close(parent_to_child_p5[0])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close15: ", strerror(errno));
            return -1;     
          }  
          NO_EINTR(close_ret = close(child_to_parent_p5[1])); 
          if(close_ret ==-1){
            fprintf(stderr, "%s%s\n","Close16: ", strerror(errno));
            return -1;     
          }  
          free(arr_p5);
          free(c_matrix_elements_p5);                     
        }
        exit(0); 
      default:
        break;
    }
  }
  parentPid=getpid();
  sigemptyset(&emptyMask); //set mask to empty mask after creating child processes

  NO_EINTR(close_ret = close(child_to_parent_p2[1])); // close the unwanted child_to_parent_p2 write side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close17: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(parent_to_child_p2[0])); // close the unwanted parent_to_child_p2 read side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close18: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(child_to_parent_p3[1])); // close the unwanted child_to_parent_p3 write side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close19: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(parent_to_child_p3[0])); // close the unwanted parent_to_child_p3 read side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close20: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(child_to_parent_p4[1])); // close the unwanted child_to_parent_p4 write side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close21: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(parent_to_child_p4[0])); // close the unwanted parent_to_child_p4 read side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close22: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(child_to_parent_p5[1])); // close the unwanted child_to_parent_p5 write side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close23: ", strerror(errno));
    return -1;     
  }    
  NO_EINTR(close_ret = close(parent_to_child_p5[0])); // close the unwanted parent_to_child_p5 read side
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close24: ", strerror(errno));
    return -1;     
  } 

  int *c_elements_p2=(int*) malloc(size*sizeof(int)); //part of the matrix c calculated by p2
  int *c_elements_p3=(int*) malloc(size*sizeof(int)); //part of the matrix c calculated by p3
  int *c_elements_p4=(int*) malloc(size*sizeof(int)); //part of the matrix c calculated by p4
  int *c_elements_p5=(int*) malloc(size*sizeof(int)); //part of the matrix c calculated by p5

  /***following four write call write to pipes for the childs to read***/
  NO_EINTR(write_ret = write(parent_to_child_p2[1], rows_and_columns_p2, size*sizeof(int)));
  if(write_ret ==-1){
    fprintf(stderr, "%s%s\n","Write7: ", strerror(errno));
    return -1;     
  }   
  NO_EINTR(write_ret = write(parent_to_child_p3[1], rows_and_columns_p3, size*sizeof(int)));
  if(write_ret ==-1){
    fprintf(stderr, "%s%s\n","Write8: ", strerror(errno));
    return -1;     
  }
  NO_EINTR(write_ret = write(parent_to_child_p4[1], rows_and_columns_p4, size*sizeof(int)));
  if(write_ret ==-1){
    fprintf(stderr, "%s%s\n","Write9: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(write_ret = write(parent_to_child_p5[1], rows_and_columns_p5, size*sizeof(int)));
  if(write_ret ==-1){
    fprintf(stderr, "%s%s\n","Write10: ", strerror(errno));
    return -1;     
  }

  /***following four read call read from pipes the calculation of matrix C's parts***/
  NO_EINTR(read_ret = read(child_to_parent_p2[0], c_elements_p2, size*sizeof(int)));
  if(read_ret ==-1){
    fprintf(stderr, "%s%s\n","Read7: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(read_ret = read(child_to_parent_p3[0], c_elements_p3, size*sizeof(int)));
  if(read_ret ==-1){
    fprintf(stderr, "%s%s\n","Read8: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(read_ret = read(child_to_parent_p4[0], c_elements_p4, size*sizeof(int)));
  if(read_ret ==-1){
    fprintf(stderr, "%s%s\n","Read9: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(read_ret = read(child_to_parent_p5[0], c_elements_p5, size*sizeof(int)));
  if(read_ret ==-1){
    fprintf(stderr, "%s%s\n","Read10: ", strerror(errno));
    return -1;     
  } 

  /***following eight close calls close the pipes***/
  NO_EINTR(close_ret = close(child_to_parent_p2[0])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close25: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(parent_to_child_p2[1])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close26: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(child_to_parent_p3[0])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close27: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(parent_to_child_p3[1])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close28: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(child_to_parent_p4[0])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close29: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(parent_to_child_p4[1])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close30: ", strerror(errno));
    return -1;     
  } 
  NO_EINTR(close_ret = close(child_to_parent_p5[0])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close31: ", strerror(errno));
    return -1;     
  }  
  NO_EINTR(close_ret = close(parent_to_child_p5[1])); 
  if(close_ret ==-1){
    fprintf(stderr, "%s%s\n","Close32: ", strerror(errno));
    return -1;     
  } 

  int c_size=(int)sqrt(size); //row and column sizes of martix c
  float **c_matrix = malloc( c_size * sizeof(float *)); //matrix C

  for(i = 0; i < c_size; i++) //memory aloocation for c_matrix 
    c_matrix[i] = malloc( c_size * sizeof(float));

  /***fills the martix C. First quarter P2's calculation, second quarter P3's calculation, and so on.***/
  temp=0, m=0, k=0, n=0;
  for(i=0; i<c_size; ++i){
    for(j=0; j<c_size; ++j){
      if(i<(c_size/2)){
        if(temp ==0){
          c_matrix[i][j]=(float)c_elements_p2[k];
          ++k;
          if( (k % (c_size/2)) == 0){
            temp=1;
            k=m;
          }
        }else{         
          c_matrix[i][j]=(float)c_elements_p3[k];
          ++k;
          if( (k % (c_size/2)) == 0){
            temp=0;
            if(i ==(c_size/2-1))
              k=0;
            else
              m += c_size/2;
          }
        }
      }else{
        if(temp==0){
          c_matrix[i][j]=(float)c_elements_p4[k];
          ++k;
          if( (k % (c_size/2)) == 0){
            temp=1;
            k=n;
          }
        }else{          
          c_matrix[i][j]=(float)c_elements_p5[k];
          ++k;
          if( (k % (c_size/2)) == 0){
            temp=0;
            n += c_size/2;
          }
        }
      }
    }
  }

  float **right_matrix = malloc( c_size * sizeof(float *)); //right orthogonal transformation matrix 
  float *singular_values=(float*) malloc(c_size*sizeof(float)); //singular values

  for(i = 0; i < c_size; i++) //memory aloocation for matrices 
    right_matrix[i] = malloc( c_size * sizeof(float));

  svd(c_matrix, c_size, c_size, singular_values, right_matrix);

  printf("Singular values of matrix C:\n");
  for(i=0; i<c_size; ++i)
    printf("%.3f\n",singular_values[i]);

  /* Parent comes here: wait for SIGCHLD until all children are dead */
  while (numLiveChildren > 0) {
   if (sigsuspend(&emptyMask) == -1 && errno != EINTR){
      fprintf(stderr, "%s%s\n","Sigsuspend: ", strerror(errno));
      return -1;
    }
  }

  free(power);
  free(inputPathA);
  free(inputPathB);
  for(i = 0; i < input_size; i++){ 
    free(inputA[i]);
    free(inputB[i]);
  }
  free(inputA);
  free(inputB);
  for(i = 0; i < 2; i++){ 
    free(matrix_elements_p2[i]);
    free(matrix_elements_p3[i]);
    free(matrix_elements_p4[i]);
    free(matrix_elements_p5[i]);
  }
  free(matrix_elements_p2);
  free(matrix_elements_p3);
  free(matrix_elements_p4);
  free(matrix_elements_p5);
  
  free(rows_and_columns_p2);
  free(rows_and_columns_p3);
  free(rows_and_columns_p4);
  free(rows_and_columns_p5);
  
  free(c_elements_p2);
  free(c_elements_p3);
  free(c_elements_p4);
  free(c_elements_p5);
  for(i = 0; i < c_size; i++){
    free(c_matrix[i]);
    free(right_matrix[i]);
  }
  free(c_matrix);
  free(right_matrix);
  free(singular_values);

  exit(0);

  return 0;
}

void handler(int sig){
  if(sig==SIGCHLD){
    int status, savedErrno;
    pid_t childPid;
    savedErrno = errno; /* In case we modify 'errno' */
    while ((childPid = waitpid(-1, &status, WNOHANG)) > 0){
      if (childPid == -1 && errno != ECHILD){
        fprintf(stderr, "%s%s\n","Waitpid: ", strerror(errno));
        exit(1);
      }
      numLiveChildren--;
    }
    errno= savedErrno;
  }
  if(sig==SIGINT){
    if(parentPid != -1){ //parent waits for children exit to avoid zombie process
      wait(NULL);
      wait(NULL);
      wait(NULL);
      wait(NULL);
      printf("SIGINT is caught. Program exited.\n");
    }
    exit(0);
  }
}

//calculates the matrix c's parts
int *calculate_c_matrix_elements(int *arr, int size){
  int i, j, k, m, n;
  int *c=(int*) malloc(size/4*sizeof(int)); 

  k=0, n=size/2;
  for(i=0; i<size/4; ++i){
    c[i]=0;
    if(n == size){
      n=size/2;
      k += sqrt(size);
    }

    m=k;
    for(j=0; j<sqrt(size); ++j){
      c[i] += arr[m] * arr[n];
      ++m;
      ++n;
    }
  }
  return c;
}

/* 
 * svdcomp - SVD decomposition routine. 
 * Takes an mxn matrix a and decomposes it into udv, where u,v are
 * left and right orthogonal transformation matrices, and d is a 
 * diagonal matrix of singular values.
 *
 * This routine is adapted from svdecomp.c in XLISP-STAT 2.1 which is 
 * code from Numerical Recipes adapted by Luke Tierney and David Betz.
 *
 * Input to svd is as follows:
 *   a = mxn matrix to be decomposed, gets overwritten with u
 *   m = row dimension of a
 *   n = column dimension of a
 *   w = returns the vector of singular values of a
 *   v = returns the right orthogonal transformation matrix
*/

static double PYTHAG(double a, double b)
{
    double at = fabs(a), bt = fabs(b), ct, result;

    if (at > bt)       { ct = bt / at; result = at * sqrt(1.0 + ct * ct); }
    else if (bt > 0.0) { ct = at / bt; result = bt * sqrt(1.0 + ct * ct); }
    else result = 0.0;
    return(result);
}


int svd(float **a, int m, int n, float *w, float **v)
{
    int flag, i, its, j, jj, k, l, nm;
    double c, f, h, s, x, y, z;
    double anorm = 0.0, g = 0.0, scale = 0.0;
    double *rv1;
  
    if (m < n) 
    {
        fprintf(stderr, "#rows must be > #cols \n");
        return(0);
    }
  
    rv1 = (double *)malloc((unsigned int) n*sizeof(double));

/* Householder reduction to bidiagonal form */
    for (i = 0; i < n; i++) 
    {
        /* left-hand reduction */
        l = i + 1;
        rv1[i] = scale * g;
        g = s = scale = 0.0;
        if (i < m) 
        {
            for (k = i; k < m; k++) 
                scale += fabs((double)a[k][i]);
            if (scale) 
            {
                for (k = i; k < m; k++) 
                {
                    a[k][i] = (float)((double)a[k][i]/scale);
                    s += ((double)a[k][i] * (double)a[k][i]);
                }
                f = (double)a[i][i];
                g = -SIGN(sqrt(s), f);
                h = f * g - s;
                a[i][i] = (float)(f - g);
                if (i != n - 1) 
                {
                    for (j = l; j < n; j++) 
                    {
                        for (s = 0.0, k = i; k < m; k++) 
                            s += ((double)a[k][i] * (double)a[k][j]);
                        f = s / h;
                        for (k = i; k < m; k++) 
                            a[k][j] += (float)(f * (double)a[k][i]);
                    }
                }
                for (k = i; k < m; k++) 
                    a[k][i] = (float)((double)a[k][i]*scale);
            }
        }
        w[i] = (float)(scale * g);
    
        /* right-hand reduction */
        g = s = scale = 0.0;
        if (i < m && i != n - 1) 
        {
            for (k = l; k < n; k++) 
                scale += fabs((double)a[i][k]);
            if (scale) 
            {
                for (k = l; k < n; k++) 
                {
                    a[i][k] = (float)((double)a[i][k]/scale);
                    s += ((double)a[i][k] * (double)a[i][k]);
                }
                f = (double)a[i][l];
                g = -SIGN(sqrt(s), f);
                h = f * g - s;
                a[i][l] = (float)(f - g);
                for (k = l; k < n; k++) 
                    rv1[k] = (double)a[i][k] / h;
                if (i != m - 1) 
                {
                    for (j = l; j < m; j++) 
                    {
                        for (s = 0.0, k = l; k < n; k++) 
                            s += ((double)a[j][k] * (double)a[i][k]);
                        for (k = l; k < n; k++) 
                            a[j][k] += (float)(s * rv1[k]);
                    }
                }
                for (k = l; k < n; k++) 
                    a[i][k] = (float)((double)a[i][k]*scale);
            }
        }
        anorm = MAX(anorm, (fabs((double)w[i]) + fabs(rv1[i])));
    }
  
    /* accumulate the right-hand transformation */
    for (i = n - 1; i >= 0; i--) 
    {
        if (i < n - 1) 
        {
            if (g) 
            {
                for (j = l; j < n; j++)
                    v[j][i] = (float)(((double)a[i][j] / (double)a[i][l]) / g);
                    /* double division to avoid underflow */
                for (j = l; j < n; j++) 
                {
                    for (s = 0.0, k = l; k < n; k++) 
                        s += ((double)a[i][k] * (double)v[k][j]);
                    for (k = l; k < n; k++) 
                        v[k][j] += (float)(s * (double)v[k][i]);
                }
            }
            for (j = l; j < n; j++) 
                v[i][j] = v[j][i] = 0.0;
        }
        v[i][i] = 1.0;
        g = rv1[i];
        l = i;
    }
  
    /* accumulate the left-hand transformation */
    for (i = n - 1; i >= 0; i--) 
    {
        l = i + 1;
        g = (double)w[i];
        if (i < n - 1) 
            for (j = l; j < n; j++) 
                a[i][j] = 0.0;
        if (g) 
        {
            g = 1.0 / g;
            if (i != n - 1) 
            {
                for (j = l; j < n; j++) 
                {
                    for (s = 0.0, k = l; k < m; k++) 
                        s += ((double)a[k][i] * (double)a[k][j]);
                    f = (s / (double)a[i][i]) * g;
                    for (k = i; k < m; k++) 
                        a[k][j] += (float)(f * (double)a[k][i]);
                }
            }
            for (j = i; j < m; j++) 
                a[j][i] = (float)((double)a[j][i]*g);
        }
        else 
        {
            for (j = i; j < m; j++) 
                a[j][i] = 0.0;
        }
        ++a[i][i];
    }

    /* diagonalize the bidiagonal form */
    for (k = n - 1; k >= 0; k--) 
    {                             /* loop over singular values */
        for (its = 0; its < 30; its++) 
        {                         /* loop over allowed iterations */
            flag = 1;
            for (l = k; l >= 0; l--) 
            {                     /* test for splitting */
                nm = l - 1;
                if (fabs(rv1[l]) + anorm == anorm) 
                {
                    flag = 0;
                    break;
                }
                if (fabs((double)w[nm]) + anorm == anorm) 
                    break;
            }
            if (flag) 
            {
                c = 0.0;
                s = 1.0;
                for (i = l; i <= k; i++) 
                {
                    f = s * rv1[i];
                    if (fabs(f) + anorm != anorm) 
                    {
                        g = (double)w[i];
                        h = PYTHAG(f, g);
                        w[i] = (float)h; 
                        h = 1.0 / h;
                        c = g * h;
                        s = (- f * h);
                        for (j = 0; j < m; j++) 
                        {
                            y = (double)a[j][nm];
                            z = (double)a[j][i];
                            a[j][nm] = (float)(y * c + z * s);
                            a[j][i] = (float)(z * c - y * s);
                        }
                    }
                }
            }
            z = (double)w[k];
            if (l == k) 
            {                  /* convergence */
                if (z < 0.0) 
                {              /* make singular value nonnegative */
                    w[k] = (float)(-z);
                    for (j = 0; j < n; j++) 
                        v[j][k] = (-v[j][k]);
                }
                break;
            }
            if (its >= 30) {
                free((void*) rv1);
                fprintf(stderr, "No convergence after 30,000! iterations \n");
                return(0);
            }
    
            /* shift from bottom 2 x 2 minor */
            x = (double)w[l];
            nm = k - 1;
            y = (double)w[nm];
            g = rv1[nm];
            h = rv1[k];
            f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
            g = PYTHAG(f, 1.0);
            f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g, f))) - h)) / x;
          
            /* next QR transformation */
            c = s = 1.0;
            for (j = l; j <= nm; j++) 
            {
                i = j + 1;
                g = rv1[i];
                y = (double)w[i];
                h = s * g;
                g = c * g;
                z = PYTHAG(f, h);
                rv1[j] = z;
                c = f / z;
                s = h / z;
                f = x * c + g * s;
                g = g * c - x * s;
                h = y * s;
                y = y * c;
                for (jj = 0; jj < n; jj++) 
                {
                    x = (double)v[jj][j];
                    z = (double)v[jj][i];
                    v[jj][j] = (float)(x * c + z * s);
                    v[jj][i] = (float)(z * c - x * s);
                }
                z = PYTHAG(f, h);
                w[j] = (float)z;
                if (z) 
                {
                    z = 1.0 / z;
                    c = f * z;
                    s = h * z;
                }
                f = (c * g) + (s * y);
                x = (c * y) - (s * g);
                for (jj = 0; jj < m; jj++) 
                {
                    y = (double)a[jj][j];
                    z = (double)a[jj][i];
                    a[jj][j] = (float)(y * c + z * s);
                    a[jj][i] = (float)(z * c - y * s);
                }
            }
            rv1[l] = 0.0;
            rv1[k] = f;
            w[k] = (float)x;
        }
    }
    free((void*) rv1);
    return(1);
}


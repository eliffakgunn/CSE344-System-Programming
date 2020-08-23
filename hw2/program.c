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

#define BUFFER_SIZE 5999999
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR); //to avoid EINTR errors

double *least_square(unsigned int *bytes); //calculates coefficients
int write_coordinates_with_equation(unsigned int *ascii, double *coeffs, int fd_open_temp); //writes the coordinates and equations to template file
int write_coordinates_with_errors(double *coordinat, int fd_open_output, int fd_open_temp, int where, double *errors); //writes lines reads from temp file with errors
double *calculate_errors(double *coordinat); //calculates errors
void handler(int signal); //signal handler
double *calculate_line(char *buffer, int where); //cast char array to double 
int print_errors_means_std_devs(double **all_errors, int counter); //print on screen for each error metric, its mean and standard deviation

sig_atomic_t usr1_interrupt = 0; //ust1_interrupt=1 when SIGUSR1 is received
sig_atomic_t usr2_interrupt = 0; //ust2_interrupt=1 when SIGUSR2 is received

int main(int argc, char* argv[]){
  pid_t pid;
  char buffer[BUFFER_SIZE];
  int option, fd_open_input,fd_open_output, fd_read_input, i, j=0, temp=0, m=0, where=0, flag=1, unlink_flag=0; //where indicate offset of file
                                                                                                                //flag=0 when there is no byte in temp file
                                                                                                                //unlink_flag=1 when input file deleted
  int fd_open_temp, fd_close_temp, fd_close_input, fd_close_output, fd_read_temp, line_count, counter=0, SIGINT_flag=0; //counter=line_count to keep errors
                                                                                                                        //SIGINT_flag=1 when SIGINT received in critical section
  int fcntl_ret, write_ret;
  char* input_fname = (char*) malloc(10*sizeof(char)); //input file
  char* output_fname = (char*) malloc(32*sizeof(char)); //output file
  char *bytes = (char*)malloc(20 * sizeof(char)); //collect the chars that reads from input file
  unsigned int *ascii = (unsigned int*) malloc(20 * sizeof(unsigned int)); //ascii codes of chars
  double *coeffs = ( double*) malloc(2 * sizeof( double)); // a and b (y=ax+b)
  double *coordinat = ( double*) malloc(22 * sizeof( double)); // coordinates
  char temp_file[] = "/tmp/myfileXXXXXX"; //temporary file for p1's output
  char* temporary_file = (char*) malloc(20*sizeof(char)); 
  double* errors = (double*) malloc(3*sizeof(double)); //stores error metrics for each line
  char* print = (char*) malloc(1000*sizeof(char)); // to print p1' output to screen
  char result[10]; //for casting from double/int to string
  double **all_errors = malloc( 10000 * sizeof(double *)); //10000 = max num of row
  sigset_t blocked_signals, block_mask, mask, oldmask, mask_SIGTERM, waitingmask; //signal masks
  struct sigaction act, sa_act, sa_act2; 
  struct flock lock;

  for(i = 0; i < 1000; i++) //memory aloocation for **all_errors 
    all_errors[i] = malloc( 3 * sizeof(double));
  
 
  if(argc!=5){
    NO_EINTR(write_ret = write(1, "Command line arguments must be this format to program runs correctly:\n./program -i inputPath -o outputPath\n", 108));
    if( write_ret ==-1){
      fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
      return -1;     
    } 
    return -1;
  }

  while((option = getopt(argc, argv, "io")) != -1){ //get option from the getopt() method
     switch(option){
        case 'i':
          strcpy(input_fname, argv[optind]);
          break;
        case 'o':
          strcpy(output_fname, argv[optind]);
          break;
        default:
          break;
    }
  }

  //signal mask for SIGINT and SIGSTOP
  sigemptyset(&blocked_signals);
  sigaddset(&blocked_signals, SIGINT);
  sigaddset(&blocked_signals, SIGSTOP);
  //signal handler for SIGINT and SIGSTOP
  sigemptyset(&sa_act.sa_mask);
  sa_act.sa_handler = handler;
  sa_act.sa_flags = 0;
  if(sigaction (SIGINT, &sa_act, NULL)==-1){
    fprintf(stderr, "%s%s\n","Sigaction: ", strerror(errno));
    return -1;
  }
  /*****************SIGSTOP CAN NOT HANDLE******************/
  /*if(sigaction (SIGSTOP, &sa_act, NULL)==-1){ 
    fprintf(stderr, "%s%s\n","Sigaction: ", strerror(errno));
    return -1;
  }*/

  //signal mask for SIGUSR2 -SIGUSR2 used for check temp file wheter has coordinates-
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR2);
  //signal handler for SIGUSR1 and SIGUSR2
  sigfillset (&block_mask);
  act.sa_handler = handler;
  act.sa_mask = block_mask;
  act.sa_flags = 0;
  if(sigaction (SIGUSR1, &act, NULL)==-1){
    fprintf(stderr, "%s%s\n","Sigaction: ", strerror(errno));
    return -1;
  }
  if(sigaction (SIGUSR2, &act, NULL)==-1){
    fprintf(stderr, "%s%s\n","Sigaction: ", strerror(errno));
    return -1;
  }

  //signal mask for SIGTERM
  sigemptyset(&mask_SIGTERM);
  sigaddset(&mask_SIGTERM, SIGTERM);
  //signal handler for SIGTERM
  sa_act2.sa_handler = handler;
  sa_act2.sa_mask = mask_SIGTERM;
  sa_act2.sa_flags = 0;
  if(sigaction (SIGTERM, &sa_act2, NULL)==-1){
    fprintf(stderr, "%s%s\n","Sigaction: ", strerror(errno));
    return -1;
  }

  memset(&lock, 0, sizeof(lock)); //for fcntl call
    
  strcpy(temporary_file,temp_file); 

  fd_open_temp = mkstemp(temporary_file); /* Create and open temp file */
  if (fd_open_temp == -1){
    fprintf(stderr, "%s%s\n","Mkstemp: ", strerror(errno));
    return -1;
  }

  //printf("Temporary file is %s\n", temporary_file);  /* Print it for information */

  pid = fork();
  if(pid==-1){
    fprintf(stderr, "%s%s\n","Fork: ", strerror(errno));
    return -1;
  }

  if(pid>0){ //parent process
    //raise(SIGTERM);
    NO_EINTR(fd_open_input=open(input_fname, O_RDONLY));
    if (fd_open_input == -1){
      fprintf(stderr, "%s%s\n","Open1: ", strerror(errno));
      return -1;
    }

    NO_EINTR(fd_read_input=read(fd_open_input,buffer,BUFFER_SIZE));
    if (fd_read_input == -1){
      fprintf(stderr, "%s%s\n","Read: ", strerror(errno));
      return -1;
    }
    //first close file then send SIGUSR1 to child
    NO_EINTR(fd_close_input=close(fd_open_input));
    if(fd_close_input==-1){
      fprintf(stderr, "%s%s\n","Close: ", strerror(errno));
      return -1;
    }

    if(kill (0, SIGUSR1)==-1){ //after close the inpur file, send SIGUSR1 to child
      fprintf(stderr, "%s%s\n","Kill: ", strerror(errno));
      return -1;
    }

    //find \n counts
    for (i = 0; i < fd_read_input; ++i)
      if(buffer[i]=='\n')
        temp++;

    line_count=(fd_read_input-temp)/20; //if assumed no line deleted from temp file, these is line_count line in the temp file

    while((fd_read_input-temp-m)>=20){ //gets 20 bytes
      for(j=0; j<20; ++j){
        while(buffer[m]=='\n'){
          m++;
          temp--;
        }
        bytes[j] = (char)buffer[m];
        m++;  
      }
      for(i=0; i<20; ++i){
        ascii[i] = (unsigned char)bytes[i];
      }
      //block SIGINT and SIGSTOP for critical section     
      if(sigprocmask (SIG_BLOCK, &blocked_signals, NULL)==-1){
        fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
        return -1;
      }

      //raise(SIGINT);
      //To check which signals received
      sigpending (&waitingmask);
      if (sigismember (&waitingmask, SIGINT)==1)
        SIGINT_flag=1;

      coeffs = least_square(ascii); 
      //unblock signals after cs
      if(sigprocmask (SIG_UNBLOCK, &blocked_signals, NULL)==-1){
        fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
        return -1;
      }

      lock.l_type=F_WRLCK; //locks temp file for writing
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }

      write_coordinates_with_equation(ascii, coeffs, fd_open_temp); //writes coordinates with equations to temp file

      lock.l_type=F_UNLCK; //after writing unlocks the temp file 
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }
      //after each writing to temp file, send SIGUSR2 to child to report temp file has coordinates
      if(kill(0, SIGUSR2)==-1){
        fprintf(stderr, "%s%s\n","Kill: ", strerror(errno));
        return -1;        
      }
    }
    //raise(SIGTERM);

    NO_EINTR(fd_close_temp=close(fd_open_temp));
    if(fd_close_temp==-1){
      fprintf(stderr, "%s%s\n","Close: ", strerror(errno));
      return -1;
    }
    //printing on screen how many bytes it has read as input, how many line equations it
    //has estimated, and which signals where sent to P1 while it was in a critical section.
    strcat(print,"P1 has read ");
    sprintf(result, "%d", (fd_read_input-temp)-(fd_read_input-temp)%20); 
    strcat(print, result);
    strcat(print, " bytes as input.\n");
    sprintf(result, "%d", line_count); 
    strcat(print, result);
    strcat(print," line equations estimated.\n");
    if(SIGINT_flag==1)
      strcat(print,"SIGINT was sent in critical section.\n");
    else
       strcat(print,"No signal was sent in critical section.\n");

    NO_EINTR(write(1, print, strlen(print)));
    if(write_ret == -1){
      fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
      return -1;      
    }

    free(input_fname);
    free(bytes);
    free(ascii);
    free(coeffs);

    wait(NULL);
    exit(0);
  }else{

    //raise(SIGTERM);
  
    while(flag){ //flag equals the num of bytes that read from temp file, when temp file is empty loop ends
      //deletes the input file when SIGUSR1 is received
      if(usr1_interrupt==1 && unlink_flag==0){ //only 1 time ensures this condition
        if(unlink(input_fname)==-1){
          fprintf(stderr, "%s%s\n","Unlink: ", strerror(errno));
          return -1;      
        }
        unlink_flag=1;
      }
  
      NO_EINTR(fd_open_temp=open(temporary_file, O_RDWR));
      if (fd_open_temp == -1){
        fprintf(stderr, "%s%s\n","Open2: ", strerror(errno));
        return -1;
      }
  
      lock.l_type=F_RDLCK; //lock the temp file for reading
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){ 
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }
  
      NO_EINTR(fd_read_temp=read(fd_open_temp,buffer,BUFFER_SIZE)); 
      if (fd_read_temp == -1){
        fprintf(stderr, "%s%s\n","Read: ", strerror(errno));
        return -1;
      }
  
      lock.l_type=F_UNLCK; //unlock the temp file 
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }
      
      if(fd_read_temp==0){ //if did not write to temp file
        if(sigprocmask (SIG_BLOCK, &mask, &oldmask)==-1){
          fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
          return -1;
        }
        while (!usr2_interrupt){ //when temp file has coordinates, usr2_interrupt=1
          if (sigsuspend(&oldmask) == -1 && errno != EINTR){ //suspends until a line is added to the tem file
            fprintf(stderr, "%s%s\n","Sigsuspend: ", strerror(errno));
            return -1; 
          }
        }
        if(sigprocmask (SIG_UNBLOCK, &mask, NULL)==-1){
          fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
          return -1;
        }
  
        lock.l_type=F_RDLCK; //lock the temp file for reading
        NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
        if(fcntl_ret == -1){
          fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
          return -1;
        }
  
        NO_EINTR(fd_read_temp=read(fd_open_temp,buffer,BUFFER_SIZE));  
        if (fd_read_temp == -1){
          fprintf(stderr, "%s%s\n","Read: ", strerror(errno)); 
          return -1;
        }
  
        lock.l_type=F_UNLCK; //unlock the temp file 
        NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
        if(fcntl_ret == -1){
          fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
          return -1;
        }
        usr2_interrupt=0;
      }
      //raise(SIGTERM);
  
      for(i=0; i<fd_read_temp; ++i){ //finds offset for lseek
        if(buffer[i]=='\n'){
          where=i;
          break;
        }
      }
  
      coordinat=calculate_line(buffer, where); //cast char array to double to get coordinates
  
      NO_EINTR(fd_open_output = open(output_fname,O_WRONLY | O_CREAT | O_APPEND,0666));
      if(fd_open_output == -1){
        fprintf(stderr, "%s%s\n","Open3: ", strerror(errno));
        return -1;
      }
      //blocks SIGINT and SIGSTOP for cs
      if(sigprocmask (SIG_BLOCK, &blocked_signals, NULL)==-1){
        fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
        return -1;
      }
      //raise(SIGINT);
      errors=calculate_errors(coordinat);

      //unblocks SIGINT and SIGSTOP   
      if(sigprocmask (SIG_UNBLOCK, &blocked_signals, NULL)==-1){
        fprintf(stderr, "%s%s\n","Sigprocmask: ", strerror(errno));
        return -1;
      }
  
      all_errors[counter]=errors; //errors for each line
  
      write_coordinates_with_errors(coordinat, fd_open_output, fd_open_temp, where+1, errors); //writes output file lines with errors
  
      lock.l_type=F_RDLCK; //lock the temp file for reading
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }
  
      NO_EINTR(fd_read_temp=read(fd_open_temp,buffer,BUFFER_SIZE)); 
      if (fd_read_temp == -1){
        fprintf(stderr, "%s%s\n","Read: ", strerror(errno)); 
        return -1;
      }
  
      lock.l_type=F_UNLCK; //unlock the temp file 
      NO_EINTR(fcntl_ret = fcntl(fd_open_temp, F_SETLKW, &lock))
      if(fcntl_ret == -1){
        fprintf(stderr, "%s%s\n","Fcntl: ", strerror(errno));
        return -1;
      }

      ++counter; //line number
      if(fd_read_temp==0)
        break;
    }
    //raise(SIGTERM);
  
    NO_EINTR(fd_close_temp=close(fd_open_temp)); //close temp file
    if(fd_close_temp==-1){
      fprintf(stderr, "%s%s\n","Close: ", strerror(errno));
      return -1;
    }
  
    if(unlink(temporary_file)==-1){ //remove temp file
      fprintf(stderr, "%s%s\n","Unlink: ", strerror(errno));
      return -1;      
    }
  
    NO_EINTR(fd_close_output=close(fd_open_output)); //close output file
    if(fd_close_output==-1){
      fprintf(stderr, "%s%s\n","Close: ", strerror(errno));
      return -1;
    }
  
    print_errors_means_std_devs(all_errors, counter); //print on screen for each error metric, its mean and standard deviation
  
    free(output_fname);
    free(temporary_file);
    free(coordinat);
    free(errors);
    free(print);
    for(i = 0; i < 1000; i++) 
      free(all_errors[i]);
    free(all_errors);
    _exit(0);
  }
  return 0;
}

void handler(int signal){
  if(signal==SIGUSR1){
    usr1_interrupt = 1;
  }
  if(signal==SIGUSR2){
    usr2_interrupt=1;
  }
  if(signal==SIGTERM){
    printf("SIGTERM is caught.\n");
  }
}

//applies the least squares method to the corresponding 2D coordinates and calculate the coefficients
double *least_square(unsigned int *byteArr){
  double *coeffs = ( double*) malloc(2 * sizeof( double)); 
  int i, n=10, sumX=0, sumY=0, sumX2=0, sumXY=0, det; //n -> number of coordinates
                                                      //sumX -> sum of x coordinates
                                                      //sumY -> sum of y coordinates
                                                      //sumX2 -> sum of square of x coordinates
                                                      //sumXY -> sum of x_i*y_i
  for(i=0; i<20; ++i){
    sumX+=byteArr[i];
    sumY+=byteArr[i+1];
    sumX2+=byteArr[i]*byteArr[i];
    sumXY+=byteArr[i]*byteArr[i+1];
    ++i;
  }
  coeffs[0]=(double)(n*sumXY-sumX*sumY)/(n*sumX2-sumX*sumX); //a
  coeffs[1]=(double)(sumX2*sumY-sumXY*sumX)/(n*sumX2-sumX*sumX); //b

  return coeffs;
} 

//writes the lines with quation to temp file
int write_coordinates_with_equation(unsigned int *ascii, double *coeffs, int fd_open_temp){
  int i, j, k=0, nChars=20, nRows=1, nCols=11, fd_write_temp; // assuming a max length of 20 chars per string
  char result[12];
  char ***array1 = malloc(nRows * sizeof(char **)); 

  //memory allocation
  for(i = 0; i < nCols; ++i)
      array1[i] = malloc(nCols * sizeof(char *));
  for(i = 0; i < nRows; ++i)
      for(j = 0; j < nCols; ++j)
          array1[i][j] = malloc(nChars * sizeof(char));

  //converting to "(x_i,y_i)" format
  for(i = 0; i < nRows; ++i){
      for(j = 0; j < nCols; ++j){
        if(j==nCols-1){        
          sprintf(result, "%.3lf", (double)coeffs[0]);
          strcat(array1[i][j],result);
          strcat(array1[i][j],"x");
          strcat(array1[i][j],"+");
          sprintf(result, "%.3f", (double)coeffs[1]);
          strcat(array1[i][j],result);
        }else{
          strcat(array1[i][j],"(");
          sprintf(result, "%d", ascii[k]); 
          strcat(array1[i][j], result);
          strcat(array1[i][j], ",");
          sprintf(result, "%d", ascii[k+1]); 
          strcat(array1[i][j], result);
          strcat(array1[i][j],"),");
        }
      k+=2;
    }
  }

  for(i=0; i<11; ++i){
    NO_EINTR(fd_write_temp = write(fd_open_temp, array1[0][i], strlen(array1[0][i])));
    if (fd_write_temp == -1){ 
      fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
      return -1;
    }
  }

  NO_EINTR(fd_write_temp=write(fd_open_temp, "\n", 1)); 
  if (fd_write_temp == -1){
    fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
    return -1;
  }

  return 0;
}

//gets the coordinates from char*
double *calculate_line(char *buffer, int where){
  double *coordinat = ( double*) malloc(22 * sizeof( double)); // bytes ve coeffs kullanabilirim ona bak sonradan
  int number, temp_status=0, digit=1, flag=0, index=21, i;
  double digit2=0.001, sayi=0;
  char byte; 

  for(i=where-1; i>=0; --i){
    byte=buffer[i];
    switch (byte){
      case '0': number=0;
        break;
      case '1': number=1;
        break;
      case '2': number=2;
        break;
      case '3': number=3;
        break;
      case '4': number=4;
        break;
      case '5': number=5;
        break;
      case '6': number=6;
        break;
      case '7': number=7;
        break;
      case '8': number=8;
        break;
      case '9': number=9;
        break;
      default:
        break;
    }

    if(temp_status==0){ // fins a,b
      if(buffer[i]=='.') //tam ksıma geçti demektir
        flag=1;

      if(buffer[i]!='.' && buffer[i]!='x' && buffer[i]!='+' && buffer[i]!=',' && buffer[i] != '-'){
        if(flag==0){ //ondalık kısım
          sayi += number*digit2;
          digit2 *=10;
        }else{ //tam kısım
          sayi += number*digit;
          digit *=10;
        }
      }

      if( buffer[i] == '-')
        sayi *= -1;

      if(buffer[i]=='+'){
        coordinat[index]=sayi;
        sayi=0;
        digit=1;
        digit2=0.001;
        flag=0;
        --index;
      }
      if(buffer[i]==','){
        coordinat[index]=sayi;
        sayi=0;
        digit=1;
        digit2=0.001;
        flag=0;
        temp_status=1; //a, b bulma işi bitti 
        --index;         
      }
    }else{ //finds coordinates
      if(buffer[i]!='(' && buffer[i]!=')' && buffer[i]!=',' && buffer[i]!='\n'){
        sayi += number*digit;
        digit *=10;
      }

      if((buffer[i]==',' && buffer[i-1]!=')') || buffer[i]=='('){
        coordinat[index]=sayi;
        sayi=0;
        digit=1;
        --index;
      }
    }
  }

  return coordinat;
}

//calculates errors according to their formulas
double *calculate_errors(double *coordinat){
  double MSE=0, RMSE=0, MAE=0, err=0, sum_err_abs=0, sum_err_square=0; 
  double* errors = (double*) malloc(3*sizeof(double)); 
  int i,j;

  for(i=0; i<20; ++i){
    //error=absolute value-predicted value
    err = coordinat[i+1] - (coordinat[20]*coordinat[i]+coordinat[21]); 
    sum_err_abs+=fabs(err);
    sum_err_square+=err*err;
    ++i;
  }

  MAE = sum_err_abs/10;
  MSE = sum_err_square/10; //n=10
  RMSE = sqrt(MSE);

  errors[0]=MAE;
  errors[1]=MSE;
  errors[2]=RMSE;

  return errors;
}

//remove the line it read from the file and write its own output to the file denoted by outputPath as 10 coordinates,
//the line equation (ax+b) and the three error estimates
int write_coordinates_with_errors(double *coordinat, int fd_open_output, int fd_open_temp, int where, double *errors){
  int i, j, k=0, nChars=20, nRows=1, nCols=14, fd_write_temp;
  char ***array = malloc(nRows * sizeof(char **)); 
  char result[12], empty[]={'\0'};
  char* new_line = (char*) malloc(1*sizeof(char)); 

  new_line=empty;

  //memory allocation
  for(i = 0; i < nCols; ++i)
    array[i] = malloc(nCols * sizeof(char *));
  for(i = 0; i < nRows; ++i)
    for(j = 0; j < nCols; ++j)
      array[i][j] = malloc(nChars * sizeof(char));

  //converting to "(x_i,y_i)" format
  for(i = 0; i < nRows; ++i){
    for(j = 0; j < 11; ++j){
      if(j==10){        
        sprintf(result, "%.3lf", (double)coordinat[20]);
        strcat(array[i][j],result);
        strcat(array[i][j],"x");
        strcat(array[i][j],"+");
        sprintf(result, "%.3f", (double)coordinat[21]);
        strcat(array[i][j],result);
        strcat(array[i][j],", ");
      }else{
        strcat(array[i][j],"(");
        sprintf(result, "%d", (int)coordinat[k]); 
        strcat(array[i][j], result);
        strcat(array[i][j], ", ");
        sprintf(result, "%d", (int)coordinat[k+1]); 
        strcat(array[i][j], result);
        strcat(array[i][j],"), ");
      }
      k+=2;
    }
  }
  //adding errors
  sprintf(result, "%.3lf", (double)errors[0]);
  strcat(array[0][11],result);
  strcat(array[0][11], ", ");
  sprintf(result, "%.3lf", (double)errors[1]);
  strcat(array[0][12],result);
  strcat(array[0][12], ", ");
  sprintf(result, "%.3lf", (double)errors[2]);
  strcat(array[0][13],result);

  for(i=0; i<14; ++i){
    NO_EINTR(fd_write_temp = write(fd_open_output, array[0][i], strlen(array[0][i])));
    if (fd_write_temp == -1){
      fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
      return -1;
    }
  }

  NO_EINTR(fd_write_temp=write(fd_open_output, "\n", 1)); 
  if (fd_write_temp == -1){
    fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
    return -1;
  }

  if(lseek(fd_open_temp,0,SEEK_SET)==-1){
    fprintf(stderr, "%s%s\n","Lseek: ", strerror(errno));
    return -1;
  }
  for (i = 0; i < where; ++i){
    NO_EINTR(fd_write_temp = write(fd_open_temp, new_line, 1));
    if (fd_write_temp == -1){
      fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
      return -1;
    }
  }
}

//prints on screen for each error metric, its mean and standard deviation
int print_errors_means_std_devs(double **all_errors, int counter){
  int i,j, write_ret;
  char result[15];
  double sum_mae=0, sum_mse=0, sum_rmse=0, mean_mae, mean_mse, mean_rmse;
  double sum_of_diff_mae=0, sum_of_diff_mse=0, sum_of_diff_rmse=0, sd_mae, sd_mse, sd_rmse;

  for(i=0; i<counter; ++i){
    sum_mae +=all_errors[i][0];
    sum_mse +=all_errors[i][1];
    sum_rmse +=all_errors[i][2];  
  }

  mean_mae = sum_mae/counter;
  mean_mse = sum_mse/counter;
  mean_rmse = sum_rmse/counter;

  for(i=0; i<counter; ++i){
    sum_of_diff_mae += (all_errors[i][0]-mean_mae) * (all_errors[i][0]-mean_mae);
    sum_of_diff_mse += (all_errors[i][1]-mean_mse) * (all_errors[i][1]-mean_mse);
    sum_of_diff_rmse += (all_errors[i][2]-mean_rmse) * (all_errors[i][2]-mean_rmse);  
  }

  sd_mae=sqrt(sum_of_diff_mae/(counter-1));
  sd_mse=sqrt(sum_of_diff_mse/(counter-1));
  sd_rmse=sqrt(sum_of_diff_rmse/(counter-1));

  char* print = (char*) malloc(1000*sizeof(char)); 
  strcat(print,"Mean of MAEs=");
  sprintf(result, "%.3f", mean_mae); 
  strcat(print, result);
  strcat(print, ", Standart Devision of MAEs=");
  sprintf(result, "%.3f", sd_mae); 
  strcat(print, result);
  strcat(print, "\n");
  strcat(print,"Mean of MSEs=");
  sprintf(result, "%.3f", mean_mse); 
  strcat(print, result);
  strcat(print, ", Standart Devision of MSEs=");
  sprintf(result, "%.3f", sd_mse); 
  strcat(print, result);
  strcat(print, "\n");
  strcat(print,"Mean of RMSEs=");
  sprintf(result, "%.3f", mean_rmse); 
  strcat(print, result);
  strcat(print, ", Standart Devision of RMSEs=");
  sprintf(result, "%.3f", sd_rmse); 
  strcat(print, result);
  strcat(print, "\n");

  NO_EINTR(write_ret = write(1, print, strlen(print)))
  if(write_ret ==-1){
    fprintf(stderr, "%s%s\n","Write: ", strerror(errno));
    return -1;     
    
  }
}
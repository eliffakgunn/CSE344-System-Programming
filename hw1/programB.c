#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 9999

void fonk(char arr[], int out);

int main(int argc, char* argv[]){

   srand(time(0));
   char buffer[BUFFER_SIZE];
   int option, t, i, j=0, temp=0, random, fd_output_open, signal=0, fd_input_open, fd_read, k=0, l=0; //variables
	char *input_fname = (char*) malloc(32*sizeof(char)); //input file
	char *output_fname = (char*) malloc(32*sizeof(char)); //output file
   char *time=(char*) malloc(3*sizeof(char)); //time as string
   char *line = (char*)malloc(145 * sizeof(char)); //bir complex sayı max 9 char içeriyor. 16*9=144+1(\n)

   struct flock lock;
   memset(&lock, 0, sizeof(lock));

  	while((option = getopt(argc, argv, "iot")) != -1){ 
	   switch(option){
	      case 'i':
	     	   input_fname = argv[optind];
            if(strcmp(input_fname, "inputPathB") == 0)
               input_fname = "outputPathA";
	      	break;
	      case 'o':
	      	output_fname = argv[optind];
	         break;
	      case 't': 
            time = argv[optind];
            sscanf(time, "%d", &t);
	         break;
	   }
	}

   fd_input_open=open(input_fname, O_RDONLY);
   if (fd_input_open == -1) {
        perror ("open");
        return 1;
   }

   fd_output_open = open(output_fname, O_WRONLY /*| O_EXCL*/ | O_CREAT | O_APPEND);
   if(fd_output_open == -1){
        perror("open");
        return 2;
    }

   lock.l_type=F_WRLCK;
   fcntl(fd_input_open, F_SETLKW, &lock);

   fd_read=read(fd_input_open,buffer,BUFFER_SIZE);
   if (fd_read == -1) {
        perror ("read");
        return 3;
    }

   while(fd_read==0){
      printf("outputPahtA is emoty. Sleeping...\n");
      sleep(t/1000);
      fd_read=read(fd_input_open,buffer,BUFFER_SIZE);
   }

   lock.l_type=F_UNLCK;
   fcntl(fd_input_open, F_SETLKW, &lock);

   //printf("fd_read=%d\n", fd_read );
   //printf("buffer[%d]=%d\n",fd_read-1,buffer[fd_read-1] );

   //find \n counts
   for (i = 0; i < fd_read; ++i)
      if(buffer[i]=='\n')
         temp++;

   //printf("temp=%d\n",temp);

   random=rand()%temp; // \n sayısına göre mod al

   //finds indicated line, if indicated-indexed-line is empty then searh for non-empty line.
   for(i=0; i<=random; ++i){
      l=0;

      if(i==random && buffer[k]=='\n'){
         signal=1;
         break;
      }
      while(buffer[k]!='\n'){ 
         line[l]=buffer[k];
         ++k;
         ++l;
      }
      ++k;
   }
   if(signal==1){ //seçilen satır nullsa(\n)
      if(k==fd_read-1){///yani son satır fd_readaysa
         k=0;
         while(buffer[k]=='\n')
            k++;
         while(buffer[k]!='\n'){ 
            line[l]=buffer[k];
            ++k;
            ++l;
         }
      }else{
         while(buffer[k]=='\n'){
            k++;
            if(k==fd_read-1)
               break;
         }
         if(k!=fd_read-1){
            while(buffer[k]!='\n'){ 
               line[l]=buffer[k];
               ++k;
               ++l;
            }
         }else{
            k=0;
            while(buffer[k]=='\n')
               k++;
            while(buffer[k]!='\n'){ 
              line[k]=buffer[k];
              ++k;
            }
            l=k;
         }

      }         
   }  

   line[l]='\n';

   lock.l_type=F_WRLCK;
   fcntl(fd_input_open, F_SETLKW, &lock);

   write(fd_output_open, line, strlen(line));
   sleep(t/1000);

   lock.l_type=F_UNLCK;
   fcntl(fd_input_open, F_SETLKW, &lock);
}



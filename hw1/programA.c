#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 9999

int translate_and_write(char arr[], int out, int t);

int main(int argc, char* argv[]){

	char buffer[BUFFER_SIZE];
	int option, t, i, j=0, temp=0, fd_output_open, m=0, fd_input_open, fd_read, fd_close; //variables
	char* input_fname = (char*) malloc(32*sizeof(char)); //input file
	char* output_fname = (char*) malloc(32*sizeof(char)); //output file
  	char *time=(char*) malloc(3*sizeof(char)); //time
	char *deneme = (char*)malloc(32 * sizeof(char)); //collect the chars that reads from input file

	struct flock lock;
	memset(&lock, 0, sizeof(lock));

   	while((option = getopt(argc, argv, "iot")) != -1){ //get option from the getopt() method
   	   switch(option){
   	      case 'i':
   	     	input_fname = argv[optind];
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
  
	fd_read=read(fd_input_open,buffer,BUFFER_SIZE);
	if (fd_read == -1) {
        perror ("read");
        return 3;
    }

	//printf("open=%d\n",fd_input_open );

    //find \n counts
	for (i = 0; i < fd_read; ++i)
		if(buffer[i]=='\n')
			temp++;

	//printf("temp=%d\n",temp);
	//printf("fd_read=%d\n",fd_read);

	while((fd_read-temp-m)>=32){ //gets 32 bits
		for(j=0; j<32; ++j){
			while(buffer[m]=='\n'){
				m++;
				temp--;
			}
			deneme[j]=buffer[m];
			//printf("buffer[%d]=%c-------------deneme[%d]=%c\n",m, buffer[m], j, deneme[j]);
			m++;	
		}

		lock.l_type=F_RDLCK;
    	fcntl(fd_output_open, F_SETLKW, &lock);

		translate_and_write(deneme, fd_output_open,t);

		lock.l_type=F_UNLCK;
    	fcntl(fd_output_open, F_SETLKW, &lock);

   		sleep(t/100);	
	}



	//if(fd_read-temp-m>=0){ //yani 32den az eleman varsa veya boş bir dosyaysa veya \nlerden oluşan bir dosyaysa
	fd_close=close(fd_input_open);
	if(fd_close==-1){
        perror ("close");
		return 1;
	}

	_exit(0);

	return 0;
}

int translate_and_write(char arr[], int out, int t){
	int *ascii, i, j, k=0, nChars=10, nRows=1, nCols=16, fd_write; // assuming a max length of 10 chars per string
	char plus='+', result[3];
	ascii = (int*) malloc(32 * sizeof(int)); //ascii codes of chars
	char ***array1 = malloc(nRows * sizeof(char **)); //string array

	for(i=0; i<32; ++i)
		ascii[i]=arr[i];

	//for(i=0; i<32; ++i){
	//	printf("ascii[%d]=%d\n",i,ascii[i]);
	//}

	//memory allocation
	for(i = 0; i < nCols; ++i)
	    array1[i] = malloc(nCols * sizeof(char *));
	for(i = 0; i < nRows; ++i)
	    for(j = 0; j < nCols; ++j)
	        array1[i][j] = malloc(nChars * sizeof(char));

	//converting to "X + iY" format
	for(i = 0; i < nRows; ++i)
	    for(j = 0; j < nCols; ++j){
			sprintf(result, "%d", ascii[k]); 
		    strcat(array1[i][j], result);
		    strcat(array1[i][j], "+");
			sprintf(result, "%d", ascii[k+1]); 
		    strcat(array1[i][j], "i");
		    strcat(array1[i][j], result);
		    k+=2;
		}

	//for(i = 0; i < nRows; ++i)
	//    for(j = 0; j < nCols; ++j)
	//	    printf("array1[%d][%d]=%s\n",i,j,array1[i][j]);
        
    for(i=0; i<16; ++i){
    	fd_write = write(out, array1[0][i], strlen(array1[0][i]));
		if (fd_write == -1) {
    	    perror ("write");
    	    return 4;
    	}

       	fd_write=write(out, ",", 1);
		if (fd_write == -1) {
    	    perror ("write");
    	    return 5;
    	}
    }

    fd_write=write(out, "\n", 1); 
	if (fd_write == -1) {
	    perror ("write");
	    return 6;
	}

	return 0;
}
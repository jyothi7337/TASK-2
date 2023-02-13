
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>
#include<errno.h>


#define MAX 256

char buf[MAX];


int main(int argc, char* argv[]){
    int fd,off;
    int len = atoi(argv[2]);    
    if(argc < 2){
        perror("Please pass command line arguments\n");
        return -1;
    }
    
    fd = open("/dev/eeprom_slave",O_RDWR);
    
    off = atoi(argv[1]);
    
    if(fd < 0){
        perror("Failed Open");
        return -1;
    }


    if(lseek(fd,off,SEEK_SET)  == -1){
        perror("Failed seek");
        return -1;
    }
    printf("Reading: %d number of bytes\n",len);
    if(read(fd,buf,len) < 0){
        perror("Failed to Read");
        return -1;
    } 

    printf("Read succesfully\n");
    for(int i=0;i<256;i++){
        printf("%c",buf[i]);
        if(i%64 == 0){
            printf("\n");
        }
    }
    printf("\n");
    //printf("Read: %s\n",buf);
    close(fd);

    return 0;
}
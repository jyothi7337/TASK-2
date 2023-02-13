
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>
#include<errno.h>

int main(int argc, char** argv){
    int fd,len;   
    int off = atoi(argv[1]);
    if(argc < 2){
        perror("Please pass command line arguments\n");
        return -1;
    }
    printf("writing at loc: %d\n",off);
    fd = open("/dev/eeprom_slave",O_RDWR);
    
    if(fd < 0){
        perror("Failed Open");
        return -1;
    }


    if(lseek(fd,off,SEEK_SET)  == -1){
        perror("Failed seek");
        return -1;
    }
    len = strlen(argv[2]);

    if(write(fd,argv[2],len) < 0){
        perror("Failed to write");
        return -1;
    } 

    printf("Wrote succesfully\n");

    close(fd);

    return 0;
}
#include "LibFS.h"
#include "LibDisk.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>

#define FILETABLE_SIZE 256
#define MAX_FILES 1000


// global errno value here
int osErrno;
int fileTable[FILETABLE_SIZE];
char superBlock[SECTOR_SIZE];
int inode[32];
char block[SECTOR_SIZE];
char ibm[SECTOR_SIZE];
char dbm[SECTOR_SIZE];
char buffer[256];
int fileCounter=0;
char *filepath;
int IBMOFFSET = 3;

int
FS_Boot(char *path)
{
  	filepath = path;
  	
    printf("FS_Boot %s\n", path);

      // oops, check for errors

    if(Disk_Init() == -1) {
    	printf("Disk_Init() failed\n");
    	osErrno = E_GENERAL;
    	return -1;
    }

    Disk_Init();

    int fd = open(path, O_RDWR);
    struct stat st;
    stat(path, &st);

    if(fd >= 0) {
     	if(st.st_size == (NUM_SECTORS * SECTOR_SIZE))
     	{
     		if(superBlock[0] == '1')
     			Disk_Load(path);
     	}
    }
  	else {
  		superBlock[0] = '1';
  		Disk_Write(0,superBlock);
  		int i;
  		for(i = 0; i<SECTOR_SIZE; i++)
  		{
  			ibm[i] = '1';
  			dbm[i] = '1';
  		}
  		Disk_Write(1, ibm);
  		Disk_Write(2, dbm);
  		for(i=0;i<SECTOR_SIZE; i++)
  		{
  			block[i] = '1';
  		}
  		//allocate inode blocks
  		for(i = 3; i<253; i++)
  		{
  			Disk_Write(i, block);
  		}
  		//allocate data blocks
  		for(i = 253; i<NUM_SECTORS; i++)
  		{
  			Disk_Write(i, block);
  		}
  		//write inode bit-map
  		ibm[0] = '0';
  		//write data bit-map
  		dbm[0]= '0';
  	}
    return 0;
}

int
FS_Sync()
{
    printf("FS_Sync\n"); 
    if(Disk_Save(filepath)==-1) {
    	printf("Disk_Save(filepath) failed\n");
		osErrno = E_GENERAL;
		return -1;
    }
    Disk_Save(filepath);
    return 0;
}


//create new file of name pointed to by file with size 0
//If file already exists, return -1 with osErrno to E_CREATE
//with success, return 0
int
File_Create(char *file)
{
	printf("FS_Create\n");
	int fd = open(file, O_RDWR);
	if(fd >= 0)
	{
		osErrno = E_CREATE;
		return -1;
	}

    close(fd);
    return 0;
}

//open file and return integer file descriptor
//if file doesn't exist, return -1 and osErrno with E_NO_SUCH_FILE
//if max number of files open, return -1 osErrno and E_TOO_MANY_OPEN_FILES
int
File_Open(char *file)
{

    printf("FS_Open\n");
    int fd = open(file, O_RDWR);
   	if(fd < 0)
   	{
   		osErrno = E_NO_SUCH_FILE;
   		return -1;
   	}
   	if(fileCounter > MAX_FILES)
   	{
   		osErrno = E_TOO_MANY_OPEN_FILES;
   		return -1;
   	}
   	else{
   		fileCounter+=1;
   		return fd;
   	}
    
}

//read in size number of bytes from file dscriptor fd, read into buffer
//all reads begin at current location of file pointer, which updates after read to new location
//if file not open, return -1 with osErrno E_BAD_FD
//if open, return number of bytes equal to or lesser than size. Should be lesser if end of file is reached.
//if already at end of file, return 0
int
File_Read(int fd, void *buffer, int size)
{
    printf("FS_Read\n");
    return 0;
}

//write in size number of bytes to file with descriptor fd, write frm buffer
//writes begin at current file location and update after write to current position plus size
//If file not open, return -1 with osErrno E_BAD_FD
//write data to disk and return value of size
//E_NO_SPACE
//E_FILE_TOO_BIG
int
File_Write(int fd, void *buffer, int size)
{
    printf("FS_Write\n");
    return 0;
}

int
File_Seek(int fd, int offset)
{
    printf("FS_Seek\n");
    return 0;
}

int
File_Close(int fd)
{
    printf("FS_Close\n");
    if(lseek(fd, 0, SEEK_CUR) == -1)
    {
    	osErrno = E_BAD_FD;
    	return -1;
    }
    close(fd);
    fileCounter -= 1;
    //CLOSE FILE, BUT SET FD TO NULL
    return 0;
}

int
File_Unlink(char *file)
{
    printf("FS_Unlink\n");
    return 0;
}


// directory ops
int 
Dir_Create(char *path) 
{ 
    printf("Dir_Create %s\n", path);
    int i;
    int j;
    int available;
    int newAddress;
    bool matched = false;

    //set max directories to longest pathname / 2 (minimum 1 char pathname + /)
    int maxDirectories = 256/2;

    //delimiter path by / and store directories in an array "directories"
    char** directories = (char**)malloc(maxDirectories*sizeof(char*));
    memset(directories, 0, sizeof(char*)*maxDirectories);

    char* curToken = strtok(path, "/");

    for (i=0; curToken!=NULL; ++i) {
        directories[i] = strdup(curToken);
        curToken = strtok(NULL, "/");
    }

    //go to root directory stored in sector 3
    Disk_Read(3, buffer);

    //search inode[3-32] of each directory and use delimiter to find name and match to name of current directory array
    for (i=0; i<maxDirectories; ++i) {
        matched = false;
        available = -1;

        for (j=2; j<32; ++j) {
            char *ptr_buffer = &buffer[j];
            if(ptr_buffer != NULL) {
                char** inodePointer = (char**)malloc(2*sizeof(char*));
                memset(inodePointer, 0, sizeof(char*)*2);

                curToken = strtok(ptr_buffer, "/");
                inodePointer[0] = strdup(curToken);
                curToken = strtok(NULL, "/");
                inodePointer[1] = strdup(curToken);
            
                //if match found, go to the corresponding inode#
                if (inodePointer[0] == directories[i]) {
                    int sectorLoc = atoi(inodePointer[1]);
                    Disk_Read(sectorLoc, buffer);
                    matched = true;
                }
            }
            else {
                available = j;
            }

            //if directory is found, break out and iterate to next value in directories array
            if (matched == true)
                break;
            //if not, make sure there is an open space for a pointer and create the new directory there
            else if (available > 1) {
                Disk_Read(1, buffer);
                for (j=0; j<256; ++j) {
                    if (buffer[j] == 1) {
                        newAddress = j + IBMOFFSET;
                        buffer[j] = 0;    //change ibm to reflect newly assigned inode
                        Disk_Write(1, buffer);
                        break;
                    }
                }
                buffer[0] = '0';
                buffer[1] = '1';
                buffer[2] = newAddress;
                Disk_Write(newAddress, buffer);
            }
            //otherwise, alert the user that there is not enough space to create a new directory
            else {
                printf("Not enough space\n"); 
                    osErrno = E_CREATE; 
                    return -1;
            }
        }
    }     
    return 0; 
}

int Dir_Size(char *path)
{
    printf("Dir_Size\n");
    int fd = open(path, O_RDWR);
    struct stat st;
    stat(path, &st);
    return st.st_size;
}

int
Dir_Read(char *path, void *buffer, int size)
{
    printf("Dir_Read\n");
    return 0;
}

int
Dir_Unlink(char *path)
{
    printf("Dir_Unlink\n");
    return 0;
}


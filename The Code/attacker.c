
#include "fileIOFunctions.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#define global_mem_size 65536
#define number_Of_Hex_Chars_For_Tag 5
#define number_Of_Hex_Chars_For_Set 2
#define number_Of_Hex_Chars_For_Offset 1
#define number_Of_Lines_In_Cache 256
#define micro_seconds_delay_for_cache_miss 500000 //half a second?

//global arrays for the programmer level 
unsigned char * globalMem ;
int pointerTounAllocatedMemory=0;
char** cacheTagArray;
char** cacheDataArray;

//Simple memory read/write methods for the programmer
unsigned char memRead(int Address){
    return readByteAtAddress(Address,globalMem,cacheTagArray,cacheDataArray);
}

void memWrite(unsigned char ByteToWrite,int Address){
    writeByteAtAddress(ByteToWrite,Address, globalMem,cacheTagArray,cacheDataArray);
}

//signal function that the victim process can use to ell us that it has finished processing/waiting and it's our turn to run
void sigUsr2(int signo){ //code =12
    signal(SIGUSR2,sigUsr2);//reloading for backwords linux compatibillity
}


//Primes a specific set and also returns the address which was used to prime the set 
//(that we read/wrote to in order to bring it to cache)
int prime_Specific_Set (int specificSet){
    //firstly find out which set is the Global Memory starting at:
    int addressOfTheBeginingOfVirtualMemory=globalMem;
    int currSet =getAddress_Set(addressOfTheBeginingOfVirtualMemory);
    //calculate how many 16 byte blocks - to advance in order to get to the set:
    int Differnce=0;
    if(specificSet>currSet){
        Differnce=specificSet-currSet;
    }
    if(specificSet<currSet){
        Differnce=256-currSet;
        Differnce=Differnce+specificSet;
    }
    int AddressWithThe_specificSet=addressOfTheBeginingOfVirtualMemory+16*Differnce;
    //Now ,we that have the address - we can "prime" the cache set by reading/Writing from/to the address: 
    memWrite(0xFF,AddressWithThe_specificSet);
    return AddressWithThe_specificSet;
}


//the shared page only has the instructionsOpcodes for  MultplyAndAdd
//offset is range 0-15 (inclusive).
unsigned char ReadFromSharedMemPage(int offsetInSharedPage,char **cacheTagArray,char **cacheDataArray){
    int simulated_Location_Of_page=0x00002640; //maps to set 100 . tag =00002 .offset 0 (aligned)
    unsigned char instructionsOpcodes[16] ={0x56,0xDD,0x1F,0x71,0x13,0x26,0x31,0x44,0xEB,0x9B,0x5F,0x05,0x29,0x8A,0x0A,0x10};
    int isAddressAlreadyIncache=checkIfAddressIsIncache(simulated_Location_Of_page,cacheTagArray);
    int set=getAddress_Set(simulated_Location_Of_page);
    //if it is not currently in cache - we are evicting line 100 - and storing instructionsOpcodes[] in cache:
    if(isAddressAlreadyIncache==0){
        printf("cache miss on ReadFromSharedMemPage Address %d .evicting set %d\n" ,simulated_Location_Of_page+offsetInSharedPage,set);
        waitForCacheMissDelay(); //cache miss - 0.5 seconds delay
        int numerical_tag =getAddress_Tag(simulated_Location_Of_page);
        char *tagStr=convertTagNumberToTagstring(numerical_tag);//dynamic -should be freed
        evictSet(set,tagStr,instructionsOpcodes,cacheTagArray,cacheDataArray);
        free(tagStr);        
    }
    else{
       printf("cache HIT on ReadFromSharedMemPage Address %d . set %d\n" ,simulated_Location_Of_page+offsetInSharedPage,set);
    }
    //reading the byte from cache and returnong it
    unsigned char byte =getSpecficByteFromDataArray(set,offsetInSharedPage,cacheDataArray);
    return byte;

}


//Reads from that address and returns the time it took to complete the read
double measureTimeToReadFromAddress(int address){
    //setup:
    struct timeval  startTime, endTime;
    gettimeofday(&startTime, NULL);
    //Reading -using the ReadFromSharedMemPage instead of memRead() - because we are reading from shared memory adress in this specific attack:
    ReadFromSharedMemPage(0,cacheTagArray,cacheDataArray);
    //memRead(address);
    gettimeofday(&endTime, NULL);
    double time=(double) (endTime.tv_usec - startTime.tv_usec) / 1000000 + (double) (endTime.tv_sec - startTime.tv_sec);
   // printf ("Total time = %f seconds\n",time);
    return time;

}
//measure Runtime of the victim process -by signaling it and waiting for it to signal back to us . 
double measureOtherProcessRuntime(int victimPid){
    //setup:
    struct timeval  startTime, endTime;
    gettimeofday(&startTime, NULL);
    //5 lines for proccess switching:
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    signalOtherProccessToRun(victimPid); //letting the victim run
    pause(); //wating for victim to finish
    cacheTagArray = loadCacheTagFromFile();
    cacheDataArray=loadCacheDataArrayFromFile(); 
    gettimeofday(&endTime, NULL);
    double time=(double) (endTime.tv_usec - startTime.tv_usec) / 1000000 + (double) (endTime.tv_sec - startTime.tv_sec);
    return time;

}


//we let control of the cpu to the other process 
// before we do that we need to update the cache files with current state.
void signalOtherProccessToRun(int pid ){
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    kill(pid,SIGUSR2);
}

//plaintext is 128 bits long = 16 bytes .we write it to a file so the victim process can read and encrypt it.
void writePlainIntoFile(unsigned char * plainTextByteArray){
    FILE *fptr;
    fptr = fopen("plainTextToEncrpt.txt","w");
    char StringTowriteTofile[150]="";
    char buffer[10]="";
    int i;
    //creating the string to write to file
    for(i=0;i<16;i++){
        sprintf(buffer,"0x%02X ",plainTextByteArray[i]);
        strcat(StringTowriteTofile,buffer);
    }
    fprintf(fptr,"%s",StringTowriteTofile);
    fclose(fptr);
}

/*
double *** allocate_3D_array (int rows,int cols,int z){
    int i,j;
    double *** arr =(double ***)malloc(rows*sizeof(double **));
    for(i=0;i<rows;i++){
        for(j=0;j<cols;j++){
            
        }
    }
}
*/
//We will change the plaintext in order to test for a specific xor result (x) between the upper halfs of plain[i] and plain[j] 
void changePlaintext(int i,int j,int x,unsigned char ValueForPadding,unsigned char *plain_16_bytes){
    int index;
    //writing the ValueForPadding to plain
    for(index=0;index<16;index++){
        plain_16_bytes[index]=ValueForPadding;
    }
    unsigned char plain_j = 0;
    unsigned char plain_i = x*16; //we write X into the upper half of p_i (shift left 4 times)
    //now <p_i> xor <p_j> = (xor between upper 4 bits) =  x xor 0 = x.
    plain_16_bytes[i]=plain_i;
    plain_16_bytes[j]=plain_j;
    //Now <plain[i]> xor <plain[j]> = x .
}

void flushSboxLinesFromCache(int setSboxStartsAt , int setSboxEndsAt ){
    int i=setSboxStartsAt;
    for(i=setSboxStartsAt;i<=setSboxEndsAt;i++){
        flush(i,cacheTagArray,cacheDataArray);
    }
}

//we want to print the xors we learned :
//K[0] xor K[4] , , K[0] xor K[8], K[0] xor K[12],K[4] xor K[8],K[4] xor K[12],K[8] xor K[12],
void printAttackResults(double *** time){
    int i,j,xorRes, indexOfMin=0;
    double minTime =2147483647;
    for(i=0;i<=12;i++){
        for(j=i+4;j<=12;j++){
            //now we need to find the minimum index out of 0-15
            minTime =2147483647;
            indexOfMin=0;
            for(xorRes=0;xorRes<16;xorRes++){
                if( time[i][j][xorRes] <   minTime){ //new minimum
                    minTime=time[i][j][xorRes];
                    indexOfMin=xorRes;
                }
                printf ( "<K_%d> xor <K_%d> = %d  \n",i,j,indexOfMin);
            }
        }
    }
}


int main(){
    //loading the signal sigusr2
    signal(SIGUSR2,sigUsr2);
    cacheTagArray = loadCacheTagFromFile();
    cacheDataArray=loadCacheDataArrayFromFile(); 
    //defining the global memory space (in DRAM) for this process:
    globalMem=(unsigned char *)malloc(global_mem_size*(sizeof(unsigned char)));
    writeAttackerPidToFile();
    int victimPid =readVictimPidFromFile();
    double excutionTimeAfterEvicting[256];
    flushSboxLinesFromCache(0,255);
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray); 
    //return 0;

    unsigned char plain_16_bytes [16]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    //writePlainIntoFile(plain_16_bytes);
    //Creating the Time (i , j , xorRes) array to measure runtime for all combinations of 
    double time[16][16][16];
    int i,j,xorRes ,paddingIndex=0;
    unsigned char valuesForPadding[4]={0,0xFF,0xAA,0x77};
    //To shorten the runtime we will only calculate pairs in {0,4,8,12}
    for(i=0;i<16;i=i+4){
        for(j=0;j<16;j=j+4){
            if(j==i){
                continue;//we dont need to measure time for <pi> xor <pi> because it doesnt give as information
            }
            for (xorRes=0;xorRes<16;xorRes++){ //xorRes will iterate over the options for xor res between plain[i] and plain[j] 
                for(paddingIndex=0;paddingIndex<4;paddingIndex++){
                    //flushing the cache to ensure that we dont get collisions between different runs.
                    flushSboxLinesFromCache(0,50); 
                    //choose a plaintext to test i,j,xorRes:
                    changePlaintext(i,j,xorRes,valuesForPadding[paddingIndex],plain_16_bytes); 
                    //writing plain to a file before invoking the victim to run and encrypt the plaintext.
                    writePlainIntoFile(plain_16_bytes);
                    //running and timing the victim with our changed plaintext:
                    //Dividing by 4 -to get an average out of 4 measurments
                    time[i][j][xorRes]+=measureOtherProcessRuntime(victimPid)/4;                    
                }

            }
        }

    }
    printf(" ****************RESULTS FOR THE CACHE COLLISION **********\n");
    //print the results :
    for(i=0;i<16;i=i+4){
        for(j=0;j<16;j=j+4){
            if(j==i)
                continue;//we dont need to measure time for <pi> xor <pi> because it doesnt give as information

                printf("\nTime for i= %d j= %d is : \n",i,j);
                for (xorRes=0;xorRes<16;xorRes++){
                    printf(" %f ",time[i][j][xorRes]);
            }
        }
    }
    //printAttackResults:
    printf("\n The results for the cache collison attack :\n");
    int  indexOfMin=0;
    double minTime =2147483647;
    for(i=0;i<=12;i=i+4){
        for(j=i+4;j<=12;j=j+4){
            //now we need to find the minimum index out of 0-15
            minTime =2147483647;
            indexOfMin=0;
            for(xorRes=0;xorRes<16;xorRes++){
                if( time[i][j][xorRes] <   minTime){ //new minimum
                    minTime=time[i][j][xorRes];
                    indexOfMin=xorRes;
                }
               
            }
            printf("\n");
             printf ( "<K_%d> xor <K_%d> = %d  \n",i,j,indexOfMin);
        }
    }
    printf("******************Attacker MAIN ENDED SUCCESSFULLY !**********************\n");
    free(globalMem);
    return 0;
}

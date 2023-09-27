
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

//Reads from that address and returns the time it took to complete the read
double measureTimeToReadFromAddress(int address){
    //setup:
    struct timeval  startTime, endTime;
    gettimeofday(&startTime, NULL);
    //Reading:
    memRead(address);
    gettimeofday(&endTime, NULL);
    double time=(double) (endTime.tv_usec - startTime.tv_usec) / 1000000 + (double) (endTime.tv_sec - startTime.tv_sec);
   // printf ("Total time = %f seconds\n",time);
    return time;

}

//we let control of the cpu to the other process 
// before we do that we need to update the cache files with current state.
void signalOtherProccessToRun(int pid ){
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    kill(pid,SIGUSR2);
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
    double timeToAccessSet_Before[256];
    double timeToAcssesSet_After [256];
    int i=0;
    //we evict all of the sets -one by one - and then we invoke the victim to run .
    //then we try to read the data again - and measure the time it took to read. 
    //(1) prime  (2) measure access time before (3) let victim run (4) measure access time after
    for(i=0;i<256;i++){
        int addressWeUsed= prime_Specific_Set(i); //(1)
        timeToAccessSet_Before[i]=measureTimeToReadFromAddress(addressWeUsed); //(2)
        signalOtherProccessToRun(victimPid); //(3)
        pause();//waits for signal from victim to proceed execution.
        //we reload from files to get the updated cache state:
        cacheTagArray = loadCacheTagFromFile();
        cacheDataArray=loadCacheDataArrayFromFile(); 
        //(4)
        timeToAcssesSet_After[i]=measureTimeToReadFromAddress(addressWeUsed);
    }

    //We  finished the attack. Now we can print the results :
    printf("      time before:           time after: \n");
    
    for(i=0;i<256;i++) {
         printf("set_%d :%f            %f\n",i,timeToAccessSet_Before[i],timeToAcssesSet_After[i] );
    }
       
    

    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    printf("******************Attacker MAIN ENDED SUCCESSFULLY !**********************\n");
    free(globalMem);
    return 0;
}

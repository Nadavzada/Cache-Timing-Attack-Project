
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
    double timeToAccess_criticalLines[32];
    int i=0;
    //we flush all of the sets -one by one - and then we invoke the victim to run .
    //then we try to read the data again - and measure the time it took to read. 
    //(1) flush  (2) wait for other procees to run (3) Reloading and measuring time 
    int sharedAddress=0x00002640;
    int setToflush =getAddress_Set(sharedAddress); //The attacker knows the shared address and it's set. 
    for(i=31;i>=0;i--){
        flush(setToflush,cacheTagArray,cacheDataArray); //((1)
        //now we let the victim to run:(2)
        UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
        signalOtherProccessToRun(victimPid);
        pause();//wating for victim to return the control to us.
        //Getting the updated cache state: 
        cacheTagArray = loadCacheTagFromFile();
        cacheDataArray=loadCacheDataArrayFromFile(); 
        //(3) Reloading
        timeToAccess_criticalLines[i]=measureTimeToReadFromAddress(sharedAddress);
    }



    //We  finished the attack. Now we can print the results :
    printf("\n****************The results are**********************  \n");
    int guessedBits [32];
    int RecoveredKey =0;
    for(i=31;i>=0;i--) {
         printf("Access time on iteration %d is %f ",32-i, timeToAccess_criticalLines[i]);
        int didWeHaveAshortAccessTime=timeToAccess_criticalLines[i]<0.5;
        if(didWeHaveAshortAccessTime)  //short time ->cache hit ->the victim accessed the critical lines ->d_i was 1.
            guessedBits[i]=1;
        else                                    //long time ->cache miss ->victim didnt accesses critical lines-> d_i was 0.
            guessedBits[i]=0;
        printf("guessed bit =%d \n", guessedBits[i]);
        RecoveredKey+=(guessedBits[i]<<i) ; //calculating the decimal value of the key : adding bit*2^weight 
        
    }

    printf("guessedKeyInBinary: ");
    for(i=31;i>=0;i--){
        printf("%d",guessedBits[i]);
    }
    printf("\n");
    printf("RecoveredKey in decimal %d . in hex :%X \n",RecoveredKey,RecoveredKey);

    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    printf("******************Attacker MAIN ENDED SUCCESSFULLY !**********************\n");
    free(globalMem);
    return 0;
}


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

//signal function to invoke the victim to run:
void sigUsr2(int signo){ //code =12
    signal(SIGUSR2,sigUsr2);//reloading for backwords linux compatibillity
}





putMultplyAndAddInstructionsInGlobalMem(int offsetInsideGlobalMem,unsigned char *globalMem){
    // We are loading a specific memory (and cache) block with "Multiply and Add" commands (part of RSA excecution).
    //this instructions will be accessed by the prorgram if they are excecuted during RSA encryption.
    int i=0;
    unsigned char instructionsOpcodes[16] ={0x56,0xDD,0x1F,0x71,0x13,0x26,0x31,0x44,0xEB,0x9B,0x5F,0x05,0x29,0x8A,0x0A,0x10};
    //copying into global mem
    for(i=0;i<16;i++){
        globalMem[offsetInsideGlobalMem+i]=instructionsOpcodes[i];
    }
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

void Do_Some_Cryptography(int offsetOfKeyInGlobalMem,int offsetOfSboxInGlobalMem){
    unsigned char randomPlainText[16];
    //reading key:
   // printf("offsetOfSboxInGlobalMem= %d\n",offsetOfSboxInGlobalMem);
    unsigned char key=memRead((int)&globalMem[offsetOfKeyInGlobalMem]);
    int i;
    //generating random plainText.
    for(i=0;i<16;i++){
        randomPlainText[i]=rand()%16;
    }
    //i ,key ,xorRes are in a register. randomPlainText isn't stored in memory.
    //now we encrypt : SBOX(key xor plain)
    //unsigned char cipher [16]={0x00,0x00,0x00,0x00,0x00,0x0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    //assaign space for the cipher in global mem:
    int OffsetOfCipherInMemory=pointerTounAllocatedMemory;
    pointerTounAllocatedMemory+=16;
    for(i=0;i<16;i++){
        unsigned char xorRes=(key^randomPlainText[i])%16;
        unsigned char Cipher= memRead(&globalMem[offsetOfSboxInGlobalMem+xorRes])  ;
        memWrite(Cipher,&globalMem[OffsetOfCipherInMemory+i]);    
    }
    //release cipher memory after calculation is finished (it is a local array to the function):
    pointerTounAllocatedMemory+=-16;
}

//we let control of the cpu to the other process 
// before we do that we need to update the cache files with current state.
void signalOtherProccessToRun(int pid ){
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    kill(pid,SIGUSR2);
}






int main(){
     writeVictimPidToFile();
    //loading the signal sigusr2
    signal(SIGUSR2,sigUsr2);
    //defining the global memory space (in DRAM) for the process:
    globalMem=(unsigned char *)malloc(global_mem_size*(sizeof(unsigned char)));
     int offsetOfSboxInGlobalMem=pointerTounAllocatedMemory;
     //Hard coded sbox is in Dram:
    putSboxOntoTheGlobalMem(offsetOfSboxInGlobalMem,&globalMem[offsetOfSboxInGlobalMem]);
    pointerTounAllocatedMemory+=16;
    //put secret Key =0xEA into memory in an arbitrary location
    int offsetOfKeyInGlobalMem=rand()%(global_mem_size-pointerTounAllocatedMemory)+pointerTounAllocatedMemory;
    globalMem[offsetOfKeyInGlobalMem]=0xEA;
    pointerTounAllocatedMemory+=16;
    
    //We Are loading the multply and add are in shared memory in a location that corresponds to set 100.
    //We will put them in an arbitrary location which corresponds to set 100: 
    cacheTagArray = loadCacheTagFromFile();
    cacheDataArray=loadCacheDataArrayFromFile(); 
    //Victim waits to be called invoked by the system/other process.Runs some cryptography and then waits again.
    //waiting for the attacker to call for the sta
    int private_SecretKey_d=0xFFABCD91;// (exponent e) - 32 bit
    int m=256;//modulu for exponent function .
    int b=0x2A771308;  //the number we calculate b^d =? (cipher to decrypt)
    int x=1 ; //result for exponent
    
    pause();// waiting for the other process (Attacker) to ask for decryption 
    int callingProcessPid =readAttackerPidFromFile();
    cacheTagArray = loadCacheTagFromFile();
    cacheDataArray=loadCacheDataArrayFromFile(); 
    printf("Victim is invoked to run RSA decryption :\n");
    int i;
    //exponent(b,(d=e),m ):
    for( i=31;i>=0;i--){ //exponent is 32 bits long
        x=pow((double)x,(double)2);
        x=x%m;
        int d_i=getSpecificBitFromInteger(private_SecretKey_d,i);
        if(d_i==1){
            //We excecute the if->we are accessing the line in shared memory -
            //and the block will be read and loaded into cache
            ReadFromSharedMemPage(0,cacheTagArray,cacheDataArray);
            x=x*b;
            x=x%m;
        }
        //at the end of every iteration we are letting the other proceess(attacker) the control over the cpu.
        UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
        printf("Victim has ended iteration %d\n",i);
        signalOtherProccessToRun(callingProcessPid);
        pause(); //wating to get the cpu again -before continuing to next iteration
        //loading cache state
        int nextI=i-1;
        printf("Victim is resuming for iteration %d\n",nextI);
        cacheTagArray = loadCacheTagFromFile();
        cacheDataArray=loadCacheDataArrayFromFile(); 

    }
    
   /* 
   while(1){
        pause();//wainting for sigusr2 signal from the system (the attacker can invoke it to run)
        int callingProcessPid =readAttackerPidFromFile();
        printf("Victim is invoked and running :\n");
        printf("Doing some symetric cryptography:\n");
        //loading cache state:
        cacheTagArray = loadCacheTagFromFile();
        cacheDataArray=loadCacheDataArrayFromFile(); 
        Do_Some_Cryptography(offsetOfKeyInGlobalMem,offsetOfSboxInGlobalMem);
        UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
        signalOtherProccessToRun(callingProcessPid);
        printf("******victim ended -waiting for another invocation******\n");
    }
    */
    free (globalMem);
    return 0;
}

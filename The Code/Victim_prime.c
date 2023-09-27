
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
    //Victim waits to be called invoked by the system/other process.Runs some cryptography and then waits again.
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
    free (globalMem);
    return 0;
}


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

void Extended_Do_Some_Cryptography(int offsetOfKeyInGlobalMem,int offsetOfSboxInGlobalMem){
    unsigned char randomPlainText[16];
    //reading key:
   // printf("offsetOfSboxInGlobalMem= %d\n",offsetOfSboxInGlobalMem);
    //Extension to 16 blocks of key !
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
    int runningOffsetOfKeyInGlobalMemory= offsetOfKeyInGlobalMem;
    for(i=0;i<8;i++){
        //Extension to use 8 byte key:
        unsigned char currKey=memRead((int)&globalMem[runningOffsetOfKeyInGlobalMemory]);
        runningOffsetOfKeyInGlobalMemory+=16;
        unsigned char xorRes=(currKey^randomPlainText[i])%16;
        unsigned char Cipher= memRead(&globalMem[offsetOfSboxInGlobalMem+xorRes])  ;
        memWrite(Cipher,&globalMem[OffsetOfCipherInMemory+i]);    
    }
    runningOffsetOfKeyInGlobalMemory=offsetOfKeyInGlobalMem;
    for(i=0;i<8;i++){
        //Extension to use 8 byte key:
        unsigned char currKey=memRead((int)&globalMem[runningOffsetOfKeyInGlobalMemory]);
        runningOffsetOfKeyInGlobalMemory+=16;
        unsigned char xorRes=(currKey^randomPlainText[i])%16;
        unsigned char Cipher= memRead(&globalMem[offsetOfSboxInGlobalMem+xorRes])  ;
        memWrite(Cipher,&globalMem[OffsetOfCipherInMemory+i]);    
    }
    //release cipher memory after calculation is finished (it is a local array to the function):
    pointerTounAllocatedMemory+=-16;
}

//We recieve the location of the sbox in memory (virtual memory/DRAM).
void perform_AES_first_round(int offsetOfPlainInGlobalMem ,int offsetOfSboxInGlobalMem,int OffsetOfKeyInGlobalMem){
    int i;
    unsigned char currKey,tableOutput,currPlain,xorRes;
    //creating pointers for easy access:
    unsigned char *plaintext=&globalMem[offsetOfPlainInGlobalMem];
    unsigned char *T0_Sbox= &globalMem[offsetOfSboxInGlobalMem];
    unsigned char *Key_arr= &globalMem[OffsetOfKeyInGlobalMem];
    printf("key array is :\n");
    for (i=0;i<16;i++){
        printf("%02X ",Key_arr[i]);
    }
    printf("\n");
    // now we perform the T0 table lookups for : Sbox [ key[i] xor plain[i]  ]  as i is in {0 ,4,8,12} 
    for(i=0;i<=12;i=i+4){
        currKey=memRead((int) &Key_arr[i]);
        currPlain=memRead((int) &plaintext[i]);
        xorRes=(currKey^currPlain);
        //Now we perform the table lookup:                                                                                        
        tableOutput=memRead(&T0_Sbox[xorRes]);
        printf("i=%d : curr key = %02X ,currPlain =%02X ,xorRes =%02X,tableOutput= %02X \n",i,currKey,currPlain,xorRes,tableOutput);
    }

}






void put_256_byte_Sbox_In_Global_Mem (int OffsetInGlobalMem){
    //(This is similiar to Rijndael S-box  -hard coded .(not identical ,but using the same values see: https://en.wikipedia.org/wiki/Rijndael_S-box)
    unsigned char Sbox [256]=
                        {   0x63 ,0x7C ,0x77 ,0x7B ,0xF2 ,0x6B ,0x6F ,0xC5 ,0x30 ,0x01 ,0x67 ,0x2B ,0xFE ,0xD7 ,0xAB ,0x76 ,
                            0x10 ,0xCA ,0x82 ,0xC9 ,0x7D ,0xFA ,0x59 ,0x47 ,0xF0 ,0xAD ,0xD4 ,0xA2 ,0xAF ,0x9C ,0xA4 ,0x72 ,
                            0xC0 ,0x20 ,0xB7 ,0xFD ,0x93 ,0x26 ,0x36 ,0x3F ,0xF7 ,0xCC ,0x34 ,0xA5 ,0xE5 ,0xF1 ,0x71 ,0xD8 ,
                            0x31 ,0x15 ,0x30 ,0x04 ,0xC7 ,0x23 ,0xC3 ,0x18 ,0x96 ,0x05 ,0x9A ,0x07 ,0x12 ,0x80 ,0xE2 ,0xEB ,
                            0x27 ,0xB2 ,0x75 ,0x40 ,0x09 ,0x83 ,0x2C ,0x1A ,0x1B ,0x6E ,0x5A ,0xA0 ,0x52 ,0x3B ,0xD6 ,0xB3 ,
                            0x29 ,0xE3 ,0x2F ,0x84 ,0x50 ,0x53 ,0xD1 ,0x00 ,0xED ,0x20 ,0xFC ,0xB1 ,0x5B ,0x6A ,0xCB ,0xBE ,
                            0x39 ,0x4A ,0x4C ,0x58 ,0xCF ,0x60 ,0xD0 ,0xEF ,0xAA ,0xFB ,0x43 ,0x4D ,0x33 ,0x85 ,0x45 ,0xF9 ,
                            0x02 ,0x7F ,0x50 ,0x3C ,0x9F ,0xA8 ,0x70 ,0x51 ,0xA3 ,0x40 ,0x8F ,0x92 ,0x9D ,0x38 ,0xF5 ,0xBC ,
                            0xB6 ,0xDA ,0x21 ,0x10 ,0xFF ,0xF3 ,0xD2 ,0x80 ,0xCD ,0x0C ,0x13 ,0xEC ,0x5F ,0x97 ,0x44 ,0x17 ,
                            0xC4 ,0xA7 ,0x7E ,0x3D ,0x64 ,0x5D ,0x19 ,0x73 ,0x90 ,0x60 ,0x81 ,0x4F ,0xDC ,0x22 ,0x2A ,0x90 ,
                            0x88 ,0x46 ,0xEE ,0xB8 ,0x14 ,0xDE ,0x5E ,0x0B ,0xDB ,0xA0 ,0xE0 ,0x32 ,0x3A ,0x0A ,0x49 ,0x06 ,
                            0x24 ,0x5C ,0xC2 ,0xD3 ,0xAC ,0x62 ,0x91 ,0x95 ,0xE4 ,0x79 ,0xB0 ,0xE7 ,0xC8 ,0x37 ,0x6D ,0x8D ,
                            0xD5 ,0x4E ,0xA9 ,0x6C ,0x56 ,0xF4 ,0xEA ,0x65 ,0x7A ,0xAE ,0x08 ,0xC0 ,0xBA ,0x78 ,0x25 ,0x2E ,
                            0x1C ,0xA6 ,0xB4 ,0xC6 ,0xE8 ,0xDD ,0x74 ,0x1F ,0x4B ,0xBD ,0x8B ,0x8A ,0xD0 ,0x70 ,0x3E ,0xB5 ,
                            0x66 ,0x48 ,0x03 ,0xF6 ,0x0E ,0x61 ,0x35 ,0x57 ,0xB9 ,0x86 ,0xC1 ,0x1D ,0x9E ,0xE0 ,0xE1 ,0xF8 ,
                            0x98 ,0x11 ,0x69 ,0xD9 ,0x8E ,0x94 ,0x9B ,0x1E ,0x87 ,0xE9 ,0xCE ,0x55 ,0x28 ,0xDF ,0xF0 ,0x8C 
                        };
    int i;
    int runningOffset=OffsetInGlobalMem;
    //copy Sbox array into global mem
    for (i=0;i<256;i++){
        globalMem[runningOffset]=Sbox[i];
        runningOffset++;
    } 
    pointerTounAllocatedMemory=pointerTounAllocatedMemory+(16*16);//allocating 16 blocks to the 256 sbox

    }

    void TesterFor_put_256_byte_Sbox_In_Global_Mem(){
        int i=0;
        //Test for 256 byte Sbox :
        int sbox_offsetInGlobalMem=pointerTounAllocatedMemory;
        put_256_byte_Sbox_In_Global_Mem(sbox_offsetInGlobalMem);
        unsigned char sboxReadFromMemory[256];
        for (i=0;i<256;i++){
            sboxReadFromMemory[i]=memRead((int)&globalMem[sbox_offsetInGlobalMem+i]); 
        } 
        //Reading back the  values we read:
        printf("\n ********************The sbox is : *********\n");
        for (i=0;i<256;i++){
            printf("%02X ,",sboxReadFromMemory[i]);
            if(i%15==0)
            printf("\n");
        } 

    }




void put_128_bit_key_into_global_mem (int offsetOfKeyInGlobalMem){
        //Extension : using 1 set for the key : loading 16 bytes into memory
    unsigned char key_16_bytes [16]={0x55,0x43,0xAF,0x68,0x91,0xDB,0xCC,0xE5,0x78,0xD3,0xA9,0x18,0x99,0xCE,0x6D,0xB8};
    int i;
    int Running_offsetOfKeyInGlobalMem=offsetOfKeyInGlobalMem;
     pointerTounAllocatedMemory+=16;//allocating space for it
     //copying into global mem (DRAM)
    for(i=0;i<16;i++){
        globalMem[Running_offsetOfKeyInGlobalMem]=key_16_bytes[i];
        Running_offsetOfKeyInGlobalMem++;
    }
}



//recieves a pointer to a 16 bit array which we will fill with data read from file:
void readPlaintextFromFile (unsigned char * plaintext){
    FILE *fptr;
    fptr = fopen("plainTextToEncrpt.txt","r");
    int i;
    const unsigned MAX_LENGTH = 82;
    char buffer[MAX_LENGTH];
    //reading the line from the file into a buffer.
    fgets(buffer, MAX_LENGTH, fptr);
    //reading 16 values from buffer and coverting to unsigned chars (bytes)
    //code is similiar to "getByteArrayOfACacheDataLine()". 
    char byteBuffer[5];
    byteBuffer[4]='\0';
    int indexOfStartOfString=0;
    for (i=0;i<16;i++){
        byteBuffer[0]=buffer[indexOfStartOfString];
        byteBuffer[1]=buffer[indexOfStartOfString+1];
        byteBuffer[2]=buffer[indexOfStartOfString+2];
        byteBuffer[3]=buffer[indexOfStartOfString+3];
        //converting "0xHH" to a byte:
        int numericalValue=(int)strtol(byteBuffer,NULL,0);
        plaintext[i]=numericalValue;
        indexOfStartOfString+=5;
    }
    fclose(fptr);
}

//method that reads from file then writes the data to the global memory. Returns the offset in global memory which it is stored at.
int getPlainText_AndPutInGlobalMemory(){
    int i;
    unsigned char plaintextReadFromFile[16];
    readPlaintextFromFile(plaintextReadFromFile);
    //put plainText into global memory:
    int offsetOfPlainTextInGlobalMem=pointerTounAllocatedMemory;
    pointerTounAllocatedMemory+=16;
    for(i=0;i<16;i++){
        globalMem[i+offsetOfPlainTextInGlobalMem]=plaintextReadFromFile[i];
    }    
    return offsetOfPlainTextInGlobalMem;
}







//we let control of the cpu to the other process 
// before we do that we need to update the cache files with current state.
void signalOtherProccessToRun(int pid ){
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    kill(pid,SIGUSR2);
}
void printPlainText (int offsetOfPlainTextInGlobalMem){
    int i;
    printf("plaintext is : ");
    for(i=0;i<16;i++){
        printf("%02X,",globalMem[i+offsetOfPlainTextInGlobalMem]);
    }
    printf("\n");
}





int main(){
    int i;
     writeVictimPidToFile();
    //loading the signal sigusr2
    signal(SIGUSR2,sigUsr2);
    //defining the global memory space (in DRAM) for the process:
    globalMem=(unsigned char *)malloc(global_mem_size*(sizeof(unsigned char)));
    cacheTagArray = loadCacheTagFromFile();
    cacheDataArray=loadCacheDataArrayFromFile();
    //put_256_byte_Sbox_In_Global_Mem(sbox_offsetInGlobalMem); 
    int sbox_offsetInGlobalMem=pointerTounAllocatedMemory;
    put_256_byte_Sbox_In_Global_Mem(sbox_offsetInGlobalMem);
    //put 16 byte key onto global memory:
    int offsetOfKeyInglobalMem=pointerTounAllocatedMemory;
    put_128_bit_key_into_global_mem(offsetOfKeyInglobalMem);
    //reading plaintext from file:
    
    
    
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    
    
    
    int counter=0;

   while(1){
        pause();//wainting for sigusr2 signal from the system (the attacker can invoke it to run)
        int callingProcessPid =readAttackerPidFromFile();
        counter++;
        printf("Victim is invoked and running for the %d time  :\n",counter);
        printf("Performing first round of AES :\n");
        cacheTagArray = loadCacheTagFromFile();
        cacheDataArray=loadCacheDataArrayFromFile(); 
        int offsetOfPlainTextInGlobalMem=getPlainText_AndPutInGlobalMemory();
        printPlainText(offsetOfPlainTextInGlobalMem);
        //Do_Some_Cryptography:
        perform_AES_first_round(offsetOfPlainTextInGlobalMem,sbox_offsetInGlobalMem,offsetOfKeyInglobalMem);
        //Returning control to the calling process:
        UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
        signalOtherProccessToRun(callingProcessPid);
        printf("******victim ended -waiting for another invocation******\n");
        //realocating space for plaintext:
        pointerTounAllocatedMemory-=16;
    }
    free (globalMem);
    return 0;
}

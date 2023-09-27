
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

#define number_Of_Hex_Chars_For_Tag 5
#define number_Of_Hex_Chars_For_Set 2
#define number_Of_Hex_Chars_For_Offset 1
#define number_Of_Lines_In_Cache 256
#define micro_seconds_delay_for_cache_miss 500000 //half a second?


void free2dArray(char ** arr,int rows);
///
///****************************************************************
//Data array METHODS 
//*****************************************************************
char ** dymaciacllyAlocateCharArr (int lines,int columns){
    char ** Arr=(char **)malloc(sizeof(char*)*lines);
    int i=0;
    for (i=0;i<lines;i++){
        Arr[i]=(char*)malloc(sizeof(char)*columns);
    }
    return Arr;
}
//
char ** loadCacheDataArrayFromFile(){
     int i,j;
     char** cacheDataArray =dymaciacllyAlocateCharArr(number_Of_Lines_In_Cache,81);
     
     FILE *fptr;
     fptr = fopen("cacheDataArray.txt","r");
     //printf("file openened\n");
     const unsigned MAX_LENGTH = 82;
     char buffer[MAX_LENGTH];
     i=0;
     
     //reading from the file line by line
     while (fgets(buffer, MAX_LENGTH, fptr)){
        strcpy(cacheDataArray[i],buffer);
       // printf("%d %s",i,cacheDataArray[i]);
        i++;
    }   
   fclose(fptr);
   return cacheDataArray;
}
//writing the array to the file when the process finishes running or 
void writeToDataArraryFile(char ** cacheDataArray){
   int i;
   FILE *fptr;
     fptr = fopen("cacheDataArray.txt","w");
     for(i=0;i<number_Of_Lines_In_Cache;i++){
         fprintf(fptr,"%s",cacheDataArray[i]);
     }
     
     fclose(fptr);

}
//retruns a byte array (char array) each char is a byte->we return 16 bytes array which is a cache line/block
unsigned char* getByteArrayOfACacheDataLine(int set ,  char **cacheDataArray ){
    int i;
    unsigned char* byteLineArr=(unsigned char*) malloc(sizeof(unsigned char)*16);
    char *lineString = cacheDataArray[set];
    char buffer[5];
    buffer[4]='\0';
    int indexOfStartOfString=0;
    for(i=0;i<16;i++){
        buffer[0]=lineString[indexOfStartOfString];
        buffer[1]=lineString[indexOfStartOfString+1];
        buffer[2]=lineString[indexOfStartOfString+2];
        buffer[3]=lineString[indexOfStartOfString+3];
        int numericalValue=(int)strtol(buffer,NULL,0);
        unsigned char byteValue=(char)   numericalValue%256;
        byteLineArr[i]=byteValue;
        //printf("i=%d ,buffer= %s,byteValue=%d |\n",i,buffer,byteValue);
        indexOfStartOfString+=5;

    }
    return byteLineArr;
    //strtol(tag1str,NULL,0)
}

//Important : if we initialize manually the byteArray the length should be const .
//meaning char arr[16]={0x1,0xFF...}. NOT char * arr !! 
void writeByteArrayToACacheDataLine(int setToUpdate ,unsigned char* byteArray , char **cacheDataArray ){
    //we will build this line and then  copy this line to the cacheDataArray:
    char wholeLine[82]="";
    //template for string:
    char number_buffer[6]={'0','x','0','0','0','\0'};
    int i=0 ,index_at_string=0;
    for(i=0;i<16;i++){
        number_buffer[2]='0';
        int startIndex=2;
        if(byteArray[i]<16){
            startIndex=3;
        }
        sprintf(&number_buffer[startIndex],"%X ",byteArray[i]);
        //printf("buffer is :%s.\n",number_buffer);
        strcat(wholeLine,number_buffer);
    }
   // printf("\nwriteByteArrayToACacheDataL:whole line is : %s\n",wholeLine);
    strcat(wholeLine,"\n");
    
    strcpy(cacheDataArray[setToUpdate],wholeLine);
   
}

//use this method to get a specific block at a cache Line at a known set.
unsigned char getSpecficByteFromDataArray(int set,int offsetInBlock, char **cacheDataArray){
    unsigned char * dataLineAtSet =getByteArrayOfACacheDataLine(set,cacheDataArray);
    int i=0;
    unsigned char byte =dataLineAtSet[offsetInBlock];
    free(dataLineAtSet);
    return byte;
}
//must check before hand that the tag is equal !  only then we can change bytes in the cache data array at a specific offset.
void writeByteToDataArray(unsigned char byteTowrite,int set,int offsetInBlock,char **cacheDataArray){
    unsigned char * dataLineAtSet =getByteArrayOfACacheDataLine(set,cacheDataArray);
    dataLineAtSet[offsetInBlock]=byteTowrite;
    writeByteArrayToACacheDataLine(set,dataLineAtSet,cacheDataArray);
    free(dataLineAtSet);
}

void testerForDataArray(){
    int i;
    printf("*********************\nMain is running\n");
    char **cacheDataArray=loadCacheDataArrayFromFile();

    cacheDataArray[1][2]='D';
    cacheDataArray[1][3]='5';
    //cacheDataArray[1][0]='1';
    writeToDataArraryFile(cacheDataArray);
    
    unsigned char* DataLine_At_Set_0 =getByteArrayOfACacheDataLine(0,cacheDataArray);
    unsigned char byte = getSpecficByteFromDataArray(1,0,cacheDataArray);
    printf("byte = %X\n",byte);
     byte = getSpecficByteFromDataArray(2,2,cacheDataArray);
    printf("2,2  = %X\n",byte);
    printf("DataLine_At_Set_0 is :\n");
    for(i=0;i<16;i++){
        printf("%d=%X ,",DataLine_At_Set_0[i],DataLine_At_Set_0[i]);
    }


    unsigned char byteArrayTowrite[16]={0x55,0xFF,0x6d,0x55,0xFF,0x6d,0x55,0xFF,0x6d,0x55,0xFF,0x6d,0x6d,0x6d,0x6d,0x6d};
    writeByteArrayToACacheDataLine(2,byteArrayTowrite,cacheDataArray);

    writeByteToDataArray(0x30,3,0,cacheDataArray);
    writeByteToDataArray(0xFF,3,15,cacheDataArray);
    writeByteToDataArray(0xFF,3,7,cacheDataArray);
    writeByteToDataArray(0xFF,0,14,cacheDataArray);
    writeToDataArraryFile(cacheDataArray);
    ////freeing arrays
    free2dArray(cacheDataArray,number_Of_Lines_In_Cache);
    free(DataLine_At_Set_0);
    printf("End Of Main *********************\n");
}


///****************************************************************
//TAG METHODS 
//*****************************************************************
//helper method :
//works for 32 bit integers - retruns 1 or 0 - the value of the requested bit.
int getSpecificBitFromInteger(int integer ,int indexOfBit){
    return (integer >> indexOfBit) & 1;
}

int convertStringTagToNumber (char * tagString){
    return strtol(tagString,NULL,0);
}

void print2dArray(char ** arr,int rows,int columns){
    int i,j;
    for (i=0;i<rows;i++){
        printf("%s\n",arr[i]);
    }
}
//set is the index of the line in the cache
char * getTagAtSet (int set,char ** cacheTagArr){
    return cacheTagArr[set];
}

void changeTagAtSet(int set,char * newTagString,char ** cacheTagArr){
    if(strlen(newTagString)>9){
        printf("error at -changeTagAtSet- input string too long(>9)");
    }
   // printf("im in changeTagAtSet\n");
    cacheTagArr[set]=strcpy(cacheTagArr[set],newTagString);
    // printf("changeTagAtSet: worked!\n");
}

//This function reads the Txt file that has the simulated Cache tag array - and stores it in a byte array of 256 *9  which it then returns
//(256 lines - 9 charachters)
char ** loadCacheTagFromFile(){
     //first we load the tag arr
     //char cacheTagArray [128][9];
     char** cacheTagArray= dymaciacllyAlocateCharArr(number_Of_Lines_In_Cache,8);
     int i=0,j=0;
     FILE *fptr;
     char buffer[10]="";
     fptr = fopen("cacheTagArray.txt","r");
      //cacheTagArray[0][0]='c';
     //going over the whole file and copying the 256 tag values into the array ;
     for(i=0;i<number_Of_Lines_In_Cache;i++){
         fscanf(fptr,"%s",buffer);
         //printf("i=%d \n",i);
         for(j=0;j<8;j++){
             //printf("i=%d , cacheTagArray[i][j]=%c\n , ",i,buffer[j]);
             cacheTagArray[i][j]=buffer[j];
         }
         //printf("\n");
     }
     //printf("the first string :%s\n",cacheTagArray[0]);

     fclose(fptr);
     return cacheTagArray;

}
//overWrites the cacheTagArray.txt file with updated array from our program
//SHOULD BE CALLED WHENEVER WE STOP THE EXCECTUTION/WAIT/SLEEP ETC- BECAUSE IT UPDATES THE CACHE THAT THE OTHER PROCESS SEES!
void writeToTagArrayFile(char **cacheTagArray){
    FILE * fptr;
    fptr= fopen("cacheTagArray.txt","w");
    int i;
    for(i=0;i<number_Of_Lines_In_Cache;i++){
        fprintf(fptr,"%s\n",cacheTagArray[i]);
    }
    fclose(fptr);
}
void testerForTagFile(){
    struct timeval  startTime, endTime;
    gettimeofday(&startTime, NULL);
    char **cacheTagArray;;
    cacheTagArray=loadCacheTagFromFile();
    print2dArray(cacheTagArray,number_Of_Lines_In_Cache,8);
    char * tag1str =getTagAtSet(0,cacheTagArray);
    printf("string of tag at set 0 is :%s \n",tag1str);
    printf("numerical value is : %d\n",strtol(tag1str,NULL,0));
    char *str ="0xFFFFF";
    changeTagAtSet(2,str,cacheTagArray);
    changeTagAtSet(1,"0x54ADC",cacheTagArray);
    writeToTagArrayFile(cacheTagArray);
    
    gettimeofday(&endTime, NULL);
    printf ("Total time = %f seconds\n",
         (double) (endTime.tv_usec - startTime.tv_usec) / 1000000 +
         (double) (endTime.tv_sec - startTime.tv_sec));
}
//********************************************************************
void free2dArray(char ** arr,int rows){
    int i;
    for (i=0;i<rows;i++){
        free(arr[i]);
    }
    free(arr);
}
//////////////////////////////////////////////////////////////////////
//Exchanging Pid's 
//////////////////////////////////////////////////////////////
void writeAttackerPidToFile(){
    int pid =getpid();
    FILE *fptr;
    fptr = fopen("Attacker_PID.txt","w");
    char buffer[100]="";
    sprintf(buffer,"%d",pid);
    fprintf(fptr,"%s",buffer);
    //printf("the pid of the attacker is :%s\n",buffer);
    fclose(fptr);
}
int readVictimPidFromFile(){
    FILE *fptr;
    fptr = fopen("Victim_PID.txt","r");    
    char buffer[100]="";
    fscanf(fptr,"%s",buffer);
    int pid =atoi(buffer);
    printf("the pid of the victim is %d\n",pid);
    fclose(fptr);
    return pid;
}

void writeVictimPidToFile(){
    int pid =getpid();
    FILE *fptr;
    fptr = fopen("Victim_PID.txt","w");
    char buffer[100]="";
    sprintf(buffer,"%d",pid);
    fprintf(fptr,"%s",buffer);
   // printf("the pid of the victim is :%s\n",buffer);
    fclose(fptr);
}
int readAttackerPidFromFile(){
    FILE *fptr;
    fptr = fopen("Attacker_PID.txt","r");    
    char buffer[100]="";
    fscanf(fptr,"%s",buffer);
    int pid =atoi(buffer);
    printf("the pid of the attacker is %d\n",pid);
    fclose(fptr);
    return pid;
}
//////////////////////////////////////////////////////////////






















///****************************************************************
//CPU PART
//*****************************************************************
//helper method :
int convertStringToNumber (char * String){
    // strtol(tagString,NULL,0) takes a string in format 0xYYYYY.. and converts to int.
    printf("\nstr  we get in convertStringToNumber: %s \n",String);
    int x=(int)strtol(String,NULL,0);
    return x;
}
//helper method ://dynamic! format of output 0xYYYYY
char * convertTagNumberToTagstring(int numericalTag){
    char * tagStr =(char*) malloc (sizeof(char)*8);
    tagStr[0]='0';
    tagStr[1]='x';
    //we add the text from the third charachter
    sprintf(&tagStr[2], "%05X", numericalTag);
    return tagStr;
}

int getAddress_Set (int address){
    int set = address/16;
    set = set%256;
    return set;
}

int getAddress_Offset (int address){
    int offset = address%16;
    return offset;
}

int getAddress_Tag (int address){
    int tag = address/4096;
    return tag;
}

int checkIfAddressIsIncache(int Address,char** cacheTagArray){
    int mySet, myTag, boolean;
    mySet = getAddress_Set(Address); //numerical
    myTag = getAddress_Tag(Address);//numerical
    char tag[8] = "0x12345";//initializing the arrays.
    char str1[8] = "0x";
    sprintf(tag, "%05X", myTag); 
    strcat(str1,tag);//creating the formatted string 0xYYYYY
    boolean = strcmp(str1, cacheTagArray[mySet]);
    if (boolean == 0){
        return 1;
    }
    else{
        return 0;
    }
}

//writing both arrays to the cache - should be called every time we "leave" the cpu or let another process to run (waiting)
void UpdateCacheTextFiles(char** cacheTagArray,char ** cacheDataArray){
    writeToDataArraryFile(cacheDataArray);
    writeToTagArrayFile(cacheTagArray);

}
void freeBothArrays(char** cacheTagArray,char ** cacheDataArray){
    free2dArray(cacheTagArray,number_Of_Lines_In_Cache);
    free2dArray(cacheDataArray,number_Of_Lines_In_Cache);
}



//************************************************************************************************************
//************************************************************************************************************
//************************************************************************************************************
//************************************************************************************************************
//************************************************************************************************************


void waitForCacheMissDelay(){
    usleep(micro_seconds_delay_for_cache_miss);
}

//writes zeroes to the tag and the array.
void flush(int setToFlush,char** cacheTagArray,char** cacheDataArray){
    //use changeTagAtSet () , and writeByteArrayToACacheDataLine - defineunsigned char *=  {0,0..16 times}
    if(setToFlush>number_Of_Lines_In_Cache ){
        printf("Error in flush method - set %d outside of range of sets",setToFlush);
        return;
    }
    //changing the tag and the data array to be all zeros. 
    changeTagAtSet (setToFlush,"0x00000",cacheTagArray);
    unsigned char zerosByteArray[16]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    writeByteArrayToACacheDataLine(setToFlush,zerosByteArray,cacheDataArray);
    //printf("set :%d was flushed !\n",setToFlush);
   // char * newTag=getTagAtSet(setToFlush,cacheTagArray);
}

    //This method will be called when the cpu decides to evict a cache line at a specific set and replace it with a new line (new tag and data)
    //Important : if we initialize manually the byteArray the length should be const .
    //meaning char arr[16]={0x1,0xFF...}. NOT char * arr !! 
    void evictSet (int set,char *newTag ,unsigned char* newDataByteArray,char** cacheTagArray,char** cacheDataArray)
    {
       // printf("****\nIm in evictSet\n");
        //printf("the set=%d,the tag=%s\n",set,newTag);
        //changing the tag and data at the line.
        changeTagAtSet (set,newTag,cacheTagArray);
        //printf("THe tag at set %d after the evict is  %s \n",set,getTagAtSet(set,cacheTagArray));
        // printf("finished changeTagAtSet\n");
     /*    printf("in evict set : the Byte Array TO WRITE! is : \nbytearray= {");
         int i;
         //checking the byte Array is correct :
         for(i=0;i<16;i++){
             printf("%d ,",newDataByteArray[i]);
         }
         printf("}\n");
         */
         
         writeByteArrayToACacheDataLine(set,newDataByteArray,cacheDataArray);
 /*        printf("in evict set : the Byte Array read from set %d is : \n{",set);
         unsigned char* byteArrReadBack =getByteArrayOfACacheDataLine(set,cacheDataArray);
                  //checking the byte Array is correct :
         for(i=0;i<16;i++){
             printf("%d ,",byteArrReadBack[i]);
         }
         free(byteArrReadBack);
         printf("}\n");
         */
        // printf("****\nEnded evictSet ****\n");
    }

 
    //preforming the memory reads.We need access to the global memory so we can 
    unsigned char  readByteAtAddress(int address,unsigned char *globalMem,char** cacheTagArray,char** cacheDataArray ){
        //firstly we decode/breaak the address into its tag ,set and offset:
        int set=getAddress_Set(address);
        int tag=getAddress_Tag(address);
        int offset=getAddress_Offset(address);
        //Then -we want to check if the address/block the address is in - is already in cache :
        int isAddressAlreadyIncache=checkIfAddressIsIncache(address,cacheTagArray);
        if(isAddressAlreadyIncache==0)
        {
            ////The address is not currently in cache-> we sould evict and replace with new tag data! 
            //also - add a artificial/simulated cache miss delay of (1/2 second?)
            waitForCacheMissDelay(); 
            //now we need to read 16 bytes from global memory and then copy that line to the cache:
            //assumption - the address is either aligned to a start of block - or it is possible to go back and read bytes 
            //that are "behind" it . (i.e. if offset=2 we need to able to accsess (address -1) (address -2) etc.)
            printf("readByteAtAddress: cache miss:evicting set %d ,new tag is :0x%05X \n",set,tag);
            unsigned char byteArr [16]={0x00,0x00,0x00,0x00,0x00,0x0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
            int i;
            
            int AddressTheBlockStartsAt=address-offset;
            unsigned char * memoryPointer =(unsigned char *)AddressTheBlockStartsAt;

            //copying from Dram(global memory) to the byteArr 
          //  printf("now we read from global mem , AddressTheBlockStartsAt=%d ,memoryPointer=%d\n",AddressTheBlockStartsAt,memoryPointer);
            for (i=0;i<16;i++){
                byteArr[i]=memoryPointer[i];
               // printf("now byteArr[%d]=%d\n",i,byteArr[i]);
            }
            //now we create the new tag string;
            char *tagStr=convertTagNumberToTagstring(tag);//dynamic -should be freed
            // :eviting the set - with new tag and data :
            //printf("now we evict:set=%d,tagStr=%s\n",set,tagStr);
            evictSet(set,tagStr,byteArr,cacheTagArray,cacheDataArray);
            free(tagStr);
        }
        else{
            printf("readByteAtAddress:CACHE HIT ON ADDRESS %d SET %d and Tag =0x%05X\n",address,set,tag);
        }
        //Now ,we know that the data is in cache . We return the byte that is stored in the cache:
        unsigned char byte =getSpecficByteFromDataArray(set,offset,cacheDataArray);
        return byte;
    }

    // We have a write-through policy.If the address isnt in cache its' block will be loaded .
    //when we write to memory we update both cache and global Memory(DRAM).
    void writeByteAtAddress(unsigned char byteTowrite,int address,unsigned char *globalMem,char** cacheTagArray,char** cacheDataArray){
        //Decoding addresses:
        int i=0;
        int set=getAddress_Set(address);
        int tag=getAddress_Tag(address);
        int offset=getAddress_Offset(address);
        int AddressTheBlockStartsAt=address-offset;
        unsigned char * memoryPointer =(unsigned char *)AddressTheBlockStartsAt;
        //Then -we want to check if the line/block the address is in - is already in cache :
        int isAddressAlreadyIncache=checkIfAddressIsIncache(address,cacheTagArray);
        if(isAddressAlreadyIncache==0){
            ////The address is not currently in cache-> we sould evict and replace with new tag data! 
            //also - add a artificial/simulated cache miss delay of (1/2 second?)
            waitForCacheMissDelay();
            unsigned char byteArr [16]={0x00,0x00,0x00,0x00,0x00,0x0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

            //copying from Dram(global memory) to the byteArr 
           //  printf("writeByteAtAddress:now we read from global mem , AddressTheBlockStartsAt=%d ,memoryPointer=%d\n",AddressTheBlockStartsAt,memoryPointer);
            for (i=0;i<16;i++){
                byteArr[i]=memoryPointer[i];
               // printf("writeByteAtAddress:now byteArr[%d]=%d\n",i,byteArr[i]);
            }
             //now we create the new tag string;
            char *tagStr=convertTagNumberToTagstring(tag);//dynamic -should be freed
            // :eviting the set - with new tag and data :
            printf("writeByteAtAddress:cache miss on address %d now we evict:set=%d,tagStr=%s\n",address,set,tagStr);
            evictSet(set,tagStr,byteArr,cacheTagArray,cacheDataArray);
           // printf("now finished evicting\n");
            free(tagStr);           
            
        }
        else{
            printf("writeByteAtAddress:CACHE HIT ON ADDRESS %d SET %d and Tag =0x%05X\n",address,set,tag);
        }
        //now we write the Byte to the cache and global Memory :
        memoryPointer[offset]=byteTowrite; //write to global Memory
        writeByteToDataArray(byteTowrite,set,offset,cacheDataArray);//write to cache
    }

//Helper method initialies the sbox onto the DRAM - initial state of the program.
void putSboxOntoTheGlobalMem(int offsetInsideGlobalMem,unsigned char *globalMem){
    int i;
    //mapping of 8 bits to 8 bits.sbox is precalculated:
    //sbox is hard coded
    unsigned char sbox[16]={0x0F,0x03,0x07,0x0A,0x0E,0x04,0x0D,0x01,0x00,0x05,0x6,0x0C,0x2,0x09,0xB,0x08};
    for(i=0;i<16;i++){
        globalMem[offsetInsideGlobalMem+i]=sbox[i];
    }

}


void testerFor_flush_evictSet_waitForCacheMissDelay(){
      int mySet1 = 1269764;
    char ** cacheTagArray;
    cacheTagArray = loadCacheTagFromFile();
    char ** cacheDataArray=loadCacheDataArrayFromFile();
    flush(4,cacheTagArray,cacheDataArray);
    unsigned char  byteArrayForSet3[16]={0x13,0x23,0x33,0x43,0x03,0x50,0x06,0x07,0x1d,0x05,0x2,0x00,0x2,0x10,0x9,0x1};
    evictSet(4,"0x12345",byteArrayForSet3,cacheTagArray,cacheDataArray);
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    //cache miss tester
    printf("we wait for cache miss \n:");
    waitForCacheMissDelay();
    printf("waiting ended\n");
    //tester for convertTagNumberToTagstring()
//    int tag=0x12345;
  //  char * str =convertTagNumberToTagstring(tag);
  //  printf("\n \n********** \nline 135 :convertTagNumberToTagstring(0x12345) =%s \n******* \n",str);
  //  free(str);
      UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    freeBothArrays(cacheTagArray,cacheDataArray);
    printf("******************MAIN ENDED SUCCESSFULLY !**********************\n");
}

void testerFor_readByteAtAddress_writeByteAtAddress(){
    char ** cacheTagArray;
    cacheTagArray = loadCacheTagFromFile();
    char ** cacheDataArray=loadCacheDataArrayFromFile();
//readByteAtAddress tester :
    //define global memory array :

    //THIS  WORKs :
    //1. readByteAtAddress is reading correctly the whole sbox and updating the data array
    //2 .tag is showing - but for some reason the addresses that are genetated for the start of globalMem are always at set 3.
    //Creating the global memory (DRAM):
    int offsetInsideGlobalMemForSbox =0;
    unsigned char * globalMem =(unsigned char *)malloc(65536*(sizeof(unsigned char))); //2^16 bytes .
    printf("the starting address for the globalMem: %u \n",&globalMem[0] );
    int offsetInGlobalMem =((unsigned int) globalMem )%16 ; //we want to be aligned to a start of a block.
    putSboxOntoTheGlobalMem(offsetInGlobalMem,&globalMem[0]);
    //we read the whole sbox which is 16 bytes long - We should get 1 miss and then 15 hits:
    int i=0;
    while (i<16){
    //printf("the offsetToStartFrom in globalMem is : %d\n",offsetInGlobalMem);
    char byteFromMemory =readByteAtAddress((int)&globalMem[0+i],globalMem,cacheTagArray,cacheDataArray);
    printf("Byte we read : %d \n",byteFromMemory);
   
    //changeTagAtSet(39,"0x11111",cacheTagArray);
    i++;
    }

    //Test for writeByteAtAddress: changing the first byte of the sbox to 0x99:
    //writing
    writeByteAtAddress(0x99,(int)&globalMem[0],globalMem,cacheTagArray,cacheDataArray);
    //reding back to see if the write was successfull :
    unsigned char byteReadAfterChange =readByteAtAddress((int)&globalMem[0],globalMem,cacheTagArray,cacheDataArray);
    printf("Byte we byteReadAfterChanging from cache(sould be 0x99)  : 0x%02X \n",byteReadAfterChange);
    printf("Byte we byteReadAfterChanging globalMem(sould be 0x99)  : 0x%02X \n",globalMem[0]);

    //preforming a read from the line after the sbox: (sould give us a cache miss:)
    readByteAtAddress((int)&globalMem[0+i],globalMem,cacheTagArray,cacheDataArray);

     //freeing arrays and finishing Test
    UpdateCacheTextFiles(cacheTagArray,cacheDataArray);
    free(globalMem);
    freeBothArrays(cacheTagArray,cacheDataArray);
    printf("******************Test ENDED SUCCESSFULLY !**********************\n");


}



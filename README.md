
Readme for :
Cache-timing-attack-project 

How to run this attack on your pc :
The code is written, compiled and tested for unix enviroments only.
So you will need either linux running natively on your pc or on a virtual machine.

files should be compiled with the gcc compiler.
You need to have the FileIoFunctions.h in the same directory (used as a library for all attackers and Victims)
Also, you nedd to have "cacheDataArray.txt" , "cacheTagArray.txt" , "SharedMemoryPage.txt" in the same directory as well.
The attacker and victim runnables should also run from the same directory.

***Overall - just  have the files in the same directory ***
___________________________________________________________________________________________________
How to run after compiling (or run the exsting compiled runnable files) ?
Open the terminal , navigate to the folder where all the files are.

The Victim should always run first so run it by typing :
 "  ./Victim &  "    //Victim is the name of the compiled executable file . '&' is so that we can keep typing and run the attacker
 Then run the attacker file :
 "./attacker"
 
 
 You can write your own attackers and victims . More explanations in the pdf file for the project . 
 




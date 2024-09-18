#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int i; // For loops

    if (argc != 3)
    {
        printf("ARGV ERROR: Enter file name and integer for interrupt timer.");
        exit(EXIT_FAILURE);
    }

    // Open file
    char *fileName = argv[1];
    FILE *file = fopen(fileName, "r");
    if(file==NULL)
    {
        printf("FILE ERROR, please try again.");
        exit(EXIT_FAILURE);
    }

    // Create pipes before fork
    int cpuToMem[2];
    int memToCpu[2];

    if (pipe(cpuToMem) == -1 || pipe(memToCpu) == -1) {
        perror("PIPE ERROR");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        printf("FORK ERROR");
        exit(EXIT_FAILURE);
    }

        //// Child process (Memory) ////
    else if (pid == 0) {
        int mem[2000];

        // To prevent garbage values, filling array with 0s (won't be a problem data-wise since instr never starts with 0)
        for (i = 0; i < 2000; i++) {
            mem[i] = 0;
        }

        int userPtr = 0;

        char tempLine[500];
        int tempInt;

        while (fgets(tempLine, 500, file)) // While there is a next line
        {
            if (tempLine[0] == '.') {
                char jumpTo[4];

                // Removing the period then storing the jump to value
                for (i = 1; i < 5; i++)
                    jumpTo[i - 1] = tempLine[i];

                userPtr = atoi(jumpTo);
            } else if (isdigit(tempLine[0])) {
                mem[userPtr] = atoi(tempLine);
                //printf("%d\n", mem[userPtr]);
                //printf("%d\n", userPtr);
                userPtr++;
            }

            // If neither dot nor digit, it will skip the line entirely

        }

        // At this point, all the data is stored in memory.
        fclose(file);

        // Now take in CPU requests which will consist of r or w or e followed by 4 bit max address
        char cpuRequest[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
        int addressInt = 0;
        int fetchedInst = 0;
        char rweCommand = '\0';

        //close(cpuToMem[1]);
        //close(memToCpu[0]);

        while (true) // Will continue to read requests until terminated through "e" request by parent
        {
            read(cpuToMem[0], &cpuRequest, 6);
            rweCommand = cpuRequest[0];

            // Remove the letter from the request and store as an address value
            for (i = 0; i < 5; i++)
                cpuRequest[i] = cpuRequest[i + 1];

            addressInt = atoi(cpuRequest);

            if (rweCommand == 'r') // Send parent data at requested address
            {
                fetchedInst = mem[addressInt];
                //printf("memory at %d = %d\n", addressInt, fetchedInst);
                write(memToCpu[1], &fetchedInst, sizeof(fetchedInst));
            }

            if (rweCommand == 'w') // Already read request, so now read data to store
            {
                int storeVal = 0;
                read(cpuToMem[0], &storeVal, sizeof(storeVal)); // storeVal now has the write value;
                mem[addressInt] = storeVal;
            }

            if (rweCommand == 'e')
                exit(EXIT_SUCCESS);
        }
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        /// Parent process (CPU) ////
    else
    {
        int timer = atoi(argv[2]);
        int instCount = 0;
        bool usrMode = true;
        bool intrMode = false;
        int timerIntrQueue = 0;

        // Set up registers in this parent process b/c it executes the instructions
        int topUsrStack = 999;
        int topSysStack = 1999;
        int pc = 0; // Instructions start at 0;
        int sp = topUsrStack; // Starts in user mode, so points to top of user stack (grows towards 0 so sp-- each time we store)
        int ir;
        int ac;
        int x;
        int y;

        while (true) // Will run and evaluate instructions until End is called
        {
            bool jumped = false; // Used to prevent incrementing the pc when we jumped

            char tmpWriteBuff[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
            int instToProc = 0;

            // For when we are processing a syscall but need to schedule a timer interrupt after it is done.
            if( intrMode && (instCount%timer == 0) && instCount > 0 )
            {
                timerIntrQueue++;
            }

            // Perform timer interrupt here before next inst fetch.
            // Checking if there was a timer already queued before OR for if we need a timer interrupt at this int.
            // The usrMode condition essentially disables timer interrupts until sys call is done.
            if( ( usrMode && (timerIntrQueue > 0) ) || ( usrMode && (instCount%timer == 0) && (instCount > 0) ) )
            {
                timerIntrQueue--;
                intrMode = true;
                usrMode = false;
                int upc = pc; // Not pc + 1 like in system intr because we have not executed inst at pc yet.
                int usp = sp;
                pc = 1000; // Bc a timer int. should cause execution at address 1000 (1500 for sys call)
                sp = topSysStack; // Now accessing system stack, which is not available to user.
                jumped = true; // We jumped to address 1000

                // Now we write those old pc and sp values into the system stack
                // We saved pc FIRST, so sp will be retrieved first when we pop stack

                snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Requesting to write at mem[sp]
                write(cpuToMem[1], &usp, sizeof(usp)); // Giving child old sp value to write

                sp--;
                snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Requesting to write at mem[sp]
                write(cpuToMem[1], &upc, sizeof(upc)); // Giving child old pc value to write

                sp--;
                continue;
                // Now instructions at 1000 will be executed until 27 is called and old values restored.
            }

            // Counts every instruction, even the ones in kernel mode
            instCount++;

            // Read next instruction
            snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc); // Copy pc's request to read to buffer
            write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
            read(memToCpu[0], &instToProc, sizeof(instToProc)); // Read what mem returned.

            // At this point, we either have the instruction ready to process or a 0 (empty)

            if (instToProc != 0) // If the instruction at that memory is not empty
            {
                ir = instToProc;

                // Load value
                if (ir == 1)
                {
                    pc++;
                    int valFromMem = 0;
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.
                    ac = valFromMem;
                    //printf("\nchanged ac to%d", ac);
                }

                // Load addr
                if (ir == 2)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                    // Now we fetched the address to look in, so check its value:
                    //printf("\ntrying to fetch from address %d", valFromMem);
                    if(valFromMem > 999 && usrMode == true)
                    {
                        printf("\nMEMORY ERROR: User mode cannot access system memory.");
                        exit(EXIT_FAILURE);
                    }

                    else // Valid memory address
                    {
                        // Get value from specific mem and store in ac
                        snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", valFromMem);
                        write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                        read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                        ac = valFromMem;
                    }
                }

                // LoadInd addr
                if (ir == 3)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                    // Now we fetched the address to look for other address in, so check its value:

                    if(valFromMem > 999 && usrMode == true)
                    {
                        printf("\nMEMORY ERROR: User mode cannot access system memory.");
                        exit(EXIT_FAILURE);
                    }

                    else // Valid memory address
                    {
                        // Get value from specific mem and store in ac
                        snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", valFromMem);
                        write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                        read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                        // Now we fetched the address in the address, so check its value as well:

                        if(valFromMem > 999 && usrMode == true)
                        {
                            printf("\nMEMORY ERROR: User mode cannot access system memory.");
                            exit(EXIT_FAILURE);
                        }

                        else // Valid memory address
                        {
                            // Get value from specific mem and store in ac
                            snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", valFromMem);
                            write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                            read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                            ac = valFromMem;
                        }
                    }
                }

                // LoadIdxX addr
                if (ir == 4)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                    // Now we fetched the address to look in, so add to it and check its value:

                    valFromMem = x + valFromMem;

                    if(valFromMem > 999 && usrMode == true)
                    {
                        printf("\nMEMORY ERROR: User mode cannot access system memory.");
                        exit(EXIT_FAILURE);
                    }

                    else // Valid memory address
                    {
                        // Get value from specific mem and store in ac
                        snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", valFromMem);
                        write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                        read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                        ac = valFromMem;
                    }
                }

                // LoadIdxY addr
                if (ir == 5)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                    // Now we fetched the address to look in, so add to it and check its value:

                    valFromMem = y + valFromMem;

                    if(valFromMem > 999 && usrMode == true)
                    {
                        printf("\nMEMORY ERROR: User mode cannot access system memory.");
                        exit(EXIT_FAILURE);
                    }

                    else // Valid memory address
                    {
                        // Get value from specific mem and store in ac
                        snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", valFromMem);
                        write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                        read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                        ac = valFromMem;
                    }
                }

                // LoadSpX
                if (ir == 6)
                {
                    int spx = sp + x + 1;
                    int valFromMem = 0;

                    if(spx > 999)
                    {
                        printf("\nMEMORY ERROR: User mode cannot access system memory.");
                        exit(EXIT_FAILURE);
                    }

                    else // Valid memory address
                    {
                        // Get value from specific mem and store in ac
                        snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", spx);
                        write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                        read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned.

                        ac = valFromMem;
                        //printf("\nretrieved %d from stack\n", ac);
                    }
                }

                // Store addr
                if (ir == 7)
                {
                    pc++;
                    int valFromMem = 0;

                    // Retrieve address value after the 7 instruction
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned


                    // Now we fetched the address to STORE in:

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", valFromMem);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); ///////////////////


                    //snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "%d", ac);
                    write(cpuToMem[1], &ac, sizeof(ac));

                }

                // Get
                if (ir == 8)
                {
                    srand(time(NULL));
                    ac = rand() % 100 + 1;
                }

                // Put port
                if (ir == 9)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned

                    // Now valFromMem has either port 1 or 2

                    if (valFromMem == 1)
                    {
                        printf("%d", ac);
                    }

                    if (valFromMem == 2)
                    {
                        printf("%c", ac); // Prints char with ascii decimal value of ac register
                    }
                }

                    //AddX
                else if (ir == 10)
                    ac += x;

                    //AddY
                else if (ir == 11)
                    ac += y;

                    //SubX
                else if (ir == 12)
                    ac -= x;

                    //SubY
                else if (ir == 13)
                    ac -= y;

                    //CopyToX
                else if (ir == 14)
                    x = ac;

                    //CopyFromX
                else if (ir == 15)
                    ac = x;

                    //CopyToY
                else if (ir == 16)
                    y = ac;

                    //CopyFromY
                else if (ir == 17)
                    ac = y;

                    //CopyToSp
                else if (ir == 18)
                    sp = ac;

                    //CopyFromSp
                else if (ir == 19)
                    ac = sp;

                    // Jump addr
                else if (ir == 20)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned

                    jumped = true;
                    pc = valFromMem;
                }

                    // JumpIfEqual addr
                else if (ir == 21)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned

                    if(ac == 0)
                    {
                        jumped = true;
                        pc = valFromMem;
                    }
                }

                    // JumpIfNotEqual addr
                else if (ir == 22)
                {
                    pc++;
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read what mem returned

                    if(ac != 0)
                    {
                        jumped = true;
                        pc = valFromMem;
                    }
                }

                    // Call addr
                else if (ir == 23)
                {
                    //printf("\n sp value before jump: %d", sp);
                    pc++; // to fetch the address to jump to
                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", pc); // fetch instr at pc
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem)); // Read instr mem returned
                    //printf("\ncurrent valFromMem = %d\n", valFromMem);
                    // valFromMem has the address we will jump to after putting ra on stack

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff));

                    //printf("\npc before inc: %d\n", pc);
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc)); // telling mem to save this value

                    sp--; // grows down towards 0;
                    //printf("\n sp value after jump: %d", sp);

                    // Now jump to that address after "23" that is in valFromMem
                    jumped = true;
                    pc = valFromMem;
                    //printf("\ncurrent pc: %d\n", pc);

                }

                    // Ret
                else if (ir == 24)
                {
                    sp++; // Trying to retrieve the "top" of the stack.

                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem));

                    // Now valFromMem has the previous return address we stored, so simply jump to it.

                    jumped = true;
                    pc = valFromMem;

                }

                    // IncX
                else if (ir == 25)
                {
                    x++;
                }

                    // DecX
                else if (ir == 26)
                {
                    x--;
                }

                    // Push
                else if (ir == 27)
                {
                    //printf("\n sp value before writing ac to mem: %d", sp);
                    // We want to write ac into whatever sp is currently at:
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff));

                    write(cpuToMem[1], &ac, sizeof(ac));

                    sp--;
                    //printf("\n sp value after writing ac to mem: %d", sp);
                }

                    // Pop
                else if (ir == 28)
                {
                    sp++; // Accessing element "on top of" the current stack address.

                    int valFromMem = 0;

                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem));

                    // Now valFromMem has the previous stack value we stored, so simply store in ac:

                    ac = valFromMem;

                }

                    // Int
                else if (ir == 29)
                {
                    //printf("\nperforming system int call !!!!!!!!!!!");
                    intrMode = true;
                    usrMode = false;
                    int upc = pc + 1; // pc + 1 because we don't want to jump back to the interrupt call again
                    int usp = sp;
                    pc = 1500; // Bc a timer int. should cause execution at address 1000 (1500 for sys call)
                    sp = topSysStack; // Now accessing system stack, which is not available to user.
                    jumped = true; // We jumped to address 1000

                    // Now we write those old pc and sp values into the system stack
                    // Also remember that we saved pc FIRST, so sp will be retrieved first when we pop stack
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Requesting to write at mem[sp]
                    //snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "%d", usp);
                    write(cpuToMem[1], &usp, sizeof(usp)); // Giving child old sp value to write

                    sp--;
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "w%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Requesting to write at mem[sp]
                    //snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "%d", upc);
                    write(cpuToMem[1], &upc, sizeof(upc)); // Giving child old pc value to write

                    sp--;
                    continue;
                    // Now instructions at 1000 will be executed until 27 is called and old values restored.

                }

                    // IRet
                else if (ir == 30)
                {
                    //printf("\nIRET CALL");
                    // Bring back to user mode and fetch top of system stack for old sp then pc
                    int valFromMem = 0;
                    usrMode = true;
                    intrMode = false;
                    jumped = true;

                    sp++;
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem));
                    pc = valFromMem; // basically jumping back to the instr we never got to processing
                    //printf("\n retrieved old pc val: %d", pc);

                    sp++;
                    snprintf(tmpWriteBuff, sizeof(tmpWriteBuff), "r%d", sp);
                    write(cpuToMem[1], &tmpWriteBuff, sizeof(tmpWriteBuff)); // Write from buffer to mem
                    read(memToCpu[0], &valFromMem, sizeof(valFromMem));
                    sp = valFromMem;
                    //printf("\n retrieved old sp val: %d", sp);
                }

                    // End
                else if (ir == 50)
                {
                    //Signal memory to exit
                    write(cpuToMem[1], "e", 1);

                    //Close all pipes
                    close(cpuToMem[0]);
                    close(memToCpu[0]);
                    close(cpuToMem[1]);
                    close(memToCpu[1]);

                    //Exit
                    wait(NULL);
                    //printf("\nac val: %d\n", ac);
                    exit(EXIT_SUCCESS);
                }

                if(!jumped)
                    pc++;
            }
        }
    }
}

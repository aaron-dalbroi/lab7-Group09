#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0

// Struct describing contents of a single TLB entry.
struct TableLookAsideBufferEntry{
    int VPN; // 8 bits.
    int PFN; // 8 bits.
    int processID; // 2 bits.
    int valid; // 1 bit.
    int replacementIndex; // 8 bits.
} typedef TLBEntry;

// Struct describing contents of a single Linear Page Table entry.
struct LinearPageTableEntry{
    int PFN; // 8 bits.
    int present; // 8 bits.
    int valid; // 1 bit.
}typedef LPTEntry;

// Converts TLB struct to uint32_t.
uint32_t TLBEntryToBinary(const TLBEntry * entryData);

// Converts uint32_t to TLB struct.
void BinaryToTLBEntry(uint32_t binaryData, TLBEntry * entryData);

// Converts LPT struct to uint32_t.
uint32_t LPTEntryToBinary(const LPTEntry * entryData);

// Converts uint32_t to LPT struct.
void BinaryToLPTEntry(uint32_t binaryData, LPTEntry * entryData);

// Initializes physical memory.
void InitializePhysicalMemory(int numOffsetBits, int numPFNBits);

// Initializes TLB.
void InitializeTLB();

// Initializes page table.
void InitializePT(int numVPNBits);

// Swithes context.
void SwitchToPID(int processID);

// Increments replacement indexes of all valid TLB entries.
void TLBIncrementReplacementIndexes();

// Checks the TLB for VPN entry. Returns index of valid entry, -1 otherwise.
// Here it checks that TLB entry is valid.
int CheckTLBValid(int VPN);

// Checks the TLB for VPN entry. Returns index of entry, -1 otherwise.
// Here it doesn't check that TLB entry is valid.
int CheckTLB(int VPN);

// Finds first invalid TLB entry. Returns its index. If none are invalid, returns -1.
int FirstInvalidTLBIndex();

// Finds the index of TLB entry that is next in line to be overridden.
int OverrideTLBIndex();

// Maps VPN to PFN. 
void MapVPNtoPFN(int VPN, int PFN);

// Unmaps VPN is the current context.
void UnmapVPN(int VPN);

//
void load(char *reg, char *arg);

//
void add();

// Output file
FILE *output_file;

// TLB replacement strategy (FIFO or LRU)
char *strategy;

// Keeps track of if we have used define
int defined_occured = FALSE;

// Keeps track of current processID id
int process_id = 0;

// ptr to representations of physical mem and the TLB
uint32_t *physical_memory;
uint32_t *TLB;
uint32_t * PT[4];

// Two registers
uint32_t r1;
uint32_t r2;

// Register values must be saved and restored when context switching
// There are 4 processes and 2 registers that need to be saved, use offset of id to get proper values
uint32_t save_registers[8];

char **tokenize_input(char *input)
{
    char **tokens = NULL;
    char *token = strtok(input, " ");
    int num_tokens = 0;

    while (token != NULL)
    {
        num_tokens++;
        tokens = realloc(tokens, num_tokens * sizeof(char *));
        tokens[num_tokens - 1] = malloc(strlen(token) + 1);
        strcpy(tokens[num_tokens - 1], token);
        token = strtok(NULL, " ");
    }

    num_tokens++;
    tokens = realloc(tokens, num_tokens * sizeof(char *));
    tokens[num_tokens - 1] = NULL;

    return tokens;
}

int main(int argc, char *argv[])
{
    const char usage[] = "Usage: memsym.out <strategy> <input trace> <output trace>\n";
    char *input_trace;
    char *output_trace;
    char buffer[1024];

    // Parse command line arguments
    if (argc != 4)
    {
        printf("%s", usage);
        return 1;
    }
    strategy = argv[1];
    input_trace = argv[2];
    output_trace = argv[3];

    // Open input and output files
    FILE *input_file = fopen(input_trace, "r");
    output_file = fopen(output_trace, "w");

    // Define needs to be the first instruction
    int first_instruction = 1;

    while (!feof(input_file))
    {
        // Read input file line by line
        char *rez = fgets(buffer, sizeof(buffer), input_file);
        if (!rez)
        {
            fprintf(stderr, "Reached end of trace. Exiting...\n");
            return -1;
        }
        else
        {
            // Remove endline character
            buffer[strlen(buffer) - 1] = '\0';
        }
        char **tokens = tokenize_input(buffer);

        // TODO: Implement your memory simulator

        printf("%s", tokens[0]);
        // Skip comments
        if (strcmp(tokens[0], "%") == 0)
        {
            continue;
        }

        // This is to handle the first instruction must always be define
        if (first_instruction)
        {
            // If we are at first command and it is define, initialize physical memory
            if (strcmp(tokens[0], "define") == 0)
            {
                // TODO: Check that atoi will go through.
                int numOffsetBits = atoi(tokens[1]);
                int numPFNBits = atoi(tokens[2]);
                int numVPNBits = atoi(tokens[3]);

                InitializePhysicalMemory(numOffsetBits, numPFNBits);
                InitializeTLB();
                InitializePT(numVPNBits);

                fprintf(output_file, "Current PID: %d. Memory instantiation complete. OFF bits: %s. PFN bits: %s. VPN bits: %s\n", process_id, tokens[1], tokens[2], tokens[3]);
            }
            else
            {
                fprintf(output_file, "Current PID: %d. Error: attempt to execute instruction before define\n", process_id);
                exit(EXIT_FAILURE);
            }
            first_instruction = 0;
        }
        else
        {
            // If after 1st instruction we try to call define, end program.
            if (strcmp(tokens[0], "define") == 0)
            {
                fprintf(output_file, "Current PID: %d. Error: multiple calls to define in the same trace\n", process_id);
                exit(EXIT_FAILURE);
            }

            // Call to handle context switch.
            if (strcmp(tokens[0], "ctxswitch") == 0)
            {
                SwitchToPID(atoi(tokens[1]));
            }

            // Call to map.
            if(strcmp(tokens[0], "map") == 0){
                int VPN = atoi(tokens[1]);
                int PFN = atoi(tokens[2]);
                MapVPNtoPFN(VPN, PFN);
                fprintf(output_file, "Current PID: %d. Mapped virtual page number %d to physical frame number %d\n", process_id, VPN, PFN);
            }

            //Call to unmap.
            if(strcmp(tokens[0], "unmap") == 0){
                int VPN = atoi(tokens[1]);

                UnmapVPN(VPN);

                fprintf(output_file, "Current PID: %d. Unmapped virtual page number %d\n", process_id, VPN);
            }

            // Call to handle load instruction.
            if (strcmp(tokens[0], "load") == 0)
            {
                load(tokens[1], tokens[2]);
            }

            // Call to handle add instruction.
            if (strcmp(tokens[0], "add") == 0)
            {
                add();
            }
        }

        // Deallocate tokens
        for (int i = 0; tokens[i] != NULL; i++)
            free(tokens[i]);
        free(tokens);
    }

    // Close input and output files
    fclose(input_file);
    fclose(output_file);

    // TODO: Deallocate TLB, PT, Physical memory.

    return 0;
}

uint32_t TLBEntryToBinary(const TLBEntry * entryData){
    // Clear any existing data.
    uint32_t binaryData = 0;

    // Encode VPN.
    binaryData += entryData->VPN;

    // Encode PFN.
    binaryData = binaryData << 8;
    binaryData += entryData->PFN;

    // Encode processID.
    binaryData = binaryData << 2;
    binaryData += entryData->processID;

    // Encode valid.
    binaryData = binaryData << 1;
    binaryData += entryData->valid;

    // Encode replacementIndex.
    binaryData = binaryData << 8;
    binaryData += entryData->replacementIndex;

    return binaryData;
}

void BinaryToTLBEntry(uint32_t binaryData, TLBEntry * entryData){
    // Decode replacementIndex.
    entryData->replacementIndex = binaryData & 255;
    binaryData = binaryData >> 3;

    // Decode valid.
    entryData->valid = binaryData & 1;
    binaryData = binaryData >> 1;

    // Decode processID.
    entryData->processID = binaryData & 3;
    binaryData = binaryData >> 2;

    // Decode PFN.
    entryData->PFN = binaryData & 255;
    binaryData = binaryData >> 8;

    // Decode VPN.
    entryData->VPN = binaryData & 255;
}

uint32_t LPTEntryToBinary(const LPTEntry * entryData){
    // Clear any existing data.
    uint32_t binaryData = 0;

    // Encode PFN.
    binaryData += entryData->PFN;

    // Encode present.
    binaryData = binaryData << 8;
    binaryData += entryData->present;

    // Encode valid.
    binaryData = binaryData << 1;
    binaryData += entryData->valid;

    return binaryData;
}

void BinaryToLPTEntry(uint32_t binaryData, LPTEntry * entryData){
    // Decode valid.
    entryData->valid = binaryData & 1;
    binaryData = binaryData >> 1;

    // Decode present.
    entryData->present = binaryData & 255;
    binaryData = binaryData >> 8;

    // Decode PFN.
    entryData->PFN = binaryData & 255;
}

void InitializePhysicalMemory(int numOffsetBits, int numPFNBits)
{
    // Calculate the number of physical memory locations.
    int physicalMemoryLength = 1 << (numOffsetBits + numPFNBits);

    // Allocate physical memory.
    physical_memory = (uint32_t *)malloc((sizeof(uint32_t) * physicalMemoryLength));

    // Clear garbage from physical memory.
    for (int i = 0; i < physicalMemoryLength; i++)
    {
        physical_memory[i] = 0;
    }
}

void InitializeTLB()
{
    // Allocate TLB memory.
    TLB = (uint32_t *)malloc(8 * sizeof(uint32_t));

    // Clear garbage in TLB memory.
    for (int i = 0; i < 8; i++)
    {
        TLB[i] = 0;
    }
}

void InitializePT(int numVPNBits){
    int numPages = 1 << numVPNBits;

    for(int i = 0; i < 4; i++){
        PT[i] = malloc(numPages * sizeof(uint32_t));

        // Clear page tables of garbage.
        for(int j = 0 ; j < numPages; j++){
            PT[i][j] = 0;
        }
    }
}

void SwitchToPID(int processID)
{

    // Process must be either 0,1,2, or 3
    if (processID < 0 || processID > 3)
    {
        fprintf(output_file, "Current PID: %d. Invalid context switch to processID %d\n", process_id, processID);
        exit(EXIT_FAILURE);
    }
    else
    {

        // Save registers of current processID to save register block
        save_registers[2 * process_id] = r1;
        save_registers[2 * process_id + 1] = r2;

        // change processID id
        process_id = processID;

        // restore register values of new processID
        r1 = save_registers[2 * process_id];
        r2 = save_registers[2 * process_id + 1];

        fprintf(output_file, "Current PID: %d. Switched execution context to processID: %d\n", process_id, processID);
    }
}

void TLBIncrementReplacementIndexes(){
        for(int i = 0; i < 8; i++){
        // Get the TLB entry in readable format.
        TLBEntry currentLine;
        BinaryToTLBEntry(TLB[i], &currentLine);

        if(currentLine.valid){
            currentLine.replacementIndex++;
        }
    }
}

int CheckTLBValid(int VPN){

    for(int i = 0; i < 8; i++){
        // Get the TLB entry in readable format.
        TLBEntry currentLine;
        BinaryToTLBEntry(TLB[i], &currentLine);

        if(currentLine.VPN == VPN
        && currentLine.processID == process_id
        && currentLine.valid){
            return i;
        }
    }

    // If we got here, there is no valid entry.
    return -1;
}

int CheckTLB(int VPN){

    for(int i = 0; i < 8; i++){
        // Get the TLB entry in readable format.
        TLBEntry currentLine;
        BinaryToTLBEntry(TLB[i], &currentLine);

        if(currentLine.VPN == VPN
        && currentLine.processID == process_id){
            return i;
        }
    }

    // If we got here, there is no valid entry.
    return -1;
}

int FirstInvalidTLBIndex(){
    for(int i = 0; i < 8; i++){
        // Get the TLB entry in readable format.
        TLBEntry currentLine;
        BinaryToTLBEntry(TLB[i], &currentLine);

        if(currentLine.valid == 0){
            return i;
        }
    }

    // If we got here, there is no invalid entry.
    return -1;
}

int OverrideTLBIndex(){
    int loosingCount = -1; // The count of current looser index.
    int loosingIdx = 0;

    for(int i = 0; i < 8 ; i++){
        // Get the TLB entry in readable format.
        TLBEntry currentLine;
        BinaryToTLBEntry(TLB[i], &currentLine);

        if(currentLine.replacementIndex > loosingCount){
            loosingCount = currentLine.replacementIndex;
            loosingIdx = i;
        }
    }

    return loosingIdx;
}

void MapVPNtoPFN(int VPN, int PFN){
    // Update TLB.
    // First, check if this mapping is already in TLB. Valid or not.
    int lineIdx = CheckTLB(VPN);

    // If it is not in the TLB yet, find the the first invalid TLB entry.
    if(lineIdx == -1){
        lineIdx = FirstInvalidTLBIndex();
    }

    // If there are no invalid entries, find the TLB entry next in line to override.
    if(lineIdx == -1){
        lineIdx = OverrideTLBIndex();
    }

    // In both FIFO and LRU strategies, we must increment counts now, before putting the data into TLB.
    TLBIncrementReplacementIndexes();

    // Now that we have the index of TLB to write to, update the TLB.
    // Create line entry.
    TLBEntry updateLine;
    updateLine.VPN = VPN;
    updateLine.processID = process_id;
    updateLine.PFN = PFN;
    updateLine.valid = 1;
    updateLine.replacementIndex = 0;
    // Update the TLB line.
    TLB[lineIdx] = TLBEntryToBinary(&updateLine);

    // Update the page table.
    // Create line entry.
    LPTEntry ptEntry;
    ptEntry.PFN = PFN;
    ptEntry.valid = 1;
    PT[process_id][VPN] = LPTEntryToBinary(&ptEntry);
}

void UnmapVPN(int VPN){
    // Update TLB.
    // Find valid mapping.
    int lineIdx = CheckTLBValid(VPN);

    // If there is a valid mapping unmap it. Otherwise, do nothing.
    if(lineIdx != -1){
        // Now that we have the index of TLB to write to, update the TLB.
        // Create line entry in readable format.
        TLBEntry updateLine;
        BinaryToTLBEntry(TLB[lineIdx], &updateLine);
        updateLine.valid = 0; // The only thing that needs to change. Unmapping == invalidating.
        // Update the TLB line.
        TLB[lineIdx] = TLBEntryToBinary(&updateLine);
    }

    // Update page table.
    LPTEntry ptEntry;
    BinaryToLPTEntry(PT[process_id][VPN], &ptEntry);
    ptEntry.valid = 0; // The only thing that needs to change. Unmapping == invalidating.
    PT[process_id][VPN] = LPTEntryToBinary(&ptEntry);
}

void load(char *reg, char *arg)
{

    uint32_t *register_used = NULL;

    // Determine whether r1 or r2 is used, if neither abort program
    if (strcmp(reg, "r1") == 0)
    {
        register_used = &r1;
    }
    else if (strcmp(reg, "r2") == 0)
    {
        register_used = &r2;
    }
    else
    {
        fprintf(output_file, "Current PID: %d. Error: invalid register operand %s\n", process_id, reg);
        exit(EXIT_FAILURE);
    }

    // If we are dealing with an immediate, can directly read it into register
    if ((arg[0] == '#'))
    {
        // Read into register the numerical value of whatever is after the #
        *register_used = atoi(arg + 1);
        fprintf(output_file, "Current PID: %d. Loaded immediate %s into register %s\n", process_id, arg + 1, reg);
    }

    // If we must load a value from memory
    else
    {
        // TODO: implement reading from memory
    }
}

void add()
{
    // Saving this for the output string
    uint32_t old_value = r1;

    r1 = r1 + r2;
    fprintf(output_file, "Current PID: %d. Added contents of registers r1 (%d) "
                         "and r2 (%d). Result: %d\n",
            process_id, old_value, r2, r1);
}
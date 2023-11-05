#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0

void initialize_physical_memory(FILE* output_file,char* OFF, char* PFN, char* VPN);
void initialize_TLB();
void context_switch(int process);
void load(char* reg,char* arg);
void add();

// Output file
FILE* output_file;

// TLB replacement strategy (FIFO or LRU)
char* strategy;

// Keeps track of if we have used define
int defined_occured = FALSE;

// Keeps track of current process id
int process_id = 0;

//ptr to representations of physical mem and the TLB
uint32_t* physical_memory;
uint8_t* TLB;

//Two registers
uint32_t r1;
uint32_t r2;


//Register values must be saved and restored when context switching
//There are 4 processes and 2 registers that need to be saved, use offset of id to get proper values
uint32_t save_registers[8];


char** tokenize_input(char* input) {
    char** tokens = NULL;
    char* token = strtok(input, " ");
    int num_tokens = 0;

    while (token != NULL) {
        num_tokens++;
        tokens = realloc(tokens, num_tokens * sizeof(char*));
        tokens[num_tokens - 1] = malloc(strlen(token) + 1);
        strcpy(tokens[num_tokens - 1], token);
        token = strtok(NULL, " ");
    }

    num_tokens++;
    tokens = realloc(tokens, num_tokens * sizeof(char*));
    tokens[num_tokens - 1] = NULL;

    return tokens;
}

int main(int argc, char* argv[]) {
    const char usage[] = "Usage: memsym.out <strategy> <input trace> <output trace>\n";
    char* input_trace;
    char* output_trace;
    char buffer[1024];

    // Parse command line arguments
    if (argc != 4) {
        printf("%s", usage);
        return 1;
    }
    strategy = argv[1];
    input_trace = argv[2];
    output_trace = argv[3];

    // Open input and output files
    FILE* input_file = fopen(input_trace, "r");
    output_file = fopen(output_trace, "w");  

    //Define needs to be the first instruction
    int first_instruction = 1;
    
    while ( !feof(input_file) ) {
        // Read input file line by line
        char *rez = fgets(buffer, sizeof(buffer), input_file);
        if ( !rez ) {
            fprintf(stderr, "Reached end of trace. Exiting...\n");
            return -1;
        } else {
            // Remove endline character
            buffer[strlen(buffer) - 1] = '\0';
        }
        char** tokens = tokenize_input(buffer);

        // TODO: Implement your memory simulator
        printf("%s",tokens[0]);       
        //Skip comments
        if(strcmp(tokens[0],"%") == 0){
            continue;
        }

        //This is to handle the first instruction must always be define
        if(first_instruction){
            //If we are at first command and it is define, initialize physical memory
            if(strcmp(tokens[0],"define") == 0){
                initialize_physical_memory(output_file,tokens[1],tokens[2],tokens[3]);
                initialize_TLB();
                
            }
            else{
                fprintf(output_file,"Current PID: %d. Error: attempt to execute instruction before define\n",process_id);  
                exit(EXIT_FAILURE);             
            }
            first_instruction = 0;
        }
        else{
            //If after 1st instruction we try to call define, end program
            if(strcmp(tokens[0],"define") == 0){
                fprintf(output_file,"Current PID: %d. Error: multiple calls to define in the same trace\n",process_id);
                exit(EXIT_FAILURE);
        }
        //End of logic to handle define

        //Logic for handling ctxswitch
        if(strcmp(tokens[0],"ctxswitch") == 0){
            context_switch(atoi(tokens[1]));
        }

        //Logic for handling load
        if(strcmp(tokens[0],"load") == 0){
            load(tokens[1],tokens[2]);
        }
        
        //Logic for handling add
        if(strcmp(tokens[0],"add") == 0){
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

    return 0;
}

void initialize_physical_memory(FILE* output_file,char* OFF, char* PFN, char* VPN){

    int two_pow_OFF = 0;
    int two_pow_PFN = 0;
    int array_length;
    
    //Calulate 2^OFF and 2^PFN without having to include math.h
    for(int i = 0; i < atoi(OFF); i++){
        two_pow_OFF += 2;
    } 

    for(int i = 0; i < atoi(PFN); i++){
        two_pow_PFN += 2;
    }
    array_length = two_pow_OFF*two_pow_OFF;

    physical_memory = (uint32_t*) malloc((sizeof(uint32_t) *array_length));

    //On startup, all locations in te array are set to 0
    for(int i = 0; i < array_length; i++){
        physical_memory[i] = 0;
    }

    fprintf(output_file,"Current PID: %d. Memory instantiation complete. OFF bits: %s. PFN bits: %s. VPN bits: %s\n",process_id,OFF,PFN,VPN);

}

void initialize_TLB(){

    // Create TLB, 8 entries of size 8 bits and initialize them to zero
    
    TLB = (uint8_t*) malloc(8*sizeof(uint8_t));

    for(int i = 0; i < 8;i++){
        TLB[i] = 0;
    }


}

void context_switch(int process){

    //Process must be either 0,1,2, or 3
    if(process < 0 || process > 3){
        fprintf(output_file,"Current PID: %d. Invalid context switch to process %d\n",process_id,process);
        exit(EXIT_FAILURE);        
    }
    else{
 
        //Save registers of current process to save register block
        save_registers[2*process_id] = r1;
        save_registers[2*process_id+1] = r2;
        
        //change process id
        process_id = process;

        //restore register values of new process
        r1 = save_registers[2*process_id];
        r2 = save_registers[2*process_id+1];

        fprintf(output_file,"Current PID: %d. Switched execution context to process: %d\n",process_id,process);
    }

}

void load(char* reg,char* arg){
    
    uint32_t* register_used = NULL;

    //Determine whether r1 or r2 is used, if neither abort program
    if(strcmp(reg,"r1") == 0){
        register_used = &r1;
    }
    else if(strcmp(reg,"r2") == 0){
        register_used = &r2;
    }
    else{
        fprintf(output_file,"Current PID: %d. Error: invalid register operand %s\n",process_id,reg);
        exit(EXIT_FAILURE);        
    }
    
    //If we are dealing with an immediate, can directly read it into register
    if((arg[0] == '#')){
        //Read into register the numerical value of whatever is after the #
        *register_used = atoi(arg + 1);
        fprintf(output_file,"Current PID: %d. Loaded immediate %s into register %s\n",process_id,arg + 1,reg);
    }

    //If we must load a value from memory
    else{
    //TODO: implement reading from memory

    }
}

void add(){
    //Saving this for the output string
    uint32_t old_value = r1;

    r1 = r1 + r2;
    fprintf(output_file, "Current PID: %d. Added contents of registers r1 (%d) "
                   "and r2 (%d). Result: %d\n",
                   process_id, old_value, r2, r1);
     
}
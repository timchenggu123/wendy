#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "data.h"
#include <stdbool.h>

// memory.h - Felix Guo
// This module manages the memory model for WendyScript.

// Sentinal value for when no closure needs to be created
#define NO_CLOSURE (unsigned int)(-1)

typedef unsigned int address;

typedef struct {
	char id[MAX_IDENTIFIER_LEN + 1];
	address val;
	bool is_closure;
} stack_entry;

typedef struct mem_block mem_block;
struct mem_block {
	size_t size;
	address start;
	mem_block* next;
};

extern data* memory;
extern mem_block* free_memory;
extern stack_entry* call_stack;
extern address* mem_reg_stack;

// Includes a list of closures, used to store temporary stack frames.
//   The size of the closure lists is also stored for easy iteration.
extern stack_entry** closure_list;
extern size_t* closure_list_sizes;

extern address frame_pointer;
extern address stack_pointer;
extern address closure_list_pointer;
extern size_t closure_list_size;
extern address mem_reg_pointer;
extern address arg_pointer;

// init_memory() initializes the memory module
void init_memory(void);

// c_free_memory() deallocates the memory
void c_free_memory(void);

// check_memory(line) ensures all the pointers are within the memory space
void check_memory(int line);

// garbage_collect() collects unused memory and stores it in the linked list
//   free_memory. Returns true if some memory was collected and false otherwise.
bool garbage_collect(size_t size);

// print_free_memory() prints out a list of the free memory blocks available
//   in Wendy
void print_free_memory(void);

// has_memory(size) returns true if a memory block of that size can be found
//   and false otherwise.
bool has_memory(size_t size);

// pls_give_memory(size) requests memory from Wendy and returns the address
//   of the requested block.
address pls_give_memory(size_t size, int line);

// here_u_go(a, size) returns memory to Wendy
void here_u_go(address a, size_t size);

// push_frame(name) creates a new stack frame (when starting a function call)
void push_frame(char* name, address ret, int line);

// push_auto_frame() creates an automatical local variable frame
void push_auto_frame(address ret, char* type, int line);

// pop_frame(is_ret) ends a function call, pops the latest stack frame
//   (including automatically created local frames IF is_ret!
//   is_ret is true if we RET instead of ending bracket
//
//   pop_frame also returns true if the popped frame is a function frame
bool pop_frame(bool is_ret, address* ret);

// write_memory(location, d) safely writes new data to the given location
void write_memory(unsigned int location, data d, int line);

// push_memory(t) adds the given number of data t into the memory in order
//   and returns the address of the first one
address push_memory(data t, int line);

// push_memory_wendy_list(t, size) finds a continuous block of size in memory and sets
//   it to the array a appending a list header data..
address push_memory_wendy_list(data* a, int size, int line);

// push_memory_array(a, size) finds a continuous block of size in memory and
//   sets it to the array "a" directly.
address push_memory_array(data* a, int size, int line);

// pop_memory() removes a data from the memory after a push operation
data pop_memory(void);

// push_stack_entry(id, t) adds a new entry into the
// stack frame (eg variable declaration).
void push_stack_entry(char* id, address val, int line);

// copy_stack_entry(se) copies the given stack entry into the top of the call
//   stack, USE FOR CLOSURES
void copy_stack_entry(stack_entry se, int line);

// id_exist(id, search_main) returns true if id exists in the current stackframe
bool id_exist(char* id, bool search_main);

// get_address_of_id(id, line) returns address of the id given
//   requires: id exist in the stackframe
address get_address_of_id(char* id, int line);

// get_value_of_id(id, line) returns the value of the id given
//   requires: id exist in the stackframe
data* get_value_of_id(char* id, int line);

// get_value_of_address(address, line) returns the value of the address given
data* get_value_of_address(address a, int line);

// print_call_stack prints out the callstack
void print_call_stack(FILE* file, int maxlines);

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(char* id, int line);

// push_arg(t) pushes a data t into the other end of memory
void push_arg(data t, int line);

// pop_arg(line) returns the top data t at the other end of memory
data pop_arg(int line);

// top_arg(line) returns the pointer to top data without popping!!
data* top_arg(int line);

// clear_arg_stack() clears the operational stack
void clear_arg_stack(void);

// create_closure() creates a closure with the current stack frame and returns
//   the index of the closure frame
address create_closure(void);

// write_state(fp) writes the current state for debugging to the file fp
void write_state(FILE* fp);

// push_mem_reg(memory_register) saves the memory register to the stack
void push_mem_reg(address memory_register, int line);

// pop_mem_reg() pops the saved memory register
address pop_mem_reg(void);

// unwind_stack() pops all stack frames other than the main
//   * used after each run in REPL in case REPL leaves the stack in a non-
//   stable state
void unwind_stack(void);
#endif

#ifndef MEMORY_H
#define MEMORY_H

#include "token.h"
#include <stdbool.h>
#include "macros.h"

// memory is a list of tokens holding values.
// call_stack/array includes all the frames and each frame has entries.
//   Each entry is either:
//		identifier -> address to previous stack 
//			identifier is either FUNC (function ret) or LOC (local automatic)
//		identifier -> address to memory
//		identifier -> function address
// frame_pointer is the address to the start to the current frame.
// stack_pointer is the address to the front of the stack, last empty

// an address is an unsigned int
// stack_entry holds char[60] -> address
// frame_pointer is an address

typedef unsigned int address;

// sizeof(stack_entry) = 64
typedef struct {
	char id[MAX_IDENTIFIER_LEN + 1];
	address val;
} stack_entry;

typedef struct mem_block mem_block;
struct mem_block {
	unsigned int size;
	address start;
	mem_block* next;
};

// FIRST SLOT OF MEMORY IS RESERVED FOR NONE TOKEN
token* memory;
mem_block* free_memory;

// 128bytes per entry, 8mb of stack size
// 8 * 1024 * 1024 = 8388608 bytes = 65536 entries in 8mb
stack_entry* call_stack;

extern address frame_pointer;
extern address stack_pointer;
extern bool enable_gc;

// init_memory() allocates the memory
void init_memory();

// c_free_memory() deallocates the memory
void c_free_memory();

// check_memory() ensures all the pointers are within the memory space
void check_memory();

// garbage_collect() collects unused memory and stores it in the linked list
//   free_memory. Returns true if some memory was collected and false otherwise.
bool garbage_collect(int size);

// print_free_memory() prints out a list of the free memory blocks available
//   in Wendy
void print_free_memory();

// has_memory(size) returns true if a memory block of that size can be found
//   and false otherwise.
bool has_memory(int size);

// pls_give_memory(size) requests memory from Wendy and returns the address
//   of the requested block.
address pls_give_memory(int size);

// here_u_go(a, size) returns memory to Wendy
void here_u_go(address a, int size);

// memory_pointer stores the address of the next available memory space
extern address memory_pointer;

// push_frame(name) creates a new stack frame (when starting a function call)
void push_frame(char* name, address ret);

// push_auto_frame() creates an automatical local variable frame
void push_auto_frame(address ret, char* type);

// pop_frame(is_ret) ends a function call, pops the latest stack frame 
//   (including automatically created local frames IF is_ret!
//   is_ret is true if we RET instead of ending bracket
//
//   pop_frame also returns true if the popped frame is a function frame
bool pop_frame(bool is_ret, address* ret);

// push_memory(t) adds the given number of token t into the memory in order
//   and returns the address of the first one
address push_memory(token t);

// push_memory_s(t, size) finds a continuous block of size in memory and sets
//   it to t
address push_memory_s(token t, int size);

// push_memory_a(t, size) finds a continuous block of size in memory and sets
//   it to the array a.
address push_memory_a(token* a, int size);

// push_memory_array(a, size) finds a continuous block of size in memory and
//   sets it to the array a.
address push_memory_array(token* a, int size);

// pop_memory() removes a token from the memory after a push operation
token pop_memory();

// replace_memory(t, a) places the token t in the address a
void replace_memory(token t, address a);

// push_stack_entry(id, t) adds a new entry into the 
// stack frame (eg variable declaration).
void push_stack_entry(char* id, address val);

// id_exist(id, search_main) returns true if id exists in the current stackframe 
bool id_exist(char* id, bool search_main);

// get_address_of_id(id, line) returns address of the id given
//   requires: id exist in the stackframe
address get_address_of_id(char* id, int line);

// get_value_of_id(id, line) returns the value of the id given
//   requires: id exist in the stackframe
token* get_value_of_id(char* id, int line);

// get_value_of_address(address, line) returns the value of the address given
token* get_value_of_address(address a, int line);

// print_call_stack prints out the callstack
void print_call_stack();

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(char* id, int line);

// push_arg(t) pushes a token t into the other end of memory
void push_arg(token t);

// pop_arg(line) returns the top token t at the other end of memory
token pop_arg(int line);
#endif

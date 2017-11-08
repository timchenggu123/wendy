#define _GNU_SOURCE
#include "error.h"
#include "memory.h"
#include "source.h"
#include "vm.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static bool error_flag = false;

void reset_error_flag() {
    error_flag = false;
}

bool get_error_flag() {
    return error_flag;
}

// Error Functions:
void print_verbose_info() {
    if (get_settings_flag(SETTINGS_VERBOSE)) {
        fprintf(stderr, RED "VERBOSE ERROR DUMP\n" RESET);
        fprintf(stderr, GRN "Limits\n" RESET);
        fprintf(stderr, "MEMORY_SIZE %d\n", MEMORY_SIZE);
        fprintf(stderr, "STACK_SIZE %d\n", STACK_SIZE);
        fprintf(stderr, "ARGSTACK_SIZE %d\n", ARGSTACK_SIZE);
        fprintf(stderr, "RESERVED_MEMORY %d\n", RESERVED_MEMORY);
        fprintf(stderr, "CLOSURES_SIZE %d\n", CLOSURES_SIZE);
        fprintf(stderr, "MEMREGSTACK_SIZE %d\n", MEMREGSTACK_SIZE);
        fprintf(stderr, GRN "Memory\n" RESET);
        fprintf(stderr, "FP: %d %x\n", frame_pointer, frame_pointer);
        fprintf(stderr, "SP: %d %x\n", stack_pointer, stack_pointer);
        fprintf(stderr, "AP: %d %x\n", arg_pointer, arg_pointer);
        fprintf(stderr, "CP: %d %x\n", closure_list_pointer, closure_list_pointer);
        print_free_memory();
    }
}

char* error_message(char* message, va_list args) {
    char* result;
    vasprintf(&result, message, args);
    return result;
}

void error_general(char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    fprintf(stderr, RED "Fatal Error: " RESET "%s\n", msg);
    print_verbose_info();

    // Cannot be safe, because vasprintf uses malloc!
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}

void error_lexer(int line, int col, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    fprintf(stderr, RED "Parser Error" RESET " on line " YEL "%d" RESET ": %s\n",
        line, msg);

    if (has_source()) {
        fprintf(stderr, "==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        fprintf(stderr, "%5d " RED "%s\n" RESET, line, get_source_line(line));
        fprintf(stderr, "      %*c^\n", col, ' ');
    }
    print_verbose_info();
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}


void error_compile(int line, int col, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    fprintf(stderr, RED "Compile Error" RESET " on line " YEL "%d" RESET ": %s\n",
        line, msg);

    if (has_source()) {
        fprintf(stderr, "==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        fprintf(stderr, "%5d " RED "%s\n" RESET, line, get_source_line(line));
        fprintf(stderr, "      %*c^\n", col, ' ');
    }
    print_verbose_info();
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}

void error_runtime(int line, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    fprintf(stderr, RED "Runtime Error" RESET " on line " YEL "%d" RESET" (" YEL "0x%X"
        RESET "): %s\n", line, get_instruction_pointer(), msg);

    if (has_source()) {
        if (!is_source_accurate()) {
            fprintf(stderr, YEL "Note: " RESET "Source was automatically loaded "
            "and may not reflect the actual source of the compiled code.\n");
        }
        fprintf(stderr, "==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        int start_line = (line - 2 > 0) ? (line - 2) : 1;
        int end_line = start_line + 5;
        for (int i = start_line; i < end_line; i++) {
            if (is_valid_line_num(i)) {
                if (i == line) {
                    fprintf(stderr, "%5d " RED "%s\n" RESET, i, get_source_line(i));
                }
                else {
                    fprintf(stderr, "%5d %s\n" RESET, i, get_source_line(i));
                }
            }
        }
        fprintf(stderr, "==========================\n");
    }
    free(msg);
    // REPL Don't print call stack unless verbose is on!
    if (!get_settings_flag(SETTINGS_REPL) || get_settings_flag(SETTINGS_VERBOSE)) {
        print_call_stack(20);
        print_verbose_info();
    }
    fflush(stdout);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}

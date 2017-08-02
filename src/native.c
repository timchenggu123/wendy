// Implementation of Native

#include "native.h"
#include "memory.h"
#include "error.h"
#include "token.h"
#include <string.h>
#include "codegen.h"

typedef struct native_function {
    char* name;
    int argc;
    token (*function)(token*);
} native_function;

static token native_printCallStack(token* args);
static token native_reverseString(token* args);
static token native_examineMemory(token* args);

static native_function native_functions[] = {
    { "printCallStack", 1, native_printCallStack },
    { "reverseString", 1, native_reverseString },
    { "examineMemory", 2, native_examineMemory }
};

static double native_to_numeric(token* t) {
    return t->t_data.number;
}

static char* native_to_string(token* t) {
    return t->t_data.string;
}

static token native_printCallStack(token* args) {
    print_call_stack((int)native_to_numeric(args));
    return noneret_token();
}

static token native_reverseString(token* args) {
    char* string = native_to_string(args);
    int len = strlen(string);
    token t = make_token(T_STRING, make_data_str(""));
    strcpy(t.t_data.string, string);
    for (int i = 0; i < len / 2; i++) {
        char tmp = t.t_data.string[i];
        t.t_data.string[i] = t.t_data.string[len - i - 1];
        t.t_data.string[len - i - 1] = tmp;
    }
    return t;
}

static token native_examineMemory(token* args) {
    double arg1_from = native_to_numeric(args);
    double arg2_to = native_to_numeric(args + 1);
    printf("Memory Contents: \n");
    for (int i = arg1_from; i < arg2_to; i++) {
        token t = memory[i];
        printf("[0x%04X] [%s] ", i, token_string[t.t_type]);
        if (is_numeric(t)) {
            printf("[%f][%d][0x%X]", t.t_data.number, (int)t.t_data.number, 
                (int)t.t_data.number);
        }
        else {
            printf("[%s]", t.t_data.string);
        }
        printf("\n");
    }
    return noneret_token();
}

void native_call(char* function_name, int expected_args, int line) {
    int functions = sizeof(native_functions) / sizeof(native_functions[0]);
    bool found = false;
    for (int i = 0; i < functions; i++) {
        if(strcmp(native_functions[i].name, function_name) == 0) {
            int argc = native_functions[i].argc;
            token* arg_list = safe_malloc(sizeof(token) * argc);
            // Popped in reverse order
            for (int j = argc - 1; j >= 0; j--) {
                arg_list[j] = pop_arg(line);
            }
            push_arg(native_functions[i].function(arg_list), line);
            safe_free(arg_list);
            found = true;
            break;
        }
    }
    if (!found) {
        error_runtime(line, VM_INVALID_NATIVE_CALL, function_name);
    }
}

#include "scanner.h"
#include <stdbool.h>
#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <execpath.h>
#include "macros.h"

static size_t source_len;
static size_t current; // is used to keep track of source current
static size_t start;
static char* source;
static token* tokens;
static size_t tokens_alloc_size;
static size_t t_curr; // t_curr is used to keep track of addToken
static size_t line;

static void add_token(token_type type);
static void add_token_V(token_type type, data val);

// is_at_end() returns true if scanning index reached the end
bool is_at_end() {
	return current >= source_len;
}

// match(expected, source) advances the index and checks the next character
bool match(char expected) {
	if (is_at_end()) return false;
	if (source[current] != expected) return false;

	current++;
	return true;
}

// peek(char) returns the next character or \0 if we are at the end
char peek() {
	if (is_at_end()) return '\0';
	return source[current];
}

// advance() returns the current character then moves onto the next one
char advance() {
	current++;
	return source[current - 1];
}
// peek_next() peeks at the character after the next or \0 if at end
char peek_next() {
	if(current + 1 >= source_len) return '\0';
	return source[current + 1];
}

// is_alpha(c) returns true if c is alphabet
bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

// is_digit(c) returns true if c is digit
bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

// is_alpha_numeric(c) returns true if c is alpha AND numeric
bool is_alpha_numeric(char c) {
	return is_alpha(c) || is_digit(c);
}

// handle_req() processes when a file is "req"ed, this is called when we 
//   encounter a req
//   We first find the file and get the file's contents, then append it
//   right after in the source code and increase the length as needed.
void handle_req() {

	add_token_V(REQ, make_data_str("req"));
	// we scan a string
	// t_curr points to space after REQ
	int str_loc = t_curr;
	while (peek() != ';' && !is_at_end()) {
		start = current;
		scan_token();
	}
	// the last token should be a string now
	if (is_at_end() || tokens[str_loc].t_type != STRING) {
		error(line, SYNTAX_ERROR);
		return;
	}
	// consume the semicolon
	advance();
	add_token_V(SEMICOLON, make_data_str(";"));

	// We need path to the wendy libraries
	char* path = get_path();
	long length = 0;
	
	char* buffer;
	strcat(path, "wendy-lib/");
	strcat(path, tokens[str_loc].t_data.string);
	FILE * f = fopen(path, "r");
//	printf("Attempting to open %s\n", path);
	if (f) {
		fseek (f, 0, SEEK_END);
		length = ftell (f);
		fseek (f, 0, SEEK_SET);
		buffer = malloc(length + 1); // + 1 for null terminator
		if (buffer) {
			fread (buffer, 1, length, f);
			buffer[length] = '\0'; // fread doesn't add a null terminator!
			int newlen = source_len + strlen(buffer);
			char* tmp = realloc(source, newlen + 1);
			size_t o_source_len = source_len;
			if (tmp) {
				source = tmp;
				source_len = newlen;
			}
			else {
				printf("req: Realloc Error! \n");
				exit(1);
			}
			// at this point, we inset the buffer into the source
			// current should be the next item, we move it down buffer bytes
//			printf("old source: \n%s\n", source);
			memmove(&source[current + strlen(buffer)], 
					&source[current], o_source_len - current + 1);
			memcpy(&source[current], buffer, strlen(buffer));
//			printf("newsource: \n%s\n", source);
		}
		free(buffer);
		fclose (f);
	}
	else {
		error(line, REQ_FILE_READ_ERR);
	}

	free(path);
}

// handle_struct() processes the next struct definition
//   called when we encounter a STRUCT
void handle_struct() {
//	printf("HANDLING STRUCT\n");
	add_token_V(STRUCT, make_data_str("struct"));
	// we scan to the end of the line, recording the whole struct line.
	// t_curr points to the space after STRUCT
	// That's where the ID is.
	int id_loc = t_curr; 
	while (peek() != ';' && !is_at_end()) {
		start = current;		
		scan_token();
	}
	if (is_at_end() || tokens[id_loc].t_type != IDENTIFIER ||
			tokens[id_loc + 1].t_type != DEFFN) {
		error(line, SYNTAX_ERROR);
		return;
	}

	// The closing ;.
	add_token_V(SEMICOLON, make_data_str(";"));
	advance(); //consume it

	// STRUCT Syntax:
	//   struct ID => (param1, param2, ...);
	// t_curr is now at the location after the semicolon
	// We first go through and collect all the param tokens.
	// tokens[id_loc] now has the struct identifier.
	token param[1024];
	int param_c = 0;
	// 3rd token after should be a identifier, 
	//   then every other one until the end
	for (int i = id_loc + 3; tokens[i].t_type != SEMICOLON; i += 2) {
		if (tokens[i].t_type != IDENTIFIER) {
			error(line, SYNTAX_ERROR);
			return;
		}
		else {
			param[param_c++] = tokens[i];
		}
	}
//	printf("PARAM C = %d\n", param_c);
	// We build the constructor function.
	// let ID => (param1, param2, ...) {
	//   let newobj[param_c];
	//   let newobj[0] = param1;
	//   let newobj[1] = param2;
	//   ...
	//   ret &newobj;
	// }
	add_token_V(LET, make_data_str("let"));
	tokens[t_curr++] = tokens[id_loc];
	add_token_V(DEFFN, make_data_str("=>"));
	add_token_V(LEFT_PAREN, make_data_str("("));
	for (int i = 0; i < param_c; i++) {
		tokens[t_curr++] = param[i];
		if (i != param_c - 1) {
			add_token_V(COMMA, make_data_str(","));
		}
	}
	add_token_V(RIGHT_PAREN, make_data_str(")"));
	add_token_V(LEFT_BRACE, make_data_str("{"));
	add_token_V(LET, make_data_str("let"));
	add_token_V(IDENTIFIER, make_data_str("newobj"));
	add_token_V(EQUAL, make_data_str("="));
	add_token_V(LEFT_BRACK, make_data_str("["));
//	add_token_V(NUMBER, make_data_num(param_c));
	for (int i = 0; i < param_c; i++) {
		if (i != 0) {
			add_token_V(COMMA, make_data_str(","));
		}
		tokens[t_curr++] = param[i];
	}
	add_token_V(RIGHT_BRACK, make_data_str("]"));
	add_token_V(SEMICOLON, make_data_str(";"));
	add_token_V(RET, make_data_str("ret"));
	add_token_V(IDENTIFIER, make_data_str("newobj"));
	add_token_V(SEMICOLON, make_data_str(";"));
	add_token_V(RIGHT_BRACE, make_data_str("}"));
	add_token_V(SEMICOLON, make_data_str(";"));


//	printf("END OF CONSTRUCTOR\n");
	// END OF CONSTRUCTOR
	// ACCESSOR FUNCTIONS
	// let IDENTIFIER_PARAMNAME => (objptr) {
	//   ret *(objptr + PARAID);
	// }
	for (int i = 0; i < param_c; i++) {
		add_token_V(LET, make_data_str("let"));
		token orig = tokens[id_loc];
		strcat(orig.t_data.string, "_");
		strcat(orig.t_data.string, param[i].t_data.string);
		tokens[t_curr++] = orig;
		add_token_V(DEFFN, make_data_str("=>"));
		add_token_V(LEFT_PAREN, make_data_str("("));
		add_token_V(IDENTIFIER, make_data_str("obj"));
		add_token_V(RIGHT_PAREN, make_data_str(")"));
		add_token_V(LEFT_BRACE, make_data_str("{"));
		add_token_V(RET, make_data_str("ret"));
		add_token_V(IDENTIFIER, make_data_str("obj"));
		add_token_V(LEFT_BRACK, make_data_str("["));
		add_token_V(NUMBER, make_data_num(i));
		add_token_V(RIGHT_BRACK, make_data_str("]"));
		add_token_V(SEMICOLON, make_data_str(";"));
		add_token_V(RIGHT_BRACE, make_data_str("}"));
		add_token_V(SEMICOLON, make_data_str(";"));
	}
//	printf("END OF HANDLE STRUCT\n");
//	printf("NEXT TOKEN TO HANDLE = %d\n", current);
}

// handle_obj_type() processes the next oBJ_TYPE
void handle_obj_type() {
	while (peek() != '>' && !is_at_end()) {
		if (peek() == '\n') line++;
		advance();
	}

	// Unterminated string.
	if (is_at_end()) {
		error(line, SYNTAX_ERROR);
		return;
	}

	// The closing >.
	advance();

	// Trim the surrounding quotes.
	int s_length = current - 2 - start;
	char value[s_length + 1]; // + 1 for null terminator
	
	memcpy(value, &source[start + 1], s_length);
	value[s_length] = '\0';

	add_token_V(OBJ_TYPE, make_data_str(value));	
}

// handle_string() processes the next string
void handle_string() {
	while (peek() != '"' && !is_at_end()) {
		if (peek() == '\n') line++;
		advance();
	}

	// Unterminated string.
	if (is_at_end()) {
		error(line, UNTERMINATED_STRING);
		return;
	}

	// The closing ".
	advance();

	// Trim the surrounding quotes.
	int s_length = current - 2 - start;
	char value[s_length + 1]; // + 1 for null terminator
	
	memcpy(value, &source[start + 1], s_length);
	value[s_length] = '\0';

	add_token_V(STRING, make_data_str(value));	
}

// identifier() processes the next identifier and also handles wendyScript 
//   keywords
void identifier() {
	while (is_alpha_numeric(peek())) advance();
	
	char text[current - start + 1];
	memcpy(text, &source[start], current - start);
	text[current - start] = '\0';

	/*if (strcmp(text, "empty") == 0) { add_token_V(NUMBER, make_data_num(0)); }
	else*/ if (strcmp(text, "and") == 0)	{ add_token(AND); }
	else if (strcmp(text, "else") == 0)	{ add_token(ELSE); }
	else if (strcmp(text, "false") == 0)	{ add_token(FALSE); }
	else if (strcmp(text, "if") == 0)	{ add_token(IF); }
	else if (strcmp(text, "or") == 0)	{ add_token(OR); }
	else if (strcmp(text, "true") == 0)	{ add_token(TRUE); }

	else if (strcmp(text, "printstack") == 0)	{ add_token(PRINTSTACK); }
	else if (strcmp(text, "let") == 0)	{ add_token(LET); }
	else if (strcmp(text, "set") == 0)	{ add_token(SET); }
//	else if (strcmp(text, "memset") == 0){ add_token(MEMSET); }
	else if (strcmp(text, "loop") == 0)	{ add_token(LOOP); }
	else if (strcmp(text, "none") == 0)	{ add_token(NONE); }
	
/*	else if (strcmp(text, "Bool") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "String") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "Number") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "List") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "Address") == 0)	{ add_token(OBJ_TYPE); }*/

//	else if (strcmp(text, "typeof") == 0)	{ add_token(TYPEOF); }


	else if (strcmp(text, "ret") == 0)	{ add_token(RET); }
	else if (strcmp(text, "explode") == 0) { add_token(EXPLODE); }
	else if (strcmp(text, "req") == 0)	{ 
		handle_req();
	}
	else if (strcmp(text, "assert") == 0) { add_token(ASSERT); }
	else if (strcmp(text, "time") == 0) { add_token(TIME); }
	else if (strcmp(text, "inc") == 0)	{ add_token(INC); }
	else if (strcmp(text, "dec") == 0)	{ add_token(DEC); }
	else if (strcmp(text, "input") == 0)	{ add_token(INPUT); }
	else if (strcmp(text, "struct") == 0){ 
		// HANDLE STRUCT
		//handle_struct(); 
		add_token(STRUCT);
	
	}
	else { add_token(IDENTIFIER); }
}

// handle_number() processes the next number
void handle_number() {
	while (is_digit(peek())) advance();

	// Look for a fractional part.
	if (peek() == '.' && is_digit(peek_next())) {
		// Consume the "."
		advance();

		while (is_digit(peek())) advance();
	}
	
	char num_s[current - start + 1];
	memcpy(num_s, &source[start], current-start);
	num_s[current - start] = '\0';
	double num = strtod(num_s, NULL); 
	add_token_V(NUMBER, make_data_num(num));
}

// handle_accessor() processes a accessor after a ., eg list.size
/*void handle_accessor() {
	while (is_alpha(peek())) advance();

	char text[current - start + 1];
	memcpy(text, &source[start], current - start);
	text[current - start] = '\0';
//	printf("%s", text);
	if (strcmp(text, ".size") == 0) { add_token(ACCESS_SIZE); }
	else { error(line, UNRECOGNIZED_ACCESSOR); }
}*/

// scan_token() processes the next token 
bool ignore_next = false;
void scan_token() {
	char c = advance();	
	switch(c) {
		case '(': add_token(LEFT_PAREN); break;
		case ')': add_token(RIGHT_PAREN); break;
		case '[': add_token(LEFT_BRACK); break;
		case ']': add_token(RIGHT_BRACK); break;
		case '{': add_token(LEFT_BRACE); break;
		case '}': add_token(RIGHT_BRACE); break;
		case '&': add_token(AMPERSAND); break;
		case '~': add_token(TILDE); break;
		case ',': add_token(COMMA); break;
		case '.': 
			if (match('.')) {
				add_token(RANGE_OP);
			}
			/*else if (is_alpha(peek())) {
				advance();
				handle_accessor();
			}*/
			else {
				add_token(DOT); 
			}
			break;
		case '-': 
			if (is_digit(peek())) {
				advance();
				handle_number();
			}
			else {
				add_token(MINUS); 
			}
			break;
		case '+': add_token(PLUS); break;
		case '\\': add_token(INTSLASH); break;
		case '%': add_token(PERCENT); break;
		case '@': add_token(AT); break;
		case ';': add_token(SEMICOLON); break;
		case ':': add_token(COLON); break;
		case '#': 
			if (match(':')) {
				add_token(LAMBDA);
			}
			else if (match('#')) {
				add_token(DEBUG);
				ignore_next = true;
			}
			else {
				add_token(HASH);
			}
			break;
			
		case '*': add_token(match('/') ? B_COMMENT_END : STAR); break;
		case '!': add_token(match('=') ? NOT_EQUAL : NOT); break;
		case '=': 
			if (match('='))
			{
				add_token(EQUAL_EQUAL);
			}
			else if (match('>'))
			{
				add_token(DEFFN);
			}
			else
			{
				add_token(EQUAL);
			}
			break;
		case '<': 
			if (match('=')) {
				add_token(LESS_EQUAL);
			}
			else if (is_alpha(peek())) {
				handle_obj_type();
			}
			else {
				add_token(LESS);
			}
			break;
		case '>': add_token(match('=') ? GREATER_EQUAL : GREATER); break;
		case '/':
			if (match('/')) {
				// A comment goes until the end of the line.
				while (peek() != '\n' && !is_at_end()) advance();
			}
			else if (match('*')) {
				add_token(B_COMMENT_START);
			}
			else {
				add_token(SLASH);
			}
			break;
		case ' ':
		case '\r':
		case '\t':
			// Ignore whitespace. and commas
			break;
		case '\n':
			if (!ignore_next) line++;
			else ignore_next = false;
			break;
		case '"': handle_string(); break;
		default:
			if (is_digit(c)) {
				handle_number();
			}
			else if (is_alpha(c)) {
				identifier();
			}
			else {
				printf("Char is %d, %c\n", c, c);
				error(line, UNEXPECTED_CHARACTER);
			}
			break;
		// END SWITCH HERE
	}
}
int scan_tokens(char* source_, token** destination, size_t* alloc_size) {
	// allocate enough space for the max length of the source, probably not
	//   needed but it's a good setup.

	source_len = strlen(source_);
	source = malloc(source_len + 1);
	strcpy(source, source_);

	tokens_alloc_size = source_len;
	tokens = (token*)malloc(tokens_alloc_size * sizeof(token));
	t_curr = 0;
	current = 0; 
	line = 1;

	while (!is_at_end()) {
		start = current;
		scan_token();
	}
	// append end of file
	//add_token_V(EOFILE, make_data_str("EOFILE"));
	*destination = tokens;
	*alloc_size = tokens_alloc_size;
	//free(source);
	init_error(source);
	return t_curr;
}

//void copy_tokens(token** destination) {

//	*destination = tokens;
//	memcpy(destination, tokens, t_curr * sizeof(token));
//	free(tokens);
//}

void add_token(token_type type) {
	char val[current - start + 1];
	memcpy(val, &source[start], current - start);
	val[current - start] = '\0';
	add_token_V(type, make_data_str(val));
}

void add_token_V(token_type type, data val) {
	if (t_curr != 0 && tokens[t_curr - 1].t_type == ELSE
			&& type == IF) {

		token new_t = { ELSEIF, line,make_data_str("else if") };
		tokens[t_curr - 1] = new_t;
	}
	else if (type == NONE) { tokens[t_curr++] = none_token(); }
	else if (type == TRUE) { tokens[t_curr++] = true_token(); }
	else if (type == FALSE) { tokens[t_curr++] = false_token(); }
	else {
		token new_t = { type, line, val };
		tokens[t_curr++] = new_t;
	}
	if (t_curr == tokens_alloc_size) {
		tokens_alloc_size += 200;
		token* tmp = (token*)realloc(tokens, tokens_alloc_size * sizeof(token));
		if (tmp) {
			tokens = tmp;
		}
		else {
			printf("You have been blessed by a realloc error. Good luck figuring this out!\n");
			exit(1);
		}
	}
}

void print_token_list(token* tokens, size_t size) {
	for(int i = 0; i < size; i++) {

		if(tokens[i].t_type == NUMBER) {
			printf("{ %d - %d -> %f }\n", i, tokens[i].t_type, tokens[i].t_data.number);
		}
		else {
			printf("{ %d - %d -> %s }\n", i, tokens[i].t_type, tokens[i].t_data.string);
		}
//		printf("%d\n", i);

		
	
	}
//	printf("EXIT LOOP");
}



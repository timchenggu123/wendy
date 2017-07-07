#include "ast.h"
#include "token.h"
#include "error.h"
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "global.h"

#define match(...) fnmatch(sizeof((token_type []) {__VA_ARGS__}) / sizeof(token_type), __VA_ARGS__)

// Wrapping SafeFree from global for use in a function pointer.
void ast_safe_free(void* ptr) {
	safe_free(ptr);
}

static token* tokens = 0;
static size_t length = 0;
static int curr_index = 0;
static bool pre_order = 0;
static int indentation = 0;
static bool error_thrown = false;

// Forward Declarations
static void traverse_expr(expr* expression, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
static void traverse_expr_list(expr_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
static void traverse_statement_list(statement_list* list,
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
static void traverse_statement(statement* state, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));

static expr* make_lit_expr(token t);
static expr* make_bin_expr(expr* left, token op, expr* right);
static expr* make_una_expr(token op, expr* operand);
static expr* make_call_expr(expr* left, expr_list* arg_list);
static expr* make_list_expr(expr_list* list);
static expr* make_func_expr(expr_list* parameters, statement* body);
static expr* make_assign_expr(expr* left, expr* right, token op);
static expr* make_lvalue_expr(expr* left, token op, expr* right);
static expr* parse_expression();
static statement* parse_statement();
static statement_list* parse_statement_list();
static expr* expression();
static expr* or();
static void print_e(void* expre);
static void print_el(void* expre);
static void print_s(void* expre);
static void print_sl(void* expre);
static bool check(token_type t);
static token advance();
static token previous();

// Public Methods
statement_list* generate_ast(token* _tokens, size_t _length) {
	error_thrown = false;
	tokens = _tokens;
	length = _length;
	curr_index = 0;
	return parse_statement_list();
}

void print_ast(statement_list* ast) {
	pre_order = true;
	traverse_statement_list(ast, print_e, print_el, print_s, print_sl);
}

void free_ast(statement_list* ast) {
	pre_order = false;
	traverse_statement_list(ast, 
		ast_safe_free, ast_safe_free, ast_safe_free, ast_safe_free);
}

bool ast_error_flag() {
	return error_thrown;
}

// Private Methods
static bool is_at_end() {
	return curr_index == length;
}

static bool fnmatch(int count, ...) {
	va_list a_list;
    va_start(a_list, count);	
	for (int i = 0; i < count; i++) {
		token_type next = va_arg(a_list, token_type);
		if (check(next)) {
			advance();
			return true;
		}
	}
	return false;
}

static token advance() {
	if (!is_at_end()) curr_index++;
	return previous();
}

static token previous() {
	return tokens[curr_index - 1];
}

static token peek() {
	return tokens[curr_index];
}

static bool check(token_type t) {
	if (is_at_end()) return false;
	return peek().t_type == t;
}

static void consume(token_type t) {
	if (check(t)) {
		advance();
	}
	else {
		token t = previous();
		error_lexer(t.t_line, t.t_col, AST_EXPECTED_TOKEN, 
			token_string[t.t_type]);
		advance();
		error_thrown = true;
		return;
	}
}

static expr_list* identifier_list() {
	expr_list* list = safe_malloc(sizeof(expr_list));
	list->next = 0;
	expr** curr = &list->elem;
	expr_list* curr_list = list;
	while (1) {
		if (match(T_IDENTIFIER)) {
			*curr = make_lit_expr(previous());
			if (match(T_COMMA)) {
				curr_list->next = safe_malloc(sizeof(expr_list));
				curr_list = curr_list->next;
				curr_list->next = 0;
				curr = &curr_list->elem;
			} else break;	
		}
		else {
			token t = previous();
			error_lexer(t.t_line, t.t_col, AST_EXPECTED_IDENTIFIER);
			error_thrown = true;
		}
	}
	return list;
}

static expr_list* expression_list(token_type end_delimiter) {
	if (peek().t_type == end_delimiter) return 0;	
	expr_list* list = safe_malloc(sizeof(expr_list));
	list->next = 0;
	expr** curr = &list->elem;
	expr_list* curr_list = list;
	while (1) {
		*curr = expression();
		if (match(T_COMMA)) {
			curr_list->next = safe_malloc(sizeof(expr_list));
			curr_list = curr_list->next;
			curr_list->next = 0;
			curr = &curr_list->elem;
		} else break;
	}
	if (error_thrown) {
		// Rollback
		traverse_expr_list(list, 
			ast_safe_free, ast_safe_free, ast_safe_free, ast_safe_free);
		return 0;
	}
	else {
		return list;
	}
}

static expr* lvalue() {
	expr* exp = or();
	token last = previous();
	return exp;
}

static expr* primary() {
	if (match(T_STRING, T_NUMBER, T_TRUE, T_FALSE, T_NONE, T_IDENTIFIER, T_OBJ_TYPE)) {
		return make_lit_expr(previous());	
	}
	else if (match(T_LEFT_BRACK)) {
		expr_list* list = expression_list(T_RIGHT_BRACK);
		expr* list_expr = make_list_expr(list);
		consume(T_RIGHT_BRACK);
		return list_expr;
	}
	else if (match(T_LEFT_PAREN)) {
		expr* left = expression();
		consume(T_RIGHT_PAREN);
		return left;
	}
	else if (match(T_LAMBDA)) {
		consume(T_LEFT_PAREN);
		expr_list* list = expression_list(T_RIGHT_PAREN);
		consume(T_RIGHT_PAREN);
		statement* fn_body = parse_statement();
		return make_func_expr(list, fn_body);
	}
	else {
		token t = previous();
		error_lexer(t.t_line, t.t_col, AST_EXPECTED_PRIMARY);
		advance();
		error_thrown = true;
		return 0;
	}
}

static expr* access() {
	expr* left = primary();
	while (match(T_LEFT_BRACK, T_DOT, T_LEFT_PAREN)) {
		token op = previous();
		expr* right = 0;
		if (op.t_type == T_LEFT_BRACK) {
			right = expression();
			consume(T_RIGHT_BRACK);
			left = make_bin_expr(left, op, right);
		} 
		else if (op.t_type == T_LEFT_PAREN) {
			expr_list* args = expression_list(T_RIGHT_PAREN);
			left = make_call_expr(left, args);
			consume(T_RIGHT_PAREN);
		}
		else {	
			right = primary();
			left = make_bin_expr(left, op, right);
		}	
	}
	return left;
}

static expr* unary() {
	if (match(T_MINUS, T_NOT, T_TILDE)) {
		token op = previous();
		expr* right = unary();
		return make_una_expr(op, right);
	}
	return access();
}
static expr* factor() {
	expr* left = unary();
	while (match(T_STAR, T_SLASH, T_INTSLASH, T_PERCENT)) {
		token op = previous();
		expr* right = unary();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* term() {
	expr* left = factor();
	while (match(T_PLUS, T_MINUS)) {
		token op = previous();
		expr* right = factor();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* range() {
	expr* left = term();
	while (match(T_RANGE_OP)) {
		token op = previous();
		expr* right = term();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* comparison() {
	expr* left = range();
	while (match(T_NOT_EQUAL, T_EQUAL_EQUAL, T_LESS, T_GREATER, T_LESS_EQUAL, 
					T_GREATER_EQUAL, T_TILDE)) {
		token op = previous();
		expr* right = range();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* and() {
	expr* left = comparison();
	while (match(T_AND)) {
		token op = previous();
		expr* right = comparison();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* or() {
	expr* left = and();
	while (match(T_OR)) {
		token op = previous();
		expr* right = and();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static expr* assignment() {
	expr* left = lvalue();
	if (match(T_EQUAL, T_ASSIGN_PLUS, T_ASSIGN_MINUS, 
		T_ASSIGN_STAR, T_ASSIGN_SLASH, T_ASSIGN_INTSLASH)) {
		token op = previous();
		expr* right = or();
		left = make_assign_expr(left, right, op);
	}
	else if (match(T_DEFFN)) {
		token op = make_token(T_EQUAL, make_data_str("equal"));
		consume(T_LEFT_PAREN);
		expr_list* parameters = expression_list(T_RIGHT_PAREN);
		consume(T_RIGHT_PAREN);
		statement* fnbody = parse_statement();
		expr* rvalue = make_func_expr(parameters, fnbody);
		left = make_assign_expr(left, rvalue, 
				make_token(T_EQUAL, make_data_str("=")));
	}
	return left;
}
static expr* expression() {
	expr* res = assignment();
	if (error_thrown) {
		// Rollback
		traverse_expr(res, 
			ast_safe_free, ast_safe_free, ast_safe_free, ast_safe_free);
		return 0;
	}
	else {
		return res;
	}
}

static statement* parse_statement() {
	token first = advance();
	statement* sm = safe_malloc(sizeof(statement));
	sm->src_line = first.t_line;
	switch (first.t_type) {
		case T_LEFT_BRACE: {
			statement_list* sl = parse_statement_list();
			consume(T_RIGHT_BRACE);
			sm->type = S_BLOCK;
			sm->op.block_statement = sl;
			break;
		}
		case T_LET: {
			// Read an expression for a LVALUE
			consume(T_IDENTIFIER);
			token lvalue = previous();
			expr* rvalue = 0;
			if (match(T_EQUAL)) {
				rvalue = expression();
			}
			else if (match(T_DEFFN)) {
				consume(T_LEFT_PAREN);
				expr_list* parameters = expression_list(T_RIGHT_PAREN);
				consume(T_RIGHT_PAREN);
				statement* fnbody = parse_statement();
				rvalue = make_func_expr(parameters, fnbody);
			}
			else {
				rvalue = make_lit_expr(none_token());
			}
			sm->type = S_LET;
			sm->op.let_statement.lvalue = lvalue;
			sm->op.let_statement.rvalue = rvalue;
			break;
		}
		case T_IF: {
			expr* condition = expression();
			statement* run_if_true = parse_statement();
			statement* run_if_false = 0;
			if (match(T_ELSE) || match(T_COLON)) {
				run_if_false = parse_statement();
			}	
			sm->type = S_IF;
			sm->op.if_statement.condition = condition;
			sm->op.if_statement.statement_true = run_if_true;
			sm->op.if_statement.statement_false = run_if_false;
			break;
		}
		case T_LOOP: {
			expr* index_var = expression();
			token a_index;
			expr* condition;
			if (match(T_COLON) || match(T_IN)) {
				condition = expression();
				if (index_var->type != E_LITERAL || 
						index_var->op.lit_expr.t_type != T_IDENTIFIER) {
					token t = previous();
					error_lexer(t.t_line, t.t_col, AST_EXPECTED_IDENTIFIER_LOOP);
				}
				a_index = index_var->op.lit_expr;
				safe_free(index_var);
			}
			else {
				condition = index_var;
				index_var = 0;
				a_index = empty_token();
			}
			statement* run_if_true = parse_statement();
			sm->type = S_LOOP;
			sm->op.loop_statement.condition = condition;
			sm->op.loop_statement.index_var = a_index;
			sm->op.loop_statement.statement_true = run_if_true;
			break;
		}
		case T_STRUCT: {
			if (!match(T_IDENTIFIER)) {
				error_lexer(first.t_line, first.t_col, 
					AST_STRUCT_NAME_IDENTIFIER);	
			}
			token name = previous();
			token parent = empty_token();
			if (match(T_COLON)) {
				if (!match(T_IDENTIFIER)) {
					error_lexer(first.t_line, first.t_col, 
						AST_STRUCT_PARENT_IDENTIFIER);			
				}
				parent = previous();
			}
			consume(T_DEFFN);
			expr_list* static_members = 0;
			expr_list* instance_members = 0;
			int saved_before_iden = 0;
			while (match(T_LEFT_PAREN, T_LEFT_BRACK)) {
				if (previous().t_type == T_LEFT_PAREN) {
					saved_before_iden = curr_index;
					if (match(T_RIGHT_PAREN)) continue;
					instance_members = identifier_list();
					consume(T_RIGHT_PAREN);
				}
				else {
					if (match(T_RIGHT_BRACK)) continue;
					static_members = identifier_list();
					consume(T_RIGHT_BRACK);
				}
			}
			// Default Initiation Function
			statement_list* init_fn = safe_malloc(sizeof(statement_list));
			statement_list* curr = init_fn;
			statement_list* prev = 0;

			expr_list* tmp_ins = instance_members;
			while (tmp_ins) {
				curr->elem = safe_malloc(sizeof(statement));
				curr->elem->type = S_EXPR;
				curr->elem->op.expr_statement = safe_malloc(sizeof(expr));
				curr->elem->op.expr_statement->type = E_ASSIGN;
				expr* ass_expr = curr->elem->op.expr_statement;
				ass_expr->op.assign_expr.operator = 
						make_token(T_EQUAL, make_data_str("="));
				ass_expr->op.assign_expr.rvalue = 
						make_lit_expr(tmp_ins->elem->op.lit_expr);
				// Binary Dot Expr
				expr* left = make_lit_expr(make_token(T_IDENTIFIER, 
								make_data_str("this")));
				expr* right = make_lit_expr(tmp_ins->elem->op.lit_expr);
				token op = make_token(T_DOT, make_data_str("."));
				ass_expr->op.assign_expr.lvalue = 
						make_bin_expr(left, op, right);
				
				if (prev) prev->next = curr;
				prev = curr;
				curr = safe_malloc(sizeof(statement_list));
				
				tmp_ins = tmp_ins->next;
			}
			// Return This Operation
			curr->elem = safe_malloc(sizeof(statement));
			if(prev) prev->next = curr;
			curr->next = 0;
			curr->elem->type = S_OPERATION;
			curr->elem->op.operation_statement.operator = make_token(T_RET, 
					make_data_str("ret"));
			curr->elem->op.operation_statement.operand = make_lit_expr(
				make_token(T_IDENTIFIER, make_data_str("this")));

			expr_list* parameters = 0;	
			if (instance_members) {
				int saved_before_pop = curr_index;
				curr_index = saved_before_iden;
				parameters = identifier_list();
				curr_index = saved_before_pop;
			}
			statement* function_body = safe_malloc(sizeof(statement));
			function_body->type = S_BLOCK;
			function_body->op.block_statement = init_fn;
			// init_fn is now the list of statements, to make it a function
			expr* function_const = make_func_expr(parameters, function_body);

			sm->type = S_STRUCT;
			sm->op.struct_statement.name = name;
			sm->op.struct_statement.init_fn = function_const;
			sm->op.struct_statement.parent = parent;
			sm->op.struct_statement.instance_members = instance_members;
			sm->op.struct_statement.static_members = static_members;
			break;
		}
		case T_INC:
		case T_DEC:
		case T_INPUT:
		case T_EXPLODE:
		case T_AT:	{
			sm->type = S_OPERATION;
			sm->op.operation_statement.operator = first;
			sm->op.operation_statement.operand = expression();
			break;
		}
		case T_REQ: {
			sm->type = S_IMPORT;
			if (match(T_IDENTIFIER, T_STRING)) {
				// Don't actually need to do anything if it's a string, but we
				//   delegate that task to codegen to decide.
				sm->op.import_statement = previous();
			}
			else {
				error_lexer(first.t_line, first.t_col, AST_UNRECOGNIZED_IMPORT);
			}
			break;
		}
		case T_RET: {
			sm->type = S_OPERATION;
			sm->op.operation_statement.operator = first;
			if (peek().t_type != T_SEMICOLON && peek().t_type != T_RIGHT_BRACE) {
				sm->op.operation_statement.operand = expression();
			}
			else {
				sm->op.operation_statement.operand = make_lit_expr(noneret_token());
			}
			break;
		}
		default:
			// We advanced it so we gotta roll back.
			curr_index--;	
			// Handle as expression.
			sm->type = S_EXPR;
			sm->op.expr_statement = expression();
			break;
	}
	match(T_SEMICOLON);
	if (error_thrown) {
		// Rollback
		traverse_statement(sm, 
			ast_safe_free, ast_safe_free, ast_safe_free, ast_safe_free);
		return 0;
	}
	return sm;
}

static statement_list* parse_statement_list() {
	statement_list* ast = safe_malloc(sizeof(statement_list));
	ast->next = 0;
	statement** curr = &ast->elem;
	statement_list* curr_ast = ast;
	while (true) {
		*curr = parse_statement();
		if (!is_at_end() && peek().t_type != T_RIGHT_BRACE) {
			curr_ast->next = safe_malloc(sizeof(statement_list));
			curr_ast = curr_ast->next;
			curr_ast->next = 0;
			curr = &curr_ast->elem;
		} else break;
	}
	if (error_thrown) {
		// Rollback
		traverse_statement_list(ast, 
			ast_safe_free, ast_safe_free, ast_safe_free, ast_safe_free);
		return 0;
	}
	else {
		return ast;
	}
}

// Expression Traversal:
/* 	void (*a)(expr*), void (*b)(expr_list*), 
	void (*c)(statement*), void (*d)(statement_list*) */
static void traverse_statement_list(statement_list* list,
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!list) return;
	if (pre_order) d(list);
	indentation++;
	traverse_statement(list->elem, a, b, c, d);
	indentation--;
	traverse_statement_list(list->next, a, b, c, d);
	if (!pre_order) d(list);
}

static void traverse_statement(statement* state, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!state) return;
	if (pre_order) c(state);
	indentation++;
	if (state->type == S_LET) {
		traverse_expr(state->op.let_statement.rvalue, a, b, c, d);
	}
	else if (state->type == S_OPERATION) {
		traverse_expr(state->op.operation_statement.operand, a, b, c, d);
	}
	else if (state->type == S_EXPR) {
		traverse_expr(state->op.expr_statement, a, b, c, d);
	}
	else if (state->type == S_BLOCK) {
		traverse_statement_list(state->op.block_statement, a, b, c, d);
	}
	else if (state->type == S_STRUCT) {
		traverse_expr(state->op.struct_statement.init_fn, a, b, c, d);
		traverse_expr_list(state->op.struct_statement.instance_members, a, b, c, d);
		traverse_expr_list(state->op.struct_statement.static_members, a, b, c, d);
	}
	else if (state->type == S_IF) {
		traverse_expr(state->op.if_statement.condition, a, b, c, d);
		traverse_statement(state->op.if_statement.statement_true, a, b, c, d);
		traverse_statement(state->op.if_statement.statement_false, a, b, c, d);
	}
	else if (state->type == S_LOOP) {
		traverse_expr(state->op.loop_statement.condition, a, b, c, d);
		traverse_statement(state->op.loop_statement.statement_true, a, b, c, d);
	}
	else if (state->type == S_IMPORT) {
		// Do nothing.
	}
	else {
		printf("Traverse Statement: Unknown Type\n");
	}
	indentation--;
	if(!pre_order) c(state);
}

static void traverse_expr(expr* expression, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!expression) return;
	if (pre_order) a(expression);
	indentation++;
	if (expression->type == E_LITERAL) {
		
	}
	else if (expression->type == E_BINARY || expression->type == E_BIN_LVALUE) {
		traverse_expr(expression->op.bin_expr.left, a, b, c, d);
		traverse_expr(expression->op.bin_expr.right, a, b, c, d);
	}
	else if (expression->type == E_UNARY) {
		traverse_expr(expression->op.una_expr.operand, a, b, c, d);
	}
	else if (expression->type == E_CALL) {
		traverse_expr(expression->op.call_expr.function, a, b, c, d);
		traverse_expr_list(expression->op.call_expr.arguments, a, b, c, d);
	}
	else if (expression->type == E_LIST) {
		traverse_expr_list(expression->op.list_expr.contents, a, b, c, d);
	}
	else if (expression->type == E_FUNCTION) {
		traverse_expr_list(expression->op.func_expr.parameters, a, b, c, d);
		traverse_statement(expression->op.func_expr.body, a, b, c, d);
	}
	else if (expression->type == E_ASSIGN) {
		traverse_expr(expression->op.assign_expr.lvalue, a, b, c, d);	
		traverse_expr(expression->op.assign_expr.rvalue, a, b, c, d);	
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
	indentation--;
	if (!pre_order) a(expression);
}

static void traverse_expr_list(expr_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!list) return;
	if (pre_order) b(list);
	indentation++;
	traverse_expr(list->elem, a, b, c, d);
	indentation--;
	traverse_expr_list(list->next, a, b, c, d);
	if (!pre_order) b(list);
}
static void print_indent() {
	char a[50] = {0};
	for (int i = 0; i < indentation; i++) {
		strcat(a, "| ");
	}
	strcat(a, "`-");
	printf("%s", a);
}
static void print_e(void* expre) {
	print_indent();
	expr* expression = (expr*)expre;
	printf(YEL);
	if (expression->type == E_LITERAL) {
		printf("Literal Expression " GRN);
		print_token(&expression->op.lit_expr);
		printf(RESET);
	}
	else if (expression->type == E_BINARY) {
		printf("Binary Expression " GRN);
		print_token(&expression->op.bin_expr.operator);
		printf(RESET);
	}
	else if (expression->type == E_BIN_LVALUE) {
		printf("Binary LValue Expression\n");
	}
	else if (expression->type == E_UNARY) {
		printf("Unary Expression\n");
	}
	else if (expression->type == E_CALL) {
		printf("Call Expression\n");
	}
	else if (expression->type == E_LIST) {
		printf("List Expression\n");
	}
	else if (expression->type == E_FUNCTION) {
		printf("Function Expression\n");
	}
	else if (expression->type == E_ASSIGN) {
		printf("Assignment Expression \n");
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
	printf(RESET);
}
static void print_el(void* el) {
	print_indent();
	printf(CYN "<Expression List Item>\n" RESET);
}
static void print_s(void* s) {
	print_indent();
	statement* state = (statement*)s;
	printf(BLU);
	if (state->type == S_LET) {
		printf("Let Statement " GRN "(%s)\n" RESET,
			state->op.let_statement.lvalue.t_data.string);
	}
	else if (state->type == S_OPERATION) {
		printf("Operation Statement " GRN);
		print_token_inline(&state->op.operation_statement.operator, stdout);
		printf(" \n" RESET);
	}
	else if (state->type == S_EXPR) {
		printf("Expression Statement \n");	
	}
	else if (state->type == S_BLOCK) {
		printf("Block Statement \n");
	}
	else if (state->type == S_STRUCT) {
		printf("Struct Statement " GRN);
		print_token_inline(&state->op.struct_statement.name, stdout);
		printf(RESET ":" GRN);
		print_token_inline(&state->op.struct_statement.parent, stdout);
		printf("\n" RESET);
	}
	else if (state->type == S_IF) {
		printf("If Statement\n");
	}
	else if (state->type == S_LOOP) {
		printf("Loop Statement\n");
	}
	else if (state->type == S_IMPORT) {
		printf("Import Statement\n");
	}
	else {
		printf("Traverse Statement: Unknown Type\n");
	}
	printf(RESET);
}
static void print_sl(void* sl) {
	print_indent();
	printf(MAG "<Statement List Item>\n" RESET);
}

static void traverse_ast(statement_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*), bool order) {
	pre_order = false;
	traverse_statement_list(list, a, b, c, d);	
}
 
static expr* make_lit_expr(token t) {
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_LITERAL;
	node->op.lit_expr = t;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static expr* make_lvalue_expr(expr* left, token op, expr* right) {
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_BIN_LVALUE;
	node->op.bin_expr.operator = op;
	node->op.bin_expr.left = left;
	node->op.bin_expr.right = right;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static expr* make_bin_expr(expr* left, token op, expr* right) {
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_BINARY;
	node->op.bin_expr.operator = op;
	node->op.bin_expr.left = left;
	node->op.bin_expr.right = right;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static expr* make_una_expr(token op, expr* operand) {
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_UNARY;
	node->op.una_expr.operator = op;
	node->op.una_expr.operand = operand;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static expr* make_call_expr(expr* left, expr_list* arg_list) {
	token t = tokens[curr_index];
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_CALL;
	node->op.call_expr.function = left;
	node->op.call_expr.arguments = arg_list;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static expr* make_list_expr(expr_list* list) {
	token t = tokens[curr_index];
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_LIST;
	int size = 0;
	expr_list* start = list;
	while (start) {
		start = start->next;
		size++;
	}
	node->op.list_expr.length = size;
	node->op.list_expr.contents = list;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static expr* make_assign_expr(expr* left, expr* right, token op) {
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_ASSIGN;
	node->op.assign_expr.lvalue = left;
	node->op.assign_expr.rvalue = right;
	node->op.assign_expr.operator = op;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static expr* make_func_expr(expr_list* parameters, statement* body) {
	token t = tokens[curr_index];
	expr* node = safe_malloc(sizeof(expr));
	node->type = E_FUNCTION;

	// Reverse Parameters
	expr_list* curr = parameters;
	expr_list* prev = 0;
	expr_list* next = 0;

	while (curr) {
		next = curr->next;
		curr->next = prev;
		prev = curr;
		curr = next;
	}
	parameters = prev;
	node->op.func_expr.parameters = parameters;
	node->op.func_expr.body = body;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TF_TYPE_FIXNUM 0xFF
#define TF_TYPE_STRING 0xFE
#define TF_TYPE_SYMBOL 0xFD

typedef unsigned int tf_size;
typedef unsigned char tf_type;
typedef int i32;
typedef char tf_bool;

typedef struct tf_stack {
  int size;
  int position;
  char* root;
} tf_stack;

typedef struct tf_item {
  int size;
  int type;
  char* data;
} tf_item;

void tf_stack_init(tf_stack *stack) {
  stack->root = 0; // realloc with 0 ptr is like malloc
  stack->size = 0;
  stack->position = 0;
}
void tf_stack_free(tf_stack *stack) {
  if(stack->root) free(stack->root);
  stack->root = 0;
}

// return pointer at current stack position
char* tf_stack_pt(tf_stack *stack) {
  return &stack->root[stack->position];
}

// grow stack if needed to fit extra size bytes.
// todo: make it shrink too?
void tf_stack_ensure_size(tf_stack *stack, int size) {
  // todo: too many reallocs here for big blobs in start of blob
  while(stack->position + size > stack->size) {
    int newsize = stack->size == 0 ? 16 : stack->size * 2;
    printf("\x1b[31mstack resize from %d to %d\x1b[0m\n", stack->size, newsize);
    stack->root = realloc(stack->root, newsize);
    stack->size = newsize;
  }
}

// must call ensure_size before doing this:
void _tf_stack_push_blob(tf_stack *stack, tf_size len, char* data) {
  char* block = tf_stack_pt(stack);
  memcpy(block, data, len);
  stack->position += len;
}

// place a simple blob of fixed size onto stack. this size much be known at pop-time!
char* _tf_stack_pop_blob(tf_stack *stack, tf_size len) {
  if(stack->position < len) { printf("error 6f13cf5a stack underflow\n");exit(0);}
  stack->position -= len;
  return tf_stack_pt(stack);
}

void tf_stack_push_raw_char(tf_stack *stack, char c) {
  tf_stack_ensure_size(stack, 1);
  stack->root[stack->position] = c;
  stack->position++;
}
void tf_stack_push_raw_type(tf_stack *stack, tf_type type) {
  tf_stack_ensure_size(stack, sizeof(tf_type));
  *(tf_type*)(stack->root + stack->position) = type;
  stack->position += sizeof(tf_type);
}

void tf_stack_push_raw_i32(tf_stack *stack, i32 value) {
  tf_stack_ensure_size(stack, sizeof(i32));
  *(i32*)(stack->root + stack->position) = value;
  stack->position += sizeof(i32);
}

void tf_stack_push_raw_size(tf_stack *stack, tf_size size) {
  tf_stack_ensure_size(stack, sizeof(tf_size));
  *(tf_size*)(stack->root + stack->position) = size;
  stack->position += sizeof(tf_size);
}

void tf_stack_push_fixnum(tf_stack *stack, i32 value) {
  tf_stack_push_raw_i32(stack, value);
  tf_stack_push_raw_type(stack, TF_TYPE_FIXNUM);
  tf_stack_push_raw_size(stack, sizeof(i32));
}

// pop an item from stack
void tf_stack_pop_item(tf_stack *stack, tf_item *item) {
  item->size = *(tf_size*)  _tf_stack_pop_blob(stack, sizeof(tf_size));
  item->type = *(tf_type*)  _tf_stack_pop_blob(stack, sizeof(tf_type));
  item->data =              _tf_stack_pop_blob(stack, item->size);
}

i32 tf_stack_pop_i32(tf_stack *stack) {
  tf_item top;
  tf_stack_pop_item(stack, &top);
  // todo check blocklen & type
  return *(i32*)(top.data);
}

// we don't have null-terminated strings, we have explicit
// length. wish there was a printf for that.
void tf_print_blob(char* str, int len) {
  fwrite(str, len, 1, stdout);
}

void tf_print_item(tf_item *item) {
  if(item->type == TF_TYPE_FIXNUM) {
    printf("%d", *(i32*)item->data);
  } else if (item->type == TF_TYPE_STRING) {
    printf("\""); tf_print_blob(item->data, item->size); printf("\"");
  } else if (item->type == TF_TYPE_SYMBOL) {
    tf_print_blob(item->data, item->size);
  } else
    printf("error 510ff023 invalid type %x02!!!\n", item->type);
}

void tf_stack_print(tf_stack *stack) {
  int i = 0;
  tf_stack copy = *stack;
  tf_item item;
  while(stack->position > 0) {
    int pos = stack->position;
    tf_stack_pop_item(stack, &item);
    printf("%d: %02x %d bytes: ", i, item.type, item.size);
    tf_print_item(&item); printf("\n");
    i++;
  }
  stack->position = copy.position;
}

void tf_stack_print_hex(tf_stack *stack) {
  int i;
  for(i = 0 ; i < stack->position ; i++) {
    printf("%02x ", (stack->root[i] & 0xFF));
  }
  printf("\n");
}


// ============================== reader / tokenizer ==============================
// ok     reads fixnum, string, symbols
// todo   lazy buffers / interactive / don't need everything in memory


// - a cursor has a buffer of 1 byte and can be called to read the next byte
// - this means we've only got 1 byte of the source in memory at a time
// - this means we can't use strtol for example
// - symbols and strings are pushed on the stack on the fly as they are read
// - this is slow but has a simple interface

typedef struct tf_cursor tf_cursor;
typedef char (*tf_peek_proc)(tf_cursor*);
typedef tf_bool (*tf_read_proc)(tf_cursor*);
typedef struct tf_cursor {
  void* _rbuff; // arbitrary reader data (buffers etc)
  tf_peek_proc peek;
  tf_read_proc read;
} tf_cursor;

tf_bool is_ws(char c) { if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0) return 1; return 0; }
tf_bool is_digit(char c) { if(c >= '0' && c <= '9') return 1; return 0; }
tf_bool read_ws(tf_stack *stack, tf_cursor *c) { while(is_ws(c->peek(c)) && c->read(c)) ; return 1; }

tf_bool read_fixnum(tf_stack *stack, tf_cursor *c) {
  int val = 0;
  tf_bool ok = 0;
  while(is_digit(c->peek(c))) {
    val *= 10;
    val += c->peek(c) - '0';
    ok = 1;
    if(!c->read(c)) break;
  }
  if(ok && !is_ws(c->peek(c))) {
    printf("error 26ba8965: unexpected character %c in number\n", c->peek(c));
    return 0;
  }
  if(ok) {
    tf_stack_push_fixnum(stack, val);
    return 1;
  }
  return 0;
}

// todo: escape sequences?
tf_bool read_string(tf_stack *stack, tf_cursor *c) {
  if(c->peek(c) == '"') {
    if(!c->read(c)) return 0; // consume first double-quote
    int size = 0;
    while(c->peek(c) != '"') {
      tf_stack_push_raw_char(stack, c->peek(c));
      if(!c->read(c)) {
        printf("error bdb138f3 unexpected eof while reading string\n");
        stack->position -= size;
        return 0;
      }
      size++;
    }
    tf_stack_push_raw_type(stack, TF_TYPE_STRING);
    tf_stack_push_raw_size(stack, size);
    c->read(c); // consume last doublequote (don't check for eof)
    return 1;
  }
  return 0;
}

tf_bool read_symbol(tf_stack *stack, tf_cursor *c) {
  int size = 0;
  while(!is_ws(c->peek(c))) {
    tf_stack_push_raw_char(stack, c->peek(c));
    size++;
    if(!c->read(c)) break;
  }
  if(size) {
    tf_stack_push_raw_type(stack, TF_TYPE_SYMBOL);
    tf_stack_push_raw_size(stack, size);
    return 1;
  }
  return 0;
}

// - the next character may at any time be \0 which no tokenizers will match
// - cursors must support multiple read calls after EOF
tf_bool tf_read(tf_stack *stack, tf_cursor *c) {
  char* tmp;
  if(c->peek(c) == 0) { if(!c->read(c)) return 0; }
  read_ws(stack, c);
  if(read_fixnum(stack, c)) return 1;
  else if(read_string(stack, c)) return 1;
  return read_symbol(stack, c);
  return 0;
}


// ============================== library ==============================

void tf_add(tf_stack *stack) {
  int a = tf_stack_pop_i32(stack);
  int b = tf_stack_pop_i32(stack);
  tf_stack_push_fixnum(stack, a + b);
}

void tf_subtract(tf_stack *stack) {
  i32 t0 = tf_stack_pop_i32(stack);
  tf_stack_push_fixnum(stack, tf_stack_pop_i32(stack) - t0);
}

void tf_multiply(tf_stack *stack) {
  tf_stack_push_fixnum(stack, tf_stack_pop_i32(stack) * tf_stack_pop_i32(stack));
}

void tf_dup(tf_stack *stack) {
  int n = tf_stack_pop_i32(stack);
  tf_stack_push_fixnum(stack, n);
  tf_stack_push_fixnum(stack, n);
}

typedef void (*tf_proc)(tf_stack*);

typedef struct tf_symbol {
  char *name;
  tf_proc proc;
} tf_symbol;

tf_symbol tf_procedures[] =
  { {"add", tf_add},
    {"sub", tf_subtract},
    {"mult", tf_multiply},
    {"dup", tf_dup},
    {},
  };

tf_bool tf_reader_stdin_read(tf_cursor *c) {
  int byte = getchar();
  if(byte >= 0) { c->_rbuff = (void*)(long)(byte & 0xFF); return 1;}
  else          { c->_rbuff =            0; return 0;}
}
char tf_reader_stdin_peek(tf_cursor *c) {
  return (char)(long)c->_rbuff;
}

typedef struct tf_reader_argv {
  int argc; // number of argv's
  char ** argv; // pointer to next argv
} tf_reader_argv;

tf_bool tf_reader_argv_read(tf_cursor *c) {
  tf_reader_argv *rb = (tf_reader_argv*)c->_rbuff;
  char *current = rb->argv[0];

  if(current == 0) return 0;
  rb->argv[0]++; // move pointer
  if(rb->argv[0][0] == 0) { // end of curent buffer
    if(rb->argc <= 1) return 0;
    int i;
    rb->argv++;
    rb->argc--;
  }
  return 1;
}
char tf_reader_argv_peek(tf_cursor *c) {
  return ((tf_reader_argv*)c->_rbuff)->argv[0][0];
}

void tf_eval_top(tf_stack *stack) {
  int save_pos = stack->position;
  tf_item item;
  tf_stack_pop_item(stack, &item);
  if(item.type == TF_TYPE_SYMBOL) {
    printf("executing "); tf_print_item(&item); printf("\n");
    int i = 0;
    while(1) {
      char* global = tf_procedures[i].name;
      if(global == 0) { printf("error cannot find procedure ");tf_print_item(&item);printf("\n");break;}
      if(strlen(global) == item.size && memcmp(tf_procedures[i].name, item.data, item.size) == 0) {
        printf("");
        tf_procedures[i].proc(stack);
        break;
      }
      i++;
    }
  } else // keep self-evaluating items on stack
    stack->position = save_pos;
}

int main(int argc, char **argv) {
  tf_stack _stack, *stack = &_stack; // so everybody does stack->
  tf_stack_init(stack);

  tf_reader_argv argv_reader_data = {.argc = argc - 1, .argv = &argv[1]}; // exclude progname
  tf_cursor argv_cursor = {.read = tf_reader_argv_read ,
                           .peek = tf_reader_argv_peek ,
                           ._rbuff = &argv_reader_data};
  tf_cursor stdin_cursor = {.read = tf_reader_stdin_read, .peek = tf_reader_stdin_peek};

  while(1) {
    if(!tf_read(stack, &argv_cursor)) break;
    tf_eval_top(stack);
  }
  tf_stack_print(stack);


  tf_stack_free(stack);
  return 0;
}

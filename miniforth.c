
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TF_TYPE_FIXNUM 0xFF
#define TF_TYPE_STRING 0xFE
#define TF_TYPE_SYMBOL 0xFD

typedef unsigned int tf_size;
typedef unsigned char tf_type;
typedef int i32;

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

// push an item to stack
void tf_stack_push(tf_stack *stack, tf_size len, char* data, tf_type type) {

  tf_stack_ensure_size(stack, len + sizeof(tf_type) + sizeof(tf_size));
  _tf_stack_push_blob(stack, len, data);
  _tf_stack_push_blob(stack, sizeof(tf_type), (char*)&type);
  _tf_stack_push_blob(stack, sizeof(tf_size), (char*)&len);
}

// pop an item from stack
void tf_stack_pop_item(tf_stack *stack, tf_item *item) {
  item->size = *(tf_size*)  _tf_stack_pop_blob(stack, sizeof(tf_size));
  item->type = *(tf_type*)  _tf_stack_pop_blob(stack, sizeof(tf_type));
  item->data =              _tf_stack_pop_blob(stack, item->size);
}

void tf_stack_push_fixnum(tf_stack *stack, i32 value) {
  tf_stack_push(stack, sizeof(i32), (char*)&value, TF_TYPE_FIXNUM);
}
void tf_stack_push_string(tf_stack *stack, char* str, int len) {
  tf_stack_push(stack, len, str, TF_TYPE_STRING);
}
void tf_stack_push_symbol(tf_stack *stack, char* str, int len) {
  tf_stack_push(stack, len, str, TF_TYPE_SYMBOL);
}

i32 tf_stack_pop_i32(tf_stack *stack) {
  tf_item top;
  tf_stack_pop_item(stack, &top);
  // todo check blocklen & type
  return *(i32*)(top.data);
}

int tf_native_plus(tf_stack *stack) {
  int a = tf_stack_pop_i32(stack);
  int b = tf_stack_pop_i32(stack);
  tf_stack_push_fixnum(stack, a + b);
  return 0;
}

void tf_stack_print(tf_stack *stack) {
  int i = 0;
  tf_stack copy = *stack;
  tf_item item;
  while(stack->position > 0) {
    int pos = stack->position;
    tf_stack_pop_item(stack, &item);
    printf("%d:  (%08x %4d) size: %d val: %d\n", i, item.data, pos,  item.size, item.data[0]);
    i++;
    //printf("nx %d\n", stack->position);
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


// ==================== reader / tokenizer ====================
// ok     reads fixnum, string, symbols
// todo   lazy buffers / interactive / don't need everything in memory

char is_ws(char c) { if(c == ' ' || c == '\t' || c == '\n' || c == '\r') return 1; return 0; }
char* read_ws(tf_stack *stack, char *str) { while(is_ws(str[0])) {str++;} return str; }

char* read_fixnum(tf_stack *stack, char *str) {
  char *end;
  int val = strtol(str, &end, 0);
  if(end != str && is_ws(end[0])) {
    tf_stack_push_fixnum(stack, val);
    return end;
  }
  return str;
}

char* read_string(tf_stack *stack, char *str) {
  if(str[0] == '"') {
    str++; // consume first double-quote
    char *start = str;
    while(str[0] != '"') str++; // todo: check for zero byte?
    tf_stack_push_string(stack, start, str - start);
    str++; // consume last doublequote
  }
  return str;
}

char* read_symbol(tf_stack *stack, char *str) {
  char* start = str;
  while(!is_ws(str[0])) str++;
  if(start != str) { tf_stack_push_symbol(stack, start, str - start); }
  return str;
}

char* tf_read(tf_stack *stack, char *str) {
  char* tmp;
  if(str[0] == 0) return 0;
  str = read_ws(stack, str);
  if((tmp = read_fixnum(stack, str)) != str) return tmp;
  else if((tmp = read_string(stack, str)) != str) return tmp;
  return read_symbol(stack, str);
}


int main() {
  int i;
  tf_stack _stack, *stack = &_stack; // so everybody does stack->
  tf_stack_init(stack);

  tf_stack_push_fixnum(stack, 5);
  tf_stack_push_fixnum(stack, 10);
  tf_stack_push_fixnum(stack, 7);
  tf_stack_print(stack);

  tf_native_plus(stack);
  tf_stack_print(stack);

  tf_native_plus(stack);
  tf_stack_print(stack);



  char *end;
  char _s[] = " 124  \"a b c\"  \n\r\n12 0xFF 1G \"foo\"", *s = _s;
  while(s != 0) {
    s = tf_read(stack, s);
  }
  tf_stack_print(stack);


  tf_stack_free(stack);
  return 0;
}

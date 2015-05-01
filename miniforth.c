
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
  //int type;
  char* data;
} tf_item;

void tf_stack_init(tf_stack *stack) {
  stack->root = 0; // realloc with 0 ptr is like malloc. fis pre
  stack->size = 0;
  stack->position = 0;
}
void tf_stack_free(tf_stack *stack) {
  if(stack->root) free(stack->root);
  stack->root = 0;
}

char* tf_stack_pt(tf_stack *stack) {
  return &stack->root[stack->position];
}


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

char* _tf_stack_pop_blob(tf_stack *stack, tf_size len) {
  if(stack->position < len) { printf("error 6f13cf5a stack underflow\n");exit(0);}
  stack->position -= len;
  return tf_stack_pt(stack);
}

void tf_stack_push(tf_stack *stack, tf_size len, char* data, tf_type type) {

  tf_stack_ensure_size(stack, len + sizeof(tf_type) + sizeof(tf_size));
  _tf_stack_push_blob(stack, sizeof(tf_type), (char*)&type);             // copy payload in
  _tf_stack_push_blob(stack, len, data);             // copy payload in
  _tf_stack_push_blob(stack, sizeof(tf_size), (char*)&len); // copy len descriptors in
}

// fill in with stack top witout modifying stack
void tf_stack_pop_item(tf_stack *stack, tf_item *item) {

  item->size = *(tf_size*)_tf_stack_pop_blob(stack, sizeof(tf_size));
  printf("top size: %d\n", item->size);

  item->data = _tf_stack_pop_blob(stack, item->size);
  item->type = *(tf_type*)_tf_stack_pop_blob(stack, sizeof(tf_type));
}


void tf_stack_push_i32(tf_stack *stack, i32 value) {
  tf_stack_push(stack, sizeof(i32), (char*)&value, 0);
}

i32 tf_stack_pop_i32(tf_stack *stack) {
  tf_item top;
  tf_stack_pop_item(stack, &top);
  printf("pop: %d \n", *(i32*)top.data);
  // todo check blocklen & type
  return *(i32*)(top.data);
}

int tf_native_plus(tf_stack *stack) {
  int a = tf_stack_pop_i32(stack);
  int b = tf_stack_pop_i32(stack);
  tf_stack_push_i32(stack, a + b);
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
    printf("%02x ", stack->root[i]);
  }
  printf("\n");
}

int main() {
  char mystack[] =
    "\x02\x01\x00\x00\x00"
    "\x03\x01\x00\x00\x00"
    "\x02\x01\x00\x00\x00"
    "                                                                                ";
  int i;
  tf_stack _stack;
  tf_stack *stack = &_stack; // so everybody does stack->
  tf_stack_init(stack);

  //tf_item item;
  //tf_stack_peek_item(&stack, &item);
  //printf("size %d val %d\n", item.size, item.data[0]);
  tf_stack_print(stack);
  printf("\n");
  tf_stack_push_i32(stack, 5);
  tf_stack_push_i32(stack, 7);
  tf_stack_print_hex(stack);
  tf_native_plus(stack);
  tf_stack_print(stack);

  tf_stack_free(stack);
  return 0;
}

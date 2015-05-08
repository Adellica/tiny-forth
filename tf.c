#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int  tf_size;
typedef unsigned char tf_type;
typedef int i32;
typedef char tf_bool;

typedef unsigned int tf_fixnum_t;
typedef unsigned char tf_tag;

#define TF_TAG_PAIR    '('
#define TF_TAG_FIXNUM  '0'
#define TF_TAG_NIL     'n'

typedef struct tf_obj_struct* tf_obj;
typedef void (*tf_free_proc)(tf_obj);
typedef struct tf_obj_struct {
  tf_tag tag;
  unsigned int refs;
  tf_free_proc freer;
  union {
    struct {
      tf_obj car;
      tf_obj cdr;
    } pair;
    tf_fixnum_t fixnum;
  } value;
} tf_obj_struct;

tf_obj_struct _tf_nil = {.tag = TF_TAG_NIL};
tf_obj tf_nil = &_tf_nil;

typedef struct tf_machine {
  tf_obj heap;
} tf_machine;

#define TF_OBJ_SIZE (sizeof(tf_obj_struct))

int tf_assert(int equal) {
  if(!equal) printf("error: asertion failed\n");
}

tf_obj tf_cdr(tf_obj obj) {
  tf_assert(obj->tag == TF_TAG_PAIR);
  return obj->value.pair.cdr;
}
tf_obj tf_car(tf_obj obj) {
  tf_assert(obj->tag == TF_TAG_PAIR);
  return obj->value.pair.car;
}

#define TF_PRINT_FLAG_ADR 0x01

void tf_print(tf_obj o, int flags) {
  if(flags & TF_PRINT_FLAG_ADR) {
    printf("\x1b[31m%08x\x1b[0m", o);
  }

  if(o->tag == TF_TAG_PAIR) {
    printf("(");
    tf_print(tf_car(o), flags);
    printf(" . ");
    tf_print(tf_cdr(o), flags);
    printf(")");
  } else if(o->tag == TF_TAG_FIXNUM) {
    printf("%d", o->value.fixnum);
  } else if(o->tag == TF_TAG_NIL) {
    printf("()");
  } else {
    printf("[unknown %08x %c]", o, o->tag);
  }
}

void _tf_cons(tf_obj dest, tf_obj a, tf_obj lst) {
  dest->tag = TF_TAG_PAIR;
  dest->value.pair.car = a; a->refs++;
  dest->value.pair.cdr = lst; lst->refs++;
}


// force free
void _tf_obj_free(tf_obj o) {
  printf("forced free %08x\n", o);
  free(o);
}
// does basic checking
void tf_obj_free(tf_obj o) {

  if(!o) {printf("somebody wants to free obj %08x\n", o);}

  if(o->freer) {
    printf("freeing %08x using proc %08x\n", o, o->freer);
    tf_free_proc freer = o->freer;
    o->freer = 0;
    freer(o);
  }
}

tf_obj tf_obj_alloc(tf_machine *machine) {
  tf_obj obj = malloc(TF_OBJ_SIZE);
  tf_obj dst = malloc(TF_OBJ_SIZE);
  obj->freer = _tf_obj_free;
  dst->freer = _tf_obj_free;
  printf("allocat %08x\n", obj);
  printf("allocat %08x\n", dst);
  _tf_cons(dst, obj, machine->heap);
  machine->heap = dst;
  obj->tag = TF_TAG_NIL;
  return obj;
}

tf_obj tf_cons(tf_machine *machine, tf_obj a, tf_obj lst) {
  tf_obj pair = tf_obj_alloc(machine);
  pair->tag = TF_TAG_PAIR;
  pair->value.pair.car = a;
  pair->value.pair.cdr = lst;
  return pair;
}

tf_obj tf_fixnum(tf_machine *m, tf_fixnum_t n) {
  tf_obj fix = tf_obj_alloc(m);
  fix->tag = TF_TAG_FIXNUM;
  fix->value.fixnum = n;
  return fix;
}

void tf_machine_free(tf_machine *m) {
  tf_obj o = m->heap;
  tf_obj next;
  do {
    if(o->tag == TF_TAG_PAIR) {
      next = tf_cdr(o);
      tf_obj_free(tf_car(o)); // free content
    }
    else next = 0;

    tf_obj_free(o); // free cell itself
  } while(o = next);
}

int main() {
  tf_machine _machine = {.heap = tf_nil};
  tf_machine *m = &_machine;

  tf_obj obj1 = tf_obj_alloc(m);
  tf_obj obj2 = tf_obj_alloc(m);

  printf("\n\n");

  tf_print(tf_cons(m,
                   tf_fixnum(m, 13),
                   tf_cons(m, tf_fixnum(m, 121), tf_nil)), 0);
  printf("\n");

  tf_machine_free(m);
  return 0;
}

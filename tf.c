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
  char* where;
  int lineno;
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
tf_fixnum_t tf_get_fixnum(tf_obj obj) {
  if(obj->tag == TF_TAG_FIXNUM) return obj->value.fixnum;
  printf("error: not a fixnum %08x!", obj);
  return 0;
}
tf_fixnum_t tf_pairp(tf_obj o) {
  if(o) return o->tag == TF_TAG_PAIR;
  return 0;
}

#define TF_PRINT_FLAG_ADR 0x01
#define TF_PRINT_FLAG_IN_LIST 0x02 // smart printing of cells

void tf_print(tf_obj o, int flags) {
  if(flags & TF_PRINT_FLAG_ADR) {
    printf("\x1b[31m%08x\x1b[0m", o);
  }

  if(o->tag == TF_TAG_PAIR) {
    if(flags & TF_PRINT_FLAG_IN_LIST) printf(" ");
    else printf("(");
    tf_print(tf_car(o), tf_pairp(tf_car(o)) ? flags & ~TF_PRINT_FLAG_IN_LIST : flags | TF_PRINT_FLAG_IN_LIST);
    if(!tf_pairp(o)) printf(" . ");
    tf_print(tf_cdr(o), flags | TF_PRINT_FLAG_IN_LIST);
    if(!(flags & TF_PRINT_FLAG_IN_LIST)) printf(")");
  } else if(o->tag == TF_TAG_FIXNUM) {
    printf("%d", o->value.fixnum);
  } else if(o->tag == TF_TAG_NIL) {
    if(!(flags & TF_PRINT_FLAG_IN_LIST)) printf("()");
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
  free(o);
}
char* tf_type_str(tf_obj o) {
  switch(o->tag) {
  case TF_TAG_FIXNUM: return "fixnum";
  case TF_TAG_PAIR: return "pair";
  default: return "unknown";
  }
}

// does basic checking
void tf_obj_free(tf_obj o) {

  if(!o) {printf("somebody wants to free obj %08x\n", o);}

  if(o->freer) {
    printf("freeing %7s %08x from %s:%d  %08x\n", tf_type_str(o), o, o->where, o->lineno, o->freer);
    tf_free_proc freer = o->freer;
    o->freer = 0;
    freer(o);
  }
}

tf_obj _tf_obj_alloc(tf_machine *machine, char* where, int linenum) {
  tf_obj obj = malloc(TF_OBJ_SIZE);
  tf_obj dst = malloc(TF_OBJ_SIZE);
  obj->freer = _tf_obj_free;
  dst->freer = _tf_obj_free;

  obj->where = where; obj->lineno = linenum;
  dst->where = where; dst->lineno = linenum;

  _tf_cons(dst, obj, machine->heap);
  machine->heap = dst;
  obj->tag = TF_TAG_NIL;
  return obj;
}

#define tf_obj_alloc(m) _tf_obj_alloc(m, __FILE__, __LINE__);

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

// note to self: macros in anything but LISP is a bad idea, but let's
// see if this can work:
#define tf_fold(list, iter, result)        \
  tf_obj c = list; \
  while(tf_pairp(c)) { iter; c = tf_cdr(c); } \
  if(c->tag == TF_TAG_NIL) {result;} \
  else {printf("error: imporoper list %08x\n", list);};


static tf_obj_struct tf_proc_add;
tf_obj tf_eval(tf_machine *m, tf_obj s);
tf_obj tf_apply(tf_machine *m, tf_obj proc, tf_obj args) {
  if(proc == &tf_proc_add) {
    int r = 0;
    tf_fold(args, r += tf_get_fixnum(tf_eval(m, tf_car(c))), return tf_fixnum(m, r));
  }
  else
    printf("unknown procedure %x08\n", proc);
}

tf_obj tf_eval(tf_machine *m, tf_obj s) {
  if(s->tag == TF_TAG_FIXNUM) return s;
  else if(s->tag == TF_TAG_NIL) return s;
  else if(s->tag == TF_TAG_PAIR) {
    tf_apply(m, tf_car(s), tf_cdr(s));
  }
  else {
    printf("unknown type, cannot eval: "); tf_print(s, 0);
  }
}

int main() {
  tf_machine _machine = {.heap = tf_nil};
  tf_machine *m = &_machine;

  tf_obj tst = tf_cons(m,
                       &tf_proc_add,
                       tf_cons(m,
                               tf_fixnum(m, 100),
                               tf_cons(m,
                                       tf_fixnum(m, 20),
                                       tf_nil)));

  tf_obj tst2 = tf_cons(m,
                        &tf_proc_add,
                        tf_cons(m,
                                tst,
                                tf_cons(m,
                                        tf_fixnum(m, 3),
                                        tf_nil)));
  tf_obj result = tf_eval(m, tst2);

  printf("input: ");
  tf_print(tst2, 0);
  printf("result: ");
  tf_print(result, 0);
  printf("\n");

  tf_machine_free(m);
  return 0;
}

/* Lama SM Bytecode interpreter */

#include "runtime/runtime_common.h"
#include "visitor.h"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

extern "C" void *Belem(void *p, int i);
extern "C" void *Bsta(void *v, int i, void *x);
extern "C" void *Bstring(void *p);
extern "C" int Llength(void *p);
extern "C" int Lread();
extern "C" void *alloc_array(int);
extern "C" int Btag(void *d, int t, int n);
extern "C" void *Lstring(void *p);
extern "C" int Bstring_patt(void *x, void *y);
extern "C" int Bclosure_tag_patt(void *x);
extern "C" int Bboxed_patt(void *x);
extern "C" int Bunboxed_patt(void *x);
extern "C" int Barray_tag_patt(void *x);
extern "C" int Bstring_tag_patt(void *x);
extern "C" int Bsexp_tag_patt(void *x);

extern "C" int Barray_patt(void *d, int n);

extern "C" size_t *__gc_stack_top, *__gc_stack_bottom;
extern "C" void __init();

using u32 = uint32_t;
using i32 = int32_t;
using u8 = uint8_t;

#define BOXED(x) (((u32)(x)) & 0x0001)

static i32 constexpr N_GLOBAL = 1000;
static i32 constexpr STACK_SIZE = 100000;

// stored on the stack (see std::array)
template <typename T, bool Check> struct stack {
  std::array<T, STACK_SIZE> data; // zero-initialized on the stack
  size_t *stack_begin = nullptr;
  // size_t* stack_pointer = nullptr; replaced by __gc_stack_top
  size_t *base_pointer = nullptr;
  u32 n_args = 2; // default

  stack() {
    __gc_stack_bottom = (size_t *)(data.data() + STACK_SIZE);
    stack_begin = __gc_stack_bottom - N_GLOBAL;
    base_pointer = stack_begin;
    __gc_stack_top = stack_begin;
  }

  void push(T value) {
    if constexpr (Check) {
      if ((void *)data.data() >= (void *)__gc_stack_top) {
        error("error: stack overflow");
      }
    }

    *(__gc_stack_top--) = value;
  }

  T pop() {
    if constexpr (Check) {
      if (__gc_stack_top == stack_begin) {
        error("negative stack\n");
      }
    }
    return *(++__gc_stack_top);
  }
  T top() { return *(__gc_stack_top + 1); }

  void print_ptrs() {
    fprintf(stderr, "rbp=%d rsp=%d\n", __gc_stack_bottom - base_pointer,
            __gc_stack_bottom - __gc_stack_top);
  }

  void print_content() { print_ptrs(); }
};

template <bool Check>
static inline void *myBarray(int n, stack<u32, Check> &ops_stack) {
  data *r = (data *)alloc_array(n);
  for (i32 i = n - 1; i >= 0; --i) {
    i32 elem = ops_stack.pop();
    ((int *)r->contents)[i] = elem;
  }
  return r->contents;
}

extern "C" void *alloc_sexp(int members);
extern "C" int LtagHash(char *);

template <bool Check>
static inline void *myBsexp(int n, stack<u32, Check> &ops_stack, char const *name) {
  int i;
  int ai;
  data *r;
  int fields_cnt = n;
  r = (data *)alloc_sexp(fields_cnt);
  ((sexp *)r)->tag = 0;

  for (i = n; i > 0; --i) {
    ai = ops_stack.pop();
    ((int *)r->contents)[i] = ai;
  }

  ((sexp *)r)->tag =
      UNBOX(LtagHash((char *)name)); // cast for runtime compatibility

  return (int *)r->contents;
}

extern "C" void *alloc_closure(int);

template <bool Check>
static inline void *myBclosure(int n, stack<u32, Check> &ops_stack, void *addr) {
  int i, ai;
  data *r;
  r = (data *)alloc_closure(n + 1);
  ((void **)r->contents)[0] = addr;

  for (i = n; i >= 1; --i) {
    ai = ops_stack.pop();
    ((int *)r->contents)[i] = ai;
  }

  return r->contents;
}

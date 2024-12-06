#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using u8 = std::uint8_t;

/* The unpacked representation of bytecode file */
struct __attribute__((packed)) bytefile {
  u8 *stringtab_ptr; /* A pointer to the beginning of the string table */
  u8 *last_stringtab_zero;
  int *public_ptr; /* A pointer to the beginning of publics table    */
  u8 *code_ptr;    /* A pointer to the bytecode itself               */
  u8 *code_end;
  int stringtab_size;   /* The size (in bytes) of the string table        */
  int global_area_size; /* The size (in words) of global area             */
  int public_symbols_number; /* The number of public symbols */
  u8 buffer[0];
};

static inline void error(char const *format, ...) {
  fprintf(stderr, "error: ");
  va_list argptr;
  va_start(argptr, format);
  vfprintf(stderr, format, argptr);
  fprintf(stderr, "\n");
  exit(-1);
  va_end(argptr);
}

bytefile const *read_file(char *fname);
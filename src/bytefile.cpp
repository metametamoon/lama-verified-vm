#include "bytefile.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>



/* Reads a binary bytecode file by name and unpacks it */
bytefile *read_file(char *fname) {
  FILE *f = fopen(fname, "rb");
  size_t size;
  bytefile *file;

  if (f == nullptr) {
    error("%s\n", strerror(errno));
  }

  if (fseek(f, 0, SEEK_END) == -1) {
    error("%s\n", strerror(errno));
  }
  size = ftell(f);

  file = (bytefile *)malloc(sizeof(int) * 4 + size + 100);

  if (file == nullptr) {
    error("*** FAILURE: unable to allocate memory.\n");
  }

  rewind(f);

  if (size != fread(&file->stringtab_size, 1, size, f)) {
    error("%s\n", strerror(errno));
  }

  if (file->public_symbols_number < 0) {
    error("unreasonable number of public symbols (an error?): %d\n",
          file->public_symbols_number);
  }
  if (file->stringtab_size < 0) {
    error("unreasonable size of stringtab (an error?): %d\n",
          file->public_symbols_number);
  }
  if (file->global_area_size < 0) {
    error("unreasonable size of global aread (an error?): %d\n",
          file->public_symbols_number);
  }

  fclose(f);

  file->stringtab_ptr =
      &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
  file->public_ptr = (int *)file->buffer;
  file->code_ptr = &file->stringtab_ptr[file->stringtab_size];
  file->code_end = (u8 *)&file->stringtab_size + size;
  for (file->last_stringtab_zero = file->code_ptr - 1;
       file->last_stringtab_zero > file->stringtab_ptr;
       --file->last_stringtab_zero) {
    if (*file->last_stringtab_zero == 0)
      break;
  }
  return file;
}
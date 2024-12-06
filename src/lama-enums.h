#pragma once

#include <cstdint>

using u8 = std::uint8_t;

enum class BinopLabel {
  ADD = 0,
  SUB = 1,
  MUL = 2,
  DIV = 3,
  MOD = 4,
  LT = 5,
  LEQ = 6,
  GT = 7,
  GEQ = 8,
  EQ = 9,
  NEQ = 10,
  AND = 11,
  OR = 12,
  BINOP_LAST
};

// doing my best to not clah with macros
enum class Patt {
  STR_EQ_TAG = 0,
  STR_TAG = 1,
  ARR_TAG = 2,
  SEXPR_TAG = 3,
  BOXED = 4,
  UNBOXED = 5,
  CLOS_TAG = 6,
  LAST
};

enum class HCode : u8 {
  BINOP = 0,
  MISC1 = 1,
  LD = 2,
  LDA = 3,
  ST = 4,
  MISC2 = 5,
  PATT = 6,
  CALL = 7,
  STOP = 15,
};

enum class Misc1LCode : u8 {
  CONST = 0,
  STR = 1,
  SEXP = 2,
  STI = 3,
  STA = 4,
  JMP = 5,
  END = 6,
  RET = 7,
  DROP = 8,
  DUP = 9,
  SWAP = 10,
  ELEM = 11,
};

enum class Misc2LCode : u8 {
  CJMPZ = 0,
  CJMPNZ = 1,
  BEGIN = 2,
  CBEGIN = 3,
  CLOSURE = 4,
  CALLC = 5,
  CALL = 6,
  TAG = 7,
  ARRAY = 8,
  FAILURE = 9,
  LINE = 10,
  ELEM = 11,
};

enum class Call : u8 {
  READ = 0,
  WRITE = 1,
  LLENGTH = 2,
  LSTRING = 3,
  BARRAY = 4,
};
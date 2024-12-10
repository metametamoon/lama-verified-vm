#pragma once

#include "bytefile.h"
#include "lama-enums.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using i32 = std::int32_t;
using u32 = std::uint32_t;
using u8 = std::uint8_t;

u32 const GLOBAL = 1;
u32 const LOCAL = 2;
u32 const ARG = 3;
u32 const CAPTURED = 4;

template <typename T> class Visitor {
public:
  virtual ~Visitor() = default;
  virtual T visit_binop(u8 *decode_next_ip, u8 index) = 0;
  virtual T visit_const(u8 *decode_next_ip, i32 constant) = 0;
  virtual T visit_str(u8 *decode_next_ip, char const *) = 0;
  virtual T visit_sexp(u8 *decode_next_ip, char const *tag, i32 args) = 0;
  virtual T visit_sti(u8 *decode_next_ip) = 0;
  virtual T visit_sta(u8 *decode_next_ip) = 0;
  virtual T visit_jmp(u8 *decode_next_ip, i32 jump_location) = 0;
  virtual T visit_end_ret(u8 *decode_next_ip) = 0;
  virtual T visit_drop(u8 *decode_next_ip) = 0;
  virtual T visit_dup(u8 *decode_next_ip) = 0;
  virtual T visit_swap(u8 *decode_next_ip) = 0;
  virtual T visit_elem(u8 *decode_next_ip) = 0;
  virtual T visit_ld(u8 *decode_next_ip, u8 arg_kind, i32 index) = 0;
  virtual T visit_lda(u8 *decode_next_ip, u8 arg_kind, i32 index) = 0;
  virtual T visit_st(u8 *decode_next_ip, u8 arg_kind, i32 index) = 0;
  virtual T visit_cjmp(u8 *decode_next_ip, u8 is_negated,
                       i32 jump_location) = 0;
  virtual T visit_begin(u8 *decode_next_ip, u8 is_closure_begin, i32 n_args,
                        i32 n_locals) = 0;
  virtual T visit_closure(u8 *decode_next_ip, i32 addr, i32 n,
                          u8 *args_begin) = 0;
  virtual T visit_call_closure(u8 *decode_next_ip, i32 n_arg) = 0;
  virtual T visit_call(u8 *decode_next_ip, i32 loc, i32 n_arg) = 0;
  virtual T visit_tag(u8 *decode_next_ip, char const *name, i32 n_arg) = 0;
  virtual T visit_array(u8 *decode_next_ip, i32 size) = 0;
  virtual T visit_fail(u8 *decode_next_ip, i32 arg1, i32 arg2) = 0;
  virtual T visit_line(u8 *decode_next_ip, i32 line_number) = 0;
  virtual T visit_patt(u8 *decode_next_ip, u8 patt_kind) = 0;
  virtual T visit_call_lread(u8 *decode_next_ip) = 0;
  virtual T visit_call_lwrite(u8 *decode_next_ip) = 0;
  virtual T visit_call_llength(u8 *decode_next_ip) = 0;
  virtual T visit_call_lstring(u8 *decode_next_ip) = 0;
  virtual T visit_call_barray(u8 *decode_next_ip, i32 arg) = 0;
  virtual T visit_stop(u8 *decode_next_ip) = 0;
};

template <typename T> struct VisitResult {
  u8 *next_ip;
  T value;
};

static inline bool check_address(bytefile const *bf, u8 *addr) {
  return (bf->code_ptr <= addr) && (addr < bf->code_end);
}

bool static inline check_is_begin(bytefile const *bf, u8 *ip) {
  if (!check_address(bf, ip)) {
    return false;
  }
  u8 x = *ip;
  u8 h = (x & 0xF0) >> 4, l = x & 0x0F;
  return h == 5 && (l == 3 || l == 2);
}

template <typename T, bool BytecodeCheck = true>
static inline VisitResult<T> visit_instruction(bytefile const *bf, u8 *ip,
                                               Visitor<T> &visitor) {
#define FAIL                                                                   \
  {                                                                            \
    printf("ERROR: invalid opcode %d-%d\n", h, l);                             \
    exit(0);                                                                   \
  }
#define RET(x) return VisitResult<T>{ip, x};
  auto read_int = [&ip, &bf]() {
    if constexpr (BytecodeCheck) {
      check_address(bf, ip);
    }
    ip += sizeof(int);
    return *(int *)(ip - sizeof(int));
  };

  auto read_byte = [&ip, &bf]() {
    if constexpr (BytecodeCheck) {
      check_address(bf, ip);
    }
    return *ip++;
  };

  auto read_string = [&read_int, &ip, &bf]() {
    if constexpr (BytecodeCheck) {
      check_address(bf, ip);
    }
    return get_string(bf, read_int());
  };

  if (ip >= bf->code_end) {
    error("execution unexpectedly got out of code section\n");
  }
  u8 x = read_byte(), h = (x & 0xF0) >> 4, l = x & 0x0F;
  //   debug(stderr, "0x%.8x:\t", unsigned(ip - bf->code_ptr - 1));
  switch ((HCode)h) {
  case HCode::STOP: {
    RET(visitor.visit_stop(ip));
  }

  case HCode::BINOP: {
    RET(visitor.visit_binop(ip, l - 1));
    break;
  }

  case HCode::MISC1: {
    switch ((Misc1LCode)l) {
    case Misc1LCode::CONST: {
      auto arg = read_int();
      RET(visitor.visit_const(ip, arg));
      break;
    }

    case Misc1LCode::STR: {
      char const *string = read_string();
      RET(visitor.visit_str(ip, string));
      break;
    }

    case Misc1LCode::SEXP: {
      char const *tag = read_string();
      int n = read_int();
      RET(visitor.visit_sexp(ip, tag, n));
      break;
    }

    case Misc1LCode::STI: {
      RET(visitor.visit_sti(ip));
      break;
    }

    case Misc1LCode::STA: {
      RET(visitor.visit_sta(ip));
      break;
    }

    case Misc1LCode::JMP: {
      auto jump_location = read_int();
      RET(visitor.visit_jmp(ip, jump_location));
      break;
    }

    case Misc1LCode::END:
    case Misc1LCode::RET: {
      if (h == 7) {
      } else {
      }
      RET(visitor.visit_end_ret(ip));
      break;
    }

    case Misc1LCode::DROP:
      RET(visitor.visit_drop(ip));
      break;

    case Misc1LCode::DUP: {
      RET(visitor.visit_dup(ip));
      break;
    }

    case Misc1LCode::SWAP: {
      RET(visitor.visit_swap(ip));
      break;
    }

    case Misc1LCode::ELEM: {
      RET(visitor.visit_elem(ip));
      break;
    }

    default:
      FAIL;
    }
    break;
  }
  case HCode::LD: { // LD
    i32 const index = read_int();
    u32 kind = l + 1;
    RET(visitor.visit_ld(ip, kind, index));
    break;
  }
  case HCode::LDA: {
    i32 index = read_int();
    u32 kind = l + 1;
    RET(visitor.visit_lda(ip, kind, index));
    break;
  }
  case HCode::ST: { // ST
    i32 index = read_int();
    u32 kind = l + 1;
    RET(visitor.visit_st(ip, kind, index));
    break;
  }

  case HCode::MISC2: {
    switch ((Misc2LCode)l) {
    case Misc2LCode::CJMPZ: {
      auto jump_location = read_int();
      RET(visitor.visit_cjmp(ip, false, jump_location));
      break;
    }

    case Misc2LCode::CJMPNZ: {
      auto jump_location = read_int();
      RET(visitor.visit_cjmp(ip, true, jump_location));
      break;
    }

    case Misc2LCode::BEGIN:
    case Misc2LCode::CBEGIN: {
      int n_args = read_int();
      int n_locals = read_int();
      if (l == 3) {
      }
      RET(visitor.visit_begin(ip, l == 3, n_args, n_locals));
      break;
    }

    case Misc2LCode::CLOSURE: {
      int addr = read_int();
      int n = read_int();
      u8 *arg_begin = ip;
      ip += (sizeof(u8) + sizeof(i32)) * n;
      RET(visitor.visit_closure(ip, addr, n, arg_begin));
      break;
    };

    case Misc2LCode::CALLC: {
      int n_arg = read_int();
      RET(visitor.visit_call_closure(ip, n_arg));
      break;
    }

    case Misc2LCode::CALL: {
      int loc = read_int();
      int n_arg = read_int();
      RET(visitor.visit_call(ip, loc, n_arg));
      break;
    }

    case Misc2LCode::TAG: {
      const char *name = read_string();
      int n = read_int();
      RET(visitor.visit_tag(ip, name, n));
      break;
    }

    case Misc2LCode::ARRAY: {
      int size = read_int();
      RET(visitor.visit_array(ip, size));
      break;
    }

    case Misc2LCode::FAILURE: {
      i32 arg1 = read_int();
      i32 arg2 = read_int();
      RET(visitor.visit_fail(ip, arg1, arg2));
      break;
    }

    case Misc2LCode::LINE: {
      int line = read_int();
      RET(visitor.visit_line(ip, line));
      break;
    }
    default:
      FAIL;
    }
    break;
  }
  case HCode::PATT: {
    RET(visitor.visit_patt(ip, l));
    break;
  }
  case HCode::CALL: {
    switch ((Call)l) {
    case Call::READ: {
      RET(visitor.visit_call_lread(ip));
      break;
    }

    case Call::WRITE: {
      RET(visitor.visit_call_lwrite(ip));
      break;
    }
    case Call::LLENGTH: {
      RET(visitor.visit_call_llength(ip));
      break;
    }

    case Call::LSTRING: {
      RET(visitor.visit_call_lstring(ip));
      break;
    }

    case Call::BARRAY: {
      i32 n = read_int();
      RET(visitor.visit_call_barray(ip, n));
      break;
    }
    default:
      FAIL;
    }
  } break;

  default:
    FAIL;
  }
}
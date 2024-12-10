#pragma once

#include "lama-enums.h"
#include "runtime-decl.h"
#include "visitor.h"
#include <cstdint>
#include <optional>
#include <string>

enum class InstructionKind : u8 { CALL, JMP, CJMP, END, FAIL_KIND, OTHER };

using i8 = std::int8_t;

struct DiagnosticInformation {
  i8 depth_change;
  std::optional<std::string> error = std::nullopt;
  std::optional<i32> jump_address = 0;
  InstructionKind kind = InstructionKind::OTHER;
};

class DiagnosticVisitor final : public Visitor<DiagnosticInformation> {
public:
  ~DiagnosticVisitor() = default;
  DiagnosticVisitor(bytefile const *bf) : bf(bf) {}
  bytefile const *bf;
  DiagnosticInformation visit_binop(u8 *decode_next_ip, u8 index) {
    // auto error = index;
    std::optional<std::string> error = std::nullopt;
    if (index >= (u8)BinopLabel::BINOP_LAST) {
      error = "Unsupported binop kind";
    }
    return DiagnosticInformation{-1, error};
  }
  DiagnosticInformation visit_const(u8 *decode_next_ip, i32 constant) {
    return DiagnosticInformation{1};
  }
  DiagnosticInformation visit_str(u8 *decode_next_ip, char const *) {
    return DiagnosticInformation{1};
  }
  DiagnosticInformation visit_sexp(u8 *decode_next_ip, char const *tag,
                                   i32 args) {
    return DiagnosticInformation{(i8)(1 - args)};
  }
  DiagnosticInformation visit_sti(u8 *decode_next_ip) {
    return DiagnosticInformation{-1};
  }
  DiagnosticInformation visit_sta(u8 *decode_next_ip) {
    return DiagnosticInformation{-2};
  }
  DiagnosticInformation visit_jmp(u8 *decode_next_ip, i32 jump_location) {
    std::optional<std::string> error = std::nullopt;
    u8 *exec_next_ip = bf->code_ptr + jump_location;
    if (!check_address(bf, exec_next_ip)) {
      error = ("trying to jump out of the code area");
    }
    return DiagnosticInformation{0, error, jump_location, InstructionKind::JMP};
  }
  DiagnosticInformation visit_end_ret(u8 *decode_next_ip) {
    return DiagnosticInformation{0, std::nullopt, std::nullopt,
                                 InstructionKind::END};
  }
  DiagnosticInformation visit_drop(u8 *decode_next_ip) {
    return DiagnosticInformation{-1};
  }
  DiagnosticInformation visit_dup(u8 *decode_next_ip) {
    return DiagnosticInformation{1};
  }
  DiagnosticInformation visit_swap(u8 *decode_next_ip) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_elem(u8 *decode_next_ip) {
    return DiagnosticInformation{-1};
  }
  DiagnosticInformation visit_ld(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    auto error = arg_kind > CAPTURED ? std::optional{"unsupported arg kind"}
                                     : std::nullopt;
    if (arg_kind == GLOBAL && index > N_GLOBAL) {
      error = "querying out of bounds global";
    }

    return DiagnosticInformation{1, error};
  }
  DiagnosticInformation visit_lda(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    auto error = arg_kind > CAPTURED ? std::optional{"unsupported arg kind"}
                                     : std::nullopt;
    if (arg_kind == GLOBAL && index > N_GLOBAL) {
      error = "querying out of bounds global";
    }

    return DiagnosticInformation{2, error};
  }
  DiagnosticInformation visit_st(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    auto error = arg_kind > CAPTURED ? std::optional{"unsupported arg kind"}
                                     : std::nullopt;
    if (arg_kind == GLOBAL && index > N_GLOBAL) {
      error = "querying out of bounds global";
    }
    return DiagnosticInformation{0, error};
  }
  DiagnosticInformation visit_cjmp(u8 *decode_next_ip, u8 is_nega,
                                   i32 jump_location) {
    std::optional<std::string> error = std::nullopt;
    u8 *exec_next_ip = bf->code_ptr + jump_location;
    if (!check_address(bf, exec_next_ip)) {
      error = ("trying to jump out of the code area");
    }
    return DiagnosticInformation{-1, error, jump_location,
                                 InstructionKind::CJMP};
  }
  DiagnosticInformation visit_begin(u8 *decode_next_ip, u8 is_closure_begin,
                                    i32 n_a, i32 n_locals) {
    return DiagnosticInformation{0};
  }

  DiagnosticInformation visit_closure(u8 *decode_next_ip, i32 addr, i32 n,
                                      u8 *args_begin) {
    std::optional<std::string> error = std::nullopt;
    if (addr < 0 || addr > (bf->code_end - bf->code_ptr)) {
      error = "closure points outside of the code area";
    }
    if (!check_is_begin(bf, bf->code_ptr + addr)) {
      error = "closure does not point at begin\n";
    }
    return DiagnosticInformation{1, error};
  }

  DiagnosticInformation visit_call_closure(u8 *decode_next_ip, i32 n_arg) {
    return DiagnosticInformation{(i8)(-n_arg + 1 - 1)};
  }

  DiagnosticInformation visit_call(u8 *decode_next_ip, i32 loc, i32 n_arg) {
    std::optional<std::string> error = std::nullopt;
    if (!check_is_begin(bf, bf->code_ptr + loc)) {
      error = "CALL does not call a function\n";
    }
    return DiagnosticInformation{(i8)(-n_arg + 1), error};
  }
  DiagnosticInformation visit_tag(u8 *decode_next_ip, char const *name,
                                  i32 n_arg) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_array(u8 *decode_next_ip, i32 size) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_fail(u8 *decode_next_ip, i32 arg1, i32 arg2) {
    return DiagnosticInformation{0, std::nullopt, std::nullopt,
                                 InstructionKind::FAIL_KIND};
  }
  DiagnosticInformation visit_line(u8 *decode_next_ip, i32 line_number) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_patt(u8 *decode_next_ip, u8 patt_kind) {
    i8 depth_change = patt_kind == 0 ? -1 : 0;
    return DiagnosticInformation{depth_change};
  }
  DiagnosticInformation visit_call_lread(u8 *decode_next_ip) {
    return DiagnosticInformation{1};
  }
  DiagnosticInformation visit_call_lwrite(u8 *decode_next_ip) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_call_llength(u8 *decode_next_ip) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_call_lstring(u8 *decode_next_ip) {
    return DiagnosticInformation{0};
  }
  DiagnosticInformation visit_call_barray(u8 *decode_next_ip, i32 arg) {
    return DiagnosticInformation{(i8)(1 - arg)};
  }
  DiagnosticInformation visit_stop(u8 *decode_next_ip) {
    return DiagnosticInformation{0};
  }
};
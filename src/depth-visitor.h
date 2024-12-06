#pragma once

#include "visitor.h"
#include <cstdint>
#include <optional>

enum class InstructionKind : u8 { CALL, JMP, CJMP, END, OTHER };

using i8 = std::int8_t;

struct DepthInformation {
  i8 depth_change;
  std::optional<i32> jump_address = 0;
  InstructionKind kind = InstructionKind::OTHER;
};

class DepthVisitor final : public Visitor<DepthInformation> {
public:
  ~DepthVisitor() = default;
  DepthInformation visit_binop(u8 *decode_next_ip, u8 index) {
    return DepthInformation{-1};
  }
  DepthInformation visit_const(u8 *decode_next_ip, i32 constant) {
    return DepthInformation{1};
  }
  DepthInformation visit_str(u8 *decode_next_ip, char const *) {
    return DepthInformation{1};
  }
  DepthInformation visit_sexp(u8 *decode_next_ip, char const *tag, i32 args) {
    return DepthInformation{1};
  }
  DepthInformation visit_sti(u8 *decode_next_ip) {
    return DepthInformation{-1};
  }
  DepthInformation visit_sta(u8 *decode_next_ip) {
    return DepthInformation{-2};
  }
  DepthInformation visit_jmp(u8 *decode_next_ip, i32 jump_location) {
    return DepthInformation{0, jump_location, InstructionKind::JMP};
  }
  DepthInformation visit_end_ret(u8 *decode_next_ip) {
    return DepthInformation{0, std::nullopt, InstructionKind::END};
  }
  DepthInformation visit_drop(u8 *decode_next_ip) {
    return DepthInformation{-1};
  }
  DepthInformation visit_dup(u8 *decode_next_ip) { return DepthInformation{1}; }
  DepthInformation visit_swap(u8 *decode_next_ip) {
    return DepthInformation{0};
  }
  DepthInformation visit_elem(u8 *decode_next_ip) {
    return DepthInformation{-1};
  }
  DepthInformation visit_ld(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    return DepthInformation{1};
  }
  DepthInformation visit_lda(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    return DepthInformation{2};
  }
  DepthInformation visit_st(u8 *decode_next_ip, u8 arg_kind, i32 index) {
    return DepthInformation{0};
  }
  DepthInformation visit_cjmp(u8 *decode_next_ip, u8 is_nega,
                              i32 jump_location) {
    return DepthInformation{0, jump_location, InstructionKind::CJMP};
  }
  DepthInformation visit_begin(u8 *decode_next_ip, u8 is_closure_begin, i32 n_a,
                               i32 n_locals) {
    return DepthInformation{0};
  }

  DepthInformation visit_closure(u8 *decode_next_ip, i32 addr, i32 n,
                                 u8 *args_begin) {
    return DepthInformation{1};
  }

  DepthInformation visit_call_closure(u8 *decode_next_ip, i32 n_arg) {
    return DepthInformation{(i8)(-n_arg + 1)};
  }

  DepthInformation visit_call(u8 *decode_next_ip, i32 loc, i32 n_arg) {
    return DepthInformation{(i8)(-n_arg + 1), loc, InstructionKind::CALL};
  }
  DepthInformation visit_tag(u8 *decode_next_ip, char const *name, i32 n_arg) {
    return DepthInformation{1};
  }
  DepthInformation visit_array(u8 *decode_next_ip, i32 size) {
    return DepthInformation{0};
  }
  DepthInformation visit_fail(u8 *decode_next_ip, i32 arg1, i32 arg2) {
    return DepthInformation{0};
  }
  DepthInformation visit_line(u8 *decode_next_ip, i32 line_number) {
    return DepthInformation{0};
  }
  DepthInformation visit_patt(u8 *decode_next_ip, u8 patt_kind) {
    i8 depth_change = patt_kind == 0 ? -1 : 0;
    return DepthInformation{depth_change};
  }
  DepthInformation visit_call_lread(u8 *decode_next_ip) {
    return DepthInformation{1};
  }
  DepthInformation visit_call_lwrite(u8 *decode_next_ip) {
    return DepthInformation{0};
  }
  DepthInformation visit_call_llength(u8 *decode_next_ip) {
    return DepthInformation{0};
  }
  DepthInformation visit_call_lstring(u8 *decode_next_ip) {
    return DepthInformation{0};
  }
  DepthInformation visit_call_barray(u8 *decode_next_ip, i32 arg) {
    return DepthInformation{1};
  }
  DepthInformation visit_stop(u8 *decode_next_ip) {
    return DepthInformation{0};
  }
};
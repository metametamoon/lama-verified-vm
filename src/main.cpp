#include "bytefile.h"
#include "diagnostic-visitor.h"
#include "executing-visitor.h"
#include "lama-enums.h"
#include "visitor.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <stdio.h>
#include <string>
#include <unordered_set>

using u32 = uint32_t;
using i32 = int32_t;
using u8 = std::uint8_t;

#define BOXED(x) (((u32)(x)) & 0x0001)

/* Gets a string from a string table by an index */
static inline char const *get_string(bytefile const *f, int pos) {
  // validate its is an ok string
  char *ptr = (char *)&f->stringtab_ptr[pos];
  if (ptr > (char *)f->last_stringtab_zero) {
    fprintf(stderr, "Bad string read at offset %d (string did not terminate)\n",
            pos);
    exit(-1);
  }
  return ptr;
}

/* Gets an offset for a publie symbol */
static inline int get_public_offset(bytefile const *f, int i) {
  if (!(i < f->public_symbols_number)) {
    fprintf(stderr, "Trying to read out of bounds public member at %d", i);
    exit(-1);
  }
  auto offset = f->public_ptr[i * 2 + 1];
  if (offset >= f->code_end - f->code_ptr) {
    error("public symbol at index %d points outside code area\n", i);
  }
  return f->public_ptr[i * 2 + 1];
}

static inline void print_location(bytefile const *bf, u8 *next_ip) {
  fprintf(stderr, "at 0x%.8x:\n", unsigned((next_ip - 4) - bf->code_ptr - 1));
}

static inline void located_error(bytefile const *bf, u8 *next_ip,
                                 const char *format, ...) {
  fprintf(stderr, "error\n");
  print_location(bf, next_ip);
  va_list argptr;
  va_start(argptr, format);
  vfprintf(stderr, format, argptr);
  fprintf(stderr, "\n");
  exit(-1);
  va_end(argptr);
}

// by convention if no jump might be possible, it is in next_ip
// if jump is possible, it is in alternative_next_ip
struct InstructionResult {
  u8 *next_ip; // always not null, needed for proper traversing the program
  bool is_next_child;
  u8 *jump_ip;
  std::string decoded;
  bool is_end; // END or RET
};

static inline InstructionResult run_instruction(u8 *ip, bytefile const *bf,
                                                bool print_inst) {
#define FAIL                                                                   \
  {                                                                            \
    printf("ERROR: invalid opcode %d-%d\n", h, l);                             \
    exit(0);                                                                   \
  }

  auto read_int = [&ip, &bf]() {
    check_address(bf, ip);
    ip += sizeof(int);
    return *(int *)(ip - sizeof(int));
  };

  auto read_byte = [&ip, &bf]() {
    check_address(bf, ip);
    return *ip++;
  };

  auto read_string = [&read_int, &ip, &bf]() {
    check_address(bf, ip);
    return get_string(bf, read_int());
  };

  static char const *const ops[] = {
      "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static char const *const pats[] = {"=str", "#string", "#array", "#sexp",
                                     "#ref", "#val",    "#fun"};
  static char const *const lds[] = {"LD", "LDA", "ST"};
  // bool in_closure = false;
  auto check_is_begin = [](bytefile const *bf, u8 *ip) {
    if (!check_address(bf, ip)) {
      return false;
    }
    u8 x = *ip;
    u8 h = (x & 0xF0) >> 4, l = x & 0x0F;
    return h == 5 && (l == 3 || l == 2);
  };
  char buff[100];
  int offset = 0;

  if (ip >= bf->code_end) {
    error("execution unexpectedly got out of code section\n");
  }
  u8 x = read_byte(), h = (x & 0xF0) >> 4, l = x & 0x0F;
  switch ((HCode)h) {
  case HCode::STOP: {
    debug(stderr, "<end>\n");
    return InstructionResult{ip, false, nullptr, "<end>", true};
  }

  case HCode::BINOP: {
    offset += sprintf(buff + offset, "BINOP\t%s", ops[l - 1]);
    if (l - 1 < (i32)BinopLabel::BINOP_LAST) {
    } else {
      FAIL;
    }
    break;
  }

  case HCode::MISC1: {
    switch ((Misc1LCode)l) {
    case Misc1LCode::CONST: {
      auto arg = read_int();
      offset += sprintf(buff + offset, "CONST\t%d", arg);
      break;
    }

    case Misc1LCode::STR: {
      char const *string = read_string();
      offset += sprintf(buff + offset, "STRING\t%s", string);
      break;
    }

    case Misc1LCode::SEXP: {
      char const *tag = read_string();
      int n = read_int();
      offset += sprintf(buff + offset, "SEXP\t%s ", tag);
      offset += sprintf(buff + offset, "%d", n);
      break;
    }

    case Misc1LCode::STI: {
      offset += sprintf(buff + offset, "STI");
      break;
    }

    case Misc1LCode::STA: {
      offset += sprintf(buff + offset, "STA");
      break;
    }

    case Misc1LCode::JMP: {
      auto jump_location = read_int();
      offset += sprintf(buff + offset, "JMP\t0x%.8x", jump_location);
      u8 *jump_ip = bf->code_ptr + jump_location;
      if (!check_address(bf, jump_ip)) {
        print_location(bf, ip);
        fprintf(stderr, "trying to jump out of the code area to offset %d",
                jump_ip - bf->code_ptr);
      }
      return InstructionResult{ip, false, jump_ip, std::string{buff}, false};
      break;
    }

    case Misc1LCode::END:
    case Misc1LCode::RET: {
      if (h == 7) {
        offset += sprintf(buff + offset, "RET");
      } else {
        offset += sprintf(buff + offset, "END");
      }
      return InstructionResult{
          ip, false, nullptr, std::string{buff}, true,
      };
      break;
    }

    case Misc1LCode::DROP: {
      offset += sprintf(buff + offset, "DROP");
      break;
    }

    case Misc1LCode::DUP: {
      offset += sprintf(buff + offset, "DUP");
      break;
    }

    case Misc1LCode::SWAP: {
      offset += sprintf(buff + offset, "SWAP");
      break;
    }

    case Misc1LCode::ELEM: {
      offset += sprintf(buff + offset, "ELEM");
      break;
    }

    default:
      FAIL;
    }
    break;
  }
  case HCode::LD:
  case HCode::LDA:
  case HCode::ST: {
    offset += sprintf(buff + offset, "%s\t", lds[h - 2]);
    i32 const index = read_int();
    switch (l) {
    case 0:
      offset += sprintf(buff + offset, "G(%d)", index);
      break;
    case 1:
      offset += sprintf(buff + offset, "L(%d)", index);
      break;
    case 2:
      offset += sprintf(buff + offset, "A(%d)", index);
      break;
    case 3:
      offset += sprintf(buff + offset, "C(%d)", index);
      break;
    default:
      FAIL;
    }
    break;
  }

  case HCode::MISC2: {
    switch ((Misc2LCode)l) {
    case Misc2LCode::CJMPZ: {
      auto jump_location = read_int();
      offset += sprintf(buff + offset, "CJMPz\t0x%.8x", jump_location);
      auto possible_next = ip;

      u8 *jump_ip = bf->code_ptr + jump_location;
      if (!check_address(bf, jump_ip)) {
        print_location(bf, ip);
        fprintf(stderr, "trying to jump out of the code area to offset %d",
                jump_ip - bf->code_ptr);
      }
      auto result = InstructionResult{possible_next, true, jump_ip,
                                      std::string{buff}, false};
      return result;
      break;
    }

    case Misc2LCode::CJMPNZ: {
      auto jump_location = read_int();
      offset += sprintf(buff + offset, "CJMPz\t0x%.8x", jump_location);
      u8 *jump_ip = bf->code_ptr + jump_location;
      if (!check_address(bf, jump_ip)) {
        print_location(bf, ip);
        fprintf(stderr, "trying to jump out of the code area to offset %d",
                jump_ip - bf->code_ptr);
      }
      auto result =
          InstructionResult{ip, true, jump_ip, std::string{buff}, false};
      return result;
      break;
    }

    case Misc2LCode::BEGIN:
    case Misc2LCode::CBEGIN: {
      int n_args = read_int();
      int n_locals = read_int();
      if (l == 3) {
        offset += sprintf(buff + offset, "C");
      }
      offset += sprintf(buff + offset, "BEGIN\t%d ", n_args);
      offset += sprintf(buff + offset, "%d", n_locals);
      break;
    }

    case Misc2LCode::CLOSURE: {
      int addr = read_int();
      offset += sprintf(buff + offset, "CLOSURE\t0x%.8x", addr);

      if (addr < 0 || addr > (bf->code_end - bf->code_ptr)) {
        located_error(bf, ip, "closure points outside of the code area\n");
      }
      if (!check_is_begin(bf, bf->code_ptr + addr)) {
        located_error(bf, ip, "closure does not point at begin\n");
      }
      int n = read_int();
      for (int i = 0; i < n; i++) {
        switch (read_byte()) {
        case 0: {
          int index = read_int();
          offset += sprintf(buff + offset, "G(%d)", index);
          break;
        }
        case 1: {
          int index = read_int();
          offset += sprintf(buff + offset, "L(%d)", index);
          break;
        }
        case 2: {
          int index = read_int();
          offset += sprintf(buff + offset, "A(%d)", index);
          break;
        }
        case 3: {
          int index = read_int();
          offset += sprintf(buff + offset, "C(%d)", index);
          break;
        }
        default:
          FAIL;
        }
      }
      break;
    };

    case Misc2LCode::CALLC: {
      int n_arg = read_int();
      offset += sprintf(buff + offset, "CALLC\t%d", n_arg);
      break;
    }

    case Misc2LCode::CALL: {
      int loc = read_int();
      int n = read_int();
      offset += sprintf(buff + offset, "CALL\t0x%.8x ", loc);
      offset += sprintf(buff + offset, "%d", n);
      auto called_function_begin = bf->code_ptr + loc;
      if (!check_is_begin(bf, called_function_begin)) {
        located_error(bf, ip, "CALL does not call a function\n");
      }
      return InstructionResult{
          ip, true, called_function_begin, std::string{buff}, false,
      };
      break;
    }

    case Misc2LCode::TAG: {
      const char *name = read_string();
      int n = read_int();
      offset += sprintf(buff + offset, "TAG\t%s ", name);
      offset += sprintf(buff + offset, "%d", n);
      break;
    }

    case Misc2LCode::ARRAY: {
      int size = read_int();
      offset += sprintf(buff + offset, "ARRAY\t%d", size);
      break;
    }

    case Misc2LCode::FAILURE: {
      offset += sprintf(buff + offset, "FAIL\t%d", read_int());
      offset += sprintf(buff + offset, "%d", read_int());
      return InstructionResult{ip, false, nullptr, std::string{buff}, true};
      break;
    }

    case Misc2LCode::LINE: {
      int line = read_int();
      offset += sprintf(buff + offset, "LINE\t%d", line);
      break;
    }

    default:
      FAIL;
    }
    break;
  }
  case HCode::PATT: {
    offset += sprintf(buff + offset, "PATT\t%s", pats[l]);
    if (l == 0) { // =str
    } else if (l < (i32)Patt::LAST) {
    } else {
      fprintf(stderr, "Unsupported patt specializer: %d", l);
      FAIL;
    }
    break;
  }
  case HCode::CALL: {
    switch ((Call)l) {
    case Call::READ: {
      offset += sprintf(buff + offset, "CALL\tLread");
      break;
    }

    case Call::WRITE: {
      offset += sprintf(buff + offset, "CALL\tLwrite");
      break;
    }
    case Call::LLENGTH: {
      offset += sprintf(buff + offset, "CALL\tLlength");
      break;
    }

    case Call::LSTRING: {
      offset += sprintf(buff + offset, "CALL\tLstring");
      break;
    }

    case Call::BARRAY: {
      i32 n = read_int();
      offset += sprintf(buff + offset, "CALL\tBarray\t%d", n);
      break;
    }

    default:
      FAIL;
    }
  } break;

  default:
    FAIL;
  }
  std::string decoded{buff};
  if (print_inst) {
    fprintf(stderr, "%s; next=%x\n", decoded.c_str(), ip - bf->code_ptr);
  }
  return InstructionResult{ip, true, nullptr, decoded, false};
}

void gather_incoming_cf(bytefile const *bf, std::unordered_set<u8 *> &result) {
  std::vector<u8 *> instruction_stack;
  std::vector<bool> visited(bf->code_end - bf->code_ptr, false);
  auto push_if_not_visited = [&visited, &instruction_stack,
                              &bf](u8 *possible_ip) {
    auto offset = possible_ip - bf->code_ptr;
    if (!visited[offset]) {
      instruction_stack.push_back(possible_ip);
      visited[offset] = true;
    }
  };

  for (i32 i = 0; i < bf->public_symbols_number; i++) {
    u8 *public_symbol_entry_ip = bf->code_ptr + get_public_offset(bf, i);
    push_if_not_visited(public_symbol_entry_ip);
  }

  while (!instruction_stack.empty()) {
    u8 *ip = instruction_stack.back();
    instruction_stack.pop_back();
    auto const decoded = run_instruction(ip, bf, false);
    if (decoded.jump_ip != nullptr) {
      push_if_not_visited(decoded.jump_ip);
    }
    if (decoded.is_next_child && !decoded.is_end) {
      push_if_not_visited(decoded.next_ip);
    }
    if (decoded.jump_ip != nullptr) {
      result.insert(decoded.jump_ip);
    }
  }
}

struct DepthTracker {
  u8 *ip;
  u8 *function_begin;
  i32 current_depth = 0;
  i32 max_depth = 0;
};

template <bool Check = true>
void check_depth(bytefile *bf, std::unordered_set<u8 *> const &incoming_cf) {
  std::vector<DepthTracker> instruction_stack;
  for (i32 i = 0; i < bf->public_symbols_number; i++) {
    u8 *public_symbol_entry_ip = bf->code_ptr + get_public_offset(bf, i);
    instruction_stack.push_back(
        DepthTracker{public_symbol_entry_ip, public_symbol_entry_ip});
  }

  std::unordered_map<u8 *, i32> registered_depth;
  auto register_depth = [&registered_depth, &bf](u8 *ip, i32 depth) {
    if (registered_depth.count(ip) > 0) {
      auto prev_depth = registered_depth[ip];
      if (prev_depth != depth) {
        error("stack depth mismatch at %x", ip - bf->code_ptr);
      }
    } else {
      registered_depth[ip] = depth;
    }
  };

  auto depth_visitor = DiagnosticVisitor{bf};
  std::unordered_set<u8 *> visited; // for backward reaches
  std::unordered_map<u8 *, i32> max_stack;
  while (!instruction_stack.empty()) {
    auto next = instruction_stack.back();
    auto inst = run_instruction(next.ip, bf, false);
    instruction_stack.pop_back();
    auto const [decode_next_ip, diagnostic_info] =
        visit_instruction<DiagnosticInformation, Check>(bf, next.ip,
                                                        depth_visitor);
    if (diagnostic_info.required_depth > next.current_depth) {
      error("stack underflow 0x%x", next.ip - bf->code_ptr);
    }
    auto new_depth = next.current_depth + diagnostic_info.depth_change;
    if (new_depth < 0) {
      error("error: negative depth stack on the abstract executing at:\n%x",
            next.ip - bf->code_ptr);
    }
    if (incoming_cf.count(next.ip)) {
      register_depth(next.ip, next.current_depth);
    }
    switch (diagnostic_info.kind) {
    case InstructionKind::CALL: {
      auto jump_ip = bf->code_ptr + diagnostic_info.jump_address.value();
      if (visited.count(jump_ip) == 0) {
        visited.insert(jump_ip);
        instruction_stack.push_back(DepthTracker{jump_ip, jump_ip, 0, 0});
      }
      instruction_stack.push_back(
          DepthTracker{decode_next_ip, next.function_begin, new_depth,
                       std::max(next.max_depth, new_depth)});
      break;
    }
    case InstructionKind::JMP: {
      u8 *jump_ip = bf->code_ptr + diagnostic_info.jump_address.value();
      if (visited.count(jump_ip) == 0) {
        visited.insert(jump_ip);
        instruction_stack.push_back(
            DepthTracker{jump_ip, next.function_begin, new_depth,
                         std::max(next.max_depth, new_depth)});
      }
      register_depth(jump_ip, new_depth);
      break;
    }
    case InstructionKind::CJMP: {
      u8 *jump_ip = bf->code_ptr + diagnostic_info.jump_address.value();
      if (visited.count(jump_ip) == 0) {
        visited.insert(jump_ip);
        instruction_stack.push_back(
            DepthTracker{jump_ip, next.function_begin, new_depth,
                         std::max(next.max_depth, new_depth)});
      }
      register_depth(jump_ip, new_depth);

      instruction_stack.push_back(
          DepthTracker{decode_next_ip, next.function_begin, new_depth,
                       std::max(next.max_depth, new_depth)});
      break;
    }
    case InstructionKind::END: {
      max_stack[next.function_begin] =
          std::max(max_stack[next.function_begin], next.max_depth);
      break;
    }
    case InstructionKind::OTHER: {
      instruction_stack.push_back(
          DepthTracker{decode_next_ip, next.function_begin, new_depth,
                       std::max(next.max_depth, new_depth)});
      break;
    }
    case InstructionKind::FAIL_KIND: {
      break; // abort the execution here
    }
    }
  }
  for (auto [instr_begin, stacksize] : max_stack) {
    *(int *)(instr_begin + 1) = *(int *)(instr_begin + 1) + (stacksize << 16);
  }
}

template <bool Checks> static inline void myInterpret(bytefile const *bf) {
  auto interpeter = CheckingExecutingVisitor<Checks>{bf};
  auto ip = bf->code_ptr;
  while (true) {
    auto result =
        visit_instruction<ExecResult, Checks>(bf, ip, interpeter).value;
    if (result.exec_next_ip == nullptr) {
      break;
      exit(-1);
    } else {
      ip = result.exec_next_ip;
    }
  }
}

void run_with_runtime_checks(bytefile *bf, bool print_perf = false) {
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using std::chrono::high_resolution_clock;
  using std::chrono::milliseconds;
  auto before = high_resolution_clock::now();
  myInterpret<true>(bf);
  auto after = high_resolution_clock::now();
  if (print_perf) {
    auto exec_duration = duration_cast<milliseconds>(after - before);
    fprintf(stderr, "execution with checks took %fs\n",
            exec_duration.count() * 1.0 / 1000);
  }
}

void run_with_verifier_checks(bytefile *bf, bool print_perf = false) {
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using std::chrono::high_resolution_clock;
  using std::chrono::milliseconds;

  auto before = high_resolution_clock::now();
  std::unordered_set<u8 *> bytecodes_with_incoming_cf;
  gather_incoming_cf(bf, bytecodes_with_incoming_cf);
  check_depth(bf, bytecodes_with_incoming_cf);
  auto after_verification = high_resolution_clock::now();
  myInterpret<false>(bf);
  auto after_execution = high_resolution_clock::now();
  auto check_duration =
      duration_cast<milliseconds>(after_verification - before);
  auto exec_duration =
      duration_cast<milliseconds>(after_execution - after_verification);

  fprintf(stderr, "verification took %fs\n",
          check_duration.count() * 1.0 / 1000);
  fprintf(stderr, "execution without checks took %fs\n",
          exec_duration.count() * 1.0 / 1000);
}

int main(int argc, char *argv[]) {
  bytefile *bf = read_file(argv[1]);
  if (argc >= 3) {
    if (std::string{argv[2]} == "verify") {
      run_with_verifier_checks(bf, true);
    } else if (std::string{argv[2]} == "runtime") {
      run_with_runtime_checks(bf, true);
    }
  } else {
    run_with_runtime_checks(bf);
  }
  return 0;
}


struct UnsafeExecResult {
  u8 *exec_next_ip;
  std::optional<std::string> error = std::nullopt;
};

class UnsafeExecutingVisitor final : public Visitor<UnsafeExecResult> {
public:
  UnsafeExecutingVisitor(bytefile const *bf) : bf(bf) { __init(); }
  bytefile const *bf;
  bool in_closure = false;
  stack<u32, true> operands_stack = stack<u32, true>{};
  u32 create_reference(u32 index, u32 kind) {
    switch (kind) {
    case GLOBAL: {
      return (u32)(operands_stack.stack_begin + 1 + index);
    }
    case LOCAL: {
      return (u32)(operands_stack.base_pointer - 1 - index);
    }
    case ARG: {
      auto result =
          operands_stack.base_pointer + 2 + operands_stack.n_args - index;
      return (u32)result;
    }
    case CAPTURED: {
      auto closure_ptr =
          operands_stack.base_pointer + 2 + operands_stack.n_args + 1;
      u32 *closure = (u32 *)*closure_ptr;
      return (u32)&closure[1 + index];
    }
    default: {
      error("unsupported reference kind");
      return 0;
    }
    }
  }

  void write_reference(u32 reference, u32 value) {
    *((u32 *)reference) = value;
  };

  inline UnsafeExecResult visit_binop(u8 *next_ip, u8 index) override {
    debug(stderr, "BINOP\t%s\n", ops[index]);
    u32 t2 = UNBOX(operands_stack.pop());
    u32 t1 = UNBOX(operands_stack.pop());
    u32 result = (u32)arithm_op((i32)t1, (i32)t2, (BinopLabel)index);
    operands_stack.push(BOX(result));
    return UnsafeExecResult{next_ip};
  };

  inline UnsafeExecResult visit_const(u8 *decode_next_ip, i32 arg) override {
    debug(stderr, "CONST\t%d\n", arg);
    operands_stack.push(BOX(arg));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_str(u8 *decode_next_ip,
                              char const *literal) override {
    debug(stderr, "STRING\t%s", literal);
    char *obj_string = (char *)Bstring((void *)literal);
    operands_stack.push(u32(obj_string));
    return UnsafeExecResult{decode_next_ip};
  };

  inline UnsafeExecResult visit_sexp(u8 *decode_next_ip, char const *tag,
                               i32 args) override {
    debug(stderr, "SEXP\t%s %d\n", tag, args);
    auto value = myBsexp(args, operands_stack, tag);
    operands_stack.push(u32(value));
    return UnsafeExecResult{decode_next_ip};
  };

  inline UnsafeExecResult visit_sti(u8 *decode_next_ip) override {
    debug(stderr, "STI");
    u32 value = operands_stack.pop();
    u32 reference = operands_stack.pop();
    write_reference(reference, value);
    operands_stack.push(value);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_sta(u8 *decode_next_ip) override {
    debug(stderr, "STA");
    auto value = (void *)operands_stack.pop();
    auto i = (int)operands_stack.pop();
    auto x = (void *)operands_stack.pop();
    operands_stack.push((u32)Bsta(value, i, x));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_jmp(u8 *decode_next_ip, i32 jump_location) override {
    debug(stderr, "JMP\t0x%.8x\n", jump_location);
    u8 *exec_next_ip = bf->code_ptr + jump_location;
    if (!check_address(bf, exec_next_ip)) {
      return UnsafeExecResult{nullptr, "trying to jump out of the code area"};
    }
    return UnsafeExecResult{exec_next_ip};
  };
  inline UnsafeExecResult visit_end_ret(u8 *decode_next_ip) override {
    debug(stderr, "END\n");

    if (operands_stack.base_pointer != operands_stack.stack_begin - 1) {
      u32 ret_value = operands_stack.pop(); // preserve the boxing kind
      u32 top_n_args = operands_stack.n_args;
      __gc_stack_top = operands_stack.base_pointer - 1;
      operands_stack.base_pointer = (size_t *)operands_stack.pop();
      operands_stack.n_args = UNBOX(operands_stack.pop());
      u32 ret_ip = operands_stack.pop();
      __gc_stack_top += top_n_args;
      if (in_closure) {
        operands_stack.pop();
      }
      operands_stack.push(ret_value);
      in_closure = false;
      return UnsafeExecResult{(u8 *)ret_ip};
    } else {
      in_closure = false;
      return UnsafeExecResult{nullptr};
    }
    return UnsafeExecResult{nullptr};
  };
  inline UnsafeExecResult visit_drop(u8 *decode_next_ip) override {
    debug(stderr, "DROP\n");
    operands_stack.pop();
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_dup(u8 *decode_next_ip) override {
    debug(stderr, "DUP\n");
    u32 v = operands_stack.top();
    operands_stack.push(v);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_swap(u8 *decode_next_ip) override {
    debug(stderr, "SWAP\n");
    auto fst = operands_stack.pop();
    auto snd = operands_stack.pop();
    operands_stack.push(fst);
    operands_stack.push(snd);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_elem(u8 *decode_next_ip) override {
    debug(stderr, "ELEM\n");
    auto index = (int)operands_stack.pop();
    auto obj = (void *)operands_stack.pop();
    u32 elem = (u32)Belem(obj, index);
    operands_stack.push(elem);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_ld(u8 *decode_next_ip, u8 arg_kind,
                             i32 index) override {
    debug(stderr, "LD\t\n");
    auto value = *(u32 *)create_reference(index, arg_kind);
    operands_stack.push(value);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_lda(u8 *decode_next_ip, u8 arg_kind,
                              i32 index) override {
    debug(stderr, "LDA\t");
    auto ref = create_reference(index, arg_kind);
    operands_stack.push(ref);
    operands_stack.push(ref);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_st(u8 *decode_next_ip, u8 arg_kind,
                             i32 index) override {
    debug(stderr, "ST\t\n");
    auto top = operands_stack.top();
    write_reference(create_reference(index, arg_kind), top);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_cjmp(u8 *decode_next_ip, u8 is_negated,
                               i32 jump_location) override {
    if (is_negated) {
      debug(stderr, "CJMPnz\t0x%.8x\n", jump_location);
    } else {
      debug(stderr, "CJMPz\t0x%.8x\n", jump_location);
    }
    auto top = UNBOX(operands_stack.pop());
    if (((top == 0) && !is_negated) || ((top != 0) && is_negated)) {
      u8 *ip = bf->code_ptr + jump_location;
      if (!check_address(bf, ip)) {
        return UnsafeExecResult{nullptr, "trying to jump out of the code area"};
      }
      return UnsafeExecResult{ip};
    }
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_begin(u8 *decode_next_ip, u8 is_closure_begin,
                                i32 n_args, i32 n_locals) override {
    if (is_closure_begin) {
      debug(stderr, "C");
    }
    debug(stderr, "BEGIN\t%d ", n_args);
    debug(stderr, "%d\n", n_locals);
    operands_stack.push(BOX(operands_stack.n_args));
    operands_stack.push((u32)operands_stack.base_pointer);
    operands_stack.n_args = n_args;
    operands_stack.base_pointer = __gc_stack_top + 1;
    __gc_stack_top -= (n_locals + 1);
    memset((void *)__gc_stack_top, 0, (n_locals + 1) * sizeof(size_t));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_closure(u8 *decode_next_ip, i32 addr, i32 n,
                                  u8 *args_begin) override {
    auto read_int = [&args_begin]() {
      args_begin += sizeof(int);
      return *(int *)(args_begin - sizeof(int));
    };

    auto read_byte = [&args_begin]() { return *args_begin++; };

    debug(stderr, "CLOSURE\t0x%.8x", addr);
    if (addr < 0 || addr > (bf->code_end - bf->code_ptr)) {
      return UnsafeExecResult{nullptr, "closure points outside of the code "
                                 "area"};
    }
    if (!check_is_begin(bf, bf->code_ptr + addr)) {
      return UnsafeExecResult{nullptr, "closure does not point at begin\n"};
    }
    for (int i = 0; i < n; i++) {
      switch (read_byte()) {
      case 0: {
        int index = read_int();
        debug(stderr, "G(%d)", index);
        operands_stack.push((u32) * (u32 *)create_reference(index, GLOBAL));
        break;
      }
      case 1: {
        int index = read_int();
        debug(stderr, "L(%d)", index);
        operands_stack.push((u32) * (u32 *)create_reference(index, LOCAL));
        break;
      }
      case 2: {
        int index = read_int();
        debug(stderr, "A(%d)", index);
        operands_stack.push((u32)(*((u32 *)create_reference(index, ARG))));
        break;
      }
      case 3: {
        int index = read_int();
        debug(stderr, "C(%d)", index);
        operands_stack.push((u32)(*((u32 *)create_reference(index, CAPTURED))));
        break;
      }
      default: {
        return UnsafeExecResult{nullptr, "unsupported argument kind in closure"};
      }
      }
    }
    u32 v = (u32)myBclosure(n, operands_stack, (void *)addr);
    operands_stack.push(v);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_closure(u8 *decode_next_ip, i32 n_arg) override {
    debug(stderr, "CALLC\t%d", n_arg);
    u32 closure = *(__gc_stack_top + 1 + n_arg);
    u32 addr = (u32)(((i32 *)closure)[0]);
    operands_stack.push(u32(decode_next_ip));
    u8 *exec_next_ip = bf->code_ptr + addr;
    in_closure = true;
    return UnsafeExecResult{exec_next_ip};
  };
  inline UnsafeExecResult visit_call(u8 *decode_next_ip, i32 loc,
                               i32 n_arg) override {
    if (!check_is_begin(bf, bf->code_ptr + loc)) {
      return UnsafeExecResult{nullptr, "CALL does not call a function\n"};
    }
    operands_stack.push(u32(decode_next_ip));
    u8 *ip = bf->code_ptr + loc;
    return UnsafeExecResult{ip};
  };
  inline UnsafeExecResult visit_tag(u8 *decode_next_ip, char const *name,
                              i32 n_arg) override {
    debug(stderr, "TAG\t%s %d\n", name, n_arg);
    u32 v =
        Btag((void *)operands_stack.pop(), LtagHash((char *)name), BOX(n_arg));
    operands_stack.push(v);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_array(u8 *decode_next_ip, i32 size) override {
    debug(stderr, "ARRAY\t%d\n", size);
    size_t is_array_n =
        (size_t)Barray_patt((void *)operands_stack.pop(), BOX(size));
    operands_stack.push(is_array_n);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_fail(u8 *decode_next_ip, i32 arg1,
                               i32 arg2) override {
    return UnsafeExecResult{nullptr};
  };
  inline UnsafeExecResult visit_line(u8 *decode_next_ip, i32 line_number) override {
    debug(stderr, "LINE\t%d\n", line_number);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_patt(u8 *decode_next_ip, u8 patt_kind) override {
    debug(stderr, "PATT\t%s\n", pats[patt_kind]);
    if (patt_kind == 0) { // =str
      auto arg = (void *)operands_stack.pop();
      auto eq = (void *)operands_stack.pop();
      operands_stack.push((u32)Bstring_patt(arg, eq));
    } else if (patt_kind < (i32)Patt::LAST) {
      auto arg = operands_stack.pop();
      operands_stack.push(patts_match((void *)arg, (Patt)patt_kind));
    } else {
      return UnsafeExecResult{nullptr, "Unsupported patt specializer"};
      exit(-1);
    }
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_lread(u8 *decode_next_ip) override {
    debug(stderr, "CALL\tLread\n");
    operands_stack.push(Lread());
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_lwrite(u8 *decode_next_ip) override {
    u32 value = UNBOX(operands_stack.pop());
    debug(stderr, "CALL\tLwrite\n");
    fprintf(stdout, "%d\n", i32(value));
    operands_stack.push(BOX(0));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_llength(u8 *decode_next_ip) override {
    debug(stderr, "CALL\tLlength\n");
    int value = (int)operands_stack.pop();
    int result = Llength((void *)value);
    operands_stack.push((u32)result);
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_lstring(u8 *decode_next_ip) override {
    debug(stderr, "CALL\tLstring\n");
    operands_stack.push((u32)Lstring(((void *)operands_stack.pop())));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_call_barray(u8 *decode_next_ip, i32 n) override {
    debug(stderr, "CALL\tBarray\t%d\n", n);
    auto arr = myBarray(n, operands_stack);
    operands_stack.push(u32(arr));
    return UnsafeExecResult{decode_next_ip};
  };
  inline UnsafeExecResult visit_stop(u8 *decode_next_ip) override {
    return UnsafeExecResult{nullptr};
  }
  ~UnsafeExecutingVisitor() override = default;
};
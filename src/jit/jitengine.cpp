/* ES40 emulator -- JIT engine implementation. */
#ifdef ES40_JIT

#include "jitengine.h"
#include <cstdio>
#include <cstring>
#define ASMJIT_STATIC
#include <asmjit/x86.h>

namespace {

enum SafeOp { OP_NONE, OP_ADDQ, OP_SUBQ, OP_AND, OP_BIS, OP_XOR };

SafeOp classify(uint32_t ins)
{
  uint32_t opcode = ins >> 26;
  uint32_t func = (ins >> 5) & 0x7F;
  if (opcode == 0x10) {        // INTA
    if (func == 0x20) return OP_ADDQ;
    if (func == 0x29) return OP_SUBQ;
  } else if (opcode == 0x11) { // INTL
    if (func == 0x00) return OP_AND;
    if (func == 0x20) return OP_BIS;
    if (func == 0x40) return OP_XOR;
  }
  return OP_NONE;
}

} // namespace

CJitEngine::CJitEngine() : m_recorded(0), m_rt(nullptr)
{
  flush();                                  // clears valid bits (m_rt still null)
  m_rt = new asmjit::JitRuntime();
#ifdef JIT_VERIFY
  m_v_exec = m_v_fail = 0;
#endif
}

CJitEngine::~CJitEngine()
{
  delete (asmjit::JitRuntime*) m_rt;
}

CJitEngine::JitBlock* CJitEngine::record(uint64_t phys_pc, uint64_t virt_pc, uint32_t n_instr)
{
  JitBlock& b = m_blocks[index_of(phys_pc)];
  if (b.valid && b.tag == phys_pc) {        // same block re-seen; keep compiled state
    b.n_instr = n_instr;
    return &b;
  }
  b.tag = phys_pc;
  b.start_virt = virt_pc;
  b.n_instr = n_instr;
  b.valid = true;
  b.code = nullptr;
  b.prefix_len = 0;
  b.compiled = false;
  if (++m_recorded == 50000)
    printf("[JIT] block dispatcher active: 50000 blocks discovered.\n");
  return &b;
}

void CJitEngine::flush()
{
  for (int i = 0; i < kCacheEntries; ++i)
    m_blocks[i].valid = false;
  // Recreate the runtime to reclaim all generated code. reset()-and-reuse was
  // corrupting the JitAllocator block tree under heavy flush churn.
  if (m_rt)
  {
    delete (asmjit::JitRuntime*) m_rt;
    m_rt = new asmjit::JitRuntime();
  }
}

void CJitEngine::compile_block(JitBlock* b, const uint8_t* dram, uint64_t dram_size)
{
  using namespace asmjit;
  b->compiled = true;

  // Only non-PALmode blocks: there RREG(a)==a, so direct state.r[reg] access is
  // correct. PALmode (PC bit 0) can remap R4-7/R20-23 to the shadow bank.
  if (b->start_virt & 1) return;
  uint64_t phys = b->tag;
  if (b->n_instr == 0 || phys + (uint64_t) b->n_instr * 4 > dram_size) return;
  const uint32_t* words = (const uint32_t*) (dram + phys);   // x86 LE == Alpha LE

  uint32_t plen = 0;
  while (plen < b->n_instr && plen < 64 && classify(words[plen]) != OP_NONE)
    plen++;
  if (plen == 0) return;

  // Emit  void fn(uint64_t* regs)  -- regs in RCX (Win64); RAX scratch.
  CodeHolder code;
  if (code.init(((JitRuntime*) m_rt)->environment()) != Error::kOk) return;
  x86::Assembler a(&code);
  for (uint32_t i = 0; i < plen; ++i) {
    uint32_t ins = words[i];
    SafeOp op = classify(ins);
    int ra = (ins >> 21) & 0x1F;
    int rb = (ins >> 16) & 0x1F;
    int rc = ins & 0x1F;
    bool islit = ((ins >> 12) & 1) != 0;
    int lit = (ins >> 13) & 0xFF;

    if (ra == 31) a.xor_(x86::rax, x86::rax);
    else          a.mov(x86::rax, x86::qword_ptr(x86::rcx, ra * 8));

    if (islit) {
      switch (op) {
        case OP_ADDQ: a.add(x86::rax, imm(lit)); break;
        case OP_SUBQ: a.sub(x86::rax, imm(lit)); break;
        case OP_AND:  a.and_(x86::rax, imm(lit)); break;
        case OP_BIS:  a.or_(x86::rax, imm(lit)); break;
        case OP_XOR:  a.xor_(x86::rax, imm(lit)); break;
        default: break;
      }
    } else if (rb == 31) {              // operand2 == 0
      if (op == OP_AND) a.xor_(x86::rax, x86::rax);
    } else {
      x86::Mem m = x86::qword_ptr(x86::rcx, rb * 8);
      switch (op) {
        case OP_ADDQ: a.add(x86::rax, m); break;
        case OP_SUBQ: a.sub(x86::rax, m); break;
        case OP_AND:  a.and_(x86::rax, m); break;
        case OP_BIS:  a.or_(x86::rax, m); break;
        case OP_XOR:  a.xor_(x86::rax, m); break;
        default: break;
      }
    }

    if (rc != 31) a.mov(x86::qword_ptr(x86::rcx, rc * 8), x86::rax);
  }
  a.ret();

  JitFn fn = nullptr;
  if (((JitRuntime*) m_rt)->add(&fn, &code) != Error::kOk) return;
  b->code = fn;
  b->prefix_len = plen;
}

#ifdef JIT_VERIFY
void CJitEngine::verify_compare(uint64_t blk_virt, const uint64_t* interp, const uint64_t* jit)
{
  m_v_exec++;
  for (int r = 0; r < 31; ++r) {
    if (interp[r] != jit[r]) {
      m_v_fail++;
      printf("[JIT][VERIFY] MISMATCH at block pc=%016llx: r%d interp=%016llx jit=%016llx\n",
             (unsigned long long) blk_virt, r,
             (unsigned long long) interp[r], (unsigned long long) jit[r]);
      break;
    }
  }
  if ((m_v_exec % 500000) == 0)
    printf("[JIT][VERIFY] %llu compiled-block execs, %llu mismatches\n",
           (unsigned long long) m_v_exec, (unsigned long long) m_v_fail);
}
#endif

#endif // ES40_JIT

/* ES40 emulator -- JIT engine
 *
 * Per-CPU, direct-mapped cache of translation blocks. The dispatcher runs a
 * block's compiled safe-ALU prefix natively, then interprets the remainder.
 */
#if !defined(INCLUDED_JITENGINE_H)
#define INCLUDED_JITENGINE_H

#ifdef ES40_JIT

#include <cstdint>
#include "../config_debug.h"   // JIT_VERIFY

class CJitEngine
{
public:
  static constexpr int      kCacheBits = 14;
  static constexpr int      kCacheEntries = 1 << kCacheBits;
  static constexpr uint64_t kIndexMask = (uint64_t) kCacheEntries - 1;

  // Compiled safe-prefix entry point: applies the prefix's ALU ops to regs[0..31].
  typedef void (*JitFn)(uint64_t* regs);

  struct JitBlock
  {
    uint64_t tag;         // start physical PC (validity tag)
    uint64_t start_virt;  // start virtual PC
    uint32_t n_instr;     // instructions in the straight-line block
    bool     valid;
    JitFn    code;        // compiled safe-prefix, or null
    uint32_t prefix_len;  // # safe ALU ops in code
    bool     compiled;    // compile has been attempted
  };

  CJitEngine();
  ~CJitEngine();

  static inline uint64_t index_of(uint64_t phys_pc) { return (phys_pc >> 2) & kIndexMask; }

  inline JitBlock* lookup(uint64_t phys_pc)
  {
    JitBlock& b = m_blocks[index_of(phys_pc)];
    return (b.valid && b.tag == phys_pc) ? &b : nullptr;
  }

  JitBlock* record(uint64_t phys_pc, uint64_t virt_pc, uint32_t n_instr);
  void compile_block(JitBlock* b, const uint8_t* dram, uint64_t dram_size);
  void flush();

#ifdef JIT_VERIFY
  // Differential check: compiled result (jit) vs interpreter result (interp), r[0..30].
  void verify_compare(uint64_t blk_virt, const uint64_t* interp, const uint64_t* jit);
#endif

private:
  JitBlock m_blocks[kCacheEntries];
  uint64_t m_recorded;
  void*    m_rt;          // asmjit::JitRuntime*
#ifdef JIT_VERIFY
  uint64_t m_v_exec, m_v_fail;
#endif
};

#endif // ES40_JIT
#endif // INCLUDED_JITENGINE_H

/* ES40 emulator -- JIT engine implementation (Stage 1: block-discovery cache). */
#ifdef ES40_JIT

#include "jitengine.h"
#include <cstdio>

CJitEngine::CJitEngine() : m_recorded(0)
{
  flush();
}

void CJitEngine::record(uint64_t phys_pc, uint64_t virt_pc, uint32_t n_instr)
{
  JitBlock& b = m_blocks[index_of(phys_pc)];
  b.tag = phys_pc;
  b.start_virt = virt_pc;
  b.n_instr = n_instr;
  b.valid = true;
  if (++m_recorded == 50000)
    printf("[JIT] Stage 1 block dispatcher active: 50000 blocks discovered.\n");
}

void CJitEngine::flush()
{
  for (int i = 0; i < kCacheEntries; ++i)
    m_blocks[i].valid = false;
}

#endif // ES40_JIT

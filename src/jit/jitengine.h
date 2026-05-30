/* ES40 emulator -- JIT engine
 *
 * Per-CPU, direct-mapped cache of translation blocks discovered during
 * interpreted execution on the ES40_JIT lane.
 */
#if !defined(INCLUDED_JITENGINE_H)
#define INCLUDED_JITENGINE_H

#ifdef ES40_JIT

#include <cstdint>

class CJitEngine
{
public:
  // 2^14 = 16384 entries (~512 KB/CPU). Direct-mapped on the start physical PC.
  static constexpr int      kCacheBits = 14;
  static constexpr int      kCacheEntries = 1 << kCacheBits;
  static constexpr uint64_t kIndexMask = (uint64_t) kCacheEntries - 1;

  struct JitBlock
  {
    uint64_t tag;         // start physical PC (validity tag)
    uint64_t start_virt;  // start virtual PC (reference / debug)
    uint32_t n_instr;     // instructions in the straight-line block
    bool     valid;
  };

  CJitEngine();

  static inline uint64_t index_of(uint64_t phys_pc)
  {
    return (phys_pc >> 2) & kIndexMask;
  }

  // Cached block for this start physical PC, or nullptr on miss.
  inline JitBlock* lookup(uint64_t phys_pc)
  {
    JitBlock& b = m_blocks[index_of(phys_pc)];
    return (b.valid && b.tag == phys_pc) ? &b : nullptr;
  }

  // Insert/replace the block at its direct-mapped slot.
  void record(uint64_t phys_pc, uint64_t virt_pc, uint32_t n_instr);

  // Drop all blocks (instruction memory may have changed).
  void flush();

  uint64_t blocks_recorded() const { return m_recorded; }

private:
  JitBlock m_blocks[kCacheEntries];
  uint64_t m_recorded;   // cumulative record() count (debug stat)
};

#endif // ES40_JIT
#endif // INCLUDED_JITENGINE_H

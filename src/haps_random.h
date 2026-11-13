#ifndef HAPS_RANDOM_H
#define HAPS_RANDOM_H

#include <stdint.h>

/*
 * Global rand()/srand() kullanmak yerine kanal context'inden
 * bağımsız, tekrar üretilebilir basit bir PRNG kullanıyoruz.
 */
static inline uint32_t haps_xorshift32(uint32_t *state)
{
  uint32_t x = *state;

  if (x == 0)
    x = 0x6d2b79f5U;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;

  *state = x;
  return x;
}

/*
 * Review fix (E9): SplitMix32-style seed finaliser (Murmur3 fmix32 constants).
 * Any nonzero 32-bit state is a valid xorshift32 state; mixing makes the
 * first outputs independent of the magnitude of the user seed.
 */
static inline uint32_t haps_seed_mix(uint32_t seed)
{
  uint32_t z = seed + 0x9E3779B9U;
  z = (z ^ (z >> 16)) * 0x85EBCA6BU;
  z = (z ^ (z >> 13)) * 0xC2B2AE35U;
  z ^= z >> 16;
  return z != 0 ? z : 0x6d2b79f5U;
}

/*
 * Açık aralıkta uniform rastgele değişken:
 *
 * 0 < u < 1
 */
static inline double haps_uniform_open01(uint32_t *state)
{
  const uint32_t value = haps_xorshift32(state);

  return ((double)value + 0.5) / 4294967296.0;
}

#endif

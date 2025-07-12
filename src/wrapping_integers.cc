#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

// zero_point是一层包装
// 计算出最终的偏移量，然后加在checkpoint上
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t upper = static_cast<uint64_t>( UINT32_MAX ) + 1;
  const uint32_t ckpt_mod = Wrap32::wrap( checkpoint, zero_point ).raw_value_;
  uint32_t offset = raw_value_ - ckpt_mod;
  if ( offset <= ( upper >> 1 ) || checkpoint + offset < upper )
    return checkpoint + offset;
  return checkpoint + offset - upper;
}
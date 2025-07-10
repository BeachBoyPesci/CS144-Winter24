#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

// zero_point是一层包装
// 计算出最终的偏移量，然后加在checkpoint上
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t upper = static_cast<uint64_t>( UINT32_MAX ) + 1;
  uint32_t ckpt_mod = Wrap32::wrap( checkpoint, zero_point ).raw_value_;
  uint32_t distance = raw_value_ - ckpt_mod;
  if ( distance <= ( upper >> 1 )
       || checkpoint + distance < upper ) // distabnce小于等于2^31，或者checkpoint + distance太小，无法减去upper
    return checkpoint + distance;
  return checkpoint + distance - upper;
}
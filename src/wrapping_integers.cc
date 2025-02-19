#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  constexpr uint64_t upper = static_cast<uint64_t>( UINT32_MAX ) + 1;
  uint32_t ckpt_mod = wrap( checkpoint, zero_point ).raw_value_; // 将checkpoint转化为32位
  uint32_t dif = raw_value_ - ckpt_mod;                          // 求出偏移量
  if ( dif <= ( upper >> 1 ) || checkpoint + dif < upper )
    // 如果偏移量小于2^31，或者checkpoint太小使得checkpoint左边不可取得（因为至少需要减去2^32）
    return checkpoint + dif;
  return checkpoint + dif - upper;
}
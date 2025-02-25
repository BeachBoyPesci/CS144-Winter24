#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t upper = static_cast<uint64_t>( UINT32_MAX ) + 1;
  uint32_t ck_mod = Wrap32::wrap( checkpoint, zero_point ).raw_value_; // 将checkpoint映射到uint32区间内
  uint64_t dis = raw_value_ - ck_mod;
  if ( dis < ( upper >> 1 ) || checkpoint + dis < upper ) // 如果dis小于2^15或者答案小于2^16，就不需要减去2^16
    return checkpoint + dis;
  return checkpoint - upper + dis;
}
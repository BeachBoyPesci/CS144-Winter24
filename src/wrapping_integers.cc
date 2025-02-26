#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t upper = static_cast<uint64_t>( UINT32_MAX ) + 1;
  const uint32_t ck_mod
    = Wrap32::wrap( checkpoint, zero_point ).raw_value_; // 将checkpoint映射到32位数，便于计算dis
  uint32_t dis = raw_value_ - ck_mod;
  if ( dis < ( upper >> 1 ) || checkpoint + dis < upper ) // 如果dis<2^15或者checkpoint太小
    return checkpoint + dis;
  return checkpoint + dis - upper;
}
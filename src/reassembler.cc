#include "reassembler.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& bytes_writer = output_.writer();
  if ( const uint64_t unacceptable_index = expecting_index_ + bytes_writer.available_capacity();
       bytes_writer.is_closed() || bytes_writer.available_capacity() == 0
       || first_index >= unacceptable_index ) // 写端关闭，无空间可用，数据超出可接受范围
    return;
  else if ( first_index + data.size() >= unacceptable_index ) { // 截断数据中超出可接受范围的部分
    is_last_substring = false;
    data.resize( unacceptable_index - first_index );
  }

  if ( first_index > expecting_index_ )
    cache_bytes( first_index, move( data ), is_last_substring );
  else
    push_bytes( first_index, move( data ), is_last_substring );
  flush_buffer();
}

uint64_t Reassembler::bytes_pending() const
{
  return num_bytes_pending_;
}

void Reassembler::push_bytes( uint64_t first_index, string data, bool is_last_string )
{
  if ( first_index < expecting_index_ )
    data.erase( 0, expecting_index_ - first_index );
  expecting_index_ += data.size();
  output_.writer().push( move( data ) );

  if ( is_last_string ) {
    output_.writer().close();
    unordered_bytes_.clear();
    num_bytes_pending_ = 0;
  }
}

void Reassembler::cache_bytes( uint64_t first_index, string data, bool is_last_string )
{
  auto end = unordered_bytes_.end();
  auto left = lower_bound( unordered_bytes_.begin(), end, first_index, []( auto&& e, uint64_t idx ) -> bool {
    return idx > ( get<0>( e ) + get<1>( e ).size() );
  } );
  auto right = upper_bound( left, end, first_index + data.size(), []( uint64_t nxt_idx, auto&& e ) -> bool {
    return nxt_idx < get<0>( e );
  } );

  if ( const uint64_t next_index = first_index + data.size(); left != end ) {
    auto& [l_point, dat, _] = *left;
    if ( const uint64_t r_point = l_point + dat.size();
         first_index >= l_point && next_index <= r_point ) // 数据已全部存在
      return;
    else if ( next_index < l_point )
      right = left;
    else if ( !( first_index <= l_point && r_point <= next_index ) ) {
      if ( first_index >= l_point )
        data.insert( 0, string_view( dat.c_str(), dat.size() - ( r_point - first_index ) ) );
      else {
        data.resize( data.size() - ( next_index - l_point ) );
        data.append( dat );
      }
      first_index = min( first_index, l_point );
    }
  }

  if ( const uint64_t next_index = first_index + data.size(); right != left && !unordered_bytes_.empty() ) {
    auto& [l_point, dat, _] = *prev( right );
    if ( const uint64_t r_point = l_point + dat.size(); r_point > next_index ) {
      data.resize( data.size() - ( next_index - l_point ) );
      data.append( dat );
    }
  }

  for ( ; left != right; left = unordered_bytes_.erase( left ) ) {
    num_bytes_pending_ -= get<1>( *left ).size();
    is_last_string |= get<2>( *left );
  }
  num_bytes_pending_ += data.size();
  unordered_bytes_.insert( left, { first_index, move( data ), is_last_string } );
}

void Reassembler::flush_buffer()
{
  while ( !unordered_bytes_.empty() ) {
    auto& [idx, data, last] = unordered_bytes_.front();
    if ( idx > expecting_index_ )
      break;
    num_bytes_pending_ -= data.size();
    push_bytes( idx, move( data ), last );
    if ( !unordered_bytes_.empty() )
      unordered_bytes_.pop_front();
  }
}
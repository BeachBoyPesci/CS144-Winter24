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
  if ( first_index < expecting_index_ ) // 删除旧的，在接受范围之前的数据
    data.erase( 0, expecting_index_ - first_index );
  expecting_index_ += data.size();
  output_.writer().push( move( data ) );

  if ( is_last_string ) {
    output_.writer().close();
    ordered_bytes_.clear();
    num_bytes_pending_ = 0;
  }
}

void Reassembler::cache_bytes( uint64_t first_index, string data, bool is_last_string )
{
  auto end = ordered_bytes_.end();
  auto left = lower_bound(
    ordered_bytes_.begin(),
    end,
    first_index,
    []( auto&& e, uint64_t idx ) -> bool { // lower_bound()返回第一个使comp条件为false的迭代器
      return idx > ( get<0>( e )
                     + get<1>( e ).size() ); // 寻找第一个右端点大于或等于first_index的区间，它可能与data重叠
    } );
  auto right = upper_bound( left, end, first_index + data.size(), []( uint64_t nxt_idx, auto&& e ) -> bool {
    return nxt_idx < get<0>( e ); // 寻找第一个在data范围右边的，无重叠的区间
  } );

  if ( const uint64_t next_index = first_index + data.size(); left != end ) { // 处理左端点
    auto& [l_point, dat, _] = *left;
    if ( const uint64_t r_point = l_point + dat.size();
         first_index >= l_point && next_index <= r_point ) // 数据已全部存在
      return;
    else if ( next_index < l_point ) /* data与任何区间都没有重叠，无需处理重叠情况，跳转86行。另外，即使next_index
                                        == l_point这种情况，也是不会出问题的，因为最后还有个flush_buffer()操作*/
      right = left;
    else if ( !( first_index <= l_point && r_point <= next_index ) ) { // 条件句表示data完全包含区间
      if ( first_index >= l_point )
        data.insert( 0, string_view( dat.c_str(), dat.size() - ( r_point - first_index ) ) );
      else { // 注意上面已经排除掉了“data完全包含区间”这种情况，所以这里必然有next_index <=
             // r_point，下面的截断不会损失数据
        data.resize( data.size() - ( next_index - l_point ) );
        data.append( dat );
      }
      first_index = min( first_index, l_point );
    }
  }

  if ( const uint64_t next_index = first_index + data.size();
       right != left && !ordered_bytes_.empty() ) { // 处理右端点
    auto& [l_point, dat, _] = *prev( right );
    if ( const uint64_t r_point = l_point + dat.size(); r_point > next_index ) { // 合并区间内的数据
      data.resize( data.size() - ( next_index - l_point ) );
      data.append( dat );
    }
  }

  for ( ; left != right; left = ordered_bytes_.erase( left ) ) { // 处理中间被data覆盖的节点
    num_bytes_pending_ -= get<1>( *left ).size();
    is_last_string |= get<2>( *left );
  }
  num_bytes_pending_ += data.size();
  ordered_bytes_.insert( left, { first_index, move( data ), is_last_string } );
}

void Reassembler::flush_buffer()
{
  while ( !ordered_bytes_.empty() ) {
    auto& [idx, data, last] = ordered_bytes_.front();
    if ( idx > expecting_index_ )
      break;
    num_bytes_pending_ -= data.size();
    push_bytes( idx, move( data ), last );
    if ( !ordered_bytes_.empty() ) // push_bytes函数可能把容器清空
      ordered_bytes_.pop_front();
  }
}

// 算法不会进行太多合并，即使两个区间首尾相连，但是对于链表，erase()操作复杂度不高
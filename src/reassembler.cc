#include "reassembler.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer_ = output_.writer();

  uint64_t first_unacceptable_ = expecting_index_ + writer_.available_capacity();
  if ( is_last_substring ) {
    terminate_index_ = first_index + data.size(); // 记录终止字节序号
    is_last_substring_ = true;                    // 如果所有终止字节序号之前的都已经push之后，可以关闭写端
    first_unacceptable_ = min( terminate_index_, first_unacceptable_ );
  }

  if ( writer_.is_closed() || first_index >= first_unacceptable_ || writer_.available_capacity() == 0 ) {
    if ( is_last_substring_ && expecting_index_ == terminate_index_ )
      writer_.close();
    return;
  }

  if ( first_index + data.size() >= first_unacceptable_ ) // 去掉超出可接受范围的数据
    data.resize( first_unacceptable_ - first_index );
  if ( first_index < expecting_index_ ) { // 去掉过时数据
    data.erase( 0, expecting_index_ - first_index );
    first_index = expecting_index_;
  }

  // 将data插入ordered_bytes_中
  auto end = ordered_bytes_.end();
  auto left = lower_bound( ordered_bytes_.begin(), end, first_index, []( auto&& e, uint64_t idx ) { // 左端点
    return idx > e.first + e.second.size(); // lower_bound函数会根据表达式找到第一个***不满足***条件的元素
  } );
  auto right // 右端点
    = upper_bound( left, end, first_index + data.size(), []( uint64_t idx, auto&& e ) {
        return idx < e.first;
      } ); // upper_bound函数会找到第一个满足条件的元素

  // 合并左端点数据
  if ( const uint64_t next_index_ = first_index + data.size(); left != end ) {
    auto& [l, str] = *left;
    const uint64_t r = l + str.size();
    if ( first_index >= l && next_index_ <= r ) // 数据已存在
      return;
    if ( next_index_ < l ) // 数据与左端点完全不重合，无需合并
      right = left;
    else {                    // 合并数据
      if ( first_index >= l ) // data左边有数据需要合并
        data.insert( 0, string_view( str.c_str(), first_index - l ) );
      if ( next_index_ < r ) { // data右边有数据需要合并
        data.resize( data.size() - ( next_index_ - l ) );
        data.append( move( str ) );
      }
    }
    first_index = min( first_index, l );
  }

  // 合并右端点数据
  if ( const uint64_t next_index_ = first_index + data.size(); left != right && !ordered_bytes_.empty() ) {
    auto& [l, str] = *prev( right ); // upper_bound函数找到的是右端点的下一个节点，因此要找上一个节点合并数据
    // 只合并data右边的即可，如果左边有未合并的数据，那么这个节点=left，会在上面那段代码进行合并
    if ( next_index_ < l + str.size() ) {
      data.resize( data.size() - ( next_index_ - l ) );
      data.append( move( str ) );
    }
  }

  // 去掉中间的数据重复的节点
  for ( ; left != right; left = ordered_bytes_.erase( left ) )
    bytes_pending_ -= left->second.size();

  // 插入
  bytes_pending_ += data.size();
  if ( !data.empty() )
    ordered_bytes_.insert( left, { first_index, move( data ) } );

  // push数据
  while ( !ordered_bytes_.empty() ) {
    auto& [l, str] = ordered_bytes_.front();
    if ( expecting_index_ < l )
      break;
    bytes_pending_ -= str.size();
    expecting_index_ += str.size();
    writer_.push( move( str ) );
    ordered_bytes_.pop_front();
  }

  // 判断写端是否需要关闭
  if ( is_last_substring_ && expecting_index_ == terminate_index_ )
    writer_.close();
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

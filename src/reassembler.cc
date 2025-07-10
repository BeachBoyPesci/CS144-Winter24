#include "reassembler.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 检查是否接收字符串
  auto& writer_ = output_.writer();
  auto& reader_ = output_.reader();
  uint64_t unacceptable_index_ = expecting_index_ + writer_.available_capacity();
  if ( reader_.is_finished() || writer_.available_capacity() == 0 || first_index >= unacceptable_index_ ) {
    return;
  }
  if ( is_last_substring ) {
    is_closed_ = true;
    terminate_index_ = first_index + data.size(); // 记录终止字节序号
  }
  if ( is_closed_ ) {
    unacceptable_index_ = min( terminate_index_, unacceptable_index_ );
  }

  // 去掉过时数据
  if ( first_index < expecting_index_ ) {
    data.erase( 0, expecting_index_ - first_index );
    first_index = expecting_index_;
  }
  // 去掉超出可接受范围的数据
  if ( first_index + data.size() >= unacceptable_index_ ) {
    data.resize( unacceptable_index_ - first_index );
  }

  // 将data插入lists中
  auto left = upper_bound( lists.begin(), lists.end(), first_index, []( uint64_t idx, auto&& e ) {
    return idx < e.first + e.second.size(); // 找到第一个满足idx<e.first的元素
  } );
  auto right = lower_bound( lists.begin(), lists.end(), first_index + data.size(), []( auto&& e, uint64_t idx ) {
    return e.first < idx; // 找到第一个满足idx<=e.first的元素
  } );

  // 如果有必要，合并左端点数据
  if ( left != lists.end() && first_index > left->first ) {
    auto temp = left->second.substr( 0, first_index - left->first );
    data = temp + data;
    first_index = left->first; // 更新first_index
  }

  // 如果有必要，合并右端点数据
  if ( right != lists.begin() && first_index + data.size() < prev( right )->first + prev( right )->second.size() ) {
    auto temp = prev( right )->second.substr( first_index + data.size() - prev( right )->first );
    data += temp;
  }

  // 去掉中间的数据重复的节点
  for ( ; left != right; left = lists.erase( left ) ) {
    bytes_pending_ -= left->second.size();
  }

  // 插入
  bytes_pending_ += data.size();
  if ( !data.empty() )
    lists.insert( left, { first_index, move( data ) } );

  // push数据
  while ( !lists.empty() && lists.front().first == expecting_index_ ) {
    auto it = lists.front();
    bytes_pending_ -= it.second.size();
    expecting_index_ += it.second.size();
    writer_.push( move( it.second ) );
    lists.pop_front();
  }

  // 判断写端是否需要关闭
  if ( is_closed_ && expecting_index_ >= terminate_index_ )
    writer_.close();
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

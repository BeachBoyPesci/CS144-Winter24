#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return is_closed_;
}

void Writer::push( string data )
{
  if ( is_closed() )
    return;
  if ( data.size() > available_capacity() )
    data.resize( available_capacity() );
  if ( !data.empty() ) {
    bytes_buffered_ += data.size();
    bytes_pushed_ += data.size();
    bytes_.emplace( move( data ) );
  }
  if ( view_.empty() && !bytes_.empty() )
    view_ = bytes_.front();
  return;
}

void Writer::close()
{
  if ( !is_closed_ ) {
    is_closed_ = true;
    bytes_.emplace( string( 1, EOF ) );
  }
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const // peek函数读取不是管道中所有的字符，是第一个string中剩下的字符
{
  return view_;
}

void Reader::pop( uint64_t len )
{
  uint64_t remain = len;
  while ( remain >= view_.size() && remain != 0 ) { // 为了防止view_为空，要加上remain!=0
    remain -= view_.size(); // 如果view_中所有字符都需要被弹出，说明这个string已被读取完毕，应该pop()，并更新view_
    bytes_.pop();
    view_ = bytes_.empty() ? ""sv : bytes_.front();
  }
  if ( !view_.empty() ) // view_保存第一个string中剩下的字符，所以改动view_即可，不用改动bytes_.front()
    view_.remove_prefix( remain );
  bytes_buffered_ -= len;
  bytes_popped_ += len;
  return;
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_buffered_;
}

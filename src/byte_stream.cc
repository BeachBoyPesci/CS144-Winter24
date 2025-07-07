#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return is_closed_;
}

void Writer::push( string data )
{
  if ( is_closed() ) {
    return;
  }
  if ( data.size() > available_capacity() ) {
    data.resize( available_capacity() );
  }
  if ( data.empty() ) {
    return;
  }
  bytes_buffered_ += data.size();
  bytes_pushed_ += data.size();
  buffer_.emplace( move( data ) );
  if ( view_.empty() && !buffer_.empty() ) {
    view_ = buffer_.front();
  }
}

void Writer::close()
{
  if ( !is_closed_ ) {
    is_closed_ = true;
    buffer_.emplace( string( 1, EOF ) );
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
  return is_closed_ && bytes_buffered_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  return view_;
}

void Reader::pop( uint64_t len )
{
  if ( len > bytes_buffered_ ) {
    len = bytes_buffered_;
  }
  auto remain = len;
  while ( remain >= view_.size() && !buffer_.empty() ) {
    remain -= view_.size();
    buffer_.pop();
    view_ = buffer_.empty() ? std::string_view() : buffer_.front();
  }
  if ( remain > 0 ) {
    view_.remove_prefix( remain );
  }
  bytes_buffered_ -= len;
  bytes_popped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_buffered_;
}
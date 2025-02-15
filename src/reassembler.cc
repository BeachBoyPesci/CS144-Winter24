#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( nread_ == stopped_ && is_closed_ ) {
    output_.writer().close();
    return;
  }
  if ( is_last_substring ) {
    is_closed_ = true;
    stopped_ = first_index + static_cast<uint64_t>( data.size() );
  }

  for ( uint64_t i = 0; i < data.size(); i++ ) {
    if ( i + first_index < nread_ || i + first_index >= nread_ + output_.writer().available_capacity() )
      continue;
    map[i + first_index] = data[i];
  }
  string str = string();
  while ( map.find( nread_ ) != map.end() ) {
    str += map[nread_];
    map.erase( nread_ );
    ++nread_;
  }
  output_.writer().push( move( str ) );
  if ( nread_ == stopped_ && is_closed_ )
    output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
  return static_cast<uint64_t>( map.size() );
}

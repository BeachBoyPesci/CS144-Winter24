#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( isn_.has_value() && message.seqno == *isn_ ) {
    return;
  }
  if ( !isn_.has_value() ) {
    if ( !message.SYN ) {
      return;
    }
    isn_ = message.seqno;
  }
  uint64_t abso_index = message.seqno.unwrap( *isn_, writer().bytes_pushed() + isn_.has_value() );
  reassembler_.insert( abso_index == 0 ? 0 : abso_index - 1, // first_index = abso_index - 1，是因为要去掉SYN占位符
                       move( message.payload ),
                       message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  auto msg = TCPReceiverMessage();
  if ( isn_.has_value() ) {
    msg.ackno = Wrap32::wrap( writer().bytes_pushed() + isn_.has_value() + writer().is_closed(), *isn_ );
  }
  msg.RST = reader().has_error();
  msg.window_size = min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) );
  return msg;
}
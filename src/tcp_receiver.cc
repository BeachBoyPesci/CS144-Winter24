#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( message.seqno == ISN_ ) { // 非法序列号
    return;
  }
  uint64_t check_point = writer().bytes_pushed() + ISN_.has_value();
  if ( !ISN_.has_value() ) {
    if ( !message.SYN )
      return;
    ISN_ = message.seqno;
  }
  uint64_t abso_sqno = message.seqno.unwrap( *ISN_, check_point );
  reassembler_.insert( abso_sqno == 0 ? 0 : abso_sqno - 1, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  uint64_t check_point = writer().bytes_pushed() + ISN_.has_value();
  if ( ISN_.has_value() ) {
    message.ackno = Wrap32::wrap( check_point + writer().is_closed(), *ISN_ );
  }
  message.window_size = min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) );
  message.RST = reader().has_error();
  return message;
}

#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  uint64_t checkpoint = writer().bytes_pushed() + ISN_.has_value() + writer().is_closed();
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( message.seqno == ISN_ )
    return;
  if ( !ISN_.has_value() ) {
    if ( !message.SYN )
      return;
    ISN_ = message.seqno;
  }
  uint64_t absolute_seqno = message.seqno.unwrap( *ISN_, checkpoint );
  uint64_t first_index = absolute_seqno == 0 ? 0 : absolute_seqno - 1;
  reassembler_.insert( first_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  if ( ISN_.has_value() )
    message.ackno = Wrap32::wrap( writer().bytes_pushed() + ISN_.has_value() + writer().is_closed(), *ISN_ );
  message.RST = reader().has_error();
  message.window_size = min( static_cast<uint64_t> UINT16_MAX, writer().available_capacity() );
  return message;
}

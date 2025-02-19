#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed() + isn_.has_value();
  if ( message.RST ) {
    reassembler_.reader().set_error();
  } else if ( checkpoint > 0 && checkpoint <= UINT32_MAX && message.seqno == isn_ )
    return; // 拦截非法的序列号
  if ( !isn_.has_value() ) {
    if ( !message.SYN )
      return;
    isn_ = message.seqno;
  }
  const uint64_t abso_seqno_ = message.seqno.unwrap( *isn_, checkpoint );
  reassembler_.insert( abso_seqno_ == 0 ? abso_seqno_ : abso_seqno_ - 1, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  if ( isn_.has_value() ) {
    const uint64_t next_byte = reassembler_.writer().bytes_pushed() + 1;
    const uint64_t abso_ackno = next_byte + reassembler_.writer().is_closed();
    msg.ackno = Wrap32::wrap( abso_ackno, *isn_ );
  }

  const uint16_t wnd_size = static_cast<uint16_t>(
    min( reassembler_.writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  msg.window_size = wnd_size;

  msg.RST = reassembler_.reader().has_error();
  return msg;
}

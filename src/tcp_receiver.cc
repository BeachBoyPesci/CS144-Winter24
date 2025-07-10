#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) { // 有错误，立即退出
    reader().set_error();
    return;
  }
  if ( message.seqno == ISN_ ) { // 非法序列号
    return;
  }
  uint64_t check_point = writer().bytes_pushed()
                         + ISN_.has_value(); // 设置检查点，由于SYN标志也占用一个序列号，因此加上ISN_是否有值的判断
  if ( !ISN_.has_value() ) {                 // 设置初始序列号
    if ( !message.SYN )
      return;
    ISN_ = message.seqno;
  }
  uint64_t abso_sqno = message.seqno.unwrap( *ISN_, check_point ); // 计算绝对序列号
  reassembler_.insert( abso_sqno == 0 ? 0 : abso_sqno - 1, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  if ( ISN_.has_value() ) {
    message.ackno = Wrap32::wrap( writer().bytes_pushed() + ISN_.has_value() + writer().is_closed(), *ISN_ );
  }
  message.window_size = min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) );
  message.RST = reader().has_error();
  return message;
}

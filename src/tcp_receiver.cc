#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  const uint64_t checkpoint
    = reassembler_.writer().bytes_pushed() + ISN.has_value(); // 这个值实际上是期待的下一个绝对字节序号
  if ( message.RST )
    reassembler_.reader().set_error(); // 设置错误标志，不过信息仍需发送
  else if ( checkpoint > 0 && checkpoint <= UINT32_MAX && message.seqno == ISN )
    return;                 // 拦截错误的报文段
  if ( !ISN.has_value() ) { // 利用ISN的方法判断是否是第一次通信，来确定isn的值
    if ( !message.SYN )
      return;
    ISN = message.seqno;
  }
  const uint64_t abso_seqno = message.seqno.unwrap( *ISN, checkpoint );        // 加上了偏移量的绝对字节序号
  const uint64_t stream_index = abso_seqno == 0 ? abso_seqno : abso_seqno - 1; // 去掉SYN标志
  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed() + ISN.has_value();
  const uint64_t capacity = reassembler_.writer().available_capacity();
  const uint16_t wnd_size = capacity <= UINT16_MAX ? capacity : UINT16_MAX; // 窗口最大值为2^16
  if ( !ISN.has_value() )
    return { {}, wnd_size, reassembler_.writer().has_error() };
  return {
    Wrap32::wrap( checkpoint + reassembler_.writer().is_closed(), *ISN ), // 如果关闭了，还有个要占用1字节的FIN标志
    wnd_size,
    reassembler_.writer().has_error() };
}

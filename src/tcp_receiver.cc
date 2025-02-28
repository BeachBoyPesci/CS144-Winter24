#include "tcp_receiver.hh"

using namespace std;

// 需要注意，SYN，FIN，RST为F时，不占用空间，这虽然不符合struct的知识，但是这道题目似乎就是这个意思，否则不会设置一个sequence_length()函数。这个事情只能解释为这个课程做了一些简化，实际的生产代码里面，估计SYN标志位除了第一次以外，都不占字节
void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( writer().has_error() )
    return;
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( !ISN_.has_value() ) {
    if ( !message.SYN )
      return;
    ISN_.emplace( message.seqno );
  }
  Wrap32 zero_point = ISN_.value();
  uint64_t checkpoint = writer().bytes_pushed() + 1; // checkpoint就是正在期待的下一个字节序号
  uint64_t abso_seqno = message.seqno.unwrap( zero_point, checkpoint );
  uint64_t stream_index = abso_seqno + message.SYN - 1;
  // 如果SYN为T，说明它是第一次传输，那么abso_seqno为0；如果SYN为F，说明是正常传输，要减去SYN标志位所占用的一个字节
  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  uint16_t wd_size
    = static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  if ( ISN_.has_value() ) {
    Wrap32 ackno = Wrap32::wrap( writer().bytes_pushed() + writer().is_closed(), ISN_.value() ) + 1;
    return { ackno, wd_size, writer().has_error() };
  }
  return { nullopt, wd_size, writer().has_error() };
}

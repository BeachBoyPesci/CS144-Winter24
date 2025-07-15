#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

using namespace std;

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t curr_size // curr_size实际上是包含了SYN和FIN信号的总大小
    = min( TCPConfig::MAX_PAYLOAD_SIZE,
           window_size_ > sequence_numbers_in_flight_ ? window_size_ - sequence_numbers_in_flight_ : 0 );
  if ( !window_size_ && !sequence_numbers_in_flight_
       && established ) // 只有连接建立之后，才准许在窗口为0的时候，假装它是1
    curr_size = 1;
  while ( 1 ) { // 只要窗口还没排满，就一直发送
    TCPSenderMessage msg {};
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    if ( window_size_ > sequence_numbers_in_flight_ ) { // 窗口还没排满的时候，重新计算curr_size
      curr_size
        = min( TCPConfig::MAX_PAYLOAD_SIZE,
               window_size_ > sequence_numbers_in_flight_ ? window_size_ - sequence_numbers_in_flight_ : 0 );
    }
    if ( !first_ack ) { // 建立连接的时候，无论窗口大小是否大于0，都要发送一个SYN信号
      first_ack = true;
      msg.SYN = true;
      ++next_seqno_;
      --curr_size;
    }
    while ( curr_size > 0 ) {
      string data( reader().peek() );
      if ( data.empty() || data == "\377" ) {
        break; // 没有更多数据可读
      }
      msg.payload += data.substr( 0, curr_size );
      auto mn = min( curr_size, data.size() );
      curr_size -= mn;
      writer().reader().pop( mn );
      next_seqno_ += mn;
    }
    if ( writer().is_closed() ) {
      is_closed_ = true;
    }
    if ( ( ( ( window_size_ > sequence_numbers_in_flight_ + msg.sequence_length() ) && !reader().bytes_buffered() )
           || curr_size ) // 窗口尚未排满但是已经读完所有信息，或者窗口为0但假装为1的时候
         && is_closed_ && !FIN_sent ) {
      FIN_sent = true;
      msg.FIN = true;
      ++next_seqno_;
    }
    msg.RST = reader().has_error();
    if ( msg.sequence_length() > 0 ) {
      unacknowledged_messages_.push( msg );
      sequence_numbers_in_flight_ += msg.sequence_length();
      impossible_ackno = max( impossible_ackno, next_seqno_ + 1 );
      transmit( msg );
    }
    if ( msg.sequence_length() == 0 ) // 空序列，退出循环
      break;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  auto msg = TCPSenderMessage();
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.SYN = false; // 这个方法是用来确认连接是否在持续的，因此SYN一直为false
  msg.FIN = false;
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) { // 连接出错
    writer().set_error();
    return;
  }
  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() )
    return;
  auto ack_no_ = ( msg.ackno.value() ).unwrap( isn_, next_seqno_ );
  if ( ack_no_ >= impossible_ackno ) { // 错误的确认号
    return;
  }
  bool poped_ = false; // 记录是否pop过message
  while ( !unacknowledged_messages_.empty() ) {
    auto& message = unacknowledged_messages_.front();
    auto first_unacked_seqno_ = message.seqno.unwrap( isn_, next_seqno_ );
    if ( first_unacked_seqno_ + message.sequence_length() <= ack_no_ ) {
      curr_RTO_ms_ = initial_RTO_ms_;   // 重置当前超时重传时间
      consecutive_retransmissions_ = 0; // 重置超时重传计数器
      sequence_numbers_in_flight_ -= message.sequence_length();
      if ( message.SYN )
        established = true; // 如果是SYN包，连接已建立
      unacknowledged_messages_.pop();
      poped_ = true;
    } else
      break;
  }
  if ( poped_ ) {
    last_tick_ms_ = 0; // 重置重传计时器
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( unacknowledged_messages_.empty() ) {
    return; // 没有未确认的消息，不需要重传
  }
  last_tick_ms_ += ms_since_last_tick;
  if ( last_tick_ms_ >= curr_RTO_ms_ ) {
    while ( !unacknowledged_messages_.empty() && unacknowledged_messages_.front().sequence_length() == 0 ) {
      unacknowledged_messages_.pop();
    }
    if ( unacknowledged_messages_.empty() ) {
      return; // 没有未确认的消息，不需要重传
    }
    transmit( unacknowledged_messages_.front() );
    ++consecutive_retransmissions_;
    if ( auto& msg = unacknowledged_messages_.front();
         msg.SYN || ( established && window_size_ ) ) // 当窗口为0，假装是1的时候，超时重传时延不会倍增
      curr_RTO_ms_ *= 2;
    last_tick_ms_ = 0; // 重置重传计时器
  }
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

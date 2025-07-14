#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

using namespace std;
// TODO: 把推入buffer的逻辑改一下，改成，SYN==false且payload==0且FIN==false的不推入，其余的都推入。最后再做
void TCPSender::push( const TransmitFunction& transmit )
{
  TCPSenderMessage msg {};
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  uint64_t payload_size = min( TCPConfig::MAX_PAYLOAD_SIZE, window_size_ - sequence_numbers_in_flight_ );
  while ( payload_size > 0 ) {
    string data( reader().peek() );
    if ( data.empty() || data == "\377" ) {
      break; // 没有更多数据可读
    }
    msg.payload += data.substr( 0, payload_size );
    auto mn = min( payload_size, data.size() );
    payload_size -= mn;
    writer().reader().pop( mn );
    next_seqno_ += mn;
  }
  if ( writer().is_closed() ) {
    is_closed_ = true;
  }
  if ( payload_size > 0 && is_closed_ && !FIN_sent ) {
    FIN_sent = true;
    msg.FIN = true;
    ++next_seqno_;
  }
  msg.RST = reader().has_error();
  if ( msg.sequence_length() > 0 || !first_ack ) {
    if ( !first_ack ) {
      first_ack = true;
      msg.SYN = true;
      ++next_seqno_;
    }
    unacknowledged_messages_.push( msg );
    sequence_numbers_in_flight_ += msg.sequence_length();
    impossible_ackno = max( impossible_ackno, next_seqno_ + 1 );
    transmit( msg );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  auto msg = TCPSenderMessage();
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.SYN = false; // 这个函数时用来确认连接是否在持续的，因此SYN为false
  msg.FIN = false;
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() )
    return;
  auto ack_no_ = ( msg.ackno.value() ).unwrap( isn_, next_seqno_ );
  if ( ack_no_ >= impossible_ackno ) {
    return;
  }
  bool poped_ = false;
  while ( !unacknowledged_messages_.empty() ) {
    auto& message = unacknowledged_messages_.front();
    auto first_unacked_seqno_ = message.seqno.unwrap( isn_, next_seqno_ );
    if ( first_unacked_seqno_ + message.sequence_length() <= ack_no_ ) {
      curr_RTO_ms_ = initial_RTO_ms_;   // 重置当前重传超时
      consecutive_retransmissions_ = 0; // 重置连续重传计数器
      sequence_numbers_in_flight_ -= message.sequence_length();
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
    // if ( window_size_ > 0 ) {
    ++consecutive_retransmissions_;
    curr_RTO_ms_ *= 2;
    // }
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

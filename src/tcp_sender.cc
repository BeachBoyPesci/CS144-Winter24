#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

using namespace std;

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t payload_size = min( static_cast<uint64_t>( UINT16_MAX ), window_size_ - sequence_numbers_in_flight_ );
  TCPSenderMessage msg {};
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.SYN = true;
  next_seqno_ += 1;
  while ( payload_size > 0 ) {
    auto data = reader().peek();
    if ( data.empty() ) {
      break; // 没有更多数据可读
    }
    msg.payload += data.substr( 0, payload_size );
    auto mn = min( payload_size, data.size() );
    payload_size -= mn;
    writer().reader().pop( mn );
    next_seqno_ += mn;
  }
  msg.FIN = writer().is_closed();
  msg.RST = reader().has_error();
  if ( !first_ack ) {
    first_ack = true;
    sequence_numbers_in_flight_ += msg.sequence_length();
    transmit( msg );
  }
  if ( payload_size > 0 ) {
    unacknowledged_messages_.push( msg );
    sequence_numbers_in_flight_ += msg.sequence_length();
    transmit( msg );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  auto msg = TCPSenderMessage();
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.SYN = false; // 这个函数时用来确认连接是否在持续的，因此SYN为false
  msg.FIN = writer().is_closed();
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( !msg.ackno.has_value() || unacknowledged_messages_.empty() )
    return;
  window_size_ = msg.window_size;
  auto ack_no_ = ( msg.ackno.value() ).unwrap( isn_, next_seqno_ );
  auto first_unacked_seqno_ = unacknowledged_messages_.front().seqno.unwrap( isn_, next_seqno_ );
  if ( ack_no_ < first_unacked_seqno_ ) {
    return; // 对旧信息的确认，不予理会
  }
  while ( !unacknowledged_messages_.empty() && ack_no_ >= first_unacked_seqno_ ) {
    auto& message = unacknowledged_messages_.front();
    if ( message.seqno.unwrap( isn_, next_seqno_ ) + message.sequence_length() <= ack_no_ ) {
      curr_RTO_ms_ = initial_RTO_ms_;   // 重置当前重传超时
      consecutive_retransmissions_ = 0; // 重置连续重传计数器
      sequence_numbers_in_flight_ -= message.sequence_length();
      unacknowledged_messages_.pop();
      if ( !unacknowledged_messages_.empty() ) {
        first_unacked_seqno_ = unacknowledged_messages_.front().seqno.unwrap( isn_, next_seqno_ );
      } else {
        first_unacked_seqno_ = next_seqno_; // 如果没有未确认的消息，重置为下一个序列号
      }
    } else
      break;
  }
  if ( !unacknowledged_messages_.empty() ) {
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
    transmit( unacknowledged_messages_.front() );
    if ( window_size_ > 0 ) {
      ++consecutive_retransmissions_;
      curr_RTO_ms_ *= 2;
    }
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

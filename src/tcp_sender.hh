#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <queue>
#include <utility>

/*
核心需求如下：
1、记录接收端告知它的窗口大小（从 TCPReceiverMessages 中读取 window sizes）；
2、从 ByteStream 中读取 payload，在窗口大小允许的情况下追加控制位 SYN
和FIN（仅在最后一次发出消息时），并持续填充报文的 payload 部分直到窗口已满或没东西可以读，再将其发送出去；
3、追踪哪些已发的报文是没收到回复的，这部分报文被称为“未完成的字节”；
4、未完成的字节在足够长的时间后依然没得到确认，重传（也就是指 TCP 的自动重传部分）。
*/

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), curr_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // TCPSenderMessage make_message( uint64_t seqno, std::string payload, bool SYN, bool FIN = false ) const;

  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  const uint64_t initial_RTO_ms_;

  uint64_t next_seqno_ { 0 };                               // 将要发送的下一个字节序号
  uint64_t impossible_ackno { 0 };                          // 不可能的确认序号
  uint64_t window_size_ { 0 };                              // 窗口大小
  uint64_t curr_RTO_ms_;                                    // 当前超时重传时延
  uint64_t last_tick_ms_ { 0 };                             // 自上次重置或第一次启动以来，过去的时间
  uint64_t consecutive_retransmissions_ { 0 };              // 超时重传次数
  uint64_t sequence_numbers_in_flight_ { 0 };               // 未确认的字节总数
  std::queue<TCPSenderMessage> unacknowledged_messages_ {}; // 未确认的字节序列
  bool first_ack { false };                                 // SYN信号已发送，用于保证全局只会push一次SYN信号
  bool is_closed_ { false };                                // 连接将要关闭，但尚未发送FIN
  bool FIN_sent { false };                                  // FIN信号已发送，用于保证只会push一次FIN信号
  bool established { false };                               // 已建立连接
};

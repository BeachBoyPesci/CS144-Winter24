#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

using namespace std;

void TCPSender::push( const TransmitFunction& transmit ) {}

TCPSenderMessage TCPSender::make_empty_message() const {}

void TCPSender::receive( const TCPReceiverMessage& msg ) {}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit ) {}

TCPSenderMessage TCPSender::make_message( uint64_t seqno, string payload, bool SYN, bool FIN ) const {}

uint64_t TCPSender::consecutive_retransmissions() const {}

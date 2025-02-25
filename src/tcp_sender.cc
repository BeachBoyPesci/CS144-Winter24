#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const {}

uint64_t TCPSender::consecutive_retransmissions() const {}

void TCPSender::push( const TransmitFunction& transmit ) {}

TCPSenderMessage TCPSender::make_empty_message() const {}

void TCPSender::receive( const TCPReceiverMessage& msg ) {}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit ) {}
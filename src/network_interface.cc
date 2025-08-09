#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

// 在该方法中，我们要将给定的下一跳 IP 地址转换为对应的以太网地址，并把 IP 数据报封装为以太网帧的 payload。
// 当目的以太网地址已知时，使用serialize() 函数将 dgram 序列化为 std::vector<std::string>类型，并装入
// EthernetFrame::payload 中； 接着完成以太网帧头部EthernetFrame::header
// 的变量设置，最后把组装好的数据帧转发出去。 如果目的以太网地址未知，这时就需要组装一个 ARPMessage
// 请求对应的以太网地址，再将这个ARP请求序列化后装载以太网帧中发出。 文档提到：为了避免频繁的 ARP
// 请求阻塞网络，我们需要保证五秒内，相同的 IP 地址的 ARP 请求只发出一次。
//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // 当调用者（如你的TCPConnection或路由器）希望将出站互联网（IP）数据报发送到下一跳时调用此方法。你的接口的任务是将此数据报转换为以太网帧并（最终）发送它。
  const auto ip = next_hop.ipv4_numeric();
  // 如果目标以太网地址已知，立即发送。创建一个以太网帧（类型为EthernetHeader::TYPE_IPv4），将有效载荷设置为序列化的数据报，并设置源地址和目标地址。
  if ( arp_map_.contains( ip ) ) {
    EthernetHeader header {
      .dst = arp_map_[ip].first, .src = ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
    transmit( { .header = header, .payload = serialize( dgram ) } );
  }
  // 如果目标以太网地址未知，广播一个ARP请求以获取下一跳的以太网地址，并将IP数据报排队，以便在收到ARP回复后发送。
  else {
    if ( wait_list_.contains( ip ) ) // 过去5000ms内已发送过相同IP地址的ARP请求
      return;
    EthernetHeader header { .dst = ETHERNET_BROADCAST, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP };
    ARPMessage req { .opcode = ARPMessage::OPCODE_REQUEST,
                     .sender_ethernet_address = ethernet_address_,
                     .sender_ip_address = ip_address_.ipv4_numeric(),
                     .target_ethernet_address = ETHERNET_REQUEST_ADDRESS,
                     .target_ip_address = ip };
    transmit( { .header = header, .payload = serialize( req ) } );
    wait_list_[ip].first.emplace_back( dgram );
  }
  // 例外：你不想用ARP请求淹没网络。如果网络接口在过去5秒内已发送过相同IP地址的ARP请求，不要发送第二个请求——只需等待第一个请求的回复。同样，将数据报排队直到你获取目标以太网地址。
}

// 这个方法需要过滤掉目的以太网地址既不是广播地址（ETHERNET_BROADCAST）、也不是本接口的以太网地址（ehternet_address_）的数据帧。
// 如果数据帧的目的地址是本接口，那么就需要按照数据帧头部指出的协议类型，将数据帧解析为对应的数据报类型（使用parse()）。
// 当数据帧的协议是 IPv4 时，且解析成功（parse()返回值为 true），那么就把解析得到的
// InternetDatagram数据报推入队列datagrams_received_中。
// 若数据帧协议为 ARP，且解析成功，那么按照得到的
// ARPMessage中的信息，分析是否是请求本接口的 IP地址和以太网地址的映射关系、或者是否是响应之前本接口发出的 ARP
// 请求。 如果是ARP 请求，那么就组装对应的ARPMessage 并发送给请求发送方；如果是 ARP
// 响应，那么就将先前缓存的、现在能发送的IP数据报全部发送出去。 注意这里无论是 ARP
// 请求还是ARP响应，只要数据帧解析成功，都要从中学习新的地址映射关系。
//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 当以太网帧从网络到达时调用此方法。代码应忽略任何非目标地址的帧（即以太网目标地址不是广播地址或接口自身的以太网地址，存储在_ethernet_address成员变量中）。
  const auto& header = frame.header;
  if ( header.dst != ETHERNET_BROADCAST && header.dst != ethernet_address_ ) {
    return;
  }
  // 如果传入的帧是IPv4，将有效载荷解析为InternetDatagram，如果成功（parse()方法返回ParseResult::NoError），将结果数据报推送到datagrams_received_queue。
  if ( header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( dgram );
    }
  }

  else if ( header.type == EthernetHeader::TYPE_ARP ) {
    // 如果传入的帧是ARP，将有效载荷解析为ARPMessage
    ARPMessage arp;
    if ( parse( arp, frame.payload ) ) {
      // 学习新的地址映射关系
      const auto sender_ip = arp.sender_ip_address;
      const auto sender_ethernet = arp.sender_ethernet_address;
      arp_map_[sender_ip].first = sender_ethernet;
      // 如果是询问我们IP地址的ARP请求，发送一个适当的ARP回复。
      if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
        EthernetHeader reply_header {
          .dst = arp.sender_ethernet_address, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP };
        ARPMessage reply { .opcode = ARPMessage::OPCODE_REPLY,
                           .sender_ethernet_address = ethernet_address_,
                           .sender_ip_address = ip_address_.ipv4_numeric(),
                           .target_ethernet_address = arp.sender_ethernet_address,
                           .target_ip_address = arp.sender_ip_address };
        transmit( { .header = reply_header, .payload = serialize( reply ) } );
      }
      // 如果是 ARP 响应，那么就将先前缓存的、现在能发送的IP数据报全部发送出去。
      else {
        if ( wait_list_.contains( sender_ip ) ) {
          for ( auto& dgram : wait_list_[sender_ip].first ) {
            EthernetHeader dgram_header {
              .dst = sender_ethernet, .src = ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
            transmit( { .header = dgram_header, .payload = serialize( dgram ) } );
          }
        }
        wait_list_.erase( sender_ip );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 随着时间流逝调用此方法。使任何已过期的IP到以太网映射失效。
  for ( auto it = arp_map_.begin(); it != arp_map_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= ARP_MAP_TTL ) {
      it = arp_map_.erase( it );
    } else {
      ++it;
    }
  }
  for ( auto it = wait_list_.begin(); it != wait_list_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= ARP_RETX_PERIOD ) {
      it = wait_list_.erase( it );
    } else {
      ++it;
    }
  }
}

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

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
//  如果找到对应的ARP条款，直接封装转发。
  auto iter = arp_table_.find(next_hop.ipv4_numeric());
  if(iter!=arp_table_.end()){
    EthernetHeader header{
      .dst = iter->second.ethernet_address,
      .src = ethernet_address_,
      .type = EthernetHeader::TYPE_IPv4
    };
    Serializer frame_serializer;
    dgram.serialize(frame_serializer);
    EthernetFrame frame{
      .header = header,
      .payload = frame_serializer.output()
    };
    transmit(frame);
    return;
  }
  //  将未发送的数据报放入队列
  dgram_buffered_.emplace_back(next_hop.ipv4_numeric(),dgram);
//  如果5s前已经发送了ARP请求，那么直接返回
  if(req_table_.find(next_hop.ipv4_numeric())!=req_table_.end())
    return;
  //  如果没有找到，运行ARP协议。
  EthernetHeader header{
    .dst = ETHERNET_BROADCAST,
    .src = ethernet_address_,
    .type = EthernetHeader::TYPE_ARP
  };
  ARPMessage message{
    .opcode = ARPMessage::OPCODE_REQUEST,
    .sender_ethernet_address = ethernet_address_,
    .sender_ip_address = ip_address_.ipv4_numeric(),
    .target_ip_address = next_hop.ipv4_numeric(),
  };
  Serializer arp_serializer;
  message.serialize(arp_serializer);
  EthernetFrame frame{
    .header = header,
    .payload = arp_serializer.output()
  };
  transmit(frame);
  req_table_[next_hop.ipv4_numeric()] = 0;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
//  排除非指向本接口的帧
  if(not compare_ethernet_address(frame.header.dst,ETHERNET_BROADCAST)\
     and not compare_ethernet_address(frame.header.dst,ethernet_address_))
    return;
//  如果是IPV4报文，就将IPV4报文放入队列中
  if(frame.header.type==EthernetHeader::TYPE_IPv4){
    Parser parser{frame.payload};
    IPv4Datagram datagram;
    datagram.parse(parser);
    if(parser.has_error())
      return;
    datagrams_received_.push(std::move(datagram));
  }
//  如果是ARP报文。
  else if(frame.header.type==EthernetHeader::TYPE_ARP){
    ARPMessage msg;
    Parser parser{frame.payload};
    msg.parse(parser);
    if(parser.has_error())
      return;
//    如果是ARP请求报文，则对比自身IP是否为目的IP
    if(msg.opcode==ARPMessage::OPCODE_REQUEST){
      if(ip_address_.ipv4_numeric()!=msg.target_ip_address){
        return;
      }
      ARPMessage ret_msg{
        .opcode = ARPMessage::OPCODE_REPLY,
        .sender_ethernet_address = ethernet_address_,
        .sender_ip_address = ip_address_.ipv4_numeric(),
        .target_ethernet_address = msg.sender_ethernet_address,
        .target_ip_address = msg.sender_ip_address,
      };
      EthernetHeader header{
        .dst = msg.sender_ethernet_address,
        .src = ethernet_address_,
        .type = EthernetHeader::TYPE_ARP
      };
      Serializer serializer;
      ret_msg.serialize(serializer);
      transmit(EthernetFrame{.header = header,.payload = serializer.output()});
    }
//  无论是ARP响应报文还是请求报文，都进行学习，并立即尝试发送数据报
    arp_table_.emplace(msg.sender_ip_address,ArpEntry{.ethernet_address=msg.sender_ethernet_address,.last_time_ms=0});
//    查找待发送数据报
    for(auto iter = dgram_buffered_.begin();iter!=dgram_buffered_.end();){
      if(iter->first==msg.sender_ip_address){
        EthernetHeader header{
          .dst = msg.sender_ethernet_address,
          .src = ethernet_address_,
          .type = EthernetHeader::TYPE_IPv4
        };
        Serializer frame_serializer;
        iter->second.serialize(frame_serializer);
        EthernetFrame ethernet_msg{
          .header = header,
          .payload = frame_serializer.output()
        };
        transmit(ethernet_msg);
        iter = dgram_buffered_.erase(iter);
      }else{
        iter++;
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for(auto iter = arp_table_.begin();iter!=arp_table_.end();){
    iter->second.last_time_ms+=ms_since_last_tick;
    if(iter->second.last_time_ms>=arp_max_time){
      iter = arp_table_.erase(iter);
    }else{
      iter++;
    }
  }
  for(auto iter = req_table_.begin();iter!=req_table_.end();){
    iter->second+=ms_since_last_tick;
    if(iter->second>=req_max_time){
      iter = req_table_.erase(iter);
    }else{
      iter++;
    }
  }
}

#include "router.hh"

#include <iostream>
#include <limits>
#include <valarray>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  RouterItem input{
    .route_prefix = route_prefix,
    .prefix_length = prefix_length,
    .next_hop = next_hop,
    .interface_num = interface_num
  };
  auto iter = router_table.begin();
  while(iter!=router_table.end()){
    if(*iter==input)
      return;
    if(iter->prefix_length<input.prefix_length){
      router_table.insert(iter,input);
      return;
    }
    iter++;
  }
  if(iter==router_table.end()){
    router_table.push_back(input);
  }
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto&& net_iter :_interfaces){
      while(not net_iter->datagrams_received().empty()){
        InternetDatagram& datagram = net_iter->datagrams_received().front();
        for(auto&&rt_iter:router_table){
          if(((UINT32_MAX-(uint32_t)pow(2,32-rt_iter.prefix_length)+1)&datagram.header.dst)==rt_iter.route_prefix){
            if(datagram.header.ttl==0 or datagram.header.ttl==1) break;
            datagram.header.ttl-=1;
            datagram.header.compute_checksum();
            _interfaces[rt_iter.interface_num]->send_datagram(datagram,\
                                                               rt_iter.next_hop.has_value()?\
                                                               rt_iter.next_hop.value():\
                                                               Address::from_ipv4_numeric(datagram.header.dst));
            break;
          }
        }
        net_iter->datagrams_received().pop();
      }
  }
}

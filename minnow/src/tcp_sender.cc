#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <assert.h>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
    return static_cast<uint64_t>(endno.value()+1-ackno.value());
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmit_times;
}

void TCPSender::push( const TransmitFunction& transmit )
{
//  如果已经完成FIN的发送，直接退出不再发送
  if(is_final_push) return;
//  此时的窗口容量
  uint64_t window_size;
  uint16_t pretend_rwnd = rwnd==0?1:rwnd;
  if(pretend_rwnd>endno.value()+1-ackno.value()){
    window_size = (uint64_t)pretend_rwnd-(uint64_t)(endno.value()+1-ackno.value());
  }else{
    window_size = 0;
  }
  uint64_t buffer_size = reader().bytes_buffered();
//  第一次发送信息，默认发送SYN，payload为0
  if(!is_first_push and window_size){
    TCPSenderMessage msg{isn_, true,"",reader().is_finished(),reader().has_error()};
    transmit(msg);
    seg_in_flight.push_back(msg);
    endno = isn_+reader().is_finished();
    is_first_push = true;
    return;
  }
  if(reader().is_finished() and window_size){
    endno = endno+1;
    TCPSenderMessage msg{endno, false,"",reader().is_finished(),reader().has_error()};
    transmit(msg);
    seg_in_flight.push_back(msg);
    is_final_push = true;
    return;
  }
//  计算此时最大传输的数据量
  uint16_t max_size = (uint16_t)std::min(window_size,buffer_size);
  uint16_t segment_number = max_size/TCPConfig::MAX_PAYLOAD_SIZE;
  uint16_t final_str_size = max_size%TCPConfig::MAX_PAYLOAD_SIZE;
  uint16_t cur_max_size;
  string cur_str;
  for(uint16_t i = 0,cur_size = 0;i<=segment_number;i++){
    cur_max_size = (i==segment_number)?final_str_size:TCPConfig::MAX_PAYLOAD_SIZE;
    if(!cur_max_size)
      continue;
//    装填payload
    cur_str.clear();
    cur_size = 0;
    while(cur_size!=cur_max_size){
      assert(cur_size<cur_max_size);
      string_view top_str = reader().peek();
      assert(!top_str.empty());
      if(cur_size+top_str.size()<=cur_max_size){
        cur_str+=top_str;
        input_.reader().pop(top_str.size());
        cur_size+=top_str.size();
      }else{
        cur_str+=top_str.substr(0,cur_max_size-cur_size);
        input_.reader().pop(cur_max_size-cur_size);
        cur_size = cur_max_size;
      }
    }
//    如果受到的是window_size的限制，那么不能加FIN
    bool is_FIN_set = reader().is_finished()&&(window_size>buffer_size);
    TCPSenderMessage msg{endno+1, false,cur_str,is_FIN_set,reader().has_error()};
    transmit(msg);
    seg_in_flight.push_back(msg);
    endno = endno+cur_max_size+is_FIN_set;
    is_final_push = is_FIN_set;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage{endno+1, false,"",false,reader().has_error()};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  rwnd = msg.window_size;
  if(msg.RST) writer().set_error();
  if(msg.ackno.has_value()) {
    for(auto iter = seg_in_flight.begin();iter!=seg_in_flight.end();iter++){
//      说明此时ackno绝对地址大于之前的ackno
      if(iter->seqno+iter->payload.size()+iter->SYN+iter->FIN==msg.ackno.value()){
        ackno = msg.ackno.value();
        seg_in_flight.erase(seg_in_flight.begin(),++iter);
        current_RTO_ms_ = initial_RTO_ms_;
        retransmit_times = 0;
        tcp_clock.restart();
        break;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  tcp_clock.add_duration(ms_since_last_tick);
//  如果没超时
  if(tcp_clock.get_duration()<current_RTO_ms_){
    return;
  }
//  如果超时
  if(!seg_in_flight.empty()){
    tcp_clock.restart();
    if(rwnd) current_RTO_ms_*=2;
    retransmit_times++;
    TCPSenderMessage msg{seg_in_flight.front().seqno,\
                           seg_in_flight.front().SYN,\
                           seg_in_flight.front().payload,\
                           seg_in_flight.front().FIN,\
                           reader().has_error()};
    transmit(msg);
  }
}

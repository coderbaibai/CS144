#include "tcp_receiver.hh"

using namespace std;

// Receiver只负责将消息的序列号转换成绝对序列号，再转交给reassembler
void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST){
    reader().set_error();
  }
  if(message.SYN){
    isn = Wrap32{message.seqno};
//    这里加一是为了让SYN报文段的索引为0，从而防止重排器不对FIN进行判断
//    并且如果有数据，重排器可以将数据插入
    message.seqno=message.seqno+1;
  }
  if(isn.has_value()){
    reassembler_.insert(\
      message.seqno.unwrap(isn.value(),reassembler_.getHeader()+1)-1,\
      std::move(message.payload),\
      message.FIN);
  }
  else{
    reader().set_error();
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  return TCPReceiverMessage{
    (isn.has_value())?std::optional{Wrap32::wrap(reassembler_.getHeader()+1,isn.value())}:nullopt,\
    reassembler_.window_size(),\
    writer().has_error()};
}

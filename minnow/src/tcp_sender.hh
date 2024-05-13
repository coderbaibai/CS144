#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <deque>

class TcpSenderClock{
public:
  inline void restart(){duration = 0;}
  inline void add_duration(uint64_t addition) {duration+=addition;}
  inline uint64_t get_duration() const {return duration;}
private:
  uint64_t duration{0};
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ),
    isn_( isn ), ackno(isn), endno(isn.value()-1),
    initial_RTO_ms_( initial_RTO_ms ),
    current_RTO_ms_( initial_RTO_ms )
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
// 外部定期调用tick，并且告诉tick上次tick过去了多久
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  Wrap32 ackno;
  Wrap32 endno;
  uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  uint16_t rwnd{1};
  bool is_first_push{false};
  bool is_final_push{false};
  std::deque<TCPSenderMessage> seg_in_flight{};
  TcpSenderClock tcp_clock{};
  uint16_t retransmit_times{0};
};

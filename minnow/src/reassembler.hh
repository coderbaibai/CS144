#pragma once

#include "byte_stream.hh"
#include <deque>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

  uint64_t getHeader() const {return head_index;};

  uint16_t window_size() const {
    return UINT16_MAX<writer().available_capacity()?UINT16_MAX:writer().available_capacity();
  };

private:
  void write_into_output();
//  尚未收到的最小地址
  uint64_t head_index{};
  struct BufSegment{
      uint64_t start;
      std::string data;
      BufSegment(uint64_t start_in,std::string&& data_in):start(start_in),data(std::move(data_in)){};
      BufSegment(BufSegment&&input) noexcept : start(input.start),data(std::move(input.data)){};
      BufSegment(const BufSegment&input) = default;
      BufSegment& operator=(const BufSegment&input)= default;
      BufSegment& operator=(BufSegment&&input) noexcept {
        start = input.start;
        data = std::move(input.data);
        return *this;
      };
  };
  enum class SegPoseType:char{SP_BEFORE,SP_INNER,SP_END};
  struct SegPose{
    SegPoseType type = SegPoseType::SP_END;
    std::deque<BufSegment>::iterator iter{};
  };
  std::pair<SegPose,SegPose> get_seg_pose(const BufSegment& target);
  bool slice_input(uint64_t& first_index, std::string& data, bool& is_last_substring);
  std::deque<BufSegment> segment_queue{};
  ByteStream output_; // the Reassembler writes to this ByteStream
  bool is_finished{false};
};

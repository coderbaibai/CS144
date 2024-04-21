#include "byte_stream.hh"
using namespace std;


ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {
}

bool Writer::is_closed() const
{
  return eof;
}

// 这里源程序会提供一个构造好的独立的string,再拷贝一次并不明智
void Writer::push( string data )
{
  uint64_t size = std::min(data.size(),capacity_-cur_size);
  data.resize(size);
  if(data.empty())
    return;
  buf.push(move(data));
  total+=size;
  cur_size+=size;
}

void Writer::close()
{
  eof  = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_-cur_size;
}

uint64_t Writer::bytes_pushed() const
{
  return total;
}

bool Reader::is_finished() const
{
  return buf.empty()&&eof;
}

uint64_t Reader::bytes_popped() const
{
  return total-cur_size;
}

string_view Reader::peek() const
{
  if(buf.empty()) {
    return "";
  }
  string_view res = buf.front();
  res.remove_prefix(top_cur);
  return res;
}


void Reader::pop( uint64_t len )
{
  cur_size-=len;
  while(!buf.empty()){
    if(buf.front().size()<len+top_cur) {
      buf.pop();
      len-=buf.front().size()-top_cur;
      top_cur = 0;
    }else if(buf.front().size()>len+top_cur){
      top_cur+=len;
      break;
    }else{
      buf.pop();
      top_cur = 0;
      break;
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  return cur_size;
}

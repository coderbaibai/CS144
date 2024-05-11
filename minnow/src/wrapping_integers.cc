#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32{static_cast<uint32_t>((static_cast<uint64_t>(zero_point.raw_value_) + n)&0xffffffff)};
}

uint64_t Wrap32::udistance( uint64_t a, uint64_t b ){
  return (a<b)? b-a:a-b;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t cnt = (checkpoint>>32)&0x00000000ffffffff;
  if(raw_value_<zero_point.raw_value_) cnt++;
  uint64_t tempA = (cnt<<32)+raw_value_-zero_point.raw_value_;
  uint64_t tempB = ((cnt+1)<<32)+raw_value_-zero_point.raw_value_;
  uint64_t tempC = ((cnt-1)<<32)+raw_value_-zero_point.raw_value_;
  if( udistance(tempA,checkpoint)< udistance(tempB,checkpoint)){
    if( udistance(tempA,checkpoint)< udistance(tempC,checkpoint))
      return tempA;
    else return tempC;
  }else {
    if(udistance(tempB,checkpoint)< udistance(tempC,checkpoint))
      return tempB;
    else return tempC;
  }
}

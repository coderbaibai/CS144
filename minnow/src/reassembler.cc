#include "reassembler.hh"
#include "exception.hh"

using namespace std;

pair<Reassembler::SegPose,Reassembler::SegPose>
Reassembler::get_seg_pose(const BufSegment& target){
    pair<SegPose,SegPose> res;
    res.first.iter = segment_queue.end();
    res.second.iter = segment_queue.end();
    for(auto iter = segment_queue.begin();iter!=segment_queue.end();iter++){
      if(target.start<iter->start){
        res.first.iter = iter;
        res.first.type = SegPoseType::SP_BEFORE;
        break;
      }
      if(target.start>=iter->start and target.start<iter->start+iter->data.size()) {
        res.first.iter = iter;
        res.first.type = SegPoseType::SP_INNER;
        break;
      }
    }
    for(auto iter = segment_queue.begin();iter!=segment_queue.end();iter++){
      if(target.start+target.data.size()<iter->start){
        res.second.iter = iter;
        res.second.type = SegPoseType::SP_BEFORE;
        break;
      }
      if(target.start+target.data.size()>=iter->start and target.start+target.data.size()<=iter->start+iter->data.size()) {
        res.second.iter = iter;
        res.second.type = SegPoseType::SP_INNER;
        break;
      }
    }
    return res;
}
bool
Reassembler::slice_input(uint64_t& first_index, std::string& data, bool& is_last_substring){
  //  先将超额部分排除
  //  计算索引上限
  uint64_t max_index = head_index + output_.writer().available_capacity();
  if(first_index>=max_index)
    return true;
  if(first_index+data.size()>max_index){
    data.erase(data.begin()+static_cast<int>(max_index-first_index),data.end());
    is_last_substring = false;
  }
  //  再将滞后部分排除
  if(first_index+data.size()<=head_index){
    //    退出前把终止信息设置
    is_finished|=is_last_substring;
    return true;
  }
  if(first_index<head_index&&first_index+data.size()>head_index){
    data.erase(data.begin(),data.begin()+static_cast<int>(head_index-first_index));
    first_index = head_index;
  }
  return false;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring ){
//  如果没有空闲区域，直接返回
  if(output_.writer().available_capacity()==0)
    return;
  if( slice_input(first_index,data,is_last_substring)){
    write_into_output();
    return;
  }
  BufSegment target{first_index,std::move(data)};
  auto seg_poses = get_seg_pose(target);
  auto temp_iter = segment_queue.begin();
  switch ( seg_poses.first.type ) {
    case SegPoseType::SP_BEFORE:
      switch ( seg_poses.second.type ) {
        case SegPoseType::SP_BEFORE:
          temp_iter = segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          segment_queue.insert(temp_iter,std::move(target));
          break;
        case SegPoseType::SP_INNER:
          temp_iter = segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          target.data.erase(target.data.begin()+static_cast<int>(temp_iter->start-target.start),\
                             target.data.end());
          segment_queue.insert(temp_iter,std::move(target));
          break;
        case SegPoseType::SP_END:
          segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          segment_queue.push_back(std::move(target));
          break;
      }
      break;
    case SegPoseType::SP_INNER:
      switch ( seg_poses.second.type ) {
        case SegPoseType::SP_BEFORE:
          target.data.erase(target.data.begin(),\
                             target.data.begin()+\
                               static_cast<int>(seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start));
          target.start+=seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start;
          seg_poses.first.iter++;
          temp_iter = segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          segment_queue.insert(temp_iter, std::move(target));
          break;
        case SegPoseType::SP_INNER:
          if(seg_poses.first.iter==seg_poses.second.iter)
            break;
          target.data.erase(target.data.begin(),\
                             target.data.begin()+\
                               static_cast<int>(seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start));
          target.start+=seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start;
          seg_poses.first.iter++;
          temp_iter = segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          target.data.erase(target.data.begin()+static_cast<int>(temp_iter->start-target.start),\
                             target.data.end());
          segment_queue.insert(temp_iter, std::move(target));
          break;
        case SegPoseType::SP_END:
          target.data.erase(target.data.begin(),\
                             target.data.begin()+\
                               static_cast<int>(seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start));
          target.start+=seg_poses.first.iter->start+seg_poses.first.iter->data.size()-target.start;
          seg_poses.first.iter++;
          segment_queue.erase(seg_poses.first.iter,seg_poses.second.iter);
          segment_queue.push_back(std::move(target));
          break;
      }
      break;
    case SegPoseType::SP_END:
      switch ( seg_poses.second.type ) {
        case SegPoseType::SP_BEFORE:
          throw runtime_error("error segment type");
          break;
        case SegPoseType::SP_INNER:
          throw runtime_error("error segment type");
          break;
        case SegPoseType::SP_END:
          segment_queue.push_back(std::move(target));
          break;
      }
      break;
  }
    is_finished |= is_last_substring;
    // 最后，更新首索引，尝试写入缓冲区。
    if(first_index==head_index) write_into_output();
}

void Reassembler::write_into_output(){
  if(segment_queue.empty()){
    if(is_finished){
      head_index++;
      output_.writer().close();
    }
    return;
  }
  if(segment_queue.front().start==head_index){
    head_index+=segment_queue.front().data.size();
    output_.writer().push(std::move(segment_queue.front().data));
    segment_queue.pop_front();
    write_into_output();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t sum{};
  for(auto&&it :segment_queue){
    sum+=it.data.size();
  }
  return sum;
}

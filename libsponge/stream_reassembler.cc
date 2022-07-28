#include "stream_reassembler.hh"

#include <assert.h>
#include <cstddef>
#include <iostream>
#include <tuple>
#include <utility>
#include <algorithm>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

namespace {

    size_t update_unassembled_bytes(std::map<size_t, std::string> &cache_, const string &data, size_t index)
    {
        auto [change_bytes,
              ins,
              index_end
                //todo 为什么lower_bound不是找下界?
             ] =std::tuple{static_cast<size_t>(0),cache_.end(),index+data.size()};

        //////////////////
        // 有更好的方法尝试寻找index的前继吗?到头来,貌似使用map就一个排序的功能了,好鸡肋
        auto t = cache_.insert({index, data});
        auto ib                                                                     = cache_.end();
        if (!t.second) {
            assert(index==t.first->first);
            if (data.size() > t.first->second.size()) {
                change_bytes-=t.first->second.size();
                index+=t.first->second.size();
                t.first->second = data;
                change_bytes += data.size();
                ib=t.first;
            } else {
                return change_bytes;
            }
        }else{
            //尝试找到前一块
            if (t.first != cache_.begin()) {
                ib = t.first;
                ib--;
            }
            ////////////////

            //todo 初始大小如何增加
            //有前一块且和前一块有交集,先行处理一次
            if (ib!=cache_.end()&&(ib->first + ib->second.size() >= index)) {
                assert(ib->first <= index);
                if (ib->first + ib->second.size() < index_end) {
                    const auto &s = data.substr(ib->first + ib->second.size() - index);
                    change_bytes-=ib->first + ib->second.size() - index;
                    ib->second.append(s);
                    index += s.size();
                    change_bytes += s.size();  //提前增加大小
                    cache_.erase(t.first);//此后t失效
                } else {
                    cache_.erase(t.first);
                    return change_bytes;  // nothing to do
                }
            } else {
                ib = t.first;
                change_bytes+=data.size();
            }
        }

        //现在没有交集了
        ins = ib;
        ins++;
        
        //循环内不需要创建新字符串
        while (ins != cache_.end() && ins->first + ins->second.size() <= index_end) {
            auto ci = ins->first + ins->second.size();
            change_bytes -= ins->second.size();//减少的字节
            // change_bytes += ci - index;  //增加的字节
            index = ci;
            cache_.erase(ins++);
        }
        if (ins != cache_.end()&&index_end>=ins->first) {
            const auto &end_str = ins->second.substr(index_end - ins->first);
            ib->second.append(end_str);
            change_bytes-=ins->second.size();
            index_end += end_str.size();
            change_bytes+=end_str.size();
            index=index_end;//没什么必要
            cache_.erase(ins);//ins就失效了
        }

        return change_bytes;
    }
}  // namespace

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{

    // std::cout<<"索引是:"<<index<<",数据长度:"<<data.size()<<std::endl;
    //have no aviable space,just discard
    if(_output.remaining_capacity()==0){
        return;
    }

    //duplicate data,discard it
    if(index+data.size()<next_index_)
        return;
  
    //we hava a little free space left

    // we try to merge the data into those from cache_ first

    auto index_act = index;
    auto remainder=0;
    if (index_act < next_index_) {
        index_act=next_index_;
    }
    // data以及index必定在窗口内(容量),不可能越界,即:index+data.size()<=next_index_+_output.remaining_capacity()
    // ! 测试案例居然传递超过窗口大小的字节,不可思议,断言不成立
    // assert(index+data.size()<=next_index_+_output.remaining_capacity());
    assert(index_act <= next_index_ + _output.remaining_capacity());
    if (index+data.size() > next_index_ + _output.remaining_capacity())
        remainder=index+data.size()-(next_index_ + _output.remaining_capacity());
    unassembled_+=update_unassembled_bytes(cache_,data.substr(index_act-index,data.size()-remainder-(index_act-index)),index_act);

    auto dd = cache_.begin();
    auto df = dd->first;
    
    // std::cout<<"cache_.begin:"<<df<<",数据长度:"<<dd->second.size()<<std::endl;
    if (df == next_index_) {
        assert(unassembled_ <= _output.remaining_capacity());
        auto len = _output.write(dd->second);
        assert(len == dd->second.size());
        unassembled_ -= len;
        next_index_+=len;
        cache_.erase(dd);
        // for debug
        std::cout<<"next_index_:"<<next_index_<<std::endl;
    }
    if (eof ) {
        p_eof_.first = true;
        p_eof_.second=index+data.size();
    }
    if (p_eof_.first&&next_index_ == p_eof_.second)
        _output.end_input();

//    DUMMY_CODE(data, index, eof);
}


size_t StreamReassembler::unassembled_bytes() const {
    return unassembled_;
}

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }

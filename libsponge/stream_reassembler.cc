#include "stream_reassembler.hh"

#include <assert.h>
#include <cstddef>
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
/*
    size_t put_into_cache(std::map<size_t, std::string> &cache_, const string &data, size_t index) 
    {
        size_t extra_byte_n_ = 0;
        // we must guarantee that there is no overlap substrings in cache_

        // No matter what the situation is, we first try to insert the substring into the cache_.
        auto it = cache_.find(index);
        if (it != cache_.end()) {
            if (auto oldsize = it->second.size(); oldsize < data.size()) {
                it->second = data;
                extra_byte_n_ += it->second.size() - oldsize;
            } else
                return extra_byte_n_;
        } else if (auto pair = cache_.insert({index, data}); pair.second) {
            it = pair.first;
            extra_byte_n_ += data.size();
        } else {
            return extra_byte_n_;  // did we get here and with  something wrong?
        }
        // we have made "it" right value.

        //无论如何,这里找到的上界是足够正确的
        auto upper_b = cache_.upper_bound(index + data.size());

        // if there are overlap substrings in cache_,the following deals with it;

        //检查是否和前面的子串重叠
        if (auto i = it; i-- != cache_.begin() && i->first + i->second.size() >= index) {
            if (index + data.size() <= i->first + i->second.size()) {
                extra_byte_n_ -= it->second.size();
                cache_.erase(it);
            } else {
                auto &reserve = i->second;
                extra_byte_n_ -= reserve.size() - (index - i->first);
                reserve.resize(index - i->first);  //取这个子串的前缀作为开始部分
                reserve.append(std::move(it->second));
                size_t max_offset = i->first + reserve.size();
                i = it;  // now both i and it point to the same thing
                ++i;
                cache_.erase(it);  // don't use it any more
                //把属于[index,data.size()]之间的条目都删除
                for (auto j = i; j != upper_b;) {
                    if (j->first + j->second.size() > max_offset) {
                        if (max_offset == j->first)
                            break;
                        extra_byte_n_ -= max_offset - j->first;
                        reserve.append(j->second.substr(max_offset - j->first));
                        cache_.erase(j);
                        break;
                    }
                    extra_byte_n_ -= j->second.size();
                    cache_.erase(j++);
                }
            }
        } else {
            i = it;
            auto &reserve = i->second;
            size_t max_offset = i->first + reserve.size();
            ++i;
            // same as before
            for (auto j = i; j != upper_b;) {
                if (j->first + j->second.size() > max_offset) {
                    if (max_offset == j->first)
                        break;
                    extra_byte_n_ -= max_offset - j->first;
                    reserve.append(j->second.substr(max_offset - j->first));
                    cache_.erase(j);
                    break;
                }
                extra_byte_n_ -= j->second.size();
                cache_.erase(j++);
            }
        }
        return extra_byte_n_;
    }
*/
    size_t update_unassembled_bytes(std::map<size_t, std::string> &cache_, const string &data, size_t index)
    {
        auto [change_bytes,
              ins,
              index_end
                //todo 为什么lower_bound不是找下界?
             ] =std::tuple{static_cast<size_t>(0),cache_.end(),index+data.size()};

        //////////////////
        // 有更好的方法尝试寻找index的前继吗?到头来,貌似使用map就一个排序的功能了,好鸡肋
        auto t = cache_.insert({index, {}});
        assert(t.second==true);
        auto a = cache_.end();
        if (t.first != cache_.begin()) {
            a = t.first;
            a--;
        }
        cache_.erase(t.first);
        ////////////////

        //todo 初始大小如何增加
        //和前一块有交集,先行处理一次
        if (a != cache_.end() && (a->first + a->second.size() >= index)) {
            assert(a->first <= index);
            if (a->first + a->second.size() < index_end) {
                const auto & s=data.substr(a->first + a->second.size()-index);
                a->second.append(s);
                index += s.size();
                change_bytes+=s.size();//提前增加大小
            }
            else
                return change_bytes;//nothing to do
        } else {
            a = cache_.insert({index, data}).first;
            change_bytes+=data.size();//提前增加大小
        }
        //现在没有交集了
        ins = a;
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
            a->second.append(end_str);
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
    unassembled_+=update_unassembled_bytes(cache_,data.substr(index_act-index,data.size()-remainder),index_act);

    auto dd = cache_.begin();

    if (dd->first == next_index_) {
        assert(unassembled_ <= _output.remaining_capacity());
        auto len = _output.write(dd->second);
        assert(len == dd->second.size());
        unassembled_ -= len;
        next_index_+=len;
        cache_.erase(dd);
    }
    if (eof ) {
        p_eof_.first = true;
        p_eof_.second=index+data.size();
    }
    if (p_eof_.first&&next_index_ == p_eof_.second)
        _output.end_input();

        
    // if(eof)
    //     p_eof_={true,index+data.size()};

    // //we can assemble all or part of those chars
    // if(dd->first<=next_index_){
    //     auto offset=next_index_-dd->first;
    //     unassembled_-=offset;
    //     auto byten=_output.write(dd->second.substr(offset));
    //     unassembled_-=byten;
    //     next_index_+=byten;
    //     // if(dd->first+dd->second.size()==next_index_)
    //         cache_.erase(dd++);
    // }

    // while (dd!=cache_.end()&&dd->first==next_index_) {
    //     auto byten=_output.write(dd->second);
    //     auto ddol=dd->second.size();
    //     unassembled_-=byten;
    //     next_index_+=byten;
    //     cache_.erase(dd++);
    //     //have no free space to use
    //     if(byten<ddol){
    //         cache_.clear();
    //         unassembled_=0;
    //     }
    // }
    // if(p_eof_.first&&p_eof_.second==next_index_){
    //     _output.end_input();
    // }

    // //clear cache_ and calculate unassembled_
    // if(unassembled_>_output.remaining_capacity()){
    //     auto target_size=_output.remaining_capacity();
    //     for(auto rb=cache_.rbegin();rb!=cache_.rend();){
    //         unassembled_-=rb->second.size();
    //         cache_.erase(cache_.find((rb++)->first));//反向迭代器不能用于erase函数
    //         if(unassembled_<=target_size){
    //             break;
    //         }
    //     }
    // }

//    DUMMY_CODE(data, index, eof);
}


size_t StreamReassembler::unassembled_bytes() const {
    return unassembled_;
}

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }

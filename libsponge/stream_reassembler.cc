#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

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
        return ;

    //we hava a little free space left

    //we try to merge the data into those from cache_ first
    put_into_cache(data,index,eof);

    auto dd=cache_.begin();

    //we can assemble all or part of those chars
    if(dd->first<next_index_){
        auto offset=next_index_-dd->first;
        unassembled_-=offset;
        auto byten=_output.write(dd->second.substr(offset));
        unassembled_-=byten;
        next_index_+=byten;
        cache_.erase(dd++);
    }

    while (dd!=cache_.end()&&dd->first==next_index_) {
        auto byten=_output.write(dd->second);
        auto ddol=dd->second.size();
        unassembled_-=byten;
        next_index_+=byten;
        cache_.erase(dd++);
        //have no free space to use
        if(byten<ddol){
            cache_.clear();
            unassembled_=0;
        }
    }
    if(p_eof_.first&&p_eof_.second==next_index_){
        _output.end_input();
    }

    //clear cache_
    if(unassembled_>_output.remaining_capacity()){
        auto target_size=_output.remaining_capacity();
        for(auto rb=cache_.rbegin();rb!=cache_.rend();){
            unassembled_-=rb->second.size();
            cache_.erase(cache_.find((rb++)->first));//反向迭代器不能用于erase函数
            if(unassembled_<=target_size){
                break;
            }
        }
    }

//    DUMMY_CODE(data, index, eof);
}

void StreamReassembler::put_into_cache(const string &data,size_t index, bool eof)
{
    //we must guarantee that there is no overlap substrings in cache_

    //No matter what the situation is, we first try to insert the substring into the cache_.
    auto it=cache_.find(index);
    if(it!=cache_.end()){
        if(auto oldsize=it->second.size();oldsize<data.size()){
            it->second=data;
            unassembled_+=it->second.size()-oldsize;
        }else
            return;
    }else if(auto pair=cache_.insert({index,data});pair.second){
        it=pair.first;
        unassembled_+=data.size();
    }else{
        return;//did we get here and with  something wrong?
    }
    //we have made "it" right value.

    //无论如何,这里找到的上界是足够正确的
    auto upper_b=cache_.upper_bound(index+data.size());

    //if there are overlap substrings in cache_,the following deals with it;

    //检查是否和前面的子串重叠
    if(auto i=it;i--!=cache_.begin()&&i->first+i->second.size()>=index){
        if(index+data.size()<=i->first+i->second.size()){
            unassembled_-=it->second.size();
            cache_.erase(it);
        }else{
            auto & reserve=i->second;
            unassembled_-=reserve.size()-(index-i->first);
            reserve.resize(index-i->first);//取这个子串的前缀作为开始部分
            reserve.append(std::move(it->second));
            size_t max_offset=i->first+reserve.size();
            i=it;//now both i and it point to the same thing
            ++i;
            cache_.erase(it);//don't use it any more
            //把属于[index,data.size()]之间的条目都删除
            for(auto j=i;j!=upper_b;){
                if(j->first+j->second.size()>max_offset){
                    if(max_offset==j->first)
                        break;
                    unassembled_-=max_offset-j->first;
                    reserve.append(j->second.substr(max_offset-j->first));
                    cache_.erase(j);
                    break;
                }
                unassembled_-=j->second.size();
                cache_.erase(j++);
            }
        }
    }else{
        i=it;
        auto & reserve=i->second;
        size_t max_offset=i->first+reserve.size();
        ++i;
        //same as before
        for(auto j=i;j!=upper_b;){
            if(j->first+j->second.size()>max_offset){
                if(max_offset==j->first)
                    break;
                unassembled_-=max_offset-j->first;
                reserve.append(j->second.substr(max_offset-j->first));
                cache_.erase(j);
                break;
            }
            unassembled_-=j->second.size();
            cache_.erase(j++);
        }
    }
    if(eof)
        p_eof_={true,index+data.size()};
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_; }

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }

#include "stream_reassembler.hh"

#include <assert.h>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

namespace {
using map_iterator = std::map<size_t, std::string>::iterator;
//it必须合法,返回值不保证合法,例如,可能是容器的end()
size_t find_gap_index(map_iterator it,map_iterator it_end) {
    std::remove_cv<decltype(it->first)>::type i = 0;
    auto be=it->first;
    while (it!=it_end&&be+i==it->first) {
        i += it->second.size();
        it++;
    }
    return i;
}

size_t update_unassembled_bytes(std::map<size_t, std::string> &cache_, const string &data, size_t index) {
    //#bug 5
    if (data.size() == 0)
        return static_cast<size_t>(0);
    //注意,我们的change_bytes是变化量,需要是负数
    auto [extra_bytes, it, index_end
          // todo 为什么lower_bound不是找下界?
    ] = std::tuple{static_cast<long>(0), cache_.end(), index + data.size()};

    //////////////////
    // 有更好的方法尝试寻找index的前继吗?到头来,貌似使用map就一个排序的功能了,好鸡肋
    auto t = cache_.insert({index, data});
    auto ib = t.first;
    if(auto td=t.first; t.second){
        //尝试找到前一块
        if (td != cache_.begin()) {
            auto pre_it = td;
            pre_it--;
            if (auto cf=pre_it->first + pre_it->second.size(); cf> index) {  //找到前一块重叠处
                //#bug 2
                if (cf < index_end) {
                    extra_bytes-=cf-index;
                    pre_it->second.resize(index - pre_it->first);
                } else {
                    //# bug 3
                    cache_.erase(td);
                    return extra_bytes;
                }
            }
        }
        //无论前一块是否存在,现在都没有重叠了
        extra_bytes+=data.size();
    } else {
        assert(index == td->first);
        auto skip_index = find_gap_index(td, cache_.end());
        if (skip_index < data.size()) {
            //可能skip_index为0,因为cache_中存在empty的字符串
            //所以一开始要杜绝empty字符串放入cache_
            // assert(skip_index>0);
            index+=skip_index;
            t = cache_.insert({index, data.substr(skip_index)});
            assert(t.second==true);
            extra_bytes += t.first->second.size();
            ib = t.first;
        } else {
            return extra_bytes;
        }
        // if (data.size() > t.first->second.size()) {
        //     //#bug 4
        //     index += t.first->second.size();
        //     t = cache_.insert({index, data.substr(t.first->second.size())});
        //     assert(t.second==true);
        //     extra_bytes += t.first->second.size();
        //     ib = t.first;
        // } else {
        //     return extra_bytes;
        // }
    } 

    //现在没有交集了
    it = ib;
    it++;

    //移除覆盖的字符
    while (it != cache_.end() && it->first + it->second.size() <= index_end) {
        extra_bytes -= it->second.size();  //减少的字节
        cache_.erase(it++);
    }
    if (it != cache_.end() && index_end > it->first) {
        //#bug 1
        ib->second.resize(it->first - ib->first);
        extra_bytes -= index_end-it->first;
    }

    return extra_bytes;
}

using str_range = std::pair<size_t, size_t>;
str_range get_str_range(const str_range src, const str_range target_range) {
    auto [i, j] = src;
    const auto [x, y] = target_range;
    if (i < x)
        i = x;
    if (j > y)
        j = y;
    return {i, j};
}

size_t transform_str(std::map<size_t, std::string> &src, size_t head, ByteStream &dest) {
    size_t r_n = 0;
    auto it = src.begin();
    while (it != src.end() && it->first == head) {
        auto len = dest.write(it->second);
        assert(len == it->second.size());
        r_n += len;
        head += len;
        src.erase(it++);
    }
    return r_n;
}
}  // namespace

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // std::cout<<"索引是:"<<index<<",数据长度:"<<data.size()<<std::endl;
    // have no aviable space,just discard
    if (_output.remaining_capacity() == 0) {
        return;
    }

    // duplicate data,discard it
    if (index + data.size() < next_index_)
        return;

    // we hava a little free space left

    // we try to merge the data into those from cache_ first

    //找到合适的字符范围,放入缓存中
    const auto [i, j] =
        get_str_range({index, data.size() + index}, {next_index_, next_index_ + _output.remaining_capacity()});

    assert(j>=i&&i>=index);
    unassembled_ += update_unassembled_bytes(cache_, data.substr(i - index, j - i), i);

    //尝试从cache_中,把数据转移到字节流中
    auto write_n = transform_str(cache_, next_index_, _output);
    unassembled_ -= write_n;
    next_index_ += write_n;

    if (eof) {
        p_eof_.first = true;
        p_eof_.second = index + data.size();
    }
    if (p_eof_.first && next_index_ == p_eof_.second)
        _output.end_input();

    //    DUMMY_CODE(data, index, eof);
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

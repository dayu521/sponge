#include "stream_reassembler.hh"

#include <algorithm>
#include <assert.h>
#include <cstddef>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <utility>

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
// 迭代器必须合法,否则未定义
bool check_intersect(map_iterator a, map_iterator b) {
    auto [a_left, a_right] = std::make_pair(a->first, a->first + a->second.size());
    auto [b_left, b_right] = std::make_pair(b->first, b->first + b->second.size());
    return true;
}

size_t update_unassembled_bytes(std::map<size_t, std::string> &cache_, const string &data, size_t index) {
    //注意,我们的change_bytes是变化量,需要是负数
    auto [change_bytes, ins, index_end
          // todo 为什么lower_bound不是找下界?
    ] = std::tuple{static_cast<size_t>(0), cache_.end(), index + data.size()};

    //////////////////
    // 有更好的方法尝试寻找index的前继吗?到头来,貌似使用map就一个排序的功能了,好鸡肋
    auto t = cache_.insert({index, data});
    auto ib = cache_.end();
    if (!t.second) {
        assert(index == t.first->first);
        if (data.size() > t.first->second.size()) {
            change_bytes -= t.first->second.size();
            index += t.first->second.size();
            t.first->second = data;
            change_bytes += data.size();
            ib = t.first;
        } else {
            return change_bytes;
        }
    } else {
        //尝试找到前一块
        if (t.first != cache_.begin()) {
            ib = t.first;
            ib--;
        }
        ////////////////

        // todo 初始大小如何增加
        //有前一块且和前一块有交集,先行处理一次
        if (ib != cache_.end() && (ib->first + ib->second.size() >= index)) {
            assert(ib->first <= index);
            if (ib->first + ib->second.size() < index_end) {
                const auto &s = data.substr(ib->first + ib->second.size() - index);
                ib->second.append(s);
                index += s.size();
                change_bytes += s.size();  //提前增加大小
                cache_.erase(t.first);     //此后t失效
            } else {
                cache_.erase(t.first);
                return change_bytes;  // nothing to do
            }
        } else {
            ib = t.first;
            change_bytes += data.size();
        }
    }

    //现在没有交集了
    ins = ib;
    ins++;

    //循环内不需要创建新字符串
    while (ins != cache_.end() && ins->first + ins->second.size() <= index_end) {
        auto ci = ins->first + ins->second.size();
        change_bytes -= ins->second.size();  //减少的字节
        // change_bytes += ci - index;  //增加的字节
        index = ci;
        cache_.erase(ins++);
    }
    if (ins != cache_.end() && index_end >= ins->first) {
        const auto &end_str = ins->second.substr(index_end - ins->first);
        ib->second.append(end_str);
        //这里和前方不同,先增加大小,是因为我们需要保持change_bytes一直都是非负数
        change_bytes -= ins->second.size();
        change_bytes += end_str.size();
        index_end += end_str.size();
        index = index_end;  //没什么必要
        cache_.erase(ins);  // ins就失效了
    }

    return change_bytes;
}

using str_range = std::pair<size_t, size_t>;
str_range get_str_range(const str_range src, const str_range target) {
    auto [i, j] = src;
    const auto [x, y] = target;
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

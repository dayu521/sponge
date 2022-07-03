#include "byte_stream.hh"

#include <stdexcept>
#include <limits>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):buff_(capacity,'0')
{
    if(capacity==0)
        throw std::runtime_error{"capacity can not be zero"};
    if(capacity>std::numeric_limits<decltype (begin_)>::max())
        throw std::runtime_error{"capacity too large"};
    /*DUMMY_CODE(capacity);*/
}

size_t ByteStream::write(const string &data)
{
    if(buff_.size()-(end_-begin_)==0||data.size()==0)
        return 0;
    auto free_space=buff_.size()-(end_-begin_);
    if(free_space>data.size()){
        free_space=data.size();
    }
    if(begin_<=0||(begin_>0&&free_space<=buff_.size()-end_)){
        buff_.replace(end_,free_space,data,0,free_space);
        end_+=free_space;
        written_n_+=free_space;
        return free_space;
    }else{
        auto to_write=free_space;
        buff_.replace(end_,buff_.size()-end_,data,0,buff_.size()-end_);
        free_space-=buff_.size()-end_;
        buff_.replace(0,free_space,data,buff_.size()-end_,free_space);
        begin_=-(buff_.size()-begin_);//换算为负的开始偏移
        end_=free_space;
        written_n_+=to_write;
        return to_write;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const
{
    auto l=len;
    if(l>static_cast<size_t>(end_-begin_))
        l=end_-begin_;
    if(begin_>=0||l<=static_cast<size_t>(-begin_)/*begin的绝对值*/)
        return buff_.substr((begin_+buff_.size())%buff_.size(),l);
    else{
        string s(l,'0');
        s.assign(buff_,buff_.size()+begin_);
        l-=static_cast<size_t>(-begin_);
        s.replace(-begin_,l,buff_,0,l);
        return s;
    }
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    auto l=len;
    if(l>static_cast<size_t>(end_-begin_))
        l=end_-begin_;
    begin_+=l;
    read_n_+=l;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto s=peek_output(len);
    pop_output(len);
    return s;
}

void ByteStream::end_input()
{
    input_over=true;
}

bool ByteStream::input_ended() const { return input_over; }

size_t ByteStream::buffer_size() const { return end_-begin_; }

bool ByteStream::buffer_empty() const { return begin_==end_; }

bool ByteStream::eof() const { return buffer_empty()&&input_over; }

size_t ByteStream::bytes_written() const { return written_n_; }

size_t ByteStream::bytes_read() const { return read_n_; }

size_t ByteStream::remaining_capacity() const { return buff_.size()-(end_-begin_); }

#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const
{
    return _next_seqno-head_window_;
}

void TCPSender::fill_window()
{
    if(head_window_==0&&_next_seqno==0){//close.
        TCPSegment seg{};
        auto & header=seg.header();
        header.syn=true;
        header.seqno=wrap(_next_seqno,_isn);
        _next_seqno++;

        auto n_bytes=stream_in().buffer_size();
        if(int64_t cmp=stream_in().buffer_size()-TCPConfig::MAX_PAYLOAD_SIZE;cmp>0)
            n_bytes=TCPConfig::MAX_PAYLOAD_SIZE;
        //we send only one segment before we connect
        seg.payload()=stream_in().read(n_bytes);
        _next_seqno+=n_bytes;

        if(stream_in().buffer_size()==0&&stream_in().eof()){
            header.fin=true;
            _next_seqno++;
        }

        outstanding_.push({_next_seqno,seg});
        _segments_out.push(std::move(seg));

        timer_.start(rto_);//we haven't started the timer
        return ;
    }else if(head_window_==0&&_next_seqno>0){//syn-sent
        return ;//just wait for timeout
    }else if(!stream_in().eof()){//syn-received
        if(tail_window_==head_window_&&!fill_one_){
            tail_window_++;//we assume that the window sizes of peer are 1
            fill_one_=true;
        }
        auto n_bytes=stream_in().buffer_size();
        if(n_bytes==0||tail_window_==_next_seqno){
            return ;//have no bytes to send or current segment have sent when window size are zore
        }else if(n_bytes>tail_window_-_next_seqno){
            n_bytes=tail_window_-_next_seqno;
        }

        //we get the total byte sizes to send
        auto result=n_bytes/TCPConfig::MAX_PAYLOAD_SIZE;
        auto remainder=n_bytes-result*TCPConfig::MAX_PAYLOAD_SIZE;
        //result can be divided by TCPConfig::MAX_PAYLOAD_SIZE completely
        while (result>0) {
            TCPSegment seg{};
            auto s=stream_in().read(TCPConfig::MAX_PAYLOAD_SIZE);
            auto le=s.size();
            DUMMY_CODE(le);
            seg.payload()=std::move(s);
            seg.header().seqno=wrap(_next_seqno,_isn);
            _next_seqno+=TCPConfig::MAX_PAYLOAD_SIZE;
            outstanding_.push({_next_seqno,seg});
            _segments_out.push(std::move(seg));
            result--;
        }
        if(remainder>0){
            TCPSegment seg{};
            seg.payload()=stream_in().read(remainder);
            seg.header().seqno=wrap(_next_seqno,_isn);
            _next_seqno+=remainder;
            outstanding_.push({_next_seqno,seg});
            _segments_out.push(std::move(seg));
        }
        if(stream_in().eof()&&stream_in().buffer_size()==0&&_next_seqno<tail_window_){
            outstanding_.back().second.header().fin=true;
            outstanding_.back().first+=1;
            _segments_out.back().header().fin=true;
            _next_seqno++;
        }
        if(!timer_.is_running())
            timer_.start(rto_);
    }else if((stream_in().eof()&&_next_seqno<stream_in().bytes_read()+2)){//syn-received-2
        if(tail_window_==head_window_&&!fill_one_){
            tail_window_++;//we assume that the window sizes of peer are 1
            fill_one_=true;
        }
        if(tail_window_==_next_seqno)
            return ;
        TCPSegment seg{};
        seg.header().fin=true;
        seg.header().seqno=wrap(_next_seqno,_isn);
        _next_seqno++;
        outstanding_.push({_next_seqno,seg});
        _segments_out.push(std::move(seg));
        if(!timer_.is_running())
            timer_.start(rto_);
    }else if(stream_in().eof()&&head_window_<_next_seqno&&_next_seqno==stream_in().bytes_read()+2){//fin-sent

    }else{//fin-received
        //for the last ack
        return ;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    auto ack_left=unwrap(ackno,_isn,head_window_);
    //bug fixed:
    //credit for test: Jared Wasserman (2020)
    //Impossible ackno (beyond next seqno) is ignored
    if(ack_left>_next_seqno){
        fill_window();
        return;
    }
    if(head_window_>=ack_left){
        if(tail_window_<ack_left+window_size)
            tail_window_=ack_left+window_size;
        return ;
    }

    while (outstanding_.size()>0&&head_window_<ack_left) {
        const auto &[last_ack_seqno,segment]=outstanding_.front();
        if(last_ack_seqno<=ack_left){
            head_window_=last_ack_seqno;
            outstanding_.pop();
        }else
            break;
    }


    fill_one_=false;
    tail_window_=ack_left+window_size;
    timer_.stop();
    retransmit_n_=0;
    rto_=_initial_retransmission_timeout;
    fill_window();
    if(outstanding_.size()>0&&!timer_.is_running()){
        timer_.start(rto_);
    }
    return ;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    if(!timer_.is_running())
        return ;
    timer_.elapse(ms_since_last_tick);
    if(timer_.is_timeout()){
        segments_out().push(outstanding_.front().second);
        if(!fill_one_||head_window_==0){
            retransmit_n_++;
            rto_*=2;//can we get overflow?
        }
        timer_.start(rto_);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return retransmit_n_; }

void TCPSender::send_empty_segment()
{
    TCPSegment seg{};
    auto & header=seg.header();
    header.seqno=wrap(_next_seqno,_isn);
    _segments_out.push(std::move(seg));
}

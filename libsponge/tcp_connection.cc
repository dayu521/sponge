#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return estime_; }

void TCPConnection::segment_received(const TCPSegment &seg)
{
    const auto & header=seg.header();
    if(_sender.next_seqno_absolute()==0&&!header.syn)
        return;
    estime_=0;
    if(header.rst){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
    }else{
        _receiver.segment_received(seg);
        if(header.ack){
            _sender.ack_received(header.ackno,header.win);
        }

        if(_receiver.stream_out().input_ended()&&!_sender.stream_in().eof())
            _linger_after_streams_finish=false;

        if((seg.payload().size()>0||header.syn|header.fin)){
            _sender.fill_window();
            if(_sender.segments_out().size()==0)
                _sender.send_empty_segment();
            delivery_seg();
        }


        if(_linger_after_streams_finish&&
                _sender.stream_in().eof()&&
                _sender.bytes_in_flight()==0&&
                _receiver.stream_out().input_ended())
            time_wait_=true;
    }
}

bool TCPConnection::active() const
{
    if(_sender.stream_in().error()||_receiver.stream_out().error()){
        return false;
    }else{
        if(_receiver.stream_out().input_ended()&&
                _sender.stream_in().eof()&&
                _sender.bytes_in_flight()==0){
            if(!_linger_after_streams_finish||(_linger_after_streams_finish&&linger_>=10*_cfg.rt_timeout)){
                return false;
            }else
                return true;
        }
        return true;
    }
}

size_t TCPConnection::write(const string &data) {
    if(!_sender.stream_in().input_ended()){
        auto size=_sender.stream_in().write(data);
        if(size>0){
            _sender.fill_window();
            delivery_seg();
        }
        return size;
    }else
        return 0;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{
    estime_+=ms_since_last_tick;
    if(time_wait_){//此时发送方已经结束了
        linger_+=ms_since_last_tick;
    }
    if(_sender.consecutive_retransmissions()>=TCPConfig::MAX_RETX_ATTEMPTS){
        _sender.send_empty_segment();
        auto & reset_seg=_sender.segments_out().back();
        reset_seg.header().rst=true;
        _segments_out.push(reset_seg);

        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();

    }else{
        _sender.tick(ms_since_last_tick);
        delivery_seg();
    }
}

void TCPConnection::end_input_stream()
{
    _sender.stream_in().end_input();
    if(_receiver.stream_out().input_ended())
        _linger_after_streams_finish=false;
    _sender.fill_window();
    delivery_seg();
}

void TCPConnection::delivery_seg()
{
    auto win=_receiver.window_size();
    auto ack=_receiver.ackno().has_value();
    WrappingInt32 ackno{0};
    if(ack)
        ackno=_receiver.ackno().value();
    while (_sender.segments_out().size()>0) {//we enter loop once at least
        auto && sseg=_sender.segments_out().front();
        sseg.header().win=win;
        sseg.header().ackno=ackno;
        sseg.header().ack=ack;
        _segments_out.push(sseg);
        _sender.segments_out().pop();
    }
}

void TCPConnection::connect()
{
    _sender.fill_window();
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            auto & reset_seg=_sender.segments_out().back();
            reset_seg.header().rst=true;
            _segments_out.push(reset_seg);

            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

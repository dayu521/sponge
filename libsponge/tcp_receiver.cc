#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto & header=seg.header();
    switch (rev_state_) {
    case State::Listen:{
        if(header.syn){
            rev_state_=State::Runing;
            isn_=header.seqno;           
            _reassembler.push_substring(seg.payload().copy(),
                                        unwrap(header.seqno,isn_,_reassembler.stream_out().bytes_written()),
                                        header.fin);
            if(_reassembler.stream_out().input_ended())
                rev_state_=State::Fin;
        }else if(header.fin){
            ;
        }
    }
        break;
    case State::Runing:{
        auto absolute_seqno=unwrap(header.seqno,isn_,_reassembler.stream_out().bytes_written())-1;
        _reassembler.push_substring(seg.payload().copy(),
                                    absolute_seqno,header.fin);
        if(_reassembler.stream_out().input_ended())
            rev_state_=State::Fin;
    }
        break;
    case State::Fin:{
        //discard silently
    }
        break;
    }

}

optional<WrappingInt32> TCPReceiver::ackno() const
{
    if(rev_state_>State::Listen){
        auto absolute_ackno=_reassembler.stream_out().bytes_written()+1;
        if(rev_state_==State::Fin)
            absolute_ackno+=1;
        return wrap(absolute_ackno,isn_);
    }
    return {};
}

size_t TCPReceiver::window_size() const { return _capacity-_reassembler.stream_out().buffer_size(); }

#include "wrapping_integers.hh"

#include "limits"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn)
{
    constexpr uint64_t M=std::numeric_limits<uint32_t>::max()+static_cast<uint64_t>(1);
    auto pre_wrap=n%M;
    auto wrap=(pre_wrap+isn.raw_value())%M;
    return WrappingInt32{static_cast<uint32_t>(wrap)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint)
{
    constexpr uint64_t upbound_32=std::numeric_limits<uint32_t>::max()+static_cast<uint64_t>(1);
    constexpr uint32_t low_32_mask=std::numeric_limits<uint32_t>::max();
    constexpr uint64_t high_32_mask=static_cast<uint64_t>(low_32_mask)<<32;

    //Be careful,we should check out this:
    //https://zh.cppreference.com/w/cpp/language/operator_arithmetic
    //https://zh.cppreference.com/w/cpp/language/implicit_conversion#.E6.95.B4.E5.9E.8B.E6.8F.90.E5.8D.87
    auto base_checkpoint=checkpoint&high_32_mask;
    uint32_t last_seqno_offset=checkpoint&low_32_mask;//checkpoint%upbound_32
    auto ii=static_cast<int64_t>(n-isn);
    if(ii<0){
        ii+=upbound_32;
    }
    //ii must be less std::numeric_limits<uint32_t>::max(),so uint32_t is big enough for it
    uint32_t seqno_offset=ii;

    if(last_seqno_offset>seqno_offset){
        if(last_seqno_offset-seqno_offset<upbound_32+seqno_offset-last_seqno_offset)
            return base_checkpoint+seqno_offset;
        else
            return base_checkpoint+upbound_32+seqno_offset;
    }else if(last_seqno_offset<seqno_offset){
        if(seqno_offset-last_seqno_offset<last_seqno_offset+(upbound_32-seqno_offset)||base_checkpoint==0)
            return base_checkpoint+seqno_offset;
        else
            return base_checkpoint-upbound_32+seqno_offset;
    }else
        return checkpoint;

}

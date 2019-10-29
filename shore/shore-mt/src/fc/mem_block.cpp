/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

#include "w.h"
#include "mem_block.h"
#include "atomic_ops.h"
#include <cstdlib>
#include <stdio.h>
#include <algorithm>
#ifdef __linux
#include <malloc.h>
#endif

// #include <cassert>
#undef assert
void assert_failed(const char *desc, const char *f, int l) {
    fprintf(stdout, "Assertion failed: %s at line %d file %s ", desc, l,f);
	w_assert0(0);
}
#define assert(x)   if (!(x)) assert_failed(#x, __FILE__, __LINE__);

#define TEMPLATE_ARGS chip_size, chip_count, block_size

namespace memory_block {
#if 0
} // keep emacs happy...
#endif

// adapted from http://chessprogramming.wikispaces.com/Population+Count#SWAR-Popcount
typedef unsigned long long u64;
static inline
long popc64(u64 x) {
    u64 k1 = 0x5555555555555555ull;
    u64 k2 = 0x3333333333333333ull;
    u64 k4 = 0x0f0f0f0f0f0f0f0full;
    u64 kf = 0x0101010101010101ull;
    x =  x       - ((x >> 1)  & k1); //put count of each 2 bits into those 2 bits
    x = (x & k2) + ((x >> 2)  & k2); //put count of each 4 bits into those 4 bits
    x = (x       +  (x >> 4)) & k4 ; //put count of each 8 bits into those 8 bits
    x = (x * kf) >> 56; //returns 8 most significant bits of x + (x<<8) + (x<<16) + (x<<24) + ...
    return x;
}

size_t        block_bits::_popc(bitmap bm) {
#ifdef __GNUC__
    
#if defined(__x86_64) || defined(i386) || defined(__i386__)
// #warning "Using __builtin_popcountll"
    return __builtin_popcountll(bm);
    
#elif defined(__sparcv9)
#warning "Using gcc inline asm to access sparcv9 'popc' instruction"
    long rval;
    __asm__("popc    %[in], %[out]" : [out] "=r"(rval) : [in] "r"(x));
    return rval;
#else
#warning "using home-brew popc routine"
    return popc64(bm);
#endif
    
#else // !defined(__GNUC__)
#warning "using home-brew popc routine"
    return popc64(bm);
#endif
}

block_bits::block_bits(size_t chip_count)
    : _usable_chips(create_mask(chip_count))
    , _zombie_chips(0)
{
    assert(chip_count <= 8*sizeof(bitmap));
}

size_t block_bits::acquire(size_t chip_count) {
    (void) chip_count; // make gcc happy...
    
    /* find the rightmost set bit.

       If the map is smaller than the word size, but logically full we
       will compute an index equal to the capacity. If the map is
       physically full (_available == 0) then we'll still compute an
       index equal to capacity because 0-1 will underflow to all set
       bits. Therefore the check for full is the same whether the
       bitmap can use all its bits or not.
     */
    bitmap one_bit = _usable_chips &- _usable_chips;
    size_t index = _popc(one_bit-1);
    if(index < 8*sizeof(bitmap)) {
        // common case: we have space
        assert(index < chip_count);
        _usable_chips ^= one_bit;
    }
    else {
        // oops... full
        assert(index == 8*sizeof(bitmap));
    }

    return index;
}

void block_bits::release(size_t index, size_t chip_count) {
    // assign this chip to the zombie set for later recycling
    (void) chip_count; // keep gcc happy
    assert(index < chip_count);
    bitmap to_free = bitmap(1) << index;
    assert(! (to_free & *usable_chips()));
#if I_WISH
    bitmap was_free = __sync_fetch_and_or(&_zombie_chips, to_free);
#else
    membar_exit();
    bitmap volatile* ptr = &_zombie_chips;
    bitmap ov = *ptr;
    while(1) {
        bitmap nv = ov | to_free;
        bitmap cv = atomic_cas_64(ptr, ov, nv);
        if(cv == ov)
            break;
        ov = cv;
    }
    bitmap was_free = ov;
#endif
    (void) was_free; // keep gcc happy
    assert(! (was_free & to_free));
}

block_bits::bitmap block_bits::create_mask(size_t bits_set) {
    // doing it this way allows us to create a bitmap of all ones if need be
    return ~bitmap(0) >> (8*sizeof(bitmap) - bits_set);
}

void block_bits::recycle() {
    /* recycle the block.

       Whatever bits have gone zombie since we last recycled become
       the new set of usable bits. We also XOR them atomically back
       into the zombie set to clear them out there. That way we don't
       leak bits if a releasing thread races us and adds more bits to the
       zombie set after we read it.
    */
    bitmap newly_usable = *&_zombie_chips;
    _usable_chips |= newly_usable;
#if I_WISH
    __sync_xor_and_fetch(&_zombie_chips, newly_usable);
#else
    membar_exit();
    bitmap volatile* ptr = &_zombie_chips;
    bitmap ov = *ptr;
    while(1) {
        bitmap nv = ov ^ newly_usable;
        bitmap cv = atomic_cas_64(ptr, ov, nv);
        if(cv == ov)
            break;
        ov = cv;
    }
#endif
}

void* block::acquire(size_t chip_size, size_t chip_count, size_t /*block_size*/) {
    size_t index = _bits.acquire(chip_count);
    return (index < chip_count)? _get(index, chip_size) : 0;
}

void block::release(void* ptr, size_t chip_size, size_t chip_count, size_t block_size)
{
    /* use pointer arithmetic to find the beginning of our block,
       where the block* lives.

       Our caller is responsible to be sure this address actually
       falls inside a memory block
     */
    union { void* v; size_t n; block* b; char* c; } u = {ptr}, v=u;
    u.n &= -block_size;
    size_t offset = v.c - u.b->_data;
    size_t idx = offset/chip_size;
    assert(u.b->_data + idx*chip_size == ptr);
    u.b->_bits.release(idx, chip_count);
}

char* block::_get(size_t index, size_t chip_size) {
    return _data + index*chip_size;
}

block::block(size_t chip_size, size_t chip_count, size_t block_size)
    : _bits(chip_count)
    , _owner(0)
    , _next(0)
{
    // make sure all the chips actually fit in this block
    char* end_of_block = _get(0, chip_size)-sizeof(*this)+block_size;
    char* end_of_chips = _get(chip_count, chip_size);
    (void) end_of_block; // keep gcc happy
    (void) end_of_chips; // keep gcc happy
    assert(end_of_chips <= end_of_block);
    
    /* We purposefully don't check alignment here because some parts
       of the impl cheat for blocks which will never be used to
       allocate anything (the fake_block being the main culprit).
       The block_pool does check alignment, though.
     */
}

void* block_list::acquire(size_t chip_size, size_t chip_count, size_t block_size)
{
    if(void* ptr = _tail->acquire(TEMPLATE_ARGS)) {
        _hit_count++;
        return ptr;
    }

    // darn... gotta do it the hard way
    return _slow_acquire(TEMPLATE_ARGS);
}


block_list::block_list(block_pool* pool, size_t chip_size, size_t chip_count, size_t block_size)
    : _fake_block(TEMPLATE_ARGS)
    , _tail(&_fake_block)
    , _pool(pool)
    , _hit_count(0)
    , _avg_hit_rate(0)
{
    /* make the fake block advertize that it has nothing to give

       The first time the user tries to /acquire/ the fast case will
       detect that the fake block is "full" and fall back to the slow
       case. The slow case knows about the fake block and will remove
       it from the list.

       This trick lets us minimize the number of branches required by
       the fast path acquire.
     */
    _fake_block._bits._usable_chips = 0;
    _fake_block._bits._zombie_chips = 0;
}


void* block_list::_slow_acquire(size_t chip_size, size_t chip_count, size_t block_size)
{
    _change_blocks(TEMPLATE_ARGS);
    return acquire(TEMPLATE_ARGS);
}

block* block_list::acquire_block(size_t block_size)
{
    union { block* b; uintptr_t n; } u = {_pool->acquire_block(this)};
    (void) block_size; // keep gcc happy
    assert((u.n & -block_size) == u.n);
    return u.b;
    
}
void block_list::_change_blocks(size_t chip_size, size_t chip_count, size_t block_size)
{
    (void) chip_size; // keep gcc happy

    // first time through?
    if(_tail == &_fake_block) {
    _tail = acquire_block(block_size);
    _tail->_next = _tail;
    return;
    }
    
    /* Check whether we're chewing through our blocks too fast for the
       current ring size

       If we consistently recycle blocks while they're still more than
       half full then we need a bigger ring so old blocks have more
       time to cool off between reuses.
       
       To respond to end-of-spike gracefully, we're going to be pretty
       aggressive about returning blocks to the global pool: when
       recycling blocks we check for runs of blocks which do not meet
       some minimum utilization threshold, discarding all but one of
       them (keep the last for buffering purposes).
    */
    static double const    decay_rate = 1./5; // consider (roughly) the last 5 blocks
    // too few around suggests we should unload some extra blocks 
    size_t const max_available = chip_count - std::max((int)(.1*chip_count), 1);
    // more than 50% in-use suggests we've got too few blocks
    size_t const min_allocated = (chip_count+1)/2;
    
    _avg_hit_rate = _hit_count*(1-decay_rate) + _avg_hit_rate*decay_rate;
    if(_hit_count < min_allocated && _avg_hit_rate < min_allocated) {
        // too fast.. grow the ring
        block* new_block = acquire_block(block_size);
        new_block->_next = _tail->_next;
        _tail = _tail->_next = new_block;
    }
    else {
        // compress the run, if any
        block* prev = 0;
        block* cur = _tail;
        block* next;
        while(1) {
            next = cur->_next;
            next->recycle();
            if(next->_bits.usable_count() <= max_available)
            break;
            
            // compression possible?
            if(prev) {
            assert(prev != cur);
            assert(cur->_bits.usable_count() > max_available);
            assert(next->_bits.usable_count() > max_available);
            prev->_next = next;
            _pool->release_block(cur);
            cur = prev; // reset
            }

            // avoid the endless loop
            if(next == _tail)
            break;
            
            prev = cur;
            cur = next;
        }

        // recycle, repair the ring, and advance
        _tail = cur;
    }

    _hit_count = 0;
}

block_list::~block_list() {
    // don't free the fake block if we went unused!
    if(_tail == &_fake_block) return;
    
    // break the cycle so the loop terminates
    block* cur = _tail->_next;
    _tail->_next = 0;

    // release blocks until we hit the NULL
    while( (cur=_pool->release_block(cur)) ) ;
}

} // namespace memory_block

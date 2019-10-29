/*<std-header orig-src='shore'>

 $Id: w_bitmap.cpp,v 1.16.2.4 2010/03/19 22:17:19 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define W_SOURCE
#ifdef __GNUC__
#   pragma implementation
#endif

#include "w_base.h"
#include <cstring>
#include "w_bitmap.h"

inline int div8(long x)         { return x >> 3; }
inline int mod8(long x)         { return x & 7; }
inline int div32(long x)        { return x >> 5; }
inline int mod32(long x)        { return x & 31; }

int
w_bitmap_t::bytesForBits(uint4_t numBits)
{
    return (div8(numBits -1) + 1);
}


NORET
w_bitmap_t::w_bitmap_t(uint4_t size)
    : sz(size), mem_alloc(true)
{
    int n = bytesForBits(size);
    ptr = new uint1_t[n] ;
    if (!ptr) W_FATAL(fcOUTOFMEMORY) ; 
}

void 
w_bitmap_t::zero()
{
    int n = bytesForBits(sz);
    memset(ptr, 0, n);
}

void
w_bitmap_t::fill()
{
    int n = bytesForBits(sz);
    memset(ptr, 0xff, n);
}

bool 
w_bitmap_t::is_set(uint4_t offset) const
{
    return (ptr[div8(offset)] & (1 << mod8(offset))) != 0; 
}

void 
w_bitmap_t::set(uint4_t offset)
{
    ptr[div8(offset)] |= (1 << mod8(offset));
}

void
w_bitmap_t::clr(uint4_t offset)
{
    ptr[div8(offset)] &= ~(1 << mod8(offset));
}

w_base_t::int4_t
w_bitmap_t::first_set(uint4_t start) const
{
    w_assert9(start < sz);
    register uint1_t* p = ptr + div8(start);
    register uint4_t mask = 1 << mod8(start);
    register uint4_t size = sz;
    for (size -= start; size; start++, size--)  {
    if (*p & mask)  {
        w_assert9(is_set(start));
        return start;
    }
    if ((mask <<= 1) == 0x100)  {
        mask = 1;
        p++;
    }
    }
    
    return -1;
}

w_base_t::int4_t
w_bitmap_t::first_clr(uint4_t start) const
{
    w_assert9(start < sz);
    register uint1_t* p = ptr + div8(start);
    register uint4_t mask = 1 << mod8(start);
    register uint4_t size = sz;
    for (size -= start; size; start++, size--) {
    if ((*p & mask) == 0)    {
        return start;
    }
    if ((mask <<= 1) == 0x100)  {
        mask = 1;
        p++;
    }
    }
    
    return -1;
}

w_base_t::uint4_t
w_bitmap_t::num_set() const
{
    uint1_t* p = ptr;
    uint4_t size = sz;
    int count;
    int mask;
    for (count = 0, mask = 1; size; size--)  {
    if (*p & mask)    count++;
    if ((mask <<= 1) == 0x100)  {
        mask = 1;
        p++;
    }
    }
    return count;
}

ostream& operator<<(ostream& o, const w_bitmap_t& obj)
{
    for (register unsigned i = 0; i < obj.sz; i++)  {
    o << (obj.is_set(i) != 0);
    }
    return o;
}


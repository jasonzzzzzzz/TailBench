/*<std-header orig-src='shore' incl-file-exclusion='W_BITMAP_H'>

 $Id: w_bitmap.h,v 1.12.2.4 2010/03/19 22:17:19 nhall Exp $

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

#ifndef W_BITMAP_H
#define W_BITMAP_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_base.h>

#ifdef __GNUG__
#pragma interface
#endif

/** \brief Bitmaps of arbitrary sizes (in bits).  NOT USED by the storage manager.
 *\ingroup UNUSED 
 */
class w_bitmap_t : public w_base_t {
public:
    /// construct for \e size bits
    NORET            w_bitmap_t(uint4_t size);
    /// construct for \e size bits, using given pointer to storage 
    NORET            w_bitmap_t(uint1_t* p, uint4_t size);

    NORET            ~w_bitmap_t();

    /// clear all bits
    void            zero();
    /// set all bits
    void            fill();

    void            resetPtr(uint1_t* p, uint4_t size);
    /// set bit at offset
    void            set(uint4_t offset);
    /// return first set bit after bit at start
    int4_t            first_set(uint4_t start) const;
    /// return # bits set
    uint4_t            num_set() const;
    /// return true iff bit at offset is set
    bool            is_set(uint4_t offset) const;

    /// clear bit at offset
    void            clr(uint4_t offset);
    /// return first clear bit after bit at start
    int4_t            first_clr(uint4_t start) const;
    /// return # bits clear
    uint4_t            num_clr() const;
    /// return true iff bit at offset is clear
    bool            is_clr(uint4_t offset) const;

    /// return size in bits
    uint4_t            size() const;        // # bits

    /// return size in bytes needed for numBits
    static int        bytesForBits(uint4_t numBits);

    /// return pointer to the storage area
    uint1_t*            addr();
    /// return const pointer to the storage area
    const uint1_t*        addr() const;

    friend ostream&        operator<<(ostream&, const w_bitmap_t&);
private:
    uint1_t*             ptr;
    uint4_t            sz; // # bits
    bool            mem_alloc;

};

inline NORET
w_bitmap_t::w_bitmap_t(uint1_t* p, uint4_t size)
    : ptr(p), sz(size), mem_alloc(false)
{
}

inline NORET
w_bitmap_t::~w_bitmap_t()
{
   if (mem_alloc) delete [] ptr ; 
}

inline void 
w_bitmap_t::resetPtr(uint1_t* p, uint4_t size)
{
   w_assert9(!mem_alloc);
   sz = size;
   ptr = p;
}

inline bool
w_bitmap_t::is_clr(uint4_t offset) const
{
    return !is_set(offset);
}

inline w_base_t::uint4_t
w_bitmap_t::size() const
{
    return sz ;
}

inline w_base_t::uint4_t
w_bitmap_t::num_clr() const
{
    return sz - num_set();
}

inline w_base_t::uint1_t*
w_bitmap_t::addr() 
{
    return ptr;
}

inline const w_base_t::uint1_t*
w_bitmap_t::addr() const
{
    return ptr;
}


/*<std-footer incl-file-exclusion='W_BITMAP_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

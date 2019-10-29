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

/*<std-header orig-src='shore' incl-file-exclusion='W_BASE_H'>

 $Id: w_base.h,v 1.80 2010/06/15 17:24:25 nhall Exp $

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

#ifndef W_BASE_H
#define W_BASE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/**\file w_base.h
 *
 *\ingroup MACROS
 * Basic types.
 */

/*******************************************************/
/* get configuration definitions from config/shore.def */
/*
 * WARNING: if ON and OFF are defined, we must turn them off asap
 * because ON and OFF are re-definedelsewhere as enums
 */
#ifdef ON
#undef ON
#endif

#ifdef OFF
#undef OFF
#endif
/* end configuration definitions                       */
/*******************************************************/


#define W_UNIX

#ifdef __GNUG__
#pragma interface
#endif

#include <w_stream.h>

#ifndef W_WORKAROUND_H
#include "w_workaround.h"
#endif

#define NORET        /**/
#define CAST(t,o) ((t)(o))
#define    W_UNUSED(x)    /**/


#if W_DEBUG_LEVEL>0
#define W_IFDEBUG1(x)    x
#define W_IFNDEBUG1(x)    /**/
#else
#define W_IFDEBUG1(x)    /**/
#define W_IFNDEBUG1(x)    x
#endif

#if W_DEBUG_LEVEL>1
#define W_IFDEBUG2(x)    x
#define W_IFNDEBUG2(x)    /**/
#else
#define W_IFDEBUG2(x)    /**/
#define W_IFNDEBUG2(x)    x
#endif

#if W_DEBUG_LEVEL>2
#define W_IFDEBUG3(x)    x
#define W_IFNDEBUG3(x)    /**/
#else
#define W_IFDEBUG3(x)    /**/
#define W_IFNDEBUG3(x)    x
#endif

#if W_DEBUG_LEVEL>3
#define W_IFDEBUG4(x)    x
#define W_IFNDEBUG4(x)    /**/
#else
#define W_IFDEBUG4(x)    /**/
#define W_IFNDEBUG4(x)    x
#endif

#define W_IFDEBUG9(x)    /**/
#define W_IFNDEBUG9(x)    x

//////////////////////////////////////////////////////////
#undef  W_IFDEBUG
#undef  W_IFNDEBUG
#if W_DEBUG_LEVEL==1
#define W_IFDEBUG(x)    W_IFDEBUG1(x)
#define W_IFNDEBUG(x)    W_IFNDEBUG1(x)
#endif

#if W_DEBUG_LEVEL==2
#define W_IFDEBUG(x)    W_IFDEBUG2(x)
#define W_IFNDEBUG(x)    W_IFNDEBUG2(x)
#endif

#if W_DEBUG_LEVEL==3
#define W_IFDEBUG(x)    W_IFDEBUG3(x)
#define W_IFNDEBUG(x)    W_IFNDEBUG3(x)
#endif

#if W_DEBUG_LEVEL==4
#define W_IFDEBUG(x)    W_IFDEBUG4(x)
#define W_IFNDEBUG(x)    W_IFNDEBUG4(x)
#endif

#ifndef W_IFDEBUG
#define W_IFDEBUG(x) /**/
#endif
#ifndef W_IFNDEBUG
#define W_IFNDEBUG(x) x
#endif

//////////////////////////////////////////////////////////

#ifdef W_TRACE
#define    W_IFTRACE(x)    x
#define    W_IFNTRACE(x)    /**/
#else
#define    W_IFTRACE(x)    /**/
#define    W_IFNTRACE(x)    x
#endif

#if defined(W_DEBUG_SPACE)
void     w_space(int line, const char *file);
#define W_SPACE w_space(__LINE__,__FILE__)
#else
#define W_SPACE
#endif

/// Default assert/debug level is 0.
#define w_assert0(x)    do {                        \
    if (!(x)) w_base_t::assert_failed(#x, __FILE__, __LINE__);    \
    W_SPACE;                            \
} while(0)

#ifndef W_DEBUG_LEVEL
#define W_DEBUG_LEVEL 0
#endif

/// Level 1 should not add significant extra time.
#if W_DEBUG_LEVEL>=1
#define w_assert1(x)    w_assert0(x)
#else
#define w_assert1(x)    /**/
#endif

/// Level 2 adds some time.
#if W_DEBUG_LEVEL>=2
#define w_assert2(x)    w_assert1(x)
#else
#define w_assert2(x)    /**/
#endif

/// Level 3 definitely adds significant time.
#if W_DEBUG_LEVEL>=3
#define w_assert3(x)    w_assert1(x)
#else
#define w_assert3(x)    /**/
#endif

/// Level 4 can be a hog.
#if W_DEBUG_LEVEL>=4
#define w_assert4    w_assert1(x)
#else
#define w_assert4(x)    /**/
#endif

/// Level 5 is not yet used.
#if W_DEBUG_LEVEL>=5
#define w_assert5    w_assert1(x)
#else
#define w_assert5(x)    /**/
#endif

/*
 * The whole idea here is to gradually move assert3's, which have
 * not been established to be useful in an mt-environment, to anoter
 * assert level. 
 * First: make them 9. Then gradually move them to level 2->5, based
 * on the cost and frequency of usefulness.
 * Make them 2 if you want them for a 'normal' debug system.
*/
/// changing an assert to an assert9 turns it off. 
#define w_assert9(x)    /**/

/**\brief  Cast to treat an enum as integer value.  
 *
 * This is used when
 * a operator<< doesn't exist for the enum.  The use of the macro
 * indicates that this enum would be printed if it had a printer,
 * rather than wanting the integer value of the enum
 */
#define    W_ENUM(x)    ((int)(x))

/**\brief  Cast to treat a pointer as a non-(char *) value.  
 *
 * This is used when
 * a operator<< is used on a pointer.   Without this cast, some values
 * would bind to 'char *' and attempt  to print a string, rather than
 * printing the desired pointer value.
 */
#define    W_ADDR(x)    ((void *)(x))

class w_rc_t;

/**\brief The mother base class for most types.
 *
 * \attention These basic 1,2,4, and 8-byte types predate the
 * now-standard "u8", etc.
 * When these were developed, we used the count to
 * refer to bytes, not bits.  So for new users, 
 * this might take a bit of getting-used-to.
 */
class w_base_t {
public:
    /*
     *  shorthands
     */
    typedef unsigned char    u_char;
    typedef unsigned short    u_short;
#ifdef FC_COMPAT_32BIT_ULONG
    /* keep u_long 32 bits on a 64 bit platform; see shore.def */
    typedef unsigned        u_long;
#else
    typedef unsigned long    u_long;
#endif
    // typedef w_rc_t        rc_t;

    /*
     *  basic types
     */
    typedef char         int1_t;
    typedef u_char       uint1_t;
    typedef short        int2_t;
    typedef u_short      uint2_t;
    typedef int          int4_t;
    typedef u_int        uint4_t;

#if defined(ARCH_LP64)
    typedef    long              int8_t;
    typedef unsigned long        uint8_t;
#elif defined(__GNUG__)
    typedef long long            int8_t;
    typedef unsigned long long   uint8_t;
#else
#error int8_t Not supported for this compiler.
#endif

    /* 
     * For statistics that are always 64-bit numbers
     */
    typedef uint8_t         large_stat_t; 

    /* 
     * For statistics that are 64-bit numbers 
     * only when #defined LARGEFILE_AWARE
     */
#if defined(LARGEFILE_AWARE) || defined(ARCH_LP64)
    typedef uint8_t         base_stat_t;
    typedef double          base_float_t;
#else
    typedef uint4_t         base_stat_t;
    typedef float           base_float_t;
#endif

    typedef float        f4_t;
    typedef double        f8_t;

    static const int1_t        int1_max, int1_min;
    static const int2_t        int2_max, int2_min;
    static const int4_t        int4_max, int4_min;
    static const int8_t        int8_max, int8_min;

    static const uint1_t    uint1_max, uint1_min;
    static const uint2_t    uint2_max, uint2_min;
    static const uint4_t    uint4_max, uint4_min;
    static const uint8_t    uint8_max, uint8_min;

    /*
     *  miscellaneous
     */
    enum { align_on = 0x8, align_mod = align_on - 1 };

    /*
    // NEH: turned into a macro for the purpose of folding
    // static uint4_t        align(uint4_t sz);
    */
#ifndef align
/// helper for alignon
#define alignonarg(a) (((ptrdiff_t)(a))-1)
/// aligns a pointer p on a size a
#define alignon(p,a) (((ptrdiff_t)((char *)(p) + alignonarg(a))) & ~alignonarg(a))


///  We now support only 8-byte alignment of records
#define ALIGNON 0x8

/// helper for align(sz)
#define ALIGNON1 (ALIGNON-1)
/// align to 8-byte boundary
#define align(sz) ((size_t)((sz + ALIGNON1) & ~ALIGNON1))
#endif /* align */

    static bool        is_aligned(size_t sz);
    static bool        is_aligned(const void* s);

    static bool        is_big_endian();
    static bool        is_little_endian();

    /*!
     * strtoi8 and strtou8 act like strto[u]ll with the following
     *  two exceptions: the only bases supported are 0, 8, 10, 16;
     *  ::errno is not set
     */
    /**\brief Convert string to 8-byte integer  
     *
     * strtoi8 acts like strto[u]ll with the following
     *  two exceptions: the only bases supported are 0, 8, 10, 16;
     *  ::errno is not set
     */
    static int8_t    strtoi8(const char *, char ** end=0 , int base=0);
    /**\brief Convert string to 8-byte unsigned integer.  
     *
     * strtou8 acts like strto[u]ll with the following
     *  two exceptions: the only bases supported are 0, 8, 10, 16;
     *  ::errno is not set
     */
    static uint8_t    strtou8(const char *, char ** end=0, int base=0);

    // Input to an instream
    static istream&    _scan_uint8(istream& i, uint8_t &, 
                bool chew_white,
                bool is_signed,
                bool& rangerr);

    static bool        is_finite(const f8_t x);
    static bool        is_infinite(const f8_t x);
    static bool        is_nan(const f8_t x);
    static bool        is_infinite_or_nan(const f8_t x);

    /*
     * Endian conversions that don't require any non-shore headers.
     * These may not be inlined, but that is the portability tradeoff.
     * w_ prefix due to typical macro problems with the names.
     * Why not use overloaded args?   Great idea, but unintentional
     * conversions could be a big problem with this stuff.
     * Used by w_opaque.
     */
    static uint2_t    w_ntohs(uint2_t);
    static uint2_t    w_htons(uint2_t);
    static uint4_t    w_ntohl(uint4_t);
    static uint4_t    w_htonl(uint4_t);

    ///  standard streams
    friend ostream&        operator<<(
        ostream&            o,
        const w_base_t&            obj);

    /// print a message and abort
    static void            assert_failed(
        const char*            desc,
        const char*            file,
        uint4_t             line);

    /// dump core
    static    void        abort();

    /**\brief Comparison Operators 
     * \enum CompareOp
     * */ 
    enum CompareOp {
    badOp=0x0, eqOp=0x1, gtOp=0x2, geOp=0x3, ltOp=0x4, leOp=0x5,
    /* for internal use only: */
    NegInf=0x100, eqNegInf, gtNegInf, geNegInf, ltNegInf, leNegInf,
    PosInf=0x400, eqPosInf, gtPosInf, gePosInf, ltPosInf, lePosInf
    };

    /**\enum lock_mode_t 
     * \brief Lock modes for the Storage Manager.
     * Note: Capital letters are used to match common usage in DB literature
     * Note: Values MUST NOT CHANGE since order is significant.
     * \ref SSMLOCK
     */
    enum lock_mode_t {
        NL = 0,         /* no lock                */
        IS,         /* intention share (read)        */
        IX,            /* intention exclusive (write)        */
        SH,            /* share (read)             */
        SIX,        /* share with intention exclusive    */
        UD,            /* update (allow no more readers)    */
        EX            /* exclusive (write)            */
    };

    /**\enum lock_duration_t
     * \brief Duration for locks
     * \ref SSMLOCK
     */
    enum lock_duration_t {
        t_instant     = 0,    /* released as soon as the lock is acquired */
        t_short     = 1,    /* held until end of some operation         */
        t_medium     = 2,    /* held until explicitly released           */
        t_long     = 3,    /* held until xct commits                   */
        t_very_long = 4,    /* held across xct boundaries               */
        t_num_durations = 5 /* not a duration -- used for typed comparisons */
    };

    /**\enum vote_t
     * \brief Votes for two-phase commit.
     * - vote_readonly : storage manager will return this from 
     *   ss_m::prepare_xct when transaction has not logged anything.
     *   See also ss_m::force_vote_readonly.
     * - vote_abort : Might be returned if error occurs.
     * - vote_commit : Usual result of prepare.
     * \ref SSM2PC
     */
    enum vote_t {
        vote_bad,    /* illegit value                */
        vote_readonly,  /* no ex locks acquired for this tx         */
        vote_abort,     /* cannot commit                            */
        vote_commit     /* can commit if so told                    */
    };
};


/* XXX compilers+environment that need this operator defined */

/**\def w_reset_strstream(s)
 *\brief Allow a ostrstream to be reused.  
 */

/* This works for the shore w_strstream.   But, wait, why
   is it different for visual c++?  It doesn't need to be?
   It is different so you can reset an ordinary strstream 
   with it also.    A better solution for w_strstreams and
   strstreams would be something overloaded instead of a macro. */

#if defined (__SUNPRO_CC)
#define w_reset_strstream(s) \
  do { \
    s.clear();                \
    s.seekp(0); \
  } while(0)
#else
#define    w_reset_strstream(s)        \
    do {                \
        s.clear();        \
        s.seekp(ios::beg);    \
    } while (0)
#endif


/*--------------------------------------------------------------*
 *  w_base_t::is_aligned()                    *
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_aligned(size_t sz)
{
    return (align(sz) == sz);
}

inline bool
w_base_t::is_aligned(const void* s)
{
    /* XXX works OK if there is a size mismatch because we are looking
       at the *low* bits */
    return is_aligned((ptrdiff_t)(s));
}

/*--------------------------------------------------------------*
 *  w_base_t::is_big_endian()                    *
 *--------------------------------------------------------------*/
inline bool w_base_t::is_big_endian()
{
#ifdef WORDS_BIGENDIAN
    return true;
#else
    return false;
#endif
}

/*--------------------------------------------------------------*
 *  w_base_t::is_little_endian()                *
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_little_endian()
{
    return ! is_big_endian();
}

/**\brief Class that adds virtual destructor to w_base_t. 
 */
class w_vbase_t : public w_base_t {
public:
    NORET                w_vbase_t()    {};
    virtual NORET        ~w_vbase_t()    {};
};

/*
 * These types are auto-initialized filler space for alignment
 * in structures.  The auto init helps with purify.
 *
 * XXX Some of the users of these structures DEPEND on zero
 * fill (sm keys, etc). Eventually this will be seperated into
 * zero fill and plain fill (zeroed with ZERO_INIT).  Until then
 * these must always be initialized to 0.
 */
/**\brief Auto-initialized 1-byte filler for alignment of structures*/
struct fill1 {
    w_base_t::uint1_t u1;
    fill1() : u1(0) {}
};

/**\brief Auto-initialized 2-byte filler for alignment of structures*/
struct fill2 {
    w_base_t::uint2_t u2;
    fill2() : u2(0) {}
};

/**\brief Auto-initialized 3-byte filler for alignment of structures*/
struct fill3 {
    w_base_t::uint1_t    u1[3];
    fill3() { u1[0] = u1[1] = u1[2] = 0; }
};

/**\brief Auto-initialized 4-byte filler for alignment of structures*/
struct fill4 {
    w_base_t::uint4_t u4;
    fill4() : u4(0) {}
};


#include <w_autodel.h>
#include <w_error.h>
#include <w_rc.h>

template<bool B> struct CompileTimeAssertion;
/** \brief Compile-time assertion trick. 
 * \details
 * See compile_time_assert.
 */
template<> struct CompileTimeAssertion<true> { void reference() {} };

/** \brief Compile-time assertion trick. 
 * \details
 * If the assertion fails 
 * you will get a compile error.
 * The problem is that you will also get an unused variable
 * complaint if warnings are turned on, so we make a bogus
 * reference to the named structure.
 *
 * This is used by macros 
 * - ASSERT_FITS_IN_LONGLONG 
 * - ASSERT_FITS_IN_POINTER 
 *   which are enabled only if built with some
 *   debug level 1 or above (e.g., configure --with-debug-level1)
 *   This enables us to continue to build a --disable-lp64 system even
 *   though it's known yet fully supported (not safe).
 *
 */
template<typename T> struct compile_time_assert 
{
    compile_time_assert() {
        CompileTimeAssertion<sizeof(long) == 8> assert_8byte_long;
        CompileTimeAssertion<sizeof(long) >= sizeof(T)> assert_long_holds_T;
    }
};

#if W_DEBUG_LEVEL > 4
#define ASSERT_FITS_IN_LONGLONG(T) {                \
    CompileTimeAssertion<sizeof(long long) >= sizeof(T)> assert__##T##__fits_in_longlong; \
    assert__##T##__fits_in_longlong.reference(); \
    }                    
#define ASSERT_FITS_IN_POINTER(T) {                \
    CompileTimeAssertion<sizeof(void*) >= sizeof(T)> assert__##T##__fits_in_pointer; \
    assert__##T##__fits_in_pointer.reference(); \
    }                    
#else

#define ASSERT_FITS_IN_POINTER(T) 
#define ASSERT_FITS_IN_LONGLONG(T) 
#endif
/*<std-footer incl-file-exclusion='W_BASE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

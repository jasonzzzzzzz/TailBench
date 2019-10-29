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

/*<std-header orig-src='shore' incl-file-exclusion='W_DEBUG_H'>

 $Id: w_debug.h,v 1.19 2010/05/26 01:20:23 nhall Exp $

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

#ifndef W_DEBUG_H
#define W_DEBUG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#ifndef W_BASE_H
/* NB: DO NOT make this include w.h -- not yet */
#include <w_base.h>
#endif /* W_BASE_H */

#include <w_stream.h>

#ifndef ERRLOG_H
#include <errlog.h>
#endif /* ERRLOG_H */

/**\file w_debug.h
 *\ingroup MACROS
 *
*  This is a set of macros for use with C or C++. They give various
*  levels of debugging printing when compiled with --enable-trace.
*  With tracing, message printing is under the control of an environment
*  variable DEBUG_FLAGS (see debug.cpp).  
*  If that variable is set, its value must 
*  be  a string.  The string is searched for __FILE__ and the function name 
*  in which the debugging message occurs.  If either one appears in the
*  string (value of the env variable), or if the string contains the
*  word "all", the message is printed.  
*
*
**** DUMP(x)  prints x (along with line & file)  
*             if "x" is found in debug environment variable
*
**** FUNC(fname)  DUMPs the function name.
**** RETURN   prints that the function named by __func__ is returning
*             This macro  MUST appear within braces if used after "if",
*              "else", "while", etc.
*
**** DBG(arg) prints line & file and the message arg if __func__
*              appears in the debug environment variable.
*             The argument must be the innermost part of legit C++
*             print statement, and it works ONLY in C++ sources.
*
*  Example :
*
* \code
*    returntype
*    proc(args)
*    {
*        FUNC(proc);
*       ....body...
*
*       DBG(
*          << "message" << value
*          << "more message";
*          if(test) {
*             cerr << "xyz";
*          }
*          cerr
*       )
*
*       ....more body...
*       if(predicate) {
*           RETURN value;
*        }
*    }
*  \endcode
*
 * DUMP, FUNC, and RETURN macros' definitions depend on how
 * the storage manager is configured.
 * They don't do a lot unless configured with --enable-trace
*/
#include <cassert>

#undef USE_REGEX

#ifdef USE_REGEX
#include "regex_posix.h"
#endif /* USE_REGEX */

/* XXX missing type in vc++, hack around it here too, don't pollute
   global namespace too badly. */
typedef    ios::fmtflags    w_dbg_fmtflags;


#ifdef W_TRACE

#define _strip_filename(f) f

#define DUMP()\
  do { \
    if(_w_debug.flag_on(__func__,__FILE__)) {\
    _w_debug.clog << __LINE__ << " " << _strip_filename(__FILE__) << ": " << __func__\
              << flushl; } } while(0)

#define FUNC(fn)\
        DUMP()

#define RETURN \
                do { \
            if(_w_debug.flag_on(__func__,__FILE__)) {\
            w_dbg_fmtflags old = _w_debug.clog.setf(ios::dec, ios::basefield); \
            _w_debug.clog  << __LINE__ << " " << _strip_filename(__FILE__) << ":" ; \
            _w_debug.clog.setf(old, ios::basefield); \
            _w_debug.clog << "return from " << __func__ << flushl; } } while(0); \
            return 

#else /* -UW_TRACE */
#    define DUMP(str)
#    define FUNC(fn)
#    undef RETURN
#    define RETURN return
#endif  /* W_TRACE*/

/* ************************************************************************  */

/* ************************************************************************  
 * 
 * Class w_debug, macros DBG, DBG_NONL, DBG1, DBG1_NONL:
 */


/**\brief An ErrLog used for tracing (configure --enable-trace)
 *
 * For tracing to be used, you must set the environment variable
 * DEBUG_FLAGS to a string containing the names of the files you
 * want traced, and
 *
 * DEBUG_FILE to the name of the output file to which the output
 * should be sent. If DEBUG_FILE is not set, the output goes to
 * stderr.
 */
class w_debug : public ErrLog {
    private:
        char *_flags;
        enum { _all = 0x1, _none = 0x2 };
        unsigned int        mask;
        int            _trace_level;

#ifdef USE_REGEX
        static regex_t        re_posix_re;
        static bool        re_ready;
        static char*        re_error_str;
        static char*        re_comp_debug(const char* pattern);
        static int        re_exec_debug(const char* string);
#endif /* USE_REGEX */

        int            all(void) { return (mask & _all) ? 1 : 0; }
        int            none(void) { return (mask & _none) ? 1 : 0; }

    public:
        w_debug(const char *n, const char *f);
        ~w_debug();
        int flag_on(const char *fn, const char *file);
        const char *flags() { return _flags; }
        void setflags(const char *newflags);
        void memdump(void *p, int len); // hex dump of memory
        int trace_level() { return _trace_level; }
};
extern w_debug _w_debug;

#if defined(W_TRACE)

#    define DBG1(a) do {\
    if(_w_debug.flag_on(__func__,__FILE__)) {                \
        w_dbg_fmtflags old = _w_debug.clog.setf(ios::dec, ios::basefield); \
        _w_debug.clog  << _strip_filename(__FILE__) << ":" << __LINE__ << ":" ; \
        _w_debug.clog.setf(old, ios::basefield); \
        _w_debug.clog  a    << flushl; \
    } } while(0)

#    define DBG(a) DBG1(a)

#else
#    define DBG(a) 
#endif  /* defined(W_TRACE) */
/* ************************************************************************  */


#define DBGTHRD(arg) DBG(<<" th."<<sthread_t::me()->id << " " arg)

/*<std-footer incl-file-exclusion='W_DEBUG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

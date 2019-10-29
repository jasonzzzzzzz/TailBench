/*<std-header orig-src='shore'>

 $Id: crash.cpp,v 1.16 2010/05/26 01:20:38 nhall Exp $

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

/*
 * This file is built only if we are using the SSM test harness,
 * so that we can simulate crashes
 */

#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_0.h>
#include <crash.h>

// For use by LOGTRACE macro
// Settable in debugger and by smsh
bool logtrace(false);

#if defined(USE_SSMTEST)

struct debuginfo {
        debuginfo_enum kind;
        bool          valid;
        bool          initialized;
        int           value;
        int           matches;
        char*         name;
};

static
struct debuginfo _debuginfo = {
    debug_none,
    false,
    false,
    0, 0, 0
};

void
_setdebuginfo(
    debuginfo_enum e,
    struct debuginfo &_d,
    const char *name, int value
) 
{
    w_assert3(strlen(name)>0);

    _d.initialized = true;
    _d.valid = true;
    _d.matches = 0;
    _d.value = value;
    _d.kind = e;

    // Make a copy
    if(_d.name) {
        delete[] _d.name;
        _d.name = 0;
    }
    {         int l = strlen(name)+1;
        _d.name = new char[l];
        w_assert3(_d.name);
        memcpy(_d.name, name, l);
    }
    cerr << __LINE__ << ":" 
        << _d.name << " = " << _d.value 
        << " init:" << _d.initialized 
        << " valid:" << _d.valid 
        << " matches:" << _d.matches 
        << " kind:" << int(_d.kind)
        << endl;
}

void
getdebuginfo( 
    /*
     * from environment:  Take 3 environment variables:
     * The 1st environment variable indicates what
     * kind of test this is: delay, crash, etc.
     *
     * The 2nd environment variable is a string that 
     * matches some string in a CRASHTEST call in the code.  
     *
     * The 3rd is an integer that indicates a what pass
     * over that CRASHTEST call this delay/crash/abort/yield should 
     * take effect.  
     */
    struct debuginfo &_d, 
        const char *K, 
        const char *T, 
        const char *V
) 
{
    w_assert3(strlen(T)>0);
    w_assert3(strlen(V)>0);
    w_assert3(strlen(K)>0);

    if(_d.initialized) return;

    char *n=0, *k=0; int val=0;
    if( (k = ::getenv(K)) ) {
        /* convert k into an enum_kind */
        debuginfo_enum _k=debug_none;
        if(strcmp(k, "crash")==0){ _k = debug_crash; }
        else if(strcmp(k, "abort")==0){ _k = debug_abort; }
        else if(strcmp(k, "yield")==0){ _k = debug_yield; }
        else if(strcmp(k, "delay")==0){ _k = debug_delay; }
        else {
            cerr << k << ": bad value for environment variable " 
                << K << endl;
        }

        if( (n = ::getenv(T)) ) {
            char *v = ::getenv(V);
            if(v) {
                val = atoi(v);
            }
            _setdebuginfo(_k,_d,n,val);
        }
    }
}
void
setdebuginfo(
    debuginfo_enum kind,
    const char *name, int value
) 
{
    _setdebuginfo(kind, _debuginfo, name, value);
}


static void
crashtest(
    log_m *   log,
    const char * W_IFTRACE(c),
    const char *file,
    int line
) 
{
    cerr << "crashtest" << endl;


    if(_debuginfo.value == 0 || 
        _debuginfo.matches == _debuginfo.value)  {

        /* Flush and sync the log to the current lsn_t, just
         * because we want the semantics of the CRASHTEST
         * to be that it crashes after the logging was
         * done for a given source line -- it just makes
         * the crash tests easier to insert this way.
         */
        if(log) {
           W_COERCE(log->flush(log->curr_lsn()));
           w_assert3(log->durable_lsn() == log->curr_lsn());
            DBG( << "Crashtest " << c
                << " durable_lsn is " << log->durable_lsn() );
        }
        // Just to be sure that everything's sent, flushed, etc.
        me()->yield();

        /* skip destructors */
        fprintf(stderr, "CRASH %d at %s from %s %d\n",
                _debuginfo.value, _debuginfo.name, file, line);
        _exit(44); // unclean shutdown/crash
    }
}

static void
delaytest(
    const char *file,
    int line
)
{
    if(_debuginfo.value != 0 ) {
        /*
         * put the thread to sleep for X millisecs
         */
        cerr << "DELAY " 
                << _debuginfo.value 
                << " at " << _debuginfo.name
                << " at line " << line
                << " file " << file
                << endl;
        me()->sleep(_debuginfo.value, _debuginfo.name);
    }
}

w_rc_t 
aborttest() 
{
    if( _debuginfo.matches == _debuginfo.value)  {
        return RC(smlevel_0::eUSERABORT);
    }
    return RCOK;
}

static void
yieldtest() 
{
    if( _debuginfo.matches == _debuginfo.value)  {
        me()->yield();
    }
}

w_rc_t
ssmtest(
    log_m *   log,
    const char *c, 
    const char *file,
    int line
) 
{
#undef LOCATING
#ifdef LOCATING
    smlevel_0::errlog->clog << emerg_prio << c << flushl;
#endif

    w_assert3(strlen(c)>0);

    // get info from environment if necessary
    getdebuginfo(_debuginfo, "SSMTEST_KIND", "SSMTEST", "SSMTEST_ITER");
    if(! _debuginfo.valid)  return RCOK;
    if(::strcmp(_debuginfo.name,c) != 0) return RCOK;
    ++_debuginfo.matches;
    fprintf(stderr, "Ssh test %s #%d value=%d kind=%d\n", 
            c, _debuginfo.matches, 
            _debuginfo.value, int(_debuginfo.kind) );

    switch(_debuginfo.kind) {
        case debug_delay:
                delaytest(file, line);
                break;
        case debug_crash:
                crashtest(log, c, file, line);
                break;
        case debug_yield:
                yieldtest();
                break;
        case debug_abort:
                return aborttest();
                break;
        default:
                cerr<< "Unknown kind: " << int(_debuginfo.kind) <<endl;
                return RCOK;
    }
    if(::strcmp(_debuginfo.name,c) != 0) return RCOK;

    return RCOK;
}
#endif /* defined(USE_SSMTEST) */


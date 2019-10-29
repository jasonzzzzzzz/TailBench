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

/*<std-header orig-src='shore' incl-file-exclusion='BF_H'>

 $Id: bf.h,v 1.100 2010/06/08 22:28:55 nhall Exp $

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

#ifndef BF_H
#define BF_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *  Buffer manager interface.  
 *  Do not put any data members in bf_m.
 *  Implementation is in bf.cpp bf_core.[ch].
 *
 *  Everything in bf_m is static since there is only one
 *        buffer manager.  
 */

#include <bf_s.h>
#include <page_s.h>

#ifdef __GNUG__
#pragma interface
#endif

struct bfcb_t;
class page_p;
class bf_core_m;
class bf_cleaner_thread_t;
class bf_filter_t;
struct bf_page_writer_control_t; // forward

class bf_m : public smlevel_0 
{
    friend class bf_cleaner_thread_t;
    friend class page_writer_thread_t;
    friend class bfcb_t;
#ifdef HTAB_UNIT_TEST_C
    friend class htab_tester; 
#endif

public:
    NORET                        bf_m(uint4_t max, char *bf, uint4_t pg_writer_cnt);
    NORET                        ~bf_m();

    static int                   collect(vtable_t&, bool names_too);
    static long                  mem_needed(int n);
    
    static int                   npages();

    static bool                  is_cached(const bfcb_t* e);

    static rc_t                  fix(
        page_s*&                           page,
        const lpid_t&                      pid, 
        uint2_t                            tag,
        latch_mode_t                       mode,
        bool                               no_read,
        store_flag_t&                      out_stflags,
        bool                               ignore_store_id = false,
        store_flag_t                       stflags = st_bad
                                        );

    static rc_t                  conditional_fix(
        page_s*&                    page,
        const lpid_t&                    pid, 
        uint2_t                            tag,
        latch_mode_t                     mode,
        bool                             no_read,
        store_flag_t&                    out_stflags,
        bool                             ignore_store_id = false,
        store_flag_t                    stflags = st_bad
        );

    static rc_t                  refix(
        const page_s*                     p,
        latch_mode_t                     mode);

    static rc_t                  get_page(
        const lpid_t&               pid,
        bfcb_t*                     b,
        uint2_t                     ptag,
        bool                        no_read,
        bool                        ignore_store_id);

    // upgrade page latch, only if would not block
    // set would_block to true if upgrade would block
    static void                  upgrade_latch_if_not_block(
        const page_s*                     p,
        // MULTI-SERVER only: remove
        bool&                             would_block);

    static latch_mode_t          latch_mode(
        const page_s*                     p
        );

    static void                  upgrade_latch(
        page_s*&                     p,
        latch_mode_t                    m
        );

    static void                  unfix(
        const page_s*                    buf, 
        bool                            dirty = false,
        int                            refbit = 1);

    static void                  unfix_dirty(
        const page_s*&                    buf, 
            int                            refbit = 1) {
        unfix(buf, true, refbit); 
    }

    static rc_t                  set_dirty(const page_s* buf);
    static bool                  is_dirty(const page_s* buf) ;

    static void                  discard_pinned_page(const page_s* buf);
    static rc_t                  discard_store(stid_t stid);
    static rc_t                  discard_volume(vid_t vid);
private:
    static bool                  _set_dirty(bfcb_t *b);
    static rc_t                  _discard_all();
public:
    
    // for debugging: used only with W_DEBUG_LEVEL > 0
    static  bool                 check_lsn_invariant(const page_s *page);
    static  bool                 check_lsn_invariant(const bfcb_t *b);

    static rc_t                  force_store(
        stid_t                           stid,
        bool                             flush);
    static rc_t                  force_page(
        const lpid_t&                     pid,
        bool                             flush = false);
    static rc_t                  force_until_lsn(
        const lsn_t&                     lsn,
        bool                             flush = false);
    static rc_t                  force_all(bool flush = false);
    static rc_t                  force_volume(
        vid_t                             vid, 
        bool                             flush = false);

    static bool                 is_mine(const page_s* buf) ;
    static const latch_t*       my_latch(const page_s* buf) ;
    static bool                 fixed_by_me(const page_s* buf) ;
    static bool                 is_bf_page(const page_s* p, 
                                                  bool and_in_htab = true);
    // true == no longer hold any old dirty pages
    // false == unable to flush all old dirty pages we hold
    bool                        force_my_dirty_old_pages(lpid_t const* 
                                                       wal_page=0) const;
    
    static bfcb_t*              get_cb(const page_s*) ;

    static void                 dump(ostream &o);
    static void                 stats(
        u_long&                     fixes, 
        u_long&                     unfixes,
        bool                        reset);
    void                        htab_stats(bf_htab_stats_t &) const;
        

    static void                 snapshot(
        u_int&                             ndirty, 
        u_int&                             nclean,
        u_int&                             nfree, 
        u_int&                             nfixed);

    static void                 snapshot_me(
        u_int&                             nsh, 
        u_int&                             nex,
        u_int&                             ndiff
        );

    static lsn_t                min_rec_lsn();
    static rc_t                 get_rec_lsn(
        int                                &start_idx, 
        int                                &count,
        lpid_t                             pid[],
        lsn_t                              rec_lsn[],
        lsn_t                              &min_rec_lsn
        );

    static rc_t                 enable_background_flushing(vid_t v);
    static rc_t                 disable_background_flushing(vid_t v);
    static rc_t                 disable_background_flushing(); // all

    static void                 activate_background_flushing(vid_t *v=0);

private:
    static bf_core_m*           _core;

    static rc_t                 _fix(
        timeout_in_ms                    timeout, 
        page_s*&                         page,
        const lpid_t&                    pid, 
        uint2_t                          tag,
        latch_mode_t                     mode,
        bool                             no_read,
        store_flag_t&                    return_store_flags,
        bool                             ignore_store_id = false,
        store_flag_t                     stflags = st_bad
        );

    static rc_t                 _scan(
        const bf_filter_t&               filter,
        bool                             write_dirty,
        bool                             discard);
    
    static rc_t                 _write_out(const page_s* b, uint4_t cnt);
    static rc_t                 _replace_out(bfcb_t* b);

    static w_list_t<bf_cleaner_thread_t, queue_based_block_lock_t>*  
                                        _cleaner_threads;
    static queue_based_block_lock_t        _cleaner_threads_list_mutex;  

    static rc_t                        _clean_buf(
        bf_page_writer_control_t *         pwc,
        int                                count, 
        lpid_t                             pids[],
        timeout_in_ms                      timeout,
        bool*                              retire_flag);
    static rc_t                        _clean_segment(
        int                                count, 
        lpid_t                             pids[],
        page_s*                            pbuf,
        timeout_in_ms                      last_pass_timeout, 
                                            // WAIT_IMMEDIATE or WAIT_FOREVER
        bool*                              cancel_flag);

    // more stats
    static void                 _incr_page_write(int number, bool bg);

};

inline rc_t
bf_m::fix(
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2_t               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    store_flag_t&        return_store_flags,
    bool                ignore_store_id, // default = false
    store_flag_t        stflags // for case no_read
)
{
    return _fix(WAIT_FOREVER, ret_page, pid, tag, mode,
        no_read, return_store_flags, ignore_store_id, stflags);
}

inline rc_t
bf_m::conditional_fix(
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2_t               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    store_flag_t&        return_store_flags,
    bool                ignore_store_id, // default = false
    store_flag_t        stflags // for case no_read
)
{
    return _fix(WAIT_IMMEDIATE, ret_page, pid, tag, mode,
        no_read, return_store_flags, ignore_store_id, stflags);
}

/*<std-footer incl-file-exclusion='BF_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

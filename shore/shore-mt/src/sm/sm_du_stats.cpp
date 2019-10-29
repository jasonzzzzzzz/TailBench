/*<std-header orig-src='shore'>

 $Id: sm_du_stats.cpp,v 1.31 2010/06/15 17:30:07 nhall Exp $

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

#define SM_DU_STATS_C

#ifdef __GNUG__
#pragma implementation
#endif

#include <w.h>
#include <cstring>
#include <w_stream.h>
#include <w_debug.h>
#include <w_list.h>
#include <w_minmax.h>
#define SM_SOURCE
#include <basics.h>
#include "lid_t.h"
#include "sm_s.h"
#include "sm_int_1.h"
#include "sm_du_stats.h"

#if W_DEBUG_LEVEL>=0
#define DEBUG_GNATS_77 1
// This prints the failures to cerr rather than requiring that
// the debugging be turned on in a trace-enabled build.
#endif

// This function is a convenient debugging breakpoint for
// detecting audit failures.
static w_rc_t
stats_audit_failed(int 
#if DEBUG_GNATS_77
        line
#endif
        )
{
#if DEBUG_GNATS_77
    cerr << "stats audit failed at line " << line 
        << " of file " __FILE__ << endl;
    w_assert1(0);
#endif 
    return RC(smlevel_0::eDUAUDITFAILED);
}

void
file_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
        ((base_stat_t*)this)[i] = 0;
    }
}

void
file_pg_stats_t::add(const file_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
        ((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
file_pg_stats_t::audit() const
{
    FUNC(file_pg_stats_t::audit);
    rc_t result;

    // BYTE COUNTS
    if (total_bytes() % smlevel_0::page_sz != 0) {
        DBG(
            << " file_pg_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
#if DEBUG_GNATS_77
        cerr << " file_pg_stats_t::total_bytes= " << total_bytes()
        << " smlevel_0::page_sz= " << int(smlevel_0::page_sz) << endl;
#endif
        result = stats_audit_failed(__LINE__);
    }

    // BYTE COUNTS
    if (lg_rec_cnt) {
        if (rec_lg_chunk_bs + rec_lg_indirect_bs == 0) {
            DBG( << " lg_rec_cnt= " << lg_rec_cnt);
            DBG( << " rec_lg_chunk_bs= " << rec_lg_chunk_bs);
            DBG( << " rec_lg_indirect_bs= " << rec_lg_indirect_bs);
            if(result.is_error()) {} // don't croak on error-not-checked
            result = stats_audit_failed(__LINE__);
        }
    }
    DBG(<<"file_pg_stats_t audit ok");
    return result;
}

base_stat_t
file_pg_stats_t::total_bytes() const
{
    base_stat_t  total = 0;
    total += hdr_bs;
    total += slots_used_bs;
    total += slots_unused_bs;
    total += rec_tag_bs;
    total += rec_hdr_bs;
    total += rec_hdr_align_bs;
    total += rec_body_bs;
    total += rec_body_align_bs;
    total += rec_lg_chunk_bs;
    total += rec_lg_indirect_bs;
    total += free_bs;
    return total;
}


ostream& operator<<(ostream& o, const file_pg_stats_t& s)
{
    /*
    return o
    << "hdr_bs "                << s.hdr_bs << endl
    << "slots_used_bs "                << s.slots_used_bs << endl
    << "slots_unused_bs "        << s.slots_unused_bs << endl
    << "rec_tag_bs "                << s.rec_tag_bs << endl
    << "rec_hdr_bs "                << s.rec_hdr_bs << endl
    << "rec_hdr_align_bs "        << s.rec_hdr_align_bs << endl
    << "rec_body_bs "                << s.rec_body_bs << endl
    << "rec_body_align_bs "        << s.rec_body_align_bs << endl

    << "rec_lg_chunk_bs "        << s.rec_lg_chunk_bs << endl
    << "rec_lg_indirect_bs "        << s.rec_lg_indirect_bs << endl
    << "free_bs "                << s.free_bs << endl

    << "small_rec_cnt "                << s.small_rec_cnt << endl
    << "lg_rec_cnt "                << s.lg_rec_cnt << endl
    ;
    */
    s.print(o, "");
    return o;
}

void file_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const file_pg_stats_t &s = *this;
    o
    << pfx << "hdr_bs "                        << s.hdr_bs << endl
    << pfx << "slots_used_bs "                << s.slots_used_bs << endl
    << pfx << "slots_unused_bs "        << s.slots_unused_bs << endl
    << pfx << "rec_tag_bs "                << s.rec_tag_bs << endl
    << pfx << "rec_hdr_bs "                << s.rec_hdr_bs << endl
    << pfx << "rec_hdr_align_bs "        << s.rec_hdr_align_bs << endl
    << pfx << "rec_body_bs "                << s.rec_body_bs << endl
    << pfx << "rec_body_align_bs "        << s.rec_body_align_bs << endl

    << pfx << "rec_lg_chunk_bs "        << s.rec_lg_chunk_bs << endl
    << pfx << "rec_lg_indirect_bs "        << s.rec_lg_indirect_bs << endl
    << pfx << "free_bs "                << s.free_bs << endl

    << pfx << "small_rec_cnt "                << s.small_rec_cnt << endl
    << pfx << "lg_rec_cnt "                << s.lg_rec_cnt << endl
    ;
}




void
lgdata_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
        ((base_stat_t*)this)[i] = 0;
    }
}

void
lgdata_pg_stats_t::add(const lgdata_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(hdr_bs) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
        ((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
lgdata_pg_stats_t::audit() const
{
    // BYTE COUNTS
    FUNC(lgdata_pg_stats_t::audit);
    w_rc_t result;
    if (total_bytes() % smlevel_0::page_sz != 0)  {
        DBG(
            << " lgdata_pg_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        if(result.is_error()) {} // don't croak on error-not-checked
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"lgdata_pg_stats_t audit ok");
    return result;
}

base_stat_t
lgdata_pg_stats_t::total_bytes() const
{
    return hdr_bs + data_bs + unused_bs;
}


ostream& operator<<(ostream& o, const lgdata_pg_stats_t& s)
{
    /*
    return o
    << "hdr_bs "                << s.hdr_bs << endl
    << "data_bs "                << s.data_bs << endl
    << "unused_bs "                << s.unused_bs << endl
    ;
    */
    s.print(o, "");
    return o;
}

void lgdata_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const lgdata_pg_stats_t &s = *this;
    o
    << pfx << "hdr_bs "                << s.hdr_bs << endl
    << pfx << "data_bs "                << s.data_bs << endl
    << pfx << "unused_bs "                << s.unused_bs << endl
    ;
}

void
lgindex_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
        ((base_stat_t*)this)[i] = 0;
    }
}

void
lgindex_pg_stats_t::add(const lgindex_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(used_bs); i++) {
        ((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
lgindex_pg_stats_t::audit() const
{
    // BYTE COUNTS
    FUNC(lgindex_pg_stats_t::audit);
    w_rc_t result;
    if (total_bytes() % smlevel_0::page_sz != 0) {
        DBG(
            << " lgindex_pg_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"lgindex_pg_stats_t audit ok");
    return result;
}

base_stat_t
lgindex_pg_stats_t::total_bytes() const
{
    return used_bs + unused_bs;
}

ostream& operator<<(ostream& o, const lgindex_pg_stats_t& s)
{
    /*
    return o
    << "used_bs "                << s.used_bs << endl
    << "unused_bs "                << s.unused_bs << endl
    ;
    */
    s.print(o, "");
    return o;
}

void lgindex_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const lgindex_pg_stats_t &s = *this;
    o 
    << pfx << "used_bs "                << s.used_bs << endl
    << pfx << "unused_bs "                << s.unused_bs << endl
    ;
}


void
file_stats_t::clear()
{
    file_pg.clear();
    lgdata_pg.clear();
    lgindex_pg.clear();

    file_pg_cnt = 0;
    lgdata_pg_cnt = 0;
    lgindex_pg_cnt = 0;
    unalloc_file_pg_cnt = 0;
    unalloc_large_pg_cnt = 0;
}

void
file_stats_t::add(const file_stats_t& stats)
{
    file_pg.add(stats.file_pg);
    lgdata_pg.add(stats.lgdata_pg);
    lgindex_pg.add(stats.lgindex_pg);

    file_pg_cnt += stats.file_pg_cnt;
    lgdata_pg_cnt += stats.lgdata_pg_cnt;
    lgindex_pg_cnt += stats.lgindex_pg_cnt;
    unalloc_file_pg_cnt += stats.unalloc_file_pg_cnt;
    unalloc_large_pg_cnt += stats.unalloc_large_pg_cnt;
}

w_rc_t
file_stats_t::audit() const
{
    FUNC(file_stats_t::audit);

    W_DO(file_pg.audit());
    W_DO(lgdata_pg.audit());
    W_DO(lgindex_pg.audit());

    base_stat_t total_alloc_pgs = alloc_pg_cnt();

    w_rc_t result;
    if (total_alloc_pgs*smlevel_0::page_sz != total_bytes()) {
        DBG(
            << " total_alloc_pgs= " << total_alloc_pgs
            << " file_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    if ( (total_alloc_pgs + unalloc_file_pg_cnt + unalloc_large_pg_cnt) % smlevel_0::ext_sz != 0) {
        DBG(
            << " total_alloc_pgs= " << total_alloc_pgs
            << " unalloc_file_pg_cnt= " << unalloc_file_pg_cnt
            << " unalloc_large_pg_cnt= " << unalloc_large_pg_cnt
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
        );
#if DEBUG_GNATS_77
            cerr 
            << " total_alloc_pgs= " << total_alloc_pgs
            << " unalloc_file_pg_cnt= " << unalloc_file_pg_cnt
            << " unalloc_large_pg_cnt= " << unalloc_large_pg_cnt
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
            << endl;
#endif
        if(result.is_error()) {} // don't croak on error-not-checked
        result = stats_audit_failed(__LINE__);
        if(result.is_error()) {}
    }
    DBG(<<"file_stats_t audit ok");
    return result;
}

base_stat_t
file_stats_t::total_bytes() const
{
    return file_pg.total_bytes() + lgdata_pg.total_bytes() +
                           lgindex_pg.total_bytes();
}

base_stat_t
file_stats_t::alloc_pg_cnt() const
{
    return file_pg_cnt + lgdata_pg_cnt + lgindex_pg_cnt;
}


ostream& operator<<(ostream& o, const file_stats_t& s)
{
    /*
    return 
    o << s.file_pg << s.lgdata_pg << s.lgindex_pg
    << "file_pg_cnt "                << s.file_pg_cnt << endl
    << "lgdata_pg_cnt "                << s.lgdata_pg_cnt << endl
    << "lgindex_pg_cnt "        << s.lgindex_pg_cnt << endl
    << "unalloc_file_pg_cnt "        << s.unalloc_file_pg_cnt << endl
    << "unalloc_large_pg_cnt "        << s.unalloc_large_pg_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void file_stats_t::print(ostream& o, const char *pfx) const
{
    const file_stats_t &s = *this;

    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
        memcpy(pfx1+pfxlen, "fipg.", 6);
        s.file_pg.print(o,pfx1); 

        memcpy(pfx1+pfxlen, "lgpg.", 6);
        s.lgdata_pg.print(o,pfx1); 

        memcpy(pfx1+pfxlen, "lgix.", 6);
        s.lgindex_pg.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx<< "file_pg_cnt "                << s.file_pg_cnt << endl
    << pfx<< "lgdata_pg_cnt "                << s.lgdata_pg_cnt << endl
    << pfx<< "lgindex_pg_cnt "        << s.lgindex_pg_cnt << endl
    << pfx<< "unalloc_file_pg_cnt "        << s.unalloc_file_pg_cnt << endl
    << pfx<< "unalloc_large_pg_cnt "        << s.unalloc_large_pg_cnt << endl
    ;
}



void
btree_lf_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
        ((base_stat_t*)this)[i] = 0;
    }
}

void
btree_lf_stats_t::add(const btree_lf_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
        ((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
btree_lf_stats_t::audit() const
{
    FUNC(btree_lf_stats_t::audit);
    w_rc_t result;
    if (total_bytes() % smlevel_0::page_sz != 0) {
        DBG(
            << " btree_lf_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"btree_lf_stats_t audit ok");
    return result;
}

base_stat_t
btree_lf_stats_t::total_bytes() const
{
    return hdr_bs + key_bs + data_bs + entry_overhead_bs + unused_bs;
}


ostream& operator<<(ostream& o, const btree_lf_stats_t& s)
{
    /*
    return 
    o
    << "hdr_bs "                << s.hdr_bs << endl
    << "key_bs "                << s.key_bs << endl
    << "data_bs "                << s.data_bs << endl
    << "entry_overhead_bs "        << s.entry_overhead_bs << endl
    << "unused_bs "                << s.unused_bs << endl
    << "entry_cnt "                << s.entry_cnt << endl
    << "unique_cnt "                << s.unique_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_lf_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_lf_stats_t &s = *this;
    o
    << pfx << "hdr_bs "                << s.hdr_bs << endl
    << pfx << "key_bs "                << s.key_bs << endl
    << pfx << "data_bs "                << s.data_bs << endl
    << pfx << "entry_overhead_bs "        << s.entry_overhead_bs << endl
    << pfx << "unused_bs "                << s.unused_bs << endl
    << pfx << "entry_cnt "                << s.entry_cnt << endl
    << pfx << "unique_cnt "                << s.unique_cnt << endl
    ;
}


void
btree_int_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
        ((base_stat_t*)this)[i] = 0;
    }
}

void
btree_int_stats_t::add(const btree_int_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
        ((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
btree_int_stats_t::audit() const
{
    FUNC(btree_int_stats_t::audit);
    w_rc_t result;
    if (total_bytes() % smlevel_0::page_sz != 0) {
        DBG(
            << " btree_int_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"btree_int_stats_t audit ok");
    return result;
}

base_stat_t
btree_int_stats_t::total_bytes() const
{
    return used_bs + unused_bs;
}


ostream& operator<<(ostream& o, const btree_int_stats_t& s)
{
    /*
    return o
    << "used_bs "                << s.used_bs << endl
    << "unused_bs "                << s.unused_bs << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_int_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_int_stats_t &s = *this;
    o
    << pfx << "used_bs "                << s.used_bs << endl
    << pfx << "unused_bs "                << s.unused_bs << endl
    ;
}


void
btree_stats_t::clear()
{
    leaf_pg.clear();
    int_pg.clear();

    leaf_pg_cnt = 0;
    int_pg_cnt = 0;
    unlink_pg_cnt = 0;
    unalloc_pg_cnt = 0;
    level_cnt = 0;
}

void
btree_stats_t::add(const btree_stats_t& stats)
{
    leaf_pg.add(stats.leaf_pg);
    int_pg.add(stats.int_pg);

    leaf_pg_cnt += stats.leaf_pg_cnt;
    int_pg_cnt += stats.int_pg_cnt;
    unlink_pg_cnt += stats.unlink_pg_cnt;
    unalloc_pg_cnt += stats.unalloc_pg_cnt;
    level_cnt = MAX(level_cnt, stats.level_cnt);
}

w_rc_t
btree_stats_t::audit() const
{
    FUNC(btree_stats_t::audit);
    // BYTE COUNTS
    W_DO(leaf_pg.audit());
    W_DO(int_pg.audit());

    w_rc_t result;
    base_stat_t total_alloc_pgs = alloc_pg_cnt();

    // BYTE COUNTS
    if (total_alloc_pgs*smlevel_0::page_sz != total_bytes()) {
        DBG(
            << " leaf_pg_cnt= " << leaf_pg_cnt
            << " int_pg_cnt= " << int_pg_cnt
            << " btree_stats_t::total_bytes= " << total_bytes()
            << " smlevel_0::page_sz= " << int(smlevel_0::page_sz)
        );
        result = stats_audit_failed(__LINE__);
    }

    // PAGE COUNTS
    if ( (total_alloc_pgs + unlink_pg_cnt + unalloc_pg_cnt) %
         smlevel_0::ext_sz != 0) {
        DBG(
            << " leaf_pg_cnt= " << leaf_pg_cnt
            << " int_pg_cnt= " << int_pg_cnt
            << " unlink_pg_cnt= " << unlink_pg_cnt
            << " unalloc_pg_cnt= " << unalloc_pg_cnt
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
        );
        if(result.is_error()) {} // don't croak on error-not-checked
        result = stats_audit_failed(__LINE__);
    }

    if ( unlink_pg_cnt > 0) {
        // We shouldn't have unlinked pages if the right locks were
        // acquired on audit
        if(result.is_error()) {} // don't croak on error-not-checked
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"btree_stats_t audit ok");
    return result;
}

base_stat_t
btree_stats_t::total_bytes() const
{
    return leaf_pg.total_bytes() + int_pg.total_bytes();
}

base_stat_t
btree_stats_t::alloc_pg_cnt() const
{
    return leaf_pg_cnt + int_pg_cnt;
}


ostream& operator<<(ostream& o, const btree_stats_t& s)
{
    /*
    return o << s.leaf_pg << s.int_pg
    << "leaf_pg_cnt "                << s.leaf_pg_cnt << endl
    << "int_pg_cnt "                << s.int_pg_cnt << endl
    << "unlink_pg_cnt "                << s.unlink_pg_cnt << endl
    << "unalloc_pg_cnt "        << s.unalloc_pg_cnt << endl
    << "level_cnt "                << s.level_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
        memcpy(pfx1+pfxlen, "lfpg.", 6);
        s.leaf_pg.print(o,pfx1); 

        memcpy(pfx1+pfxlen, "inpg.", 6);
        s.int_pg.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx << "leaf_pg_cnt "                << s.leaf_pg_cnt << endl
    << pfx << "int_pg_cnt "                << s.int_pg_cnt << endl
    << pfx << "unlink_pg_cnt "                << s.unlink_pg_cnt << endl
    << pfx << "unalloc_pg_cnt "        << s.unalloc_pg_cnt << endl
    << pfx << "level_cnt "                << s.level_cnt << endl
    ;
}


void
rtree_stats_t::clear()
{
    entry_cnt = 0;
    unique_cnt = 0;
    leaf_pg_cnt = 0;
    int_pg_cnt = 0;
    unalloc_pg_cnt = 0;
    fill_percent = 0;
    level_cnt = 0;
}

void
rtree_stats_t::add(const rtree_stats_t& stats)
{
    entry_cnt += stats.entry_cnt;
    unique_cnt += stats.unique_cnt;
    leaf_pg_cnt += stats.leaf_pg_cnt;
    int_pg_cnt += stats.int_pg_cnt;
    unalloc_pg_cnt += stats.unalloc_pg_cnt;
    // this should be a weighted average ... oh well.
    fill_percent = MAX(fill_percent, stats.fill_percent);
    level_cnt = MAX(level_cnt, stats.level_cnt);
}

w_rc_t
rtree_stats_t::audit() const
{
    FUNC(rtree_stats_t::audit);
    w_rc_t result;
    if ( (leaf_pg_cnt + int_pg_cnt + unalloc_pg_cnt) % smlevel_0::ext_sz != 0) {
        DBG(
            << " leaf_pg_cnt= " << leaf_pg_cnt
            << " int_pg_cnt= " << int_pg_cnt
            << " unalloc_pg_cnt= " << unalloc_pg_cnt
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"rtree_stats_t audit ok");
    return result;
}

base_stat_t
rtree_stats_t::total_bytes() const
{
    return (leaf_pg_cnt + int_pg_cnt) * smlevel_0::page_sz;
}


ostream& operator<<(ostream& o, const rtree_stats_t& s)
{
/*
    return o
    << "entry_cnt "                << s.entry_cnt << endl
    << "unique_cnt "                << s.unique_cnt << endl
    << "leaf_pg_cnt "                << s.leaf_pg_cnt << endl
    << "int_pg_cnt "                << s.int_pg_cnt << endl
    << "unalloc_pg_cnt "        << s.unalloc_pg_cnt << endl
    << "fill_percent "                << s.fill_percent << endl
    << "level_cnt "                << s.level_cnt << endl
    ;
*/
    s.print(o, "");
    return o;
}

void rtree_stats_t::print(ostream& o, const char *pfx) const
{
    const rtree_stats_t &s = *this;
    o
    << pfx << "entry_cnt "                << s.entry_cnt << endl
    << pfx << "unique_cnt "                << s.unique_cnt << endl
    << pfx << "leaf_pg_cnt "                << s.leaf_pg_cnt << endl
    << pfx << "int_pg_cnt "                << s.int_pg_cnt << endl
    << pfx << "unalloc_pg_cnt "        << s.unalloc_pg_cnt << endl
    << pfx << "fill_percent "                << s.fill_percent << endl
    << pfx << "level_cnt "                << s.level_cnt << endl
    ;
}


void
volume_hdr_stats_t::clear()
{
    hdr_ext_cnt = 0;
    alloc_ext_cnt   = 0;
    unalloc_ext_cnt = 0;
    extent_size = 0;
}

void
volume_hdr_stats_t::add(const volume_hdr_stats_t& stats)
{
    hdr_ext_cnt += stats.hdr_ext_cnt;
    alloc_ext_cnt   += stats.alloc_ext_cnt;
    unalloc_ext_cnt += stats.unalloc_ext_cnt;
    extent_size = MAX(extent_size, stats.extent_size);
}

w_rc_t
volume_hdr_stats_t::audit() const
{
    FUNC(volume_hdr_stats_t::audit);
    w_rc_t result;
    if (extent_size != smlevel_0::ext_sz) {
        DBG(
            << " extent_size= " << extent_size
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"volume_hdr_stats_t audit ok");
    return result;
}

base_stat_t
volume_hdr_stats_t::total_bytes() const
{
    return hdr_ext_cnt * smlevel_0::ext_sz * smlevel_0::page_sz;
}

ostream& operator<<(ostream& o, const volume_hdr_stats_t& s)
{
    /*
    return o
    << "hdr_ext_cnt "                << s.hdr_ext_cnt << endl
    << "alloc_ext_cnt "                << s.alloc_ext_cnt << endl
    << "unalloc_ext_cnt "        << s.unalloc_ext_cnt << endl
    << "extent_size "                << s.extent_size << endl
    ;
    */
    s.print(o, "");
    return o;
}

void volume_hdr_stats_t::print(ostream& o, const char *pfx) const
{
    const volume_hdr_stats_t &s = *this;
    o
    << pfx << "hdr_ext_cnt "                << s.hdr_ext_cnt << endl
    << pfx << "alloc_ext_cnt "                << s.alloc_ext_cnt << endl
    << pfx << "unalloc_ext_cnt "        << s.unalloc_ext_cnt << endl
    << pfx << "extent_size "                << s.extent_size << endl
    ;
}


void
volume_map_stats_t::clear()
{
    store_directory.clear();
    root_index.clear();
}

void
volume_map_stats_t::add(const volume_map_stats_t& stats)
{
    store_directory.add(stats.store_directory);
    root_index.add(stats.root_index);
}

w_rc_t
volume_map_stats_t::audit() const
{
    FUNC(volume_map_stats_t::audit);
    W_DO(store_directory.audit());
    W_DO(root_index.audit());

    w_rc_t result;
    if ( (alloc_pg_cnt() + unalloc_pg_cnt()) %
         smlevel_0::ext_sz != 0) {
        DBG(
            << " alloc_pg_cnt= " << alloc_pg_cnt()
            << " unalloc_pg_cnt= " << unalloc_pg_cnt()
            << " smlevel_0::ext_sz= " << int(smlevel_0::ext_sz)
        );
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"volume_hdr_stats_t audit ok");
    return result;
}

base_stat_t
volume_map_stats_t::total_bytes() const
{
    return store_directory.total_bytes() +
           root_index.total_bytes();
}

base_stat_t
volume_map_stats_t::alloc_pg_cnt() const
{
    return store_directory.alloc_pg_cnt() +
           root_index.alloc_pg_cnt();
}

base_stat_t
volume_map_stats_t::unalloc_pg_cnt() const
{
    return store_directory.unalloc_pg_cnt +
           root_index.unalloc_pg_cnt +
           store_directory.unlink_pg_cnt +
           root_index.unlink_pg_cnt;
}

base_stat_t
volume_map_stats_t::unlink_pg_cnt() const
{
    return store_directory.unlink_pg_cnt +
           root_index.unlink_pg_cnt;
}


ostream& operator<<(ostream& o, const volume_map_stats_t& s)
{
    /*
    return o << s.store_directory << s.root_index ;
    */
    s.print(o, "");
    return o;
}

void volume_map_stats_t::print(ostream& o, const char *pfx) const
{
    const volume_map_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);

    memcpy(pfx1+pfxlen, "sdir.", 6);
    s.store_directory.print(o,pfx1); 

    memcpy(pfx1+pfxlen, "rind.", 6);
    s.root_index.print(o,pfx1); 

    delete[] pfx1;
}

void
sm_du_stats_t::clear()
{
    file.clear();
    btree.clear();
    rtree.clear();
    volume_hdr.clear();
    volume_map.clear();

    file_cnt = 0;
    btree_cnt = 0;
    rtree_cnt = 0;
}

void
sm_du_stats_t::add(const sm_du_stats_t& stats)
{
    file.add(stats.file);
    btree.add(stats.btree);
    rtree.add(stats.rtree);
    volume_hdr.add(stats.volume_hdr);
    volume_map.add(stats.volume_map);

    file_cnt += stats.file_cnt;
    btree_cnt += stats.btree_cnt;
    rtree_cnt += stats.rtree_cnt;
}

w_rc_t
sm_du_stats_t::audit() const
{
    FUNC(sm_du_stats_t::audit);
    W_DO(file.audit());
    W_DO(btree.audit());
    W_DO(rtree.audit());

    W_DO(volume_hdr.audit());
    W_DO(volume_map.audit());

    base_stat_t unalloc_pg_cnt =
        (file.unalloc_file_pg_cnt + // reserved in exts for small obj store
         file.unalloc_large_pg_cnt + // reserved in exts for lg obj store
         // counts for user-defined btrees
         btree.unalloc_pg_cnt +  // reserved in exts for btree store
                                 // via extent traversal
         btree.unlink_pg_cnt +   // allocated but unaccounted-for in btree
                                 // traversal (about to be freed)
         rtree.unalloc_pg_cnt + 

         // sum of the above btree counts for root index and store directory
         volume_map.unalloc_pg_cnt() 
         );


    base_stat_t alloc_pg_cnt2 =
             // counts for user indexes:
            (btree.leaf_pg_cnt +  // referenced through the structure
             btree.int_pg_cnt + // referenced through the structure
             rtree.leaf_pg_cnt + 
             rtree.int_pg_cnt +

             volume_map.alloc_pg_cnt() + // above-two counts for
                            // store directory and root directory
                            //
            file.file_pg_cnt +  // small object file pages 
            file.lgdata_pg_cnt + // data pages needed to hold the records
                                // as computed by record byte count
            file.lgindex_pg_cnt // index pages encountered for t_large_1,2
            );

    // separate out unlink_pg_cnts - they are included in the unalloc_pg_cnt
    base_stat_t unlink_pg_cnt3 = 
             btree.unlink_pg_cnt + // unlinked in user btrees
             volume_map.unlink_pg_cnt() // unlinked in root dir and store dir
             ;

    base_stat_t alloc_and_unalloc_cnt = alloc_pg_cnt2 + unalloc_pg_cnt;
    
    xct_t* x = me()->xct();
    base_stat_t marked_for_deletion_by_me;
    x->num_extents_marked_for_deletion(marked_for_deletion_by_me);
        
    w_rc_t result;
    if (alloc_and_unalloc_cnt != 
        (volume_hdr.alloc_ext_cnt - marked_for_deletion_by_me) 
                * smlevel_0::ext_sz ) {
        DBG(
            << " alloc2 total pages = " << alloc_pg_cnt2
            << " unalloc total pages = " << unalloc_pg_cnt
            << " marked_for_deletion_by_me = " << marked_for_deletion_by_me
            << " ext total pages = " << int(volume_hdr.alloc_ext_cnt * smlevel_0::ext_sz)
            << " unlinked pages = " << unlink_pg_cnt3
        );
#if DEBUG_GNATS_77
        const char *relation =  " > ";
        if (alloc_and_unalloc_cnt <
            (volume_hdr.alloc_ext_cnt - marked_for_deletion_by_me) 
                * smlevel_0::ext_sz )  
        {
            // some extents weren't marked for deletion but should have been
            // or
            // those referenced in the store structures don't add up to
            // all those in the volume
            relation = " < ";
        }   // else, those referenced are more than the volume thinks there
        // are, so too many are marked for deletion and the storage structures
        // haven't been adjusted to catch up.  
        
        cerr << "Pages referenced in store structures " 
                << alloc_and_unalloc_cnt 
                << relation
                << " volume totals ("
                << unlink_pg_cnt3
                << " unlinked pages, included in referenced-in-structures)"
                << endl;
        cerr 
            << " alloc_and_unalloc_cnt "
            << alloc_and_unalloc_cnt  
            << " ("<<alloc_pg_cnt2<<"+" << unalloc_pg_cnt << ")" 
            << " != volume allocated extents - marked for deletion "
            << ((volume_hdr.alloc_ext_cnt - marked_for_deletion_by_me)*
                    smlevel_0::ext_sz ) 
            << "((" << volume_hdr.alloc_ext_cnt << "-"
                    << marked_for_deletion_by_me << ")* extsize)"
            << endl;
        cerr
            << " alloc2 total pages = " << alloc_pg_cnt2
            << " unalloc total pages = " << unalloc_pg_cnt
            << " marked_for_deletion_by_me = " << marked_for_deletion_by_me
            << " ext total pages = " << int(volume_hdr.alloc_ext_cnt * smlevel_0::ext_sz)
            << endl;

        cerr
        << "alloc2 broken down: "
            << " btree.leaf_pg_cnt  "
            << btree.leaf_pg_cnt 
            << " btree.int_pg_cnt  "
            << btree.int_pg_cnt 
            << " file.file_pg_cnt  "
            << file.file_pg_cnt 
            << " file.lgdata_pg_cnt  "
            << file.lgdata_pg_cnt 
            << " file.lgindex_pg_cnt  "
            << file.lgindex_pg_cnt 
            << " volume_map.alloc_pg_cnt() "
            << volume_map.alloc_pg_cnt()
            << endl;

        cerr
        << "unalloc total broken down: "

        << " file.unalloc_file_pg_cnt " <<
        file.unalloc_file_pg_cnt 
        << " file.unalloc_large_pg_cnt  " <<
        file.unalloc_large_pg_cnt 
         << " btree.unalloc_pg_cnt  " <<
         btree.unalloc_pg_cnt 
         << " btree.unlink_pg_cnt  " <<
         btree.unlink_pg_cnt 
         << " volume_map.unalloc_pg_cnt()  " <<
         volume_map.unalloc_pg_cnt() 
            << endl;

        cerr
            << "volume_map.root_index " << volume_map.root_index
            << endl
            << "volume_map.store_directory " << volume_map.store_directory
            << endl;

#endif
        result = stats_audit_failed(__LINE__);
    }


    base_stat_t alloc_pg_cnt = 
        ( volume_hdr.alloc_ext_cnt + volume_hdr.hdr_ext_cnt ) 
        * smlevel_0::ext_sz; 
    alloc_pg_cnt -= unalloc_pg_cnt;
    alloc_pg_cnt -= (marked_for_deletion_by_me * smlevel_0::ext_sz);

        
    if ( alloc_pg_cnt * smlevel_0::page_sz != total_bytes()) {
        DBG(
            << " alloc_pg_cnt= " << alloc_pg_cnt
            << " unalloc_pg_cnt= " << unalloc_pg_cnt
            << " marked_for_deletion_by_me=" << marked_for_deletion_by_me
            << " page_sz = " << int(smlevel_0::page_sz) 
            << " total bytes= " << total_bytes()
        );
        if(result.is_error()) {} // don't croak on error-not-checked
        result = stats_audit_failed(__LINE__);
    }
    DBG(<<"sm_du_stats_t audit ok");
    return result;
}

base_stat_t
sm_du_stats_t::total_bytes() const
{
    return file.total_bytes() + btree.total_bytes() +
           rtree.total_bytes() + 
           volume_hdr.total_bytes() + volume_map.total_bytes()
           ;
}

ostream& operator<<(ostream& o, const sm_du_stats_t& s)
{
    /*
    return o << s.file << s.btree << s.rtree 
             << s.volume_hdr << s.volume_map << s.small_store << endl
    << "file_cnt "        << s.file_cnt << endl
    << "btree_cnt "        << s.btree_cnt << endl
    << "rtree_cnt "        << s.rtree_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void sm_du_stats_t::print(ostream& o, const char *pfx) const
{
    const sm_du_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);

    memcpy(pfx1+pfxlen, "file.", 6);
    s.file.print(o,pfx1); 

    memcpy(pfx1+pfxlen, "btre.", 6);
    s.btree.print(o,pfx1); 

    memcpy(pfx1+pfxlen, "rtre.", 6);
    s.rtree.print(o,pfx1); 

    memcpy(pfx1+pfxlen, "volh.", 6);
    s.volume_hdr.print(o,pfx1); 

    memcpy(pfx1+pfxlen, "volm.", 6);
    s.volume_map.print(o,pfx1); 

    delete[] pfx1;

    o
    << pfx << "file_cnt "        << s.file_cnt << endl
    << pfx << "btree_cnt "        << s.btree_cnt << endl
    << pfx << "rtree_cnt "        << s.rtree_cnt << endl
    ;
}


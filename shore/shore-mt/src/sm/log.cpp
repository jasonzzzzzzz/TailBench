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

/*<std-header orig-src='shore'>

 $Id: log.cpp,v 1.127.2.19 2010/03/19 22:20:24 nhall Exp $

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

#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int_1.h"
#include "logdef_gen.cpp"
#include "log.h"
#include "log_core.h"
#include "crash.h"
#include <algorithm> // for std::swap
#include <stdio.h> // snprintf

typedef smlevel_0::fileoff_t fileoff_t;

/*********************************************************************
 *
 *  log_i::next(lsn, r)
 *
 *  Read the next record into r and return its lsn in lsn.
 *  Return false if EOF reached. true otherwise.
 *
 *********************************************************************/
bool log_i::next(lsn_t& lsn, logrec_t*& r)  
{
    bool eof = (cursor == null_lsn);
    if (! eof) {
        lsn = cursor;
        rc_t rc = log.fetch(lsn, r, &cursor);
        
        // release right away, since this is only
        // used in recovery.
        log.release();

        if (rc.is_error())  {
            last_rc = rc;
            RC_AUGMENT(last_rc);
            RC_APPEND_MSG(last_rc, << "trying to fetch lsn " << cursor);
            
            if (last_rc.err_num() == smlevel_0::eEOF)  
                eof = true;
            else  {
                smlevel_0::errlog->clog << fatal_prio 
                << "Fatal error : " << RC_PUSH(last_rc, smlevel_0::eINTERNAL) << flushl;
            }
        }
    }
    return ! eof;
}

/*********************************************************************
 *
 * log_m::_version_major
 * log_m::_version_minor
 *
 * increment version_minor if a new log record is appended to the
 * list of log records and the semantics, ids and formats of existing
 * log records remains unchanged.
 *
 * for all other changes to log records or their semantics
 * _version_major should be incremented and version_minor set to 0.
 *
 *********************************************************************/
uint4_t const log_m::_version_major = 4;
uint4_t const log_m::_version_minor = 0;
const char log_m::_SLASH = '/';
const char log_m::_master_prefix[] = "chk."; // same size as _log_prefix
const char log_m::_log_prefix[] = "log.";
char       log_m::_logdir[max_devname];

// virtual
void  log_m::shutdown() 
{
    log_core::THE_LOG->shutdown();
    delete log_core::THE_LOG; // side effect: sets THE_LOG to NULL
}


NORET  log_m::~log_m() 
{
    // This cannot call log_core destructor or we'll get into
    // an inf loop, thus we have to do this with virtual methods.
}

// Used by sm options-handling:
// static
fileoff_t  log_m::segment_size() 
   { return log_core::THE_LOG->segment_size(); }
// static
fileoff_t log_m::partition_size(long psize)
   { return log_core::partition_size(psize); }
// static
fileoff_t log_m::min_partition_size()
   { return log_core::min_partition_size(); }

fileoff_t log_m::max_partition_size() {
    fileoff_t tmp = sthread_t::max_os_file_size;
    tmp = tmp > lsn_t::max.lo() ? lsn_t::max.lo() : tmp;
    return  partition_size(tmp);
}

// like page_p::tag_name, but doesn't W_FATAL on garbage
static char const* page_tag_to_str(int page_tag) {
    switch (page_tag) {
    case page_p::t_extlink_p: 
        return "t_extlink_p";
    case page_p::t_stnode_p:
        return "t_stnode_p";
    case page_p::t_keyed_p:
        return "t_keyed_p";
    case page_p::t_btree_p:
        return "t_btree_p";
    case page_p::t_rtree_p:
        return "t_rtree_p";
    case page_p::t_file_p:
        return "t_file_p";
    case page_p::t_ranges_p:
        return "t_ranges_p";
    case page_p::t_file_mrbt_p:
        return "t_file_mrbt_p";
    default:
        return "<garbage>";
    }
}

// make dbx print log records at all, and in a readable way
char const* 
db_pretty_print(logrec_t const* rec, int /*i=0*/, char const* /*s=0*/) 
{
    static char tmp_data[1024];
    
    // what categories?
    typedef char const* str;
    str undo=(rec->is_undo()? "undo " : ""),
        redo=(rec->is_redo()? "redo " : ""),
        cpsn=(rec->is_cpsn()? "cpsn " : ""),
        logical=(rec->is_logical()? "logical " : "");

    char scratch[100];
    str extra = scratch;
    switch(rec->type()) {

    case logrec_t::t_page_mark:
    case logrec_t::t_page_reclaim:
        snprintf(scratch, sizeof(scratch), "    { idx=%d, len=%d }\n",
                 *(short*) rec->data(), *(short*) (rec->data()+sizeof(short)));
        break;
    default:
        extra = "";
        break;
    }
    
    snprintf(tmp_data, sizeof(tmp_data),
             "{\n"
             "    _len = %d\n"
             "    _type = %s\n"
             "    _cat = %s%s%s%s\n"
             "    _tid = %d.%d\n"
             "    _pid = %d.%d.%d\n"
             "    _page_tag = %s\n"
             "    _prev = %d.%lld\n"
             "    _ck_lsn = %d.%lld\n"
             "%s"
             "}",
             rec->length(),
             rec->type_str(),
             logical, undo, redo, cpsn,
             rec->tid().get_hi(), rec->tid().get_lo(),
             rec->construct_pid().vol().vol, 
                     rec->construct_pid().store(), 
                     rec->construct_pid().page,
             page_tag_to_str(rec->tag()),
             (rec->prev().hi()), (long long int)(rec->prev().lo()),
             (rec->get_lsn_ck().hi()), (long long int)(rec->get_lsn_ck().lo()),
             extra
             );
    return tmp_data;
}

log_m::log_m()
    : 
      _min_chkpt_rec_lsn(first_lsn(1)), 
      _space_available(0),
      _space_rsvd_for_chkpt(0), 
      _partition_size(0), 
      _partition_data_size(0), 
      _log_corruption(false),
      _waiting_for_space(false)
{
    pthread_mutex_init(&_space_lock, 0);
    pthread_cond_init(&_space_cond, 0);
}


/*********************************************************************
 * 
 *  log_core::set_master(master_lsn, min_rec_lsn, min_xct_lsn)
 *
 *********************************************************************/
void log_m::set_master(lsn_t mlsn, lsn_t min_rec_lsn, lsn_t min_xct_lsn) 
{
    CRITICAL_SECTION(cs, _partition_lock);
    lsn_t min_lsn = std::min(min_rec_lsn, min_xct_lsn);

    // This used to descend to raw_log or unix_log:
    w_assert1(log_core::THE_LOG != NULL);
    _write_master(mlsn, min_lsn);

    _master_lsn = mlsn;
    _min_chkpt_rec_lsn = min_lsn;
}

rc_t  
log_m::new_log_m(log_m   *&the_log,
                         const char *path,
                         int wrbufsize,
                         bool  reformat)
{
    FUNC(log_m::new_log_m);

    w_assert1(strlen(path) < sizeof(_logdir));
    strcpy(_logdir, path);

    rc_t rc = log_core::new_log_m(the_log, wrbufsize, reformat);

    w_assert1(the_log != NULL);

    if(!rc.is_error())  {
        log_core::THE_LOG->start_flush_daemon();
    }
    return rc.reset();
}

/*********************************************************************
 *
 *  log_core::make_log_name(idx, buf, bufsz)
 *
 *  Make up the name of a log file in buf.
 *
 *********************************************************************/
const char *
log_m::make_log_name(uint4_t idx, char* buf, int bufsz)
{
    // this is a static function w_assert2(_partition_lock.is_mine()==true);
    w_ostrstream s(buf, (int) bufsz);
    s << _logdir << _SLASH
      << _log_prefix << idx << ends;
    w_assert1(s);
    return buf;
}

rc_t                
log_m::scavenge(lsn_t min_rec_lsn, lsn_t min_xct_lsn) 
    { return log_core::THE_LOG->scavenge(min_rec_lsn, min_xct_lsn); }

void                
log_m:: release()  
    { log_core::THE_LOG->release(); }

rc_t 
log_m::fetch(lsn_t &lsn, logrec_t* &rec, lsn_t* nxt) 
    { return log_core::THE_LOG->fetch(lsn, rec, nxt); }

rc_t 
log_m::insert(logrec_t &r, lsn_t* ret)
{ 
    // If log corruption is turned on,  zero out
    // important parts of the log to fake crash (by making the
    // log appear to end here).
    if (_log_corruption) {
        smlevel_0::errlog->clog << error_prio 
        << "Generating corrupt log record at lsn: " << curr_lsn() << flushl;
        r.corrupt();
        // Now turn it off.
        _log_corruption = false;
    }
    return log_core::THE_LOG->insert(r, ret); 
}

rc_t 
log_m::flush(lsn_t lsn, bool block)
    { return log_core::THE_LOG->flush(lsn, block); }

rc_t 
log_m::compensate(lsn_t orig_lsn, lsn_t undo_lsn) 
    { return log_core::THE_LOG->compensate(orig_lsn, undo_lsn); }

/*********************************************************************
 * 
 *  log_m::_make_master_name(master_lsn, min_chkpt_rec_lsn, buf, bufsz)
 *
 *  Make up the name of a master record in buf.
 *
 *********************************************************************/
void
log_m::_make_master_name(
    const lsn_t&         master_lsn, 
    const lsn_t&        min_chkpt_rec_lsn,
    char*                 buf,
    int                        bufsz,
    bool                old_style)
{
    w_ostrstream s(buf, (int) bufsz);

    s << _logdir << _SLASH << _master_prefix;
    lsn_t         array[2];
    array[0] = master_lsn;
    array[1] = min_chkpt_rec_lsn;

    _create_master_chkpt_string(s, 2, array, old_style);
    s << ends;
    w_assert1(s);
}

void
log_m::_write_master(lsn_t l, lsn_t min) 
{
    /*
     *  create new master record
     */
    char _chkpt_meta_buf[CHKPT_META_BUF];
    _make_master_name(l, min, _chkpt_meta_buf, CHKPT_META_BUF);
    DBGTHRD(<< "writing checkpoint master: " << _chkpt_meta_buf);

    FILE* f = fopen(_chkpt_meta_buf, "a");
    if (! f) {
        w_rc_t e = RC(eOS);    
        smlevel_0::errlog->clog << fatal_prio 
            << "ERROR: could not open a new log checkpoint file: "
            << _chkpt_meta_buf << flushl;
        W_COERCE(e);
    }

    {        /* write ending lsns into the master chkpt record */
        lsn_t         array[PARTITION_COUNT];
#if 0
        int j=0;
        for(int i=0; i < PARTITION_COUNT; i++) {
            const partition_t *p = this->_partition(i);
            if(p->num() > 0 && (p->last_skip_lsn().hi() == p->num())) {
                array[j++] = p->last_skip_lsn();
            }
        }
#else
        int j = log_core::THE_LOG->get_last_lsns(array);
#endif
        if(j > 0) {
            w_ostrstream s(_chkpt_meta_buf, CHKPT_META_BUF);
            _create_master_chkpt_contents(s, j, array);
        } else {
            memset(_chkpt_meta_buf, '\0', 1);
        }
        int length = strlen(_chkpt_meta_buf) + 1;
        DBG(<< " #lsns=" << j
            << " write this to master checkpoint record: " <<
                _chkpt_meta_buf);

        if(fwrite(_chkpt_meta_buf, length, 1, f) != 1) {
            w_rc_t e = RC(eOS);    
            smlevel_0::errlog->clog << fatal_prio 
                << "ERROR: could not write log checkpoint file contents"
                << _chkpt_meta_buf << flushl;
            W_COERCE(e);
        }
    }
    fclose(f);

    /*
     *  destroy old master record
     */
    _make_master_name(_master_lsn, 
                _min_chkpt_rec_lsn, _chkpt_meta_buf, CHKPT_META_BUF);
    (void) unlink(_chkpt_meta_buf);
}

/*********************************************************************
 *
 * log_m::_create_master_chkpt_string
 *
 * writes a string which parse_master_chkpt_string expects.
 * includes the version, and master and min chkpt rec lsns.
 *
 *********************************************************************/

void
log_m::_create_master_chkpt_string(
                ostream&        s,
                int                arraysize,
                const lsn_t*        array,
                bool                old_style)
{
    w_assert1(arraysize >= 2);
    if (old_style)  {
        s << array[0] << '.' << array[1];

    }  else  {
        s << 'v' << _version_major << '.' << _version_minor ;
        for(int i=0; i< arraysize; i++) {
                s << '_' << array[i];
        }
    }
}

/*********************************************************************
 *
 * log_m::_check_version
 *
 * returns LOGVERSIONTOONEW or LOGVERSIONTOOOLD if the passed in
 * version is incompatible with this sources version
 *
 *********************************************************************/

rc_t
log_m::_check_version(uint4_t major, uint4_t minor)
{
        if (major == _version_major && minor <= _version_minor)
                return RCOK;

        int err = (major < _version_major)
                        ? eLOGVERSIONTOOOLD : eLOGVERSIONTOONEW;

        smlevel_0::errlog->clog << fatal_prio 
            << "ERROR: log version too "
            << ((err == eLOGVERSIONTOOOLD) ? "old" : "new")
            << " sm ("
            << _version_major << " . " << _version_minor
            << ") log ("
            << major << " . " << minor
            << flushl;

        return RC(err);
}


void
log_m::_create_master_chkpt_contents(
                ostream&        s,
                int                arraysize,
                const lsn_t*        array
                )
{
    for(int i=0; i< arraysize; i++) {
            s << '_' << array[i];
    }
    s << ends;
}


rc_t
log_m::_parse_master_chkpt_contents(
                istream&            s,
                int&                    listlength,
                lsn_t*                    lsnlist
                )
{
    listlength = 0;
    char separator;
    while(!s.eof()) {
        s >> separator;
        if(!s.eof()) {
            w_assert9(separator == '_' || separator == '.');
            s >> lsnlist[listlength];
            DBG(<< listlength << ": extra lsn = " << 
                lsnlist[listlength]);
            if(!s.fail()) {
                listlength++;
            }
        }
    }
    return RCOK;
}


/*********************************************************************
 *
 * log_m::_parse_master_chkpt_string
 *
 * parse and return the master_lsn and min_chkpt_rec_lsn.
 * return an error if the version is out of sync.
 *
 *********************************************************************/
rc_t
log_m::_parse_master_chkpt_string(
                istream&            s,
                lsn_t&              master_lsn,
                lsn_t&              min_chkpt_rec_lsn,
                int&                    number_of_others,
                lsn_t*                    others,
                bool&                    old_style)
{
    uint4_t major = 1;
    uint4_t minor = 0;
    char separator;

    s >> separator;

    if (separator == 'v')  {                // has version, otherwise default to 1.0
        old_style = false;
        s >> major >> separator >> minor;
        w_assert9(separator == '.');
        s >> separator;
        w_assert9(separator == '_');
    }  else  {
        old_style = true;
        s.putback(separator);
    }

    s >> master_lsn >> separator >> min_chkpt_rec_lsn;
    w_assert9(separator == '_' || separator == '.');

    if (!s)  {
        return RC(eBADMASTERCHKPTFORMAT);
    }

    number_of_others = 0;
    while(!s.eof()) {
        s >> separator;
        if(separator == '\0') break; // end of string

        if(!s.eof()) {
            w_assert9(separator == '_' || separator == '.');
            s >> others[number_of_others];
            DBG(<< number_of_others << ": extra lsn = " << 
                others[number_of_others]);
            if(!s.fail()) {
                number_of_others++;
            }
        }
    }

    return _check_version(major, minor);
}


/* Compute size of the biggest checkpoint we ever risk having to take...
 */
long log_m::max_chkpt_size() const 
{
    /* BUG: the number of transactions which might need to be
       checkpointed is potentially unbounded. However, it's rather
       unlikely we'll ever see more than 10k at any one time...
     */
    static long const GUESS_MAX_XCT_COUNT = 10000;
    static long const FUDGE = sizeof(logrec_t);
    long bf_tab_size = bf->npages()*sizeof(chkpt_bf_tab_t::brec_t);
    long xct_tab_size = GUESS_MAX_XCT_COUNT*sizeof(chkpt_xct_tab_t::xrec_t);
    return FUDGE + bf_tab_size + xct_tab_size;
}

w_rc_t
log_m::_read_master( 
        const char *fname,
        int prefix_len,
        lsn_t &tmp,
        lsn_t& tmp1,
        lsn_t* lsnlist,
        int&   listlength,
        bool&  old_style
)
{
    rc_t         rc;
    {
        /* make a copy */
        int        len = strlen(fname+prefix_len) + 1;
        char *buf = new char[len];
        memcpy(buf, fname+prefix_len, len);
        w_istrstream s(buf);

        rc = _parse_master_chkpt_string(s, tmp, tmp1, 
                                       listlength, lsnlist, old_style);
        delete [] buf;
        if (rc.is_error()) {
            smlevel_0::errlog->clog << fatal_prio 
            << "bad master log file \"" << fname << "\"" << flushl;
            W_COERCE(rc);
        }
        DBG(<<"_parse_master_chkpt_string returns tmp= " << tmp
            << " tmp1=" << tmp1
            << " old_style=" << old_style);
    }

    /*  
     * read the file for the rest of the lsn list
     */
    {
        char*         buf = new char[smlevel_0::max_devname];
        if (!buf)
            W_FATAL(fcOUTOFMEMORY);
        w_auto_delete_array_t<char> ad_fname(buf);
        w_ostrstream s(buf, int(smlevel_0::max_devname));
        s << _logdir << _SLASH << fname << ends;

        FILE* f = fopen(buf, "r");
        if(f) {
            char _chkpt_meta_buf[CHKPT_META_BUF];
            int n = fread(_chkpt_meta_buf, 1, CHKPT_META_BUF, f);
            if(n  > 0) {
                /* Be paranoid about checking for the null, since a lack
                   of it could send the istrstream driving through memory
                   trying to parse the information. */
                void *null = memchr(_chkpt_meta_buf, '\0', CHKPT_META_BUF);
                if (!null) {
                    smlevel_0::errlog->clog << fatal_prio 
                        << "invalid master log file format \"" 
                        << buf << "\"" << flushl;
                    W_FATAL(eINTERNAL);
                }
                    
                w_istrstream s(_chkpt_meta_buf);
                rc = _parse_master_chkpt_contents(s, listlength, lsnlist);
                if (rc.is_error())  {
                    smlevel_0::errlog->clog << fatal_prio 
                        << "bad master log file contents \"" 
                        << buf << "\"" << flushl;
                    W_COERCE(rc);
                }
            }
            fclose(f);
        } else {
            /* backward compatibility with minor version 0: 
             * treat empty file ok
             */
            w_rc_t e = RC(eOS);
            smlevel_0::errlog->clog << fatal_prio
                << "ERROR: could not open existing log checkpoint file: "
                << buf << flushl;
            W_COERCE(e);
        }
    }
    return RCOK;
}

fileoff_t log_m::take_space(fileoff_t volatile* ptr, int amt) 
{
    fileoff_t ov = *ptr;
    while(1) {
        if(ov < amt)
            return 0;
        fileoff_t nv = ov - amt;
        fileoff_t cv = atomic_cas_64((uint64_t*)ptr, ov, nv);
        if(ov == cv)
            return amt;
        ov = cv;
    }
}

fileoff_t log_m::reserve_space(fileoff_t amt) {

#if USE_LOG_RESERVATIONS
    return (amt > 0)? take_space(&_space_available, amt) : 0;
#else
    return (_space_available > amt) ? _space_available - amt : 0; 
#endif
}

fileoff_t log_m::consume_chkpt_reservation(fileoff_t amt) {
#if USE_LOG_RESERVATIONS
    if(operating_mode != t_forward_processing)
       return amt; // not yet active -- pretend it worked

    return (amt > 0)? 
        take_space(&_space_rsvd_for_chkpt, amt) : 0;
#else
    return amt; // anything > 0 means ok
#endif
}

// make sure we have enough log reservation (conservative)
bool log_m::verify_chkpt_reservation() 
{
#if USE_LOG_RESERVATIONS
    fileoff_t space_needed = max_chkpt_size();
    while(*&_space_rsvd_for_chkpt < 2*space_needed) {
        if(reserve_space(space_needed)) {
            // abuse take_space...
            take_space(&_space_rsvd_for_chkpt, -space_needed);
        } else if(*&_space_rsvd_for_chkpt < space_needed) {
            /* oops...

               can't even guarantee the minimum of one checkpoint
               needed to reclaim log space and solve the problem
             */
            W_FATAL(eOUTOFLOGSPACE);
        } else {
            // must reclaim a log partition
            return false;
        }
    }
#endif
    return true;
}

void log_m::release_space(fileoff_t amt) 
{
    log_core::THE_LOG->release_space(amt);
}

#if USE_LOG_RESERVATIONS
rc_t log_m::wait_for_space(fileoff_t &amt, timeout_in_ms timeout) 
{
    return log_core::THE_LOG->wait_for_space(amt, timeout);
}
#else
rc_t log_m::wait_for_space(fileoff_t &, timeout_in_ms ) 
{
    return RCOK;
}
#endif

void            
log_m::activate_reservations()
{
    log_core::THE_LOG->activate_reservations();
}

// Determine if this lsn is holding up scavenging of logs by (being 
// on a presumably hot page, and) being a rec_lsn that's in the oldest open
// log partition and that oldest partition being sufficiently aged....
bool                
log_m::squeezed_by(const lsn_t &self)  const 
{
    // many partitions are open
    return 
    ((curr_lsn().file() - global_min_lsn().file()) >=  (PARTITION_COUNT-2))
        &&
    (self.file() == global_min_lsn().file())  // the given lsn 
                                              // is in the oldest file
    ;
}

rc_t                
log_m::file_was_archived(const char *file)
{
    // TODO: should check that this is the oldest, 
    // and that we indeed asked for it to be archived.
    return log_core::THE_LOG->file_was_archived(file);
}

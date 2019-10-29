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

 $Id: partition.cpp,v 1.3 2010/06/08 22:28:55 nhall Exp $

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

#define debug_log false

#define SM_SOURCE
#define PARTITION_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int_1.h"
#include "logtype_gen.h"
#include "log.h"
#include "log_core.h"
// DEAD #include "log_buf.h"

// needed for skip_log
#include "logdef_gen.cpp"

// Initialize on first access:
// block to be cleared upon first use.
class block_of_zeroes {
private:
    char _block[log_core::BLOCK_SIZE];
public:
    NORET block_of_zeroes() {
        memset(&_block[0], 0, log_core::BLOCK_SIZE);
    }
    char *block() { return _block; }
};

char *block_of_zeros() {

    static block_of_zeroes z;
    return z.block();
}

char *             
partition_t::_readbuf() { return _owner->readbuf(); }
#if W_DEBUG_LEVEL > 2
void              
partition_t::check_fhdl_rd() const {
    bool isopen = is_open_for_read();
    if(_fhdl_rd == invalid_fhdl) {
        w_assert3( !isopen );
    } else {
        w_assert3(isopen);
    }
}
void 
partition_t::check_fhdl_app() const {
    if(_fhdl_app != invalid_fhdl) {
        w_assert3(is_open_for_append());
    } else {
        w_assert3(! is_open_for_append());
    }
}
#endif

bool
partition_t::is_current()  const
{
    //  rd could be open
    if(index() == _owner->partition_index()) {
        w_assert3(num()>0);
        w_assert3(_owner->partition_num() == num());
        w_assert3(exists());
        w_assert3(_owner->curr_partition() == this);
        w_assert3(_owner->partition_index() == index());
        w_assert3(this->is_open_for_append());

        return true;
    }
#if W_DEBUG_LEVEL > 2
    if(num() == 0) {
        w_assert3(!this->exists());
    }
#endif 
    return false;
}


/*
 * open_for_append(num, end_hint)
 * "open" a file  for the given num for append, and
 * make it the current file.
 */
// MUTEX: flush, insert, partition
void
partition_t::open_for_append(partition_number_t __num, 
        const lsn_t& end_hint) 
{
    FUNC(partition::open_for_append);

    // shouldn't be calling this if we're already open
    w_assert3(!is_open_for_append());
    // We'd like to use this assertion, but in the
    // raw case, it's wrong: fhdl_app() is NOT synonymous
    // with is_open_for_append() and the same goes for ...rd()
    // w_assert3(fhdl_app() == 0);

    int         fd;

    DBG(<<"open_for_append num()=" << num()
            << "__num=" << __num
            << "_num=" << _num
            << " about to peek");

    /*
    if(num() == __num) {
        close_for_read();
        close_for_append();
        _num = 0; // so the peeks below
        // will work -- it'll get reset
        // again anyway.
   }
   */
    /* might not yet know its size - discover it now  */
    peek(__num, end_hint, true, &fd); // have to know its size
    w_assert3(fd);
    if(size() == nosize) {
        // we're opening a new partition
        set_size(0);
    }
        
    _num = __num;
    // size() was set in peek()
    w_assert1(size() != partition_t::nosize);

    _set_fhdl_app(fd);
    _set_state(m_flushed);
    _set_state(m_exists);
    _set_state(m_open_for_append);

    _owner->set_current( index(), num() );
    return ;
}

void
partition_t::clear()
{
    _num=0; 
    _size = nosize; 
    _mask=0; 
    _clr_state(m_open_for_read);
    _clr_state(m_open_for_append);
    DBGTHRD(<<"partition " << num() << " clear is clobbering " 
            << _fhdl_rd << " and " << _fhdl_app);
    _fhdl_rd = invalid_fhdl;
    _fhdl_app = invalid_fhdl;
}

void              
partition_t::init(log_core *owner) 
{
    _start = 0; // always
    _owner = owner;
    _eop = owner->limit(); // always
    clear();
}

/*
 * partition::flush(int fd, bool force)
 * flush to disk whatever's been buffered. 
 * Do this with a writev of 4 parts:
 * start->end1 where start is start1 rounded down to the beginning of a BLOCK
 * start2->end2
 * a skip record 
 * enough zeroes to make the entire write become a multiple of BLOCK_SIZE 
 */
void 
partition_t::flush(
        int fd, // not necessarily fhdl_app() since flush is called from
        // skip, when peeking and this might be in recovery.
        lsn_t lsn,  // needed so that we can set the lsn in the skip_log record
        const char* const buf, 
        long start1, 
        long end1, 
        long start2, 
        long end2)
{
    long size = (end2 - start2) + (end1 - start1);
    long write_size = size;

    { // sync log: Seek the file to the right place.
        DBGTHRD( << "Sync-ing log lsn " << lsn 
                << " start1 " << start1 
                << " end1 " << end1 
                << " start2 " << start2 
                << " end2 " << end2 );

        // This change per e-mail from Ippokratis, 16 Jun 09:
        // long file_offset = _owner->floor(lsn.lo(), log_core::BLOCK_SIZE);
        // works because BLOCK_SIZE is always a power of 2
        long file_offset = log_core::floor2(lsn.lo(), log_core::BLOCK_SIZE);
        // offset is rounded down to a block_size

        long delta = lsn.lo() - file_offset;
      
        // adjust down to the nearest full block
        w_assert1(start1 >= delta); // really offset - delta >= 0, 
                                    // but works for unsigned...
        write_size += delta; // account for the extra (clean) bytes
        start1 -= delta;

        /* FRJ: This seek is safe (in theory) because only one thread
           can flush at a time and all other accesses to the file use
           pread/pwrite (which doesn't change the file pointer).
         */
        fileoff_t where = start() + file_offset;
        w_rc_t e = me()->lseek(fd, where, sthread_t::SEEK_AT_SET);
        if (e.is_error()) {
            W_FATAL_MSG(e.err_num(), << "ERROR: could not seek to "
                                    << file_offset
                                    << " + " << start()
                                    << " to write log record"
                                    << endl);
        }
    } // end sync log
    
    /*
       stolen from log_buf::write_to
    */
    { // Copy a skip record to the end of the buffer.
        skip_log* s = _owner->get_skip_log();
        s->set_lsn_ck(lsn+size);

#ifdef W_TRACE
		off_t position = lseek(fd, 0, SEEK_CUR);
		DBGTHRD(<<"setting lsn_ck in skip_log at pos " 
				<< position << " with lsn " 
                << s->get_lsn_ck() 
                << "and size " << s->length()
                );
#endif

        // Hopefully the OS is smart enough to coalesce the writes
        // before sending them to disk. If not, and it's a problem
        // (e.g. for direct I/O), the alternative is to assemble the last
        // block by copying data out of the buffer so we can append the
        // skiplog without messing up concurrent inserts. However, that
        // could mean copying up to BLOCK_SIZE bytes.
        long total = write_size + s->length();

        // This change per e-mail from Ippokratis, 16 Jun 09:
        // long grand_total = _owner->ceil(total, log_core::BLOCK_SIZE);
        // works because BLOCK_SIZE is always a power of 2
        long grand_total = log_core::ceil2(total, log_core::BLOCK_SIZE);
        // take it up to multiple of block size
        w_assert2(grand_total % log_core::BLOCK_SIZE == 0);

        typedef sdisk_base_t::iovec_t iovec_t;

        iovec_t iov[] = {
            // iovec_t expects void* not const void *
            iovec_t((char*)buf+start1,                end1-start1),
            // iovec_t expects void* not const void *
            iovec_t((char*)buf+start2,                end2-start2), 
            iovec_t(s,                        s->length()),
            iovec_t(block_of_zeros(),         grand_total-total), 
        };
    
        w_rc_t e = me()->writev(fd, iov, sizeof(iov)/sizeof(iovec_t));
        if (e.is_error()) {
            smlevel_0::errlog->clog << fatal_prio 
                                    << "ERROR: could not flush log buf:"
                                    << " fd=" << fd
                                << " xfersize=" 
                                << log_core::BLOCK_SIZE
                                << " vec parts: " 
                                << " " << iov[0].iov_len
                                << " " << iov[1].iov_len
                                << " " << iov[2].iov_len
                                << " " << iov[3].iov_len
                                    << ":" << endl
                                    << e
                                    << flushl;
            cerr 
                                    << "ERROR: could not flush log buf:"
                                    << " fd=" << fd
                                    << " xfersize=" << log_core::BLOCK_SIZE
                                << log_core::BLOCK_SIZE
                                << " vec parts: " 
                                << " " << iov[0].iov_len
                                << " " << iov[1].iov_len
                                << " " << iov[2].iov_len
                                << " " << iov[3].iov_len
                                    << ":" << endl
                                    << e
                                    << flushl;
            W_COERCE(e);
        }
    } // end copy skip record

    this->flush(fd); // fsync
}

/*
 *  partition_t::_peek(num, peek_loc, whole_size,
        recovery, fd) -- used by both -- contains
 *   the guts
 *
 *  Peek at a partition num() -- see what num it represents and
 *  if it's got anything other than a skip record in it.
 *
 *  If recovery==true,
 *  determine its size, if it already exists (has something
 *  other than a skip record in it). In this case its num
 *  had better match num().
 *
 *  If it's just a skip record, consider it not to exist, and
 *  set _num to 0, leave it "closed"
 *
 *********************************************************************/
void
partition_t::_peek(
    partition_number_t num_wanted,
    fileoff_t        peek_loc,
    fileoff_t        whole_size,
    bool recovery,
    int fd
)
{
    FUNC(partition_t::_peek);
    w_assert3(num() == 0 || num() == num_wanted);
    clear();

    _clr_state(m_exists);
    _clr_state(m_flushed);
    _clr_state(m_open_for_read);

    w_assert3(fd);

    logrec_t        *l = NULL;

    // seek to start of partition or to the location given
    // in peek_loc -- that's a location we suspect might
    // be the end of-the-log skip record.
    //
    // the lsn passed to read(rec,lsn) is not
    // inspected for its hi() value
    //
    bool  peeked_high = false;
    if(    (peek_loc != partition_t::nosize)
        && (peek_loc <= this->_eop) 
        && (peek_loc < whole_size) ) {
        peeked_high = true;
    } else {
        peek_loc = 0;
        peeked_high = false;
    }
again:
    lsn_t pos = lsn_t(uint4_t(num()), sm_diskaddr_t(peek_loc));

    lsn_t lsn_ck = pos ;
    w_rc_t rc;

    while(pos.lo() < this->_eop) {
        DBGTHRD("pos.lo() = " << pos.lo()
                << " and eop=" << this->_eop);
        if(recovery) {
            // increase the starting point as much as possible.
            // to decrease the time for recovery
            if(pos.hi() == _owner->master_lsn().hi() &&
               pos.lo() < _owner->master_lsn().lo())  {
                  if(!debug_log) {
                      pos = _owner->master_lsn();
                  }
            }
        }
        DBGTHRD( <<"reading pos=" << pos <<" eop=" << this->_eop);

        rc = read(l, pos, fd);
        DBGTHRD(<<"POS " << pos << ": tx." << *l);

        if(rc.err_num() == smlevel_0::eEOF) {
            // eof or record -- wipe it out
            DBGTHRD(<<"EOF--Skipping!");
            _skip(pos, fd);
            break;
        }

        w_assert1(l != NULL);
                
        DBGTHRD(<<"peek index " << _index 
            << " l->length " << l->length() 
            << " l->type " << int(l->type()));

        w_assert1(l->length() >= logrec_t::hdr_sz);
        {
            // check lsn
            lsn_ck = l->get_lsn_ck();
            int err = 0;

            DBGTHRD( <<"lsnck=" << lsn_ck << " pos=" << pos
                <<" l.length=" << l->length() );


            if( ( l->length() < logrec_t::hdr_sz )
                ||
                ( l->length() > sizeof(logrec_t) )
                ||
                ( lsn_ck.lo() !=  pos.lo() )
                ||
                (num_wanted  && (lsn_ck.hi() != num_wanted) )
                ) {
                err++;
            }

            if( num_wanted  && (lsn_ck.hi() != num_wanted) ) {
                // Wrong partition - break out/return
                DBGTHRD(<<"NOSTASH because num_wanted="
                        << num_wanted
                        << " lsn_ck="
                        << lsn_ck
                    );
                return;
            }

            DBGTHRD( <<"type()=" << int(l->type())
                << " index()=" << this->index() 
                << " lsn_ck=" << lsn_ck
                << " err=" << err );

            /*
            // if it's a skip record, and it's the first record
            // in the partition, its lsn might be null.
            //
            // A skip record that's NOT the first in the partiton
            // will have a correct lsn.
            */

#if W_DEBUG_LEVEL > 1
            if( l->type() == logrec_t::t_skip ) {
                smlevel_0::errlog->clog << info_prio <<
                "Found skip record " << " at " << pos
                << flushl;
            }
#endif
            if( l->type() == logrec_t::t_skip   && 
                pos == first_lsn()) {
                // it's a skip record and it's the first rec in partition
                if( lsn_ck != lsn_t::null )  {
                    DBGTHRD( <<" first rec is skip and has lsn " << lsn_ck );
                    err = 1; 
                }
            } else {
                // ! skip record or ! first in the partition
                if ( (lsn_ck.hi()-1) % PARTITION_COUNT != (uint4_t)this->index()) {
                    DBGTHRD( <<"unexpected end of log");
                    err = 2;
                }
            }
            if(err > 0) {
                // bogus log record, 
                // consider end of log to be previous record

                if(err > 1) {
                    smlevel_0::errlog->clog << error_prio <<
                    "Found unexpected end of log --"
                    << " probably due to a previous crash." 
                    << flushl;
                }

                if(peeked_high) {
                    // set pos to 0 and start this loop all over
                    DBGTHRD( <<"Peek high failed at loc " << pos);
                    peek_loc = 0;
                    peeked_high = false;
                    goto again;
                }

                /*
                // Incomplete record -- wipe it out
                */
#if W_DEBUG_LEVEL > 2
                if(pos.hi() != 0) {
                   w_assert3(pos.hi() == num_wanted);
                }
#endif 

                // assign to lsn_ck so that the when
                // we drop out the loop, below, pos is set
                // correctly.
                lsn_ck = lsn_t(num_wanted, pos.lo());
                _skip(lsn_ck, fd);
                break;
            }
        }
        // DBGTHRD(<<" changing pos from " << pos << " to " << lsn_ck );
        pos = lsn_ck;

        DBGTHRD(<< " recovery=" << recovery
            << " master=" << _owner->master_lsn()
        );
        if( l->type() == logrec_t::t_skip 
            || !recovery) {
            /*
             * IF 
             *  we hit a skip record 
             * or 
             *  if we're not in recovery (i.e.,
             *  we aren't trying to find the last skip log record
             *  or check each record's legitimacy)
             * THEN 
             *  we've seen enough
             */
            DBGTHRD(<<" BREAK EARLY ");
            break;
        }
        pos.advance(l->length());
    }

    // pos == 0 if the first record
    // was a skip or if we don't care about the recovery checks.

    w_assert1(l != NULL);
    DBGTHRD(<<"pos= " << pos << "l->type()=" << int(l->type()));

#if W_DEBUG_LEVEL > 2
    if(pos.lo() > first_lsn().lo()) {
        w_assert3(l!=0);
    }
#endif 

    if( pos.lo() > first_lsn().lo() || l->type() != logrec_t::t_skip ) {
        // we care and the first record was not a skip record
        _num = pos.hi();

        // let the size *not* reflect the skip record
        // and let us *not* set it to 0 (had we not read
        // past the first record, which is the case when
        // we're peeking at a partition that's earlier than
        // that containing the master checkpoint
        // 
        if(pos.lo()> first_lsn().lo()) set_size(pos.lo());

        // OR first rec was a skip so we know
        // size already
        // Still have to figure out if file exists

        _set_state(m_exists);

        DBGTHRD(<<"STASHED num()=" << num()
                << " size()=" << size()
            );
    } else { 
        w_assert3(num() == 0);
        w_assert3(size() == nosize || size() == 0);
        // size can be 0 if the partition is exactly
        // a skip record
        DBGTHRD(<<"SIZE NOT STASHED ");
    }
}


// Helper for _peek
void
partition_t::_skip(const lsn_t &ll, int fd)
{
    FUNC(partition_t::skip);

    // Current partition should flush(), not skip()
    w_assert1(_num == 0 || _num != _owner->partition_num());
    
    DBGTHRD(<<"skip at " << ll);

    char* _skipbuf = new char[log_core::BLOCK_SIZE*2];
    // FRJ: We always need to prime() partition ops (peek, open, etc)
    // always use a different buffer than log inserts.
    long offset = _owner->prime(_skipbuf, fd, start(), ll);
    
    // Make sure that flush writes a skip record
    this->flush(fd, ll, _skipbuf, offset, offset, offset, offset);
    delete [] _skipbuf;
    DBGTHRD(<<"wrote and flushed skip record at " << ll);

    _set_last_skip_lsn(ll);
}

/*
 * partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
 * 
 * expect ll to be correct for this partition.
 * if we're reading this for the first time,
 * for the sake of peek(), we expect ll to be
 * lsn_t(0,0), since we have no idea what
 * its lsn is supposed to be, but in fact, we're
 * trying to find that out.
 *
 * If a non-zero fd is given, the read is to be done
 * on that fd. Otherwise it is assumed that the
 * read will be done on the fhdl_rd().
 */
// MUTEX: partition
w_rc_t
partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
{
    FUNC(partition::read);

    INC_TSTAT(log_fetches);

    if(fd == invalid_fhdl) fd = fhdl_rd();

#if W_DEBUG_LEVEL > 2
    w_assert3(fd);
    if(exists()) {
        if(fd) w_assert3(is_open_for_read());
        w_assert3(num() == ll.hi());
    }
#endif 

    fileoff_t pos = ll.lo();
    fileoff_t lower = pos / XFERSIZE;

    lower *= XFERSIZE;
    fileoff_t off = pos - lower;

    DBGTHRD(<<"seek to lsn " << ll
        << " index=" << _index << " fd=" << fd
        << " pos=" << pos
        << " lower=" << lower  << " + " << start()
        << " fd=" << fd
    );

    /* 
     * read & inspect header size and see
     * and see if there's more to read
     */
    int b = 0;
    fileoff_t leftover = logrec_t::hdr_sz;
    bool first_time = true;

    rp = (logrec_t *)(_readbuf() + off);

    DBGTHRD(<< "off= " << ((int)off)
        << "_readbuf()@ " << W_ADDR(_readbuf())
        << " rp@ " << W_ADDR(rp)
    );

    while (leftover > 0) {

        DBGTHRD(<<"leftover=" << int(leftover) << " b=" << b);

        w_rc_t e = me()->pread(fd, (void *)(_readbuf() + b), XFERSIZE, start() + lower + b);
        DBGTHRD(<<"after me()->read() size= " << int(XFERSIZE));


        if (e.is_error()) {
                /* accept the short I/O error for now */
                smlevel_0::errlog->clog << fatal_prio 
                        << "read(" << int(XFERSIZE) << ")" << flushl;
                W_COERCE(e);
        }
        b += XFERSIZE;

        // 
        // This could be written more simply from
        // a logical standpoint, but using this
        // first_time makes it a wee bit more readable
        //
        if (first_time) {
            if( rp->length() > sizeof(logrec_t) || 
            rp->length() < logrec_t::hdr_sz ) {
                w_assert1(ll.hi() == 0); // in peek()
                return RC(smlevel_0::eEOF);
            }
            first_time = false;
            leftover = rp->length() - (b - off);
            DBGTHRD(<<" leftover now=" << leftover);
        } else {
            leftover -= XFERSIZE;
            w_assert3(leftover == (int)rp->length() - (b - off));
            DBGTHRD(<<" leftover now=" << leftover);
        }
    }
    DBGTHRD( << "_readbuf()@ " << W_ADDR(_readbuf())
        << " first 4 chars are: "
        << (int)(*((char *)_readbuf()))
        << (int)(*((char *)_readbuf()+1))
        << (int)(*((char *)_readbuf()+2))
        << (int)(*((char *)_readbuf()+3))
    );
    w_assert1(rp != NULL);
    return RCOK;
}


w_rc_t
partition_t::open_for_read(
    partition_number_t  __num,
    bool err // = true.  if true, it's an error for the partition not to exist
)
{
    FUNC(partition_t::open_for_read);
    // protected w_assert2(_owner->_partition_lock.is_mine()==true);
    // asserted before call in srv_log.cpp

    DBGTHRD(<<"start open for part " << __num << " err=" << err);

    w_assert1(__num != 0);

    // do the equiv of opening existing file
    // if not already in the list and opened
    //
    if(fhdl_rd() == invalid_fhdl) {
        char *fname = new char[smlevel_0::max_devname];
        if (!fname)
                W_FATAL(fcOUTOFMEMORY);
        w_auto_delete_array_t<char> ad_fname(fname);

        log_m::make_log_name(__num, fname, smlevel_0::max_devname);

        int fd;
        w_rc_t e;
        DBGTHRD(<< "partition " << __num
                << "open_for_read OPEN " << fname);
        int flags = smthread_t::OPEN_RDONLY;

        e = me()->open(fname, flags, 0, fd);

        DBGTHRD(<< " OPEN " << fname << " returned " << fd);

        if (e.is_error()) {
            if(err) {
                smlevel_0::errlog->clog << fatal_prio
                    << "Cannot open log file: partition number "
					<< __num  << " fd" << fd << flushl;
                // fatal
                W_DO(e);
            } else {
                w_assert3(! exists());
                w_assert3(_fhdl_rd == invalid_fhdl);
                // _fhdl_rd = invalid_fhdl;
                _clr_state(m_open_for_read);
                DBGTHRD(<<"fhdl_app() is " << _fhdl_app);
                return RCOK;
            }
        }

        w_assert3(_fhdl_rd == invalid_fhdl);
        _fhdl_rd = fd;

        DBGTHRD(<<"size is " << size());
        // size might not be known, might be anything
        // if this is an old partition

        _set_state(m_exists);
        _set_state(m_open_for_read);
    }
    _num = __num;
    w_assert3(exists());
    w_assert3(is_open_for_read());
    // might not be flushed, but if
    // it isn't, surely it's flushed up to
    // the offset we're reading
    //w_assert3(flushed());

    w_assert3(_fhdl_rd != invalid_fhdl);
    DBGTHRD(<<"_fhdl_rd = " <<_fhdl_rd );
    return RCOK;
}

/*
 * close for append, or if both==true, close
 * the read-file also
 */
void
partition_t::close(bool both) 
{
    bool err_encountered=false;
    w_rc_t e;

    // protected member: w_assert2(_owner->_partition_lock.is_mine()==true);
    // assert is done by callers
    if(is_current()) {
        // This assertion is bad -- the log flusher is probably trying 
        // to update dlsn right now!
        //        w_assert1(dlsn.hi() > num());
        //        _owner->_flush(_owner->curr_lsn());
        //w_assert3(flushed());
        _owner->unset_current();
    }
    if (both) {
        if (fhdl_rd() != invalid_fhdl) {
            DBGTHRD(<< " CLOSE " << fhdl_rd());
            e = me()->close(fhdl_rd());
            if (e.is_error()) {
                smlevel_0::errlog->clog << error_prio 
                        << "ERROR: could not close the log file."
                        << e << endl << flushl;
                err_encountered = true;
            }
        }
        _fhdl_rd = invalid_fhdl;
        _clr_state(m_open_for_read);
    }

    if (is_open_for_append()) {
        DBGTHRD(<< " CLOSE " << fhdl_rd());
        e = me()->close(fhdl_app());
        if (e.is_error()) {
            smlevel_0::errlog->clog << error_prio 
            << "ERROR: could not close the log file."
            << endl << e << endl << flushl;
            err_encountered = true;
        }
        _fhdl_app = invalid_fhdl;
        _clr_state(m_open_for_append);
        DBGTHRD(<<"fhdl_app() is " << _fhdl_app);
    }

    _clr_state(m_flushed);
    if (err_encountered) {
        W_COERCE(e);
    }
}


void 
partition_t::sanity_check() const
{
    if(num() == 0) {
       // initial state
       w_assert3(size() == nosize);
       w_assert3(!is_open_for_read());
       w_assert3(!is_open_for_append());
       w_assert3(!exists());
       // don't even ask about flushed
    } else {
       w_assert3(exists());
       (void) is_open_for_read();
       (void) is_open_for_append();
    }
    if(is_current()) {
       w_assert3(is_open_for_append());
    }
}



/**********************************************************************
 *
 *  partition_t::destroy()
 *
 *  Destroy a log file.
 *
 *********************************************************************/
void
partition_t::destroy()
{
    w_assert3(num() < _owner->global_min_lsn().hi());

    if(num()>0) {
        w_assert3(exists());
        w_assert3(! is_current() );
        w_assert3(! is_open_for_read() );
        w_assert3(! is_open_for_append() );

        log_core::destroy_file(num(), true);
        _clr_state(m_exists);
        // _num = 0;
        DBG(<< " calling clear");
        clear();
    }
    w_assert3( !exists());
    sanity_check();
}


void
partition_t::peek(
    partition_number_t  __num, 
    const lsn_t&        end_hint,
    bool                 recovery,
    int *                fdp
)
{
    FUNC(partition_t::peek);
    // this is a static func so we cannot assert this:
    // w_assert2(_owner->_partition_lock.is_mine()==true);
    int fd;

    // Either we have nothing opened or we are peeking at something
    // already opened.
    w_assert2(num() == 0 || num() == __num);
    w_assert3(__num != 0);

    if( num() ) {
        close_for_read();
        close_for_append();
        DBG(<< " calling clear");
        clear();
    }

    _clr_state(m_exists);
    _clr_state(m_flushed);

    char *fname = new char[smlevel_0::max_devname];
    if (!fname)
        W_FATAL(fcOUTOFMEMORY);
    w_auto_delete_array_t<char> ad_fname(fname);        
    log_m::make_log_name(__num, fname, smlevel_0::max_devname);

    smlevel_0::fileoff_t part_size = fileoff_t(0);

    DBGTHRD(<<"partition " << __num << " peek opening " << fname);

    // first create it if necessary.
    int flags = smthread_t::OPEN_RDWR | smthread_t::OPEN_SYNC
            | smthread_t::OPEN_CREATE;
    w_rc_t e;
    e = me()->open(fname, flags, 0744, fd);
    if (e.is_error()) {
        smlevel_0::errlog->clog << fatal_prio
            << "ERROR: cannot open log file: " << endl << e << flushl;
        W_COERCE(e);
    }
    DBGTHRD(<<"partition " << __num << " peek  opened " << fname);
    {
         w_rc_t e;
         sthread_base_t::filestat_t statbuf;
         e = me()->fstat(fd, statbuf);
         if (e.is_error()) {
                smlevel_0::errlog->clog << fatal_prio 
                << " Cannot stat fd " << fd << ":" 
                << endl << e  << flushl;
                W_COERCE(e);
         }
         part_size = statbuf.st_size;
         DBGTHRD(<< "partition " << __num << " peek "
             << "size of " << fname << " is " << statbuf.st_size);
    }

    // We will eventually want to write a record with the durable
    // lsn.  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition.
    //
    // If this was a pre-existing partition, we have to scan it
    // to find the *real* end of the file.

    if( part_size > 0 ) {
        w_assert3(__num == end_hint.hi() || end_hint.hi() == 0);
        _peek(__num, end_hint.lo(), part_size, recovery, fd);
    } else {
        // write a skip record so that prime() can
        // cope with it.
        // Have to do this carefully -- since using
        // the standard insert()/write code causes a
        // prime() to occur and that doesn't solve anything.

        DBGTHRD(<<" peek DESTROYING PARTITION " << __num << "  on fd " << fd);

        // First: write any-old junk
        w_rc_t e = me()->ftruncate(fd,  log_core::BLOCK_SIZE );
        if (e.is_error())        {
             smlevel_0::errlog->clog << fatal_prio
                << "cannot write garbage block " << flushl;
            W_COERCE(e);
        }
        /* write the lsn of the up-coming skip record */

        // Now write the skip record and flush it to the disk:
        _skip(first_lsn(__num), fd);

        // First: write any-old junk
        e = me()->fsync(fd);
        if (e.is_error()) {
             smlevel_0::errlog->clog << fatal_prio
                << "cannot sync after skip block " << flushl;
            W_COERCE(e);
        }

        // Size is 0
        set_size(0);
    }

    if (fdp) {
        DBGTHRD(<< "partition " << __num << " SAVED, NOT CLOSED fd " << fd);
        *fdp = fd;
    } else {
        DBGTHRD(<< " CLOSE " << fd);
        w_rc_t e = me()->close(fd);
        if (e.is_error()) {
            smlevel_0::errlog->clog << fatal_prio 
            << "ERROR: could not close the log file." << flushl;
            W_COERCE(e);
        }
        
    }
}

void                        
partition_t::flush(int fd)
{
    // We only cound the fsyncs called as
    // a result of flush(), not from peek
    // or start-up
    INC_TSTAT(log_fsync_cnt);

    w_rc_t e = me()->fsync(fd);
    if (e.is_error()) {
         smlevel_0::errlog->clog << fatal_prio
            << "cannot sync after skip block " << flushl;
        W_COERCE(e);
    }
}

void 
partition_t::close_for_append()
{
    int f = fhdl_app();
    if (f != invalid_fhdl)  {
        w_rc_t e;
        DBGTHRD(<< " CLOSE " << f);
        e = me()->close(f);
        if (e.is_error()) {
            smlevel_0::errlog->clog  << warning_prio
                << "warning: error in unix log on close(app):" 
                    << endl <<  e << endl;
        }
        _fhdl_app = invalid_fhdl;
    }
}

void 
partition_t::close_for_read()
{
    int f = fhdl_rd();
    if (f != invalid_fhdl)  {
        w_rc_t e;
        DBGTHRD(<< " CLOSE " << f);
        e = me()->close(f);
        if (e.is_error()) {
            smlevel_0::errlog->clog  << warning_prio
                << "warning: error in unix partition on close(rd):" 
                << endl <<  e << endl;
        }
        _fhdl_rd = invalid_fhdl;
    }
}

/*<std-header orig-src='shore'>

 $Id: latch1.cpp,v 1.1.2.8 2010/03/19 22:19:21 nhall Exp $

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

#include <cstdlib>
#include <ctime>
#include <w_base.h>
#include <sthread.h>
#include <w_getopt.h>

#undef ORIG_SM
#ifdef ORIG_SM
#define join wait
#endif
#include <latch.h>

#define RUN_TEST1
#undef RUN_TEST2
#undef RUN_TEST3

#include <iostream>
#include <w_strstream.h>

#define NUM_THREADS 3
#define LAST_THREAD (NUM_THREADS-1)


latch_t  the_latch;
queue_based_block_lock_t print_mutex;
// sevsem_t done;
int      done_count;
pthread_cond_t  done; // paried with done_mutex
pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER; // paired with done
bool     verbose(false);
int      testnum = -1;
const char     *argv0(NULL);

void usage(ostream &o, int ex)
{
    o << "usage: " << argv0 << " -t <1|2|3> [-v]" << endl;
    ::exit(ex);
}

struct latch_thread_id_t {
    latch_thread_id_t (int x): _id(x) {};
    int _id;
};

class latch_thread_t : public sthread_t {
public:
    latch_thread_t(int id);
    ~latch_thread_t() {
		CRITICAL_SECTION(cs, lock);
        hasExited = true;
    }

    static void sync_other(latch_thread_t *r);

protected:
    typedef enum { all_sh, one_ex, all_ex } testKind;
    static const char *kind_strings[];
    void run();
    void test1(int i, testKind t);
    void test2(int i, latch_mode_t mode1, latch_mode_t mode2);
    void test3(int i, testKind t);
    void test4();
private:
    latch_thread_id_t     _self;
    bool                  isWaiting;
    bool                  canProceed;
    bool                  hasExited;
    pthread_cond_t        quiesced;
    pthread_cond_t        proceed;
    pthread_mutex_t       lock;

    void sync() {
		CRITICAL_SECTION(cs, lock);
        isWaiting=true;
		DO_PTHREAD(pthread_cond_signal(&quiesced));
        while(!canProceed) {
			DO_PTHREAD(pthread_cond_wait(&proceed, &lock));
		}
        canProceed = false;
    }
};

latch_thread_t::latch_thread_t(int _id)
: sthread_t(t_regular),
  _self(_id),
  isWaiting(false),canProceed(false),hasExited(false)
{
    w_ostrstream_buf    s(40);        // XXX magic number
    s << "latch_thread " << _id << ends;
    this->rename(s.c_str());

    DO_PTHREAD(pthread_cond_init(&quiesced, NULL));
    DO_PTHREAD(pthread_cond_init(&proceed, NULL));
    DO_PTHREAD(pthread_mutex_init(&lock, NULL));
}

const char *latch_thread_t::kind_strings[] = {
    "all_sh", 
    "one_ex", 
     "all_ex"
};

void latch_thread_t::sync_other(latch_thread_t *r) 
{
	CRITICAL_SECTION(cs, &r->lock);
    while(!(r->isWaiting || r->hasExited)) {
		DO_PTHREAD(pthread_cond_wait(&r->quiesced, &r->lock));
	}
    r->isWaiting = false;
    r->canProceed = true;
	DO_PTHREAD(pthread_cond_signal(&r->proceed));
}

#ifdef ORIG_SM
// We renamed these methods in the new sm
#define latch_cnt lock_cnt
#define is_latched is_locked
#endif

void
dump(ostream &o, latch_t &l)
{
    o << " mode " << latch_t::latch_mode_str[ l.mode() ]
    << " num_holders " << l.num_holders()
    << " latch_cnt " << l.latch_cnt()
    << " is_latched " << (const char *)(l.is_latched()?"true":"false")
    << " is_mine " << (const char *)(l.is_mine()?"true":"false")
    << " held_by_me " << l.held_by_me()
    << endl;
}

void
check(  int line,
    const char *msg,
    latch_t &l,
    latch_mode_t expected, 
    latch_mode_t m1, 
    latch_mode_t m2, 
    int holders,
    int latch_cnt,
    bool is_latched,
    bool is_mine,
    int held_by_me
)
{
	{
		CRITICAL_SECTION(cs, print_mutex);
		cout << endl;
		cout << " {---------------------- "
		<< latch_t::latch_mode_str[m1] << " / " << latch_t::latch_mode_str[m2] 
		<< "  ------------------" << line << endl; /*}*/
		cout << "\t" << msg << endl;
		dump(cout, l);
	}

    bool do_asserts(true);
    if(do_asserts)
    {
    int last_failure(0);
    int failure(0);
    if(!(l.mode() == expected)) { failure++; last_failure=__LINE__; }
    if(!(l.num_holders() == holders)) { failure++; last_failure=__LINE__; }
    if(!(l.latch_cnt() == latch_cnt)) { failure++; last_failure=__LINE__; }
    if(!(l.is_latched() == is_latched)) { failure++; last_failure=__LINE__; }
    if(!(l.is_mine() == is_mine)) { failure++; last_failure=__LINE__; }
    if(!(l.held_by_me() == held_by_me)) { failure++; last_failure=__LINE__; }
    if(failure > 0) {
		CRITICAL_SECTION(cs, print_mutex);
        cout << l << endl;
        cout << "# failures: " << failure
        << " last @" << last_failure 
        << endl << endl;;
        w_assert1(0);
    }
    }
        /*{*/
    cout << " ---------------------------------------------------" 
    << line << "}" << endl;
}

ostream &
operator << (ostream &o, const latch_thread_id_t &ID)
{
    int j = ID._id;
    for(int i=0; i < j; i++) {
    o << "-----";
    }
    o << "----- " << ID._id << ":" ;
    return o;
}

// int i gives an id of the thread that's supposed to acquire an EX-mode
// if anyone is to do so.
//  -1 means noone, else it should be the id of some thread in the range
void latch_thread_t::test1(int i, testKind t)
{
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " starting test 1 id=" << i << "  " << kind_strings[int(t)]
    << endl;
        cerr << _self << " await sync @" << __LINE__ << endl;
    }

    sync(); // test1 #1

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " test 1 got sync " <<  endl;
        cerr << _self << " await acquire @" << __LINE__ << endl;
    }

    latch_mode_t mode = LATCH_SH;
    switch(t) {
    case one_ex:
        if(i == _self._id) mode = LATCH_EX;
        break;
    case all_ex:
        mode = LATCH_EX;
        break;
    case all_sh:
    default:
        break;
    }
    // START test 1: everyone acquire in given mode
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " latch_acquire mode= "
            << mode  << "  @" << __LINE__ << endl;
        print_all_latches();
    }
    the_latch.latch_acquire(mode);

    yield();

    w_assert1(the_latch.mode() == mode);
    w_assert1(the_latch.num_holders() > 0);
    w_assert1(the_latch.latch_cnt() > 0);
    w_assert1(the_latch.is_latched() == true);
    w_assert1(the_latch.is_mine() == (mode == LATCH_EX));
    w_assert1(the_latch.held_by_me() == 1);

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " latch_release @" << __LINE__ << endl;
    }

    the_latch.latch_release();
    yield();

    w_assert1(the_latch.is_mine() == false);
    w_assert1(the_latch.held_by_me() == 0);

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " end test 1 " <<  i << endl;
    }

	{
		CRITICAL_SECTION(cs, done_mutex);
		done_count++;
	}
	DO_PTHREAD(pthread_cond_signal(&done));
}

const char* const  latch_mode_str[3] = { "NL", "SH", "EX" };

// 
// test2 is performed by only one thread, so there are
// no races in checking the status of the latch (check(), assertions)
//
void latch_thread_t::test2(int i, latch_mode_t mode1,
    latch_mode_t mode2)
{
    // Make only one thread do anything here
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self 
        << "--------- test2 STARTING " << " modes="
        << latch_mode_str[int(mode1)] << ", " << latch_mode_str[int(mode2)] 
        << endl;
        cerr << _self << " await sync @" << __LINE__ << endl;
    }
    sync();  // test2 #1

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << "--------- test2 got sync " << endl;
    }

    if(i == _self._id)
    {
        bool is_upgrade(false);
        // NOTE: this use of the word "upgrade" is a little misleading.
        // The counts don't get increased for a "real" upgrade, but they
        // do for a duplicate latch, regardless of the mode.
        if(mode2 > mode1)
        {
            is_upgrade=true;
        }

        check(__LINE__, 
            "before first acquire ",
            the_latch, 
            LATCH_NL /* expected */,
            mode1, mode2, 
            0 /* holders */,
            0 /* latch_cnt */,
            false /* is_latched */,
            false /* is_mine */,
            0 /* # held_by_me */
         );

        if(verbose)  {
			CRITICAL_SECTION(cs, print_mutex);
            cerr << _self << " latch_acquire mode= "
                << mode1  << "  @" << __LINE__ << endl;
        }

        // FIRST ACQUIRE
        the_latch.latch_acquire(mode1);

        check(__LINE__, 
            " after first acquire ",
            the_latch, 
            (mode1==LATCH_NL)?LATCH_EX:mode1 /* expected */,
            mode1, mode2, 
            1 /* holders */,
            1 /* latch_cnt */,
            true /* is_latched */,
            (mode1==LATCH_EX) /* is_mine */,
            (mode1==LATCH_NL)?0:1 /* # held_by_me */
         );

        if(verbose)  {
			CRITICAL_SECTION(cs, print_mutex);
            cerr << _self << " double latch_acquire mode= "
                << mode2  << "  @" << __LINE__ << endl;
        }

        // 2nd ACQUIRE
        the_latch.latch_acquire(mode2);
        if(is_upgrade) 
        {
    #ifdef ORIG_SM
    // In the original sm, the number of holders was not the
    // same as the lock count
            check(__LINE__, 
            " after 2nd acquire (upgrade) ",
            the_latch, 
            mode2 /* expected */,
            mode1, mode2, 
            1 /* holders */,
            2 /* latch_cnt */,
            true /* is_latched */,
            (mode2==LATCH_EX) /* is_mine */,
            2 /* # held_by_me */
             );
    #else
            check(__LINE__, 
            " after 2nd acquire (upgrade) ",
            the_latch, 
            mode2 /* expected */,
            mode1, mode2, 
            1 /* holders */,
            2 /* latch_cnt */,
            true /* is_latched */,
            (mode2==LATCH_EX) /* is_mine */,
            2 /* # held_by_me */
             );
    #endif
        } 
        else 
        {
    #ifdef ORIG_SM
            check(__LINE__, 
            " after 2nd acquire (duplicate) ",
            the_latch, 
            mode2 /* expected */,
            mode1, mode2, 
            // hard-wired: if held in ex mode, can have only
            // one holder. Doesn't verify the counts or anything
            // THUS: num_holders is the # holders in l.mode(),
            // the latch's current mode (? what about NL?)
            (mode2==LATCH_EX)? 1 : 2 /* holders */,
            2 /* latch_cnt */,
            true /* is_latched */,
            (mode2==LATCH_EX) /* is_mine */,
            2 /* # held_by_me */
             );
    #else
            check(__LINE__, 
            " after 2nd acquire (duplicate) ",
            the_latch, 
            mode2 /* expected */,
            mode1, mode2, 
            1 /* holders */,
            2 /* latch_cnt */,
            true /* is_latched */,
            (mode2==LATCH_EX) /* is_mine */,
            2 /* # held_by_me */
             );
    #endif
        }

        if(verbose)  {
			CRITICAL_SECTION(cs, print_mutex);
            cerr << _self << " latch_release @" << __LINE__ << endl;
        }

        // FIRST RELEASE
        the_latch.latch_release();
        if(is_upgrade)
        {
    #ifdef ORIG_SM
            // This is odd... but it seems to be the case
            // that first release doesn't change the mode
            check(__LINE__, 
            " after first release (from upgrade) ",
            the_latch, 
            LATCH_EX /* expected */,
            mode1, mode2, 
            1 /* holders */,
            1 /* latch_cnt */,
            true /* is_latched */,
            true /* is_mine */,
            1 /* # held_by_me */
             );
    #else
            check(__LINE__, 
            " after first release (from upgrade) ",
            the_latch, 
            LATCH_EX /* expected */,
            mode1, mode2, 
            1 /* holders */,
            1 /* latch_cnt */,
            true /* is_latched */,
            true /* is_mine */, // --- note that this is still in EX mode
            1 /* # held_by_me */
             );
    #endif
        }
        else
        {
            check(__LINE__, 
            " after first release (from duplicate) ",
            the_latch, 
            mode1 /* expected */,
            mode1, mode2, 
            1 /* holders */,
            1 /* latch_cnt */,
            true /* is_latched */,
            (mode1==LATCH_EX) /* is_mine */,
            1 /* # held_by_me */
             );
        }

        if(verbose)  {
			CRITICAL_SECTION(cs, print_mutex);
            cerr << _self << " 2nd latch_release @" << __LINE__ << endl;
        }
        // 2nd RELEASE
        the_latch.latch_release();
        check(__LINE__, 
            " after 2nd release",
            the_latch, 
            LATCH_NL /* expected */,
            mode1, mode2, 
            0 /* holders */,
            0 /* latch_cnt */,
            false /* is_latched */,
            false /* is_mine */,
            0 /* # held_by_me */
         );

    } 
    else 
    {
        if(verbose)  {
			CRITICAL_SECTION(cs, print_mutex);
            cerr << _self << "--------- test2 VACUOUS"  << endl;
        }
    }
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << "--------- test2 END  " << mode1 << ", " << mode2 << endl;
    }

	{
		CRITICAL_SECTION(cs, done_mutex);
		done_count++;
	}
	DO_PTHREAD(pthread_cond_signal(&done));
}

void latch_thread_t::test3(int i, testKind t)
{
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " starting test 3 id=" << i << " " << kind_strings[int(t)]
    << endl;
        cerr << _self << " await sync @" << __LINE__ << endl;
    }

    sync(); // test3 #1

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " test 3 got sync " <<  endl;
        cerr << _self << " await acquire @" << __LINE__ << endl;
    }

    latch_mode_t mode = LATCH_SH;
    switch(t) {
    case one_ex:
        if(i == _self._id) mode = LATCH_EX;
        break;
    case all_ex:
        mode = LATCH_EX;
        break;
    case all_sh:
    default:
        break;
    }
    // START test 3: everyone acquire in given mode
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " latch_acquire mode= "
            << mode  << "  @" << __LINE__ << endl;
    }
    the_latch.latch_acquire(mode);

    yield();

    w_assert1(the_latch.mode() == mode);
    w_assert1(the_latch.num_holders() > 0);
    w_assert1(the_latch.latch_cnt() > 0);
    w_assert1(the_latch.is_latched() == true);
    w_assert1(the_latch.is_mine() == (mode == LATCH_EX));
    w_assert1(the_latch.held_by_me() == 1);

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " upgrade_if_not_block @" << __LINE__ << endl;
    }

    bool would_block(false);
    // This use of is_upgrade refers to a real upgrade
    // If we started with SH or NL mode, then upgrade will be true
    bool is_real_upgrade = (mode != LATCH_EX);

    W_COERCE(the_latch.upgrade_if_not_block(would_block));
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " upgrade_if_not_block would "   
        << (const char *)(would_block?"":" NOT ")
        << " have blocked "
        << __LINE__ << endl;
    }
    if(would_block) {
        // At least *I* should hold it in SH mode, even if the
        // others have released it already:
        w_assert1(the_latch.mode() == LATCH_SH);
        w_assert1(the_latch.num_holders() > 0);
        w_assert1(the_latch.latch_cnt() > 0);
        w_assert1(the_latch.is_latched() == true);
        w_assert1(the_latch.is_mine() == (mode == LATCH_EX));
        w_assert1(the_latch.held_by_me() == 1);
    } else if(is_real_upgrade) 
    {
        // upgrade worked
        w_assert1(the_latch.mode() == LATCH_EX);
        w_assert1(the_latch.num_holders() == 1);
        w_assert1(the_latch.latch_cnt() == 1);
        w_assert1(the_latch.is_latched() == true);
        w_assert1(the_latch.is_mine() == true);
        w_assert1(the_latch.held_by_me() == 1);
    } else
    {
        w_assert1(!is_real_upgrade);
        w_assert1(mode == LATCH_EX);
        // not an upgrade because original mode was LATCH_EX
        w_assert1(the_latch.mode() == mode);
        w_assert1(the_latch.num_holders() == 1);
        w_assert1(the_latch.latch_cnt() == 1);
        w_assert1(the_latch.is_latched() == true);
        w_assert1(the_latch.is_mine() == true); 
        w_assert1(the_latch.held_by_me() == 1);
    }

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " latch_release @" << __LINE__ << endl;
    }

    if( is_real_upgrade && !would_block ) {
        // upgrade worked, so these shouldn't have changed
        w_assert1(the_latch.mode() == LATCH_EX);
        w_assert1(the_latch.num_holders() == 1);
        w_assert1(the_latch.latch_cnt() == 1);
        w_assert1(the_latch.is_latched() == true);
        w_assert1(the_latch.is_mine() == true);
        w_assert1(the_latch.held_by_me() == 1);

        // first release
        the_latch.latch_release();
        yield();

        w_assert1(the_latch.is_mine() == false);
        w_assert1(the_latch.held_by_me() == 0);
        // RACY w_assert1(the_latch.latch_cnt() == 0);
        // RACY w_assert1(the_latch.mode() == LATCH_NL);
    } else  if(would_block) {
        // would have blocked - hold it only once
        // first release
        the_latch.latch_release();
        yield();

        w_assert1(the_latch.is_mine() == false);
        w_assert1(the_latch.held_by_me() == 0);
    } else {
        w_assert1(!is_real_upgrade);
        w_assert1(!would_block);

        // Did not upgrade but if mode is SH, it
        // first release
        the_latch.latch_release();
        yield();

        w_assert1(the_latch.is_mine() == false);
        w_assert1(the_latch.held_by_me() == 0);
    }

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << _self << " end test 3 " <<  i << endl;
    }

	{
		CRITICAL_SECTION(cs, done_mutex);
		done_count++;
	}
	DO_PTHREAD(pthread_cond_signal(&done));
}

void latch_thread_t::test4()
{
}

void latch_thread_t::run()
{
    latch_t::on_thread_init(this);
    switch(testnum)
    {
    case 1:
        test1(-1, all_sh);
        test1(2,  one_ex);
        test1(1,  one_ex); // ....
        test1(0,  one_ex);
        test1(-1, all_ex);
        break;

    case 2:
        test2(1, LATCH_SH, LATCH_SH); // only thread 1 does this test
        test2(1, LATCH_SH, LATCH_EX); // only thread 1 does this test
        test2(1, LATCH_EX, LATCH_EX); // only thread 1 does this test
        // Original SM w_assert9s that it never latches in
        // LATCH_NL mode. 
        // Assertions are in bf, latch
        // The code in bf with predicates if mode != LATCH_NL
        // should be removed. 
        //
        // shore-mt version lets you latch in NL mode but hangs on
        // an upgrade attempt
        // So I'm inserting an assert in latch.cpp to the extent that
        // you never latch in NL mode
        // test2(1, LATCH_NL, LATCH_SH);
        // test2(1, LATCH_NL, LATCH_EX);
        break;

    case 3:
        test3(-1, all_sh);
        test3(2,  one_ex);
        test3(1,  one_ex); // ....
        test3(0,  one_ex);
        test3(-1, all_ex);
        break;

    default:
        usage(cerr, testnum);
    }

    // Say we have exited.
	{
		CRITICAL_SECTION(cs, lock);
		hasExited = true;
	}
    latch_t::on_thread_destroy(this);
}
    
void sync_all (latch_thread_t **t)
{
	{
		CRITICAL_SECTION(cs, done_mutex);
		done_count=0;
	}

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << "{ sync_all START  syncing all " << endl; /*}*/
    }
    for(int i=0; i < NUM_THREADS; i++)
    {
        latch_thread_t::sync_other(t[i]);
    }

    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        cerr << "sync_all  synced all; awaiting them  " << endl;
    }

	{
		CRITICAL_SECTION(cs, done_mutex);
		do {
			DO_PTHREAD(pthread_cond_wait(&done, &done_mutex));
		} while(done_count < NUM_THREADS-1);

	}
    if(verbose)  {
		CRITICAL_SECTION(cs, print_mutex);
        /*{*/ cerr << "sync_all  done }" << endl;
    }
}

int main(int argc, char ** argv)
{
    argv0 = argv[0];
    int errors=0;
    char c;
    while ((c = getopt(argc, argv, "t:vh")) != EOF) {
        switch (c) {
        case 'v':
            verbose=true;
            break;
        case 'h':
        usage(cout, 0);
        break;
        case 't':
            testnum = atoi(optarg);
            break;
        default:
            errors++;
            break;
        }
    }
    if(errors > 0) {
       usage(cerr, 1);
    }

    latch_thread_t **latch_thread = new latch_thread_t *[NUM_THREADS];

    int i;
    for (i = 0; i < NUM_THREADS; i++)  {
        latch_thread[i] = new latch_thread_t(i);
        w_assert1(latch_thread[i]);
        W_COERCE(latch_thread[i]->fork());
    }
    switch(testnum)
    {
    case 1:
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        break;
    case 2:
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        // sync_all(latch_thread, 1);
        // sync_all(latch_thread, 1);
        break;
    case 3:
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        sync_all(latch_thread);
        break;
    default:
        usage(cerr, testnum);
        break;
    }

    for (i = 0; i < NUM_THREADS; i++)  {
        W_COERCE( latch_thread[i]->join());
        delete latch_thread[i];
    }

    delete [] latch_thread;

    // TODO: make this test the stats also
    if(verbose)
    sthread_t::dump_stats(cout);

    return 0;
}

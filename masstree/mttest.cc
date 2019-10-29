/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2013 President and Fellows of Harvard College
 * Copyright (c) 2012-2013 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Masstree LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Masstree LICENSE file; the license in that file
 * is legally binding.
 */
// -*- mode: c++ -*-
// mttest: key/value tester
//

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <limits.h>
#if HAVE_NUMA_H
#include <numa.h>
#endif
#if HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif
#if __linux__
#include <asm-generic/mman.h>
#endif
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include "kvstats.hh"
#include "masstree_query.hh"
#include "json.hh"
#include "kvtest.hh"
#include "kvrandom.hh"
#include "kvrow.hh"
#include "clp.h"
#include <algorithm>

static std::vector<int> cores;
volatile bool timeout[2] = {false, false};
double duration[2] = {10, 0};
// Do not start timer until asked
static bool lazy_timer = false;

uint64_t test_limit = ~uint64_t(0);
bool quiet = false;
bool print_table = false;
static const char *gid = NULL;

// all default to the number of cores
static int udpthreads = 0;
static int tcpthreads = 0;

static bool tree_stats = false;
static bool json_stats = false;
static bool pinthreads = false;
volatile uint64_t globalepoch = 1;     // global epoch, updated by main thread regularly
kvepoch_t global_log_epoch = 0;
static int port = 2117;
static int rscale_ncores = 0;

#if MEMSTATS && HAVE_NUMA_H && HAVE_LIBNUMA
static struct {
  long long free;
  long long size;
} numa[MaxNumaNode];
#endif

volatile bool recovering = false; // so don't add log entries, and free old value immediately
kvtimestamp_t initial_timestamp;

static const char *threadcounter_names[(int) tc_max];

/* running local tests */
void test_timeout(int) {
    size_t n;
    for (n = 0; n < arraysize(timeout) && timeout[n]; ++n)
	/* do nothing */;
    if (n < arraysize(timeout)) {
	timeout[n] = true;
	if (n + 1 < arraysize(timeout) && duration[n + 1])
	    xalarm(duration[n + 1]);
    }
}

template <typename T>
struct kvtest_server {
    kvtest_server()
        : limit_(test_limit), ncores_(udpthreads), kvo_() 
    { }

    ~kvtest_server() {
        if (kvo_)
            free_kvout(kvo_);
    }

    int nthreads() const { return udpthreads; }

    int id() const { return ti_->ti_index; }

    void set_table(T *table, threadinfo *ti) {
        table_ = table;
        ti_ = ti;
    }

    void reset(const String &test, int trial) {
        json_ = Json().set("table", T().name())
            .set("test", test).set("trial", trial)
            .set("thread", ti_->ti_index);
    }

    static void start_timer() {
        mandatory_assert(lazy_timer && "Cannot start timer without lazy_timer option");
        mandatory_assert(duration[0] && "Must specify timeout[0]");
        xalarm(duration[0]);
    }

    bool timeout(int which) const { return ::timeout[which]; }

    uint64_t limit() const { return limit_; }

    int ncores() const { return ncores_; }

    double now() const { return ::now(); }

    int ruscale_partsz() const { return (140 * 1000000) / 16; }

    int ruscale_init_part_no() const { return ti_->ti_index; }

    long nseqkeys() const { return 16 * ruscale_partsz(); }

    void get(long ikey);
    bool get_sync(const Str &key);
    bool get_sync(const Str &key, Str &value);

    bool get_sync(long ikey) {
        quick_istr key(ikey);
        return get_sync(key.string());
    }

    bool get_sync_key16(long ikey) {
        quick_istr key(ikey, 16);
        return get_sync(key.string());
    }

    void get_check(const Str &key, const Str &expected);

    void get_check(const char *key, const char *expected) {
        get_check(Str(key), Str(expected));
    }

    void get_check(long ikey, long iexpected) {
        quick_istr key(ikey), expected(iexpected);
        get_check(key.string(), expected.string());
    }

    void get_check(const Str &key, long iexpected) {
        quick_istr expected(iexpected);
        get_check(key, expected.string());
    }

    void get_check_key8(long ikey, long iexpected) {
        quick_istr key(ikey, 8), expected(iexpected);
        get_check(key.string(), expected.string());
    }

    void get_col_check(const Str &key, int col, const Str &value);

    void get_col_check(long ikey, int col, long ivalue) {
        quick_istr key(ikey), value(ivalue);
        get_col_check(key.string(), col, value.string());
    }

    void get_col_check_key10(long ikey, int col, long ivalue) {
        quick_istr key(ikey, 10), value(ivalue);
        get_col_check(key.string(), col, value.string());
    }

    void many_get_check(int nk, long ikey[], long iexpected[]);
    void scan_sync(const Str &firstkey, int n,
		   std::vector<Str> &keys, std::vector<Str> &values);
    void rscan_sync(const Str &firstkey, int n,
		    std::vector<Str> &keys, std::vector<Str> &values);

    void put(const Str &key, const Str &value);

    void put(const char *key, const char *value) { put(Str(key), Str(value)); }

    void put(long ikey, long ivalue) {
        quick_istr key(ikey), value(ivalue);
        put(key.string(), value.string());
    }

    void put(const Str &key, long ivalue) {
        quick_istr value(ivalue);
        put(key, value.string());
    }

    void put_key8(long ikey, long ivalue) {
        quick_istr key(ikey, 8), value(ivalue);
        put(key.string(), value.string());
    }

    void put_key16(long ikey, long ivalue) {
        quick_istr key(ikey, 16), value(ivalue);
        put(key.string(), value.string());
    }

    void put_col(const Str &key, int col, const Str &value);

    void put_col(long ikey, int col, long ivalue) {
        quick_istr key(ikey), value(ivalue);
        put_col(key.string(), col, value.string());
    }

    void put_col_key10(long ikey, int col, long ivalue) {
        quick_istr key(ikey, 10), value(ivalue);
        put_col(key.string(), col, value.string());
    }

    void remove(const Str &key);

    void remove(long ikey) {
        quick_istr key(ikey);
        remove(key.string());
    }

    void remove_key8(long ikey) {
        quick_istr key(ikey, 8);
        remove(key.string());
    }

    void remove_key16(long ikey) {
        quick_istr key(ikey, 16);
        remove(key.string());
    }

    bool remove_sync(const Str &key);

    bool remove_sync(long ikey) {
        quick_istr key(ikey);
        return remove_sync(key.string());
    }

    void puts_done() {}

    void wait_all() {}

    void rcu_quiesce() {
        uint64_t e = timestamp() >> 16;
        if (e != globalepoch)
            globalepoch = e;
        ti_->rcu_quiesce();
    }

    String make_message(StringAccum &sa) const;
    void notice(const char *fmt, ...);
    void fail(const char *fmt, ...);

    void report(const Json &result) {
        json_.merge(result);
        Json counters;
        for (int i = 0; i < tc_max; ++i)
            if (uint64_t c = ti_->counter(threadcounter(i)))
                counters.set(threadcounter_names[i], c);
        if (counters)
            json_.set("counters", counters);
#if MEMSTATS
        json_.set("treesize", ti_->pstat.tree_mem);
#endif
        if (!quiet)
            fprintf(stderr, "%d: %s\n", ti_->ti_index, json_.unparse().c_str());
    }

    T *table_;
    threadinfo *ti_;
    query<row_type> q_[10];
    kvrandom_lcg_nr rand;
    uint64_t limit_;
    Json json_;
    int ncores_;
    kvout *kvo_;

  private:
    void output_scan(std::vector<Str> &keys, std::vector<Str> &values) const;
};

static volatile int kvtest_printing;

template <typename T> inline void kvtest_print(const T &table, FILE *f, 
        int indent, threadinfo *ti) {
    // only print out the tree from the first failure
    while (!bool_cmpxchg((int *) &kvtest_printing, 0, ti->ti_index + 1))
        /* spin */;
    table.print(f, indent);
}

template <typename T> inline void kvtest_json_stats(T &table, Json &j, threadinfo *ti) {
    table.json_stats(j, ti);
}

template <typename T>
void kvtest_server<T>::get(long ikey) {
    quick_istr key(ikey);
    q_[0].begin_get1(key.string());
    (void) table_->get(q_[0], ti_);
}

template <typename T>
bool kvtest_server<T>::get_sync(const Str &key) {
    q_[0].begin_get1(key);
    return table_->get(q_[0], ti_);
}

template <typename T>
bool kvtest_server<T>::get_sync(const Str &key, Str &value) {
    q_[0].begin_get1(key);
    if (table_->get(q_[0], ti_)) {
	value = q_[0].get1_value();
	return true;
    } else
	return false;
}

template <typename T>
void kvtest_server<T>::get_check(const Str &key, const Str &expected) {
    q_[0].begin_get1(key);
    if (!table_->get(q_[0], ti_))
	fail("get(%.*s) failed (expected %.*s)\n", key.len, key.s,
	     expected.len, expected.s);
    else {
        Str val = q_[0].get1_value();
        if (expected != val)
	    fail("get(%.*s) returned unexpected value %.*s (expected %.*s)\n",
		 key.len, key.s, std::min(val.len, 40), val.s,
		 expected.len, expected.s);
    }
}

template <typename T>
void kvtest_server<T>::get_col_check(const Str &key, int col,
				     const Str &expected) {
    q_[0].begin_get1(key, col);
    if (!table_->get(q_[0], ti_))
	fail("get.%d(%.*s) failed (expected %.*s)\n",
	     col, key.len, key.s, expected.len, expected.s);
    else {
        Str val = q_[0].get1_value();
        if (expected != val)
	    fail("get.%d(%.*s) returned unexpected value %.*s (expected %.*s)\n",
		 col, key.len, key.s, std::min(val.len, 40), val.s,
		 expected.len, expected.s);
    }
}

template <typename T>
void kvtest_server<T>::many_get_check(int nk, long ikey[], long iexpected[]) {
    std::vector<quick_istr> ka(2*nk, quick_istr());
    for(int i = 0; i < nk; i++){
      ka[i].set(ikey[i]);
      ka[i+nk].set(iexpected[i]);
      q_[i].begin_get1(ka[i].string());
    }
    table_->many_get(q_, nk, ti_);
    for(int i = 0; i < nk; i++){
      Str val = q_[i].get1_value();
      if (ka[i+nk] != val){
        printf("get(%ld) returned unexpected value %.*s (expected %ld)\n",
             ikey[i], std::min(val.len, 40), val.s, iexpected[i]);
        exit(1);
      }
    }
}

template <typename T>
void kvtest_server<T>::scan_sync(const Str &firstkey, int n,
				 std::vector<Str> &keys,
				 std::vector<Str> &values) {
    if (!kvo_)
        kvo_ = new_kvout(-1, 2048);
    kvout_reset(kvo_);
    q_[0].begin_scan1(firstkey, n, kvo_);
    table_->scan(q_[0], ti_);
    output_scan(keys, values);
}

template <typename T>
void kvtest_server<T>::rscan_sync(const Str &firstkey, int n,
				  std::vector<Str> &keys,
				  std::vector<Str> &values) {
    if (!kvo_)
        kvo_ = new_kvout(-1, 2048);
    kvout_reset(kvo_);
    q_[0].begin_scan1(firstkey, n, kvo_);
    table_->rscan(q_[0], ti_);
    output_scan(keys, values);
}

template <typename T>
void kvtest_server<T>::output_scan(std::vector<Str> &keys,
				   std::vector<Str> &values) const {
    keys.clear();
    values.clear();
    Str key, value;

    kvin kvi;
    kvin_init(&kvi, kvo_->buf, kvo_->n);
    short nfields;
    while (kvcheck(&kvi, 0)) {
	kvread_str_inplace(&kvi, key);
	KVR(&kvi, nfields);
	assert(nfields == 1);
	kvread_str_inplace(&kvi, value);
	keys.push_back(key);
	values.push_back(value);
    }
}

template <typename T>
void kvtest_server<T>::put(const Str &key, const Str &value) {
    q_[0].begin_replace(key, value);
    table_->replace(q_[0], ti_);
}

template <typename T>
void kvtest_server<T>::put_col(const Str &key, int col, const Str &value) {
#if !KVDB_ROW_TYPE_STR
    if (!kvo_)
	kvo_ = new_kvout(-1, 2048);
    q_[0].begin_put(key, row_type::make_put_col_request(kvo_, col, value));
    table_->put(q_[0], ti_);
#else
    (void) key, (void) col, (void) value;
    assert(0);
#endif
}

template <typename T> inline bool kvtest_remove(kvtest_server<T> &server, const Str &key) {
    server.q_[0].begin_remove(key);
    return server.table_->remove(server.q_[0], server.ti_);
}

template <typename T>
void kvtest_server<T>::remove(const Str &key) {
    (void) kvtest_remove(*this, key);
}

template <typename T>
bool kvtest_server<T>::remove_sync(const Str &key) {
    return kvtest_remove(*this, key);
}

template <typename T>
String kvtest_server<T>::make_message(StringAccum &sa) const {
    const char *begin = sa.begin();
    while (begin != sa.end() && isspace((unsigned char) *begin))
	++begin;
    String s = String(begin, sa.end());
    if (!s.empty() && s.back() != '\n')
	s += '\n';
    return s;
}

template <typename T>
void kvtest_server<T>::notice(const char *fmt, ...) {
    va_list val;
    va_start(val, fmt);
    String m = make_message(StringAccum().vsnprintf(500, fmt, val));
    va_end(val);
    if (m && !quiet)
	fprintf(stderr, "%d: %s", ti_->ti_index, m.c_str());
}

template <typename T>
void kvtest_server<T>::fail(const char *fmt, ...) {
    static spinlock failing_lock = {0};
    static spinlock fail_message_lock = {0};
    static String fail_message;

    va_list val;
    va_start(val, fmt);
    String m = make_message(StringAccum().vsnprintf(500, fmt, val));
    va_end(val);
    if (!m)
	m = "unknown failure";

    acquire(&fail_message_lock);
    if (fail_message != m) {
	fail_message = m;
	fprintf(stderr, "%d: %s", ti_->ti_index, m.c_str());
    }
    release(&fail_message_lock);

    acquire(&failing_lock);
    fprintf(stdout, "%d: %s", ti_->ti_index, m.c_str());
    kvtest_print(*table_, stdout, 0, ti_);

    mandatory_assert(0);
}


static const char *current_test_name;
static int current_trial;
static FILE *test_output_file;
static pthread_mutex_t subtest_mutex;
static pthread_cond_t subtest_cond;

template <typename T>
struct test_thread {
    test_thread(void *arg) {
	server_.set_table(table_, (threadinfo *) arg);
	server_.ti_->enter();
	server_.ti_->rcu_start();
    }
    ~test_thread() {
	server_.ti_->rcu_stop();
    }
    static void *go(void *arg) {

	if (!table_) {
	    table_ = new T;
	    table_->initialize((threadinfo *) arg);
	    //Masstree::default_table::test((threadinfo *) arg);
	    return 0;
	}
        if (!arg) {
	    table_->stats(test_output_file);
            return 0;
        }
#if __linux__
        if (pinthreads) {
            cpu_set_t cs;
            CPU_ZERO(&cs);
            CPU_SET(cores[((threadinfo *)arg)->ti_index], &cs);
	    int r = sched_setaffinity(0, sizeof(cs), &cs);
	    mandatory_assert(r == 0);
        }
#else
        mandatory_assert(!pinthreads && "pinthreads not supported\n");
#endif

	test_thread<T> tt(arg);
	if (fetch_and_add(&active_threads_, 1) == 0)
	    tt.ready_timeouts();
	String test = ::current_test_name;
	int subtestno = 0;
	for (int pos = 0; pos < test.length(); ) {
	    int comma = test.find_left(',', pos);
	    comma = (comma < 0 ? test.length() : comma);
	    String subtest = test.substring(pos, comma - pos), tname;
	    tname = (subtest == test ? subtest : test + String("@") + String(subtestno));
	    tt.server_.reset(tname, ::current_trial);

        // Notify liblat that thread has started (HK)
        tBenchServerThreadStart();

	    tt.run(subtest);
	    if (comma == test.length())
		break;
	    pthread_mutex_lock(&subtest_mutex);
	    if (fetch_and_add(&active_threads_, -1) == 1) {
		pthread_cond_broadcast(&subtest_cond);
		tt.ready_timeouts();
	    } else
		pthread_cond_wait(&subtest_cond, &subtest_mutex);
	    fprintf(test_output_file, "%s\n", tt.server_.json_.unparse().c_str());
	    pthread_mutex_unlock(&subtest_mutex);
	    fetch_and_add(&active_threads_, 1);
	    pos = comma + 1;
	    ++subtestno;
	}
	int at = fetch_and_add(&active_threads_, -1);
	if (at == 1 && print_table)
	    kvtest_print(*table_, stdout, 0, tt.server_.ti_);
	if (at == 1 && json_stats) {
	    Json j;
	    kvtest_json_stats(*table_, j, tt.server_.ti_);
	    if (j) {
		fprintf(stderr, "%s\n", j.unparse(Json::indent_depth(4).tab_width(2).newline_terminator(true)).c_str());
		tt.server_.json_.merge(j);
	    }
	}
	fprintf(test_output_file, "%s\n", tt.server_.json_.unparse().c_str());
	return 0;
    }
    void ready_timeouts() {
	for (size_t i = 0; i < arraysize(timeout); ++i)
	    timeout[i] = false;
	if (!lazy_timer && duration[0])
	    xalarm(duration[0]);
    }
    void run(const String &test) {

        assert(test == "mycsba");

        kvtest_mycsba(server_);

        // if (test == "rw1")
        //     kvtest_rw1(server_);
        // else if (test == "palma")
        //     kvtest_palma(server_);
        // else if (test == "palmb")
        //     kvtest_palmb(server_);
        // else if (test == "rw1fixed")
        //     kvtest_rw1fixed(server_);
        // else if (test == "rw1long")
        //     kvtest_rw1long(server_);
        // else if (test == "rw2")
        //     kvtest_rw2(server_);
        // else if (test == "rw2fixed")
        //     kvtest_rw2fixed(server_);
        // else if (test == "rw2g90")
        //     kvtest_rw2g90(server_);
        // else if (test == "rw2fixedg90")
        //     kvtest_rw2fixedg90(server_);
        // else if (test == "rw2g98")
        //     kvtest_rw2g98(server_);
        // else if (test == "rw2fixedg98")
        //     kvtest_rw2fixedg98(server_);
        // else if (test == "rw3")
        //     kvtest_rw3(server_);
        // else if (test == "rw4")
        //     kvtest_rw4(server_);
        // else if (test == "rw4fixed")
        //     kvtest_rw4fixed(server_);
        // else if (test == "wd1")
        //     kvtest_wd1(10000000, 1, server_);
        // else if (test == "wd1m1")
        //     kvtest_wd1(100000000, 1, server_);
        // else if (test == "wd1m2")
        //     kvtest_wd1(1000000000, 4, server_);
        // else if (test == "same")
        //     kvtest_same(server_);
        // else if (test == "rwsmall24")
        //     kvtest_rwsmall24(server_);
        // else if (test == "rwsep24")
        //     kvtest_rwsep24(server_);
        // else if (test == "wscale")
        //     kvtest_wscale(server_);
        // else if (test == "ruscale_init")
        //     kvtest_ruscale_init(server_);
        // else if (test == "rscale") {
        //     if (server_.ti_->ti_index < ::rscale_ncores)
        //         kvtest_rscale(server_);
        // } else if (test == "uscale")
        //     kvtest_uscale(server_);
        // else if (test == "bdb")
        //     kvtest_bdb(server_);
        // else if (test == "wcol1")
        //     kvtest_wcol1(server_, 31949 + server_.id() % 48, 5000000);
        // else if (test == "rcol1")
        //     kvtest_rcol1(server_, 31949 + server_.id() % 48, 5000000);
        // else if (test == "scan1")
        //     kvtest_scan1(server_, 0);
        // else if (test == "rscan1")
        //     kvtest_rscan1(server_, 0);
        // else if (test == "scan1q80")
        //     kvtest_scan1(server_, 0.8);
        // else if (test == "rscan1q80")
        //     kvtest_rscan1(server_, 0.8);
        // else if (test == "splitremove1")
        //     kvtest_splitremove1(server_);
        // else if (test == "ycsbk")
        //     kvtest_ycsbk(server_);
        // else if (test == "mycsba")
        //     kvtest_mycsba(server_);
        // else
        //     server_.fail("unknown test %s", test.c_str());
    }
    static T *table_;
    static unsigned active_threads_;
    kvtest_server<T> server_;
};
template <typename T> T *test_thread<T>::table_;
template <typename T> unsigned test_thread<T>::active_threads_;

static struct {
    const char *treetype;
    void *(*func)(void *);
} test_thread_map[] = {
    { "masstree", test_thread<Masstree::default_table>::go },
    { "mass", test_thread<Masstree::default_table>::go },
    { "mbtree", test_thread<Masstree::default_table>::go },
    { "mb", test_thread<Masstree::default_table>::go },
    { "m", test_thread<Masstree::default_table>::go }
};


void runtest(int nthreads, void *(*func)(void *)) {
    std::vector<threadinfo *> tis;
    for (int i = 0; i < nthreads; ++i)
	tis.push_back(threadinfo::make(threadinfo::TI_PROCESS, i));
    signal(SIGALRM, test_timeout);
    for (int i = 0; i < nthreads; ++i) {
	int r = pthread_create(&tis[i]->ti_threadid, 0, func, tis[i]);
	mandatory_assert(r == 0);
    }
    for (int i = 0; i < nthreads; ++i)
	pthread_join(tis[i]->ti_threadid, 0);
}


static const char * const kvstats_name[] = {
    "ops_per_sec", "puts_per_sec", "gets_per_sec", "scans_per_sec"
};

static Json experiment_stats;

void *stat_collector(void *arg) {
    int p = (int) (intptr_t) arg;
    FILE *f = fdopen(p, "r");
    char buf[8192];
    while (fgets(buf, sizeof(buf), f)) {
	Json result = Json::parse(buf);
	if (result && result["table"] && result["test"]) {
	    String key = result["test"].to_s() + "/" + result["table"].to_s()
		+ "/" + result["trial"].to_s();
	    Json &thisex = experiment_stats.get_insert(key);
	    thisex[result["thread"].to_i()] = result;
	} else
            fprintf(stderr, "%s\n", buf);
    }
    fclose(f);
    return 0;
}


/* main loop */

enum { clp_val_normalize = Clp_ValFirstUser, clp_val_suffixdouble };
enum { opt_pin = 1, opt_port, opt_duration,
       opt_test, opt_test_name, opt_threads, opt_trials, opt_quiet, opt_print,
       opt_normalize, opt_limit, opt_notebook, opt_compare, opt_no_run,
       opt_lazy_timer, opt_gid, opt_tree_stats, opt_rscale_ncores, opt_cores,
       opt_stats };
static const Clp_Option options[] = {
    { "pin", 'p', opt_pin, 0, Clp_Negate },
    { "port", 0, opt_port, Clp_ValInt, 0 },
    { "duration", 'd', opt_duration, Clp_ValDouble, 0 },
    { "lazy-timer", 0, opt_lazy_timer, 0, 0 },
    { "limit", 'l', opt_limit, clp_val_suffixdouble, 0 },
    { "normalize", 0, opt_normalize, clp_val_normalize, Clp_Negate },
    { "test", 0, opt_test, Clp_ValString, 0 },
    { "rscale_ncores", 'r', opt_rscale_ncores, Clp_ValInt, 0 },
    { "test-rw1", 0, opt_test_name, 0, 0 },
    { "test-rw2", 0, opt_test_name, 0, 0 },
    { "test-rw3", 0, opt_test_name, 0, 0 },
    { "test-rw4", 0, opt_test_name, 0, 0 },
    { "test-rd1", 0, opt_test_name, 0, 0 },
    { "threads", 'j', opt_threads, Clp_ValInt, 0 },
    { "trials", 'T', opt_trials, Clp_ValInt, 0 },
    { "quiet", 'q', opt_quiet, 0, Clp_Negate },
    { "print", 0, opt_print, 0, Clp_Negate },
    { "notebook", 'b', opt_notebook, Clp_ValString, Clp_Negate },
    { "gid", 'g', opt_gid, Clp_ValString, 0 },
    { "tree-stats", 0, opt_tree_stats, 0, 0 },
    { "stats", 0, opt_stats, 0, 0 },
    { "compare", 'c', opt_compare, Clp_ValString, 0 },
    { "cores", 0, opt_cores, Clp_ValString, 0 },
    { "no-run", 0, opt_no_run, 0, 0 }
};

static void run_one_test(int trial, const char *treetype, const char *test,
			 const int *collectorpipe, int nruns);
enum { normtype_none, normtype_pertest, normtype_firsttest };
static void print_gnuplot(FILE *f, const char * const *types_begin, const char * const *types_end, const std::vector<String> &comparisons, int normalizetype);
static void update_labnotebook(String notebook);

int
main(int argc, char *argv[])
{
    threadcounter_names[(int) tc_root_retry] = "root_retry";
    threadcounter_names[(int) tc_internode_retry] = "internode_retry";
    threadcounter_names[(int) tc_leaf_retry] = "leaf_retry";
    threadcounter_names[(int) tc_leaf_walk] = "leaf_walk";
    threadcounter_names[(int) tc_stable_internode_insert] = "stable_internode_insert";
    threadcounter_names[(int) tc_stable_internode_split] = "stable_internode_split";
    threadcounter_names[(int) tc_stable_leaf_insert] = "stable_leaf_insert";
    threadcounter_names[(int) tc_stable_leaf_split] = "stable_leaf_split";
    threadcounter_names[(int) tc_internode_lock] = "internode_lock_retry";
    threadcounter_names[(int) tc_leaf_lock] = "leaf_lock_retry";

    int ret, ntrials = 1, normtype = normtype_pertest, firstcore = -1, corestride = 1;
    std::vector<const char *> tests, treetypes;
    std::vector<String> comparisons;
    const char *notebook = "notebook-kvtest.json";
    tcpthreads = udpthreads = sysconf(_SC_NPROCESSORS_ONLN);

    Clp_Parser *clp = Clp_NewParser(argc, argv, (int) arraysize(options), options);
    Clp_AddStringListType(clp, clp_val_normalize, 0,
			  "none", (int) normtype_none,
			  "pertest", (int) normtype_pertest,
			  "test", (int) normtype_pertest,
			  "firsttest", (int) normtype_firsttest,
			  (const char *) 0);
    Clp_AddType(clp, clp_val_suffixdouble, Clp_DisallowOptions, clp_parse_suffixdouble, 0);
    int opt;
    while ((opt = Clp_Next(clp)) != Clp_Done) {
	switch (opt) {
	case opt_pin:
	    pinthreads = !clp->negated;
	    break;
	case opt_threads:
	    tcpthreads = udpthreads = clp->val.i;
	    break;
	case opt_trials:
	    ntrials = clp->val.i;
	    break;
	case opt_quiet:
	    quiet = !clp->negated;
	    break;
	case opt_print:
	    print_table = !clp->negated;
	    break;
        case opt_rscale_ncores:
            rscale_ncores = clp->val.i;
            break;
	case opt_port:
	    port = clp->val.i;
	    break;
	case opt_duration:
	    duration[0] = clp->val.d;
	    break;
        case opt_lazy_timer:
            lazy_timer = true;
            break;
	case opt_limit:
	    test_limit = uint64_t(clp->val.d);
	    break;
	case opt_test:
	    tests.push_back(clp->vstr);
	    break;
	case opt_test_name:
	    tests.push_back(clp->option->long_name + 5);
	    break;
	case opt_normalize:
	    normtype = clp->negated ? normtype_none : clp->val.i;
	    break;
        case opt_gid:
            gid = clp->vstr;
            break;
        case opt_tree_stats:
            tree_stats = true;
            break;
        case opt_stats:
            json_stats = true;
            break;
	case opt_notebook:
	    if (clp->negated)
		notebook = 0;
	    else if (clp->have_val)
		notebook = clp->vstr;
	    else
		notebook = "notebook-kvtest.json";
	    break;
	case opt_compare:
	    comparisons.push_back(clp->vstr);
	    break;
	case opt_no_run:
	    ntrials = 0;
	    break;
      case opt_cores:
	  if (firstcore >= 0 || cores.size() > 0) {
	      Clp_OptionError(clp, "%<%O%> already given");
	      exit(EXIT_FAILURE);
	  } else {
	      const char *plus = strchr(clp->vstr, '+');
	      Json ij = Json::parse(clp->vstr),
		  aj = Json::parse(String("[") + String(clp->vstr) + String("]")),
		  pj1 = Json::parse(plus ? String(clp->vstr, plus) : "x"),
		  pj2 = Json::parse(plus ? String(plus + 1) : "x");
	      for (int i = 0; aj && i < aj.size(); ++i)
		  if (!aj[i].is_int() || aj[i].to_i() < 0)
		      aj = Json();
	      if (ij && ij.is_int() && ij.to_i() >= 0)
		  firstcore = ij.to_i(), corestride = 1;
	      else if (pj1 && pj2 && pj1.is_int() && pj1.to_i() >= 0 && pj2.is_int())
		  firstcore = pj1.to_i(), corestride = pj2.to_i();
	      else if (aj) {
		  for (int i = 0; i < aj.size(); ++i)
		      cores.push_back(aj[i].to_i());
	      } else {
		  Clp_OptionError(clp, "bad %<%O%>, expected %<CORE1%>, %<CORE1+STRIDE%>, or %<CORE1,CORE2,...%>");
		  exit(EXIT_FAILURE);
	      }
	  }
	  break;
	case Clp_NotOption: {
	    bool is_treetype = false;
	    for (int i = 0; i < (int) arraysize(test_thread_map) && !is_treetype; ++i)
		is_treetype = (strcmp(test_thread_map[i].treetype, clp->vstr) == 0);
	    (is_treetype ? treetypes.push_back(clp->vstr) : tests.push_back(clp->vstr));
	    break;
	}
	default:
	    fprintf(stderr, "Usage: kvtest [-n] [-jN] TREETYPE TESTNAME\n");
	    exit(EXIT_FAILURE);
	}
    }
    Clp_DeleteParser(clp);
    if (firstcore < 0)
        firstcore = cores.size() ? cores.back() + 1 : 0;
    for (; (int) cores.size() < udpthreads; firstcore += corestride)
        cores.push_back(firstcore);

#if PMC_ENABLED
    mandatory_assert(pinthreads && "Using performance counter requires pinning threads to cores!");
#endif
#if MEMSTATS && HAVE_NUMA_H && HAVE_LIBNUMA
    if (numa_available() != -1) {
	mandatory_assert(numa_max_node() <= MaxNumaNode);
	for (int i = 0; i <= numa_max_node(); i++)
	    numa[i].size = numa_node_size64(i, &numa[i].free);
    }
#endif

    // Initialize liblat (HK)
    tBenchServerInit(tcpthreads);

    if (treetypes.empty())
	treetypes.push_back("m");
    if (tests.empty())
	tests.push_back("rw1");

    // arrange for a per-thread threadinfo pointer
    ret = pthread_key_create(&threadinfo::key, 0);
    mandatory_assert(ret == 0);
    pthread_mutex_init(&subtest_mutex, 0);
    pthread_cond_init(&subtest_cond, 0);

    // pipe for them to write back to us
    int p[2];
    ret = pipe(p);
    mandatory_assert(ret == 0);
    test_output_file = fdopen(p[1], "w");

    pthread_t collector;
    ret = pthread_create(&collector, 0, stat_collector, (void *) (intptr_t) p[0]);
    mandatory_assert(ret == 0);
    initial_timestamp = timestamp();

    // run tests
    int nruns = ntrials * (int) tests.size() * (int) treetypes.size();
    std::vector<int> runlist(nruns, 0);
    for (int i = 0; i < nruns; ++i)
	runlist[i] = i;

    for (int counter = 0; counter < nruns; ++counter) {
	int x = random() % runlist.size();
	int run = runlist[x];
	runlist[x] = runlist.back();
	runlist.pop_back();

	int trial = run % ntrials;
	run /= ntrials;
	int t = run % tests.size();
	run /= tests.size();
	int tt = run;

	fprintf(stderr, "%d/%u %s/%s%s", counter + 1, (int) (ntrials * tests.size() * treetypes.size()),
		tests[t], treetypes[tt], quiet ? "      " : "\n");

	run_one_test(trial, treetypes[tt], tests[t], p, nruns);
	struct timeval delay;
	delay.tv_sec = 0;
	delay.tv_usec = 250000;
	(void) select(0, 0, 0, 0, &delay);

	if (quiet)
	    fprintf(stderr, "\r%60s\r", "");
    }

    fclose(test_output_file);
    pthread_join(collector, 0);

    tBenchServerFinish();

    // update lab notebook
    if (notebook)
	update_labnotebook(notebook);

    // print Gnuplot
    if (ntrials != 0)
	comparisons.insert(comparisons.begin(), "");
    print_gnuplot(stdout, kvstats_name, kvstats_name + arraysize(kvstats_name),
		  comparisons, normtype);

    exit(0);
}

static void run_one_test_body(int trial, const char *treetype, const char *test) {
    threadinfo *main_ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    main_ti->enter();
    globalepoch = timestamp() >> 16;
    for (int i = 0; i < (int) arraysize(test_thread_map); ++i)
	if (strcmp(test_thread_map[i].treetype, treetype) == 0) {
	    current_test_name = test;
	    current_trial = trial;
	    test_thread_map[i].func(main_ti); // initialize table
	    runtest(tcpthreads, test_thread_map[i].func);
            if (tree_stats)
                test_thread_map[i].func(0); // print tree_stats
	    break;
	}
}

static void run_one_test(int trial, const char *treetype, const char *test,
			 const int *collectorpipe, int nruns) {
    if (nruns == 1)
	run_one_test_body(trial, treetype, test);
    else {
	pid_t c = fork();
	if (c == 0) {
	    close(collectorpipe[0]);
	    run_one_test_body(trial, treetype, test);
	    exit(0);
	} else
	    while (waitpid(c, 0, 0) == -1 && errno == EINTR)
		/* loop */;
    }
}

static double level(const std::vector<double> &v, double frac) {
    frac *= v.size() - 1;
    int base = (int) frac;
    if (base == frac)
	return v[base];
    else
	return v[base] * (1 - (frac - base)) + v[base + 1] * (frac - base);
}

static String experiment_test_table_trial(const String &key) {
    const char *l = key.begin(), *r = key.end();
    if (l + 2 < r && l[0] == 'x' && isdigit((unsigned char) l[1])) {
	for (const char *s = l; s != r; ++s)
	    if (*s == '/') {
		l = s + 1;
		break;
	    }
    }
    return key.substring(l, r);
}

static String experiment_run_test_table(const String &key) {
    const char *l = key.begin(), *r = key.end();
    for (const char *s = r; s != l; --s)
	if (s[-1] == '/') {
	    r = s - 1;
	    break;
	} else if (!isdigit((unsigned char) s[-1]))
	    break;
    return key.substring(l, r);
}

static String experiment_test_table(const String &key) {
    return experiment_run_test_table(experiment_test_table_trial(key));
}

static bool experiment_match(String key, String match) {
    bool key_isx = (key.length() >= 2 && key[0] == 'x' && isdigit((unsigned char) key[1]));
    if (!match)
	return !key_isx;
    bool match_isx = (match.length() >= 2 && match[0] == 'x' && isdigit((unsigned char) match[1]));
    if (match_isx && match.find_left('/') < 0)
	match += "/";
    if (!match_isx && key_isx)
	key = experiment_test_table_trial(key);
    return key.substring(0, match.length()) == match;
}

namespace {
struct gnuplot_info {
    static constexpr double trialdelta = 0.015, treetypedelta = 0.04,
	testdelta = 0.08, typedelta = 0.2;
    double pos;
    double nextdelta;
    double normalization;
    String last_test;
    int normalizetype;

    std::vector<StringAccum> candlesticks;
    std::vector<StringAccum> medians;
    StringAccum xtics;

    gnuplot_info(int nt)
	: pos(1 - trialdelta), nextdelta(trialdelta), normalization(-1),
	  normalizetype(nt) {
    }
    void one(const String &xname, int ti, const String &datatype_name);
    void print(FILE *f, const char * const *types_begin);
};
constexpr double gnuplot_info::trialdelta, gnuplot_info::treetypedelta, gnuplot_info::testdelta, gnuplot_info::typedelta;

void gnuplot_info::one(const String &xname, int ti, const String &datatype_name)
{
    String current_test = experiment_test_table(xname);
    if (current_test != last_test) {
	last_test = current_test;
	if (normalizetype == normtype_pertest)
	    normalization = -1;
	if (nextdelta == treetypedelta)
	    nextdelta = testdelta;
    }
    double beginpos = pos, firstpos = pos + nextdelta;

    std::vector<int> trials;
    for (Json::object_iterator it = experiment_stats.obegin();
	 it != experiment_stats.oend(); ++it)
	if (experiment_run_test_table(it.key()) == xname)
	    trials.push_back(strtol(it.key().begin() + xname.length() + 1, 0, 0));
    std::sort(trials.begin(), trials.end());

    for (std::vector<int>::iterator tit = trials.begin();
	 tit != trials.end(); ++tit) {
	Json &this_trial = experiment_stats[xname + "/" + String(*tit)];
	std::vector<double> values;
	for (int jn = 0; jn < this_trial.size(); ++jn)
	    if (this_trial[jn].get(datatype_name).is_number())
		values.push_back(this_trial[jn].get(datatype_name).to_d());
	if (values.size()) {
	    pos += nextdelta;
	    std::sort(values.begin(), values.end());
	    if (normalization < 0)
		normalization = normalizetype == normtype_none ? 1 : level(values, 0.5);
	    if (int(candlesticks.size()) <= ti) {
		candlesticks.resize(ti + 1);
		medians.resize(ti + 1);
	    }
	    candlesticks[ti] << pos << " " << level(values, 0)
			     << " " << level(values, 0.25)
			     << " " << level(values, 0.75)
			     << " " << level(values, 1)
			     << " " << normalization << "\n";
	    medians[ti] << pos << " " << level(values, 0.5) << " " << normalization << "\n";
	    nextdelta = trialdelta;
	}
    }

    if (pos > beginpos) {
	double middle = (firstpos + pos) / 2;
	xtics << (xtics ? ", " : "") << "\"" << xname << "\" " << middle;
	nextdelta = treetypedelta;
    }
}

void gnuplot_info::print(FILE *f, const char * const *types_begin) {
    std::vector<int> linetypes(medians.size(), 0);
    int next_linetype = 1;
    for (int i = 0; i < int(medians.size()); ++i)
	if (medians[i])
	    linetypes[i] = next_linetype++;
    struct utsname name;
    fprintf(f, "set title \"%s (%d cores)\"\n",
	    (uname(&name) == 0 ? name.nodename : "unknown"),
	    udpthreads);
    fprintf(f, "set terminal png\n");
    fprintf(f, "set xrange [%g:%g]\n", 1 - treetypedelta, pos + treetypedelta);
    fprintf(f, "set xtics rotate (%s)\n", xtics.c_str());
    fprintf(f, "set key top left Left reverse\n");
    if (normalizetype == normtype_none)
	fprintf(f, "set ylabel \"count\"\n");
    else if (normalizetype == normtype_pertest)
	fprintf(f, "set ylabel \"count, normalized per test\"\n");
    else
	fprintf(f, "set ylabel \"normalized count (1=%f)\"\n", normalization);
    const char *sep = "plot ";
    for (int i = 0; i < int(medians.size()); ++i)
	if (medians[i]) {
	    fprintf(f, "%s '-' using 1:($3/$6):($2/$6):($5/$6):($4/$6) with candlesticks lt %d title '%s', \\\n",
		    sep, linetypes[i], types_begin[i]);
	    fprintf(f, " '-' using 1:($2/$3):($2/$3):($2/$3):($2/$3) with candlesticks lt %d notitle", linetypes[i]);
	    sep = ", \\\n";
	}
    fprintf(f, "\n");
    for (int i = 0; i < int(medians.size()); ++i)
	if (medians[i]) {
	    fwrite(candlesticks[i].begin(), 1, candlesticks[i].length(), f);
	    fprintf(f, "e\n");
	    fwrite(medians[i].begin(), 1, medians[i].length(), f);
	    fprintf(f, "e\n");
	}
}

}

static void print_gnuplot(FILE *f, const char * const *types_begin, const char * const *types_end,
			  const std::vector<String> &comparisons, int normalizetype) {
    std::vector<String> all_versions, all_experiments;
    for (Json::object_iterator it = experiment_stats.obegin();
	 it != experiment_stats.oend(); ++it)
	for (std::vector<String>::const_iterator cit = comparisons.begin();
	     cit != comparisons.end(); ++cit)
	    if (experiment_match(it.key(), *cit)) {
		all_experiments.push_back(experiment_run_test_table(it.key()));
		all_versions.push_back(experiment_test_table(it.key()));
		break;
	    }
    std::sort(all_experiments.begin(), all_experiments.end());
    all_experiments.erase(std::unique(all_experiments.begin(), all_experiments.end()),
			  all_experiments.end());
    std::sort(all_versions.begin(), all_versions.end());
    all_versions.erase(std::unique(all_versions.begin(), all_versions.end()),
		       all_versions.end());

    int ntypes = (int) (types_end - types_begin);
    gnuplot_info gpinfo(normalizetype);

    for (int ti = 0; ti < ntypes; ++ti) {
	double typepos = gpinfo.pos;
	for (std::vector<String>::iterator vit = all_versions.begin();
	     vit != all_versions.end(); ++vit) {
	    for (std::vector<String>::iterator xit = all_experiments.begin();
		 xit != all_experiments.end(); ++xit)
		if (experiment_test_table(*xit) == *vit)
		    gpinfo.one(*xit, ti, types_begin[ti]);
	}
	if (gpinfo.pos > typepos)
	    gpinfo.nextdelta = gpinfo.typedelta;
	gpinfo.last_test = "";
    }

    if (gpinfo.xtics)
	gpinfo.print(f, types_begin);
}

static String
read_file(FILE *f, const char *name)
{
    StringAccum sa;
    while (1) {
	size_t x = fread(sa.reserve(4096), 1, 4096, f);
	if (x != 0)
	    sa.adjust_length(x);
	else if (ferror(f)) {
	    fprintf(stderr, "%s: %s\n", name, strerror(errno));
	    return String::make_stable("???", 3);
	} else
	    return sa.take_string();
    }
}

static void
update_labnotebook(String notebook)
{
    FILE *f = (notebook == "-" ? stdin : fopen(notebook.c_str(), "r"));
    String previous_text = (f ? read_file(f, notebook.c_str()) : String());
    if (previous_text.out_of_memory())
	return;
    if (f && f != stdin)
	fclose(f);

    Json nb = Json::parse(previous_text);
    if (previous_text && (!nb.is_object() || !nb["experiments"])) {
	fprintf(stderr, "%s: unexpected contents, not writing new data\n", notebook.c_str());
	return;
    }

    if (!nb)
	nb = Json::make_object();
    if (!nb.get("experiments"))
	nb.set("experiments", Json::make_object());
    if (!nb.get("data"))
	nb.set("data", Json::make_object());

    Json old_data = nb["data"];
    if (!experiment_stats) {
	experiment_stats = old_data;
	return;
    }

    Json xjson;

    FILE *git_info_p = popen("git rev-parse HEAD | tr -d '\n'; git --no-pager diff --exit-code --shortstat HEAD >/dev/null 2>&1 || echo M", "r");
    String git_info = read_file(git_info_p, "<git output>");
    pclose(git_info_p);
    if (git_info)
	xjson.set("git-revision", git_info.trim());

    time_t now = time(0);
    xjson.set("time", String(ctime(&now)).trim());
    if (gid)
        xjson.set("gid", String(gid));

    struct utsname name;
    if (uname(&name) == 0)
	xjson.set("machine", name.nodename);

    xjson.set("cores", udpthreads);

    Json &runs = xjson.get_insert("runs");
    String xname = "x" + String(nb["experiments"].size());
    for (Json::const_iterator it = experiment_stats.begin();
	 it != experiment_stats.end(); ++it) {
	String xkey = xname + "/" + it.key();
	runs.push_back(xkey);
	nb["data"][xkey] = it.value();
    }
    xjson.set("runs", runs);

    nb["experiments"][xname] = xjson;

    String new_text = nb.unparse(Json::indent_depth(4).tab_width(2).newline_terminator(true));
    f = (notebook == "-" ? stdout : fopen((notebook + "~").c_str(), "w"));
    if (!f) {
	fprintf(stderr, "%s~: %s\n", notebook.c_str(), strerror(errno));
	return;
    }
    size_t written = fwrite(new_text.data(), 1, new_text.length(), f);
    if (written != size_t(new_text.length())) {
	fprintf(stderr, "%s~: %s\n", notebook.c_str(), strerror(errno));
	fclose(f);
	return;
    }
    if (f != stdout) {
	fclose(f);
	if (rename((notebook + "~").c_str(), notebook.c_str()) != 0)
	    fprintf(stderr, "%s: %s\n", notebook.c_str(), strerror(errno));
    }

    fprintf(stderr, "EXPERIMENT %s\n", xname.c_str());
    experiment_stats.merge(old_data);
}

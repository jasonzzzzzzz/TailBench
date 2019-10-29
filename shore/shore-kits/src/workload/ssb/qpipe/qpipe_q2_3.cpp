/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-kits -- Benchmark implementations for Shore-MT
   
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

/** @file:   qpipe_2_3.cpp
 *
 *  @brief:  Implementation of QPIPE SSB Q2_3 over Shore-MT
 *
 *  @author: Xuedong Jin
 *  @date:   November 2011
 */

#include "workload/ssb/shore_ssb_env.h"
#include "qpipe.h"

using namespace shore;
using namespace qpipe;



ENTER_NAMESPACE(ssb);


/******************************************************************** 
 *
 * QPIPE Q2_3 - Structures needed by operators 
 *
 ********************************************************************/
/*
select sum(lo_revenue), d_year, p_brand
from lineorder, [date], part, supplier
where lo_orderdatekey =  d_datekey
and lo_partkey = p_partkey
and lo_suppkey = s_suppkey
and p_brand= 'MFGR#2239'
and s_region = 'EUROPE'
group by d_year, p_brand
order by d_year, p_brand;
*/

// the tuples after tablescan projection
struct q23_lo_tuple
{
  int LO_PARTKEY;
  int LO_SUPPKEY;
  int LO_ORDERDATE;
  int LO_REVENUE;    
};

struct q23_p_tuple
{
  int P_PARTKEY;
  char P_BRAND[10];
};

struct q23_s_tuple
{
  int S_SUPPKEY;
};

struct q23_d_tuple
{ 
  int D_DATEKEY;
  int D_YEAR;
};

struct q23_join_s_tuple
{
  int LO_ORDERDATE;
  int LO_PARTKEY;
  int LO_REVENUE;
};

struct q23_join_s_p_tuple
{
  int LO_ORDERDATE;
  char P_BRAND[10];
  int LO_REVENUE;
};


struct q23_join_tuple
{
  char P_BRAND[10];
  int D_YEAR;
  int LO_REVENUE;
};

struct q23_aggregate_tuple
{
  char P_BRAND[10];
  int D_YEAR;
  int REVENUE;
};

/*struct projected_tuple
{
  int KEY;
  };*/
typedef struct q23_aggregate_tuple projected_tuple;


class q23_lineorder_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    table_row_t* _prline;
    rep_row_t _rr;

    ssb_lineorder_tuple _lineorder;

public:

    q23_lineorder_tscan_filter_t(ShoreSSBEnv* ssbdb)//,q2_3_input_t &in) 
        : tuple_filter_t(ssbdb->lineorder_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a lineorder tupple from the tuple cache and allocate space
        _prline = _ssbdb->lineorder_man()->get_tuple();
        _rr.set_ts(_ssbdb->lineorder_man()->ts(),
                   _ssbdb->lineorder_desc()->maxsize());
        _prline->_rep = &_rr;

    }

    ~q23_lineorder_tscan_filter_t()
    {
        // Give back the lineorder tuple 
        _ssbdb->lineorder_man()->give_tuple(_prline);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next lineorder and read its shipdate
        if (!_ssbdb->lineorder_man()->load(_prline, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        return (true);
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        q23_lo_tuple *dest;
        dest = aligned_cast<q23_lo_tuple>(d.data);

        _prline->get_value(3, _lineorder.LO_PARTKEY);
        _prline->get_value(4, _lineorder.LO_SUPPKEY);
        _prline->get_value(5, _lineorder.LO_ORDERDATE);
        _prline->get_value(12, _lineorder.LO_REVENUE);


        TRACE( TRACE_RECORD_FLOW, "%d|%d|%d|%d --d\n",
               _lineorder.LO_PARTKEY,
               _lineorder.LO_SUPPKEY,
               _lineorder.LO_ORDERDATE,
               _lineorder.LO_REVENUE);

        dest->LO_PARTKEY = _lineorder.LO_PARTKEY;
        dest->LO_SUPPKEY = _lineorder.LO_SUPPKEY;
        dest->LO_ORDERDATE = _lineorder.LO_ORDERDATE;
        dest->LO_REVENUE = _lineorder.LO_REVENUE;

    }

    q23_lineorder_tscan_filter_t* clone() const {
        return new q23_lineorder_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("q23_lineorder_tscan_filter_t()");
    }
};


class q23_part_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    table_row_t* _prpart;
    rep_row_t _rr;

    ssb_part_tuple _part;

  /*VARIABLES TAKING VALUES FROM INPUT FOR SELECTION*/
    char BRAND[10];

public:

    q23_part_tscan_filter_t(ShoreSSBEnv* ssbdb, q2_3_input_t &in) 
        : tuple_filter_t(ssbdb->part_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a part tuple from the tuple cache and allocate space
        _prpart = _ssbdb->part_man()->get_tuple();
        _rr.set_ts(_ssbdb->part_man()->ts(),
                   _ssbdb->part_desc()->maxsize());
        _prpart->_rep = &_rr;

	
	strcpy(BRAND,"MFGR#2239");
    }

    ~q23_part_tscan_filter_t()
    {
        // Give back the part tuple 
        _ssbdb->part_man()->give_tuple(_prpart);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next part and read its shipdate
        if (!_ssbdb->part_man()->load(_prpart, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        _prpart->get_value(4, _part.P_BRAND, STRSIZE(9));

	if (strcmp(_part.P_BRAND,BRAND)==0)
	    {
		TRACE( TRACE_RECORD_FLOW, "+ BRAND |%s --d\n",
		       _part.P_BRAND);
		return (true);
	    }
	else
	    {
		TRACE( TRACE_RECORD_FLOW, ". BRAND |%s --d\n",
		       _part.P_BRAND);
		return (false);
	    }
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        q23_p_tuple *dest;
        dest = aligned_cast<q23_p_tuple>(d.data);

        _prpart->get_value(0, _part.P_PARTKEY);
        _prpart->get_value(4, _part.P_BRAND, STRSIZE(9));

        TRACE( TRACE_RECORD_FLOW, "%d|%s --d\n",
               _part.P_PARTKEY,
               _part.P_BRAND);


        dest->P_PARTKEY = _part.P_PARTKEY;
        strcpy(dest->P_BRAND,_part.P_BRAND);
    }

    q23_part_tscan_filter_t* clone() const {
        return new q23_part_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("q23_part_tscan_filter_t()");
    }
};


class q23_supplier_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    table_row_t* _prsupp;
    rep_row_t _rr;

    ssb_supplier_tuple _supplier;

  /*VARIABLES TAKING VALUES FROM INPUT FOR SELECTION*/
    char REGION[13];
public:

    q23_supplier_tscan_filter_t(ShoreSSBEnv* ssbdb, q2_3_input_t &in) 
        : tuple_filter_t(ssbdb->supplier_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a supplier tupple from the tuple cache and allocate space
        _prsupp = _ssbdb->supplier_man()->get_tuple();
        _rr.set_ts(_ssbdb->supplier_man()->ts(),
                   _ssbdb->supplier_desc()->maxsize());
        _prsupp->_rep = &_rr;

	
	strcpy(REGION,"EUROPE");
    }

    ~q23_supplier_tscan_filter_t()
    {
        // Give back the supplier tuple 
        _ssbdb->supplier_man()->give_tuple(_prsupp);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next supplier and read its shipdate
        if (!_ssbdb->supplier_man()->load(_prsupp, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

        _prsupp->get_value(5, _supplier.S_REGION, STRSIZE(12));


	if (strcmp(_supplier.S_REGION,REGION)==0)
	    {
		TRACE( TRACE_RECORD_FLOW, "+ REGION |%s --d\n",
		       _supplier.S_REGION);
		return (true);
	    }
	else
	    {
		TRACE( TRACE_RECORD_FLOW, ". REGION |%s --d\n",
		       _supplier.S_REGION);
		return (false);
	    }
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        q23_s_tuple *dest;
        dest = aligned_cast<q23_s_tuple>(d.data);

        _prsupp->get_value(0, _supplier.S_SUPPKEY);

        TRACE( TRACE_RECORD_FLOW, "%d --d\n",
               _supplier.S_SUPPKEY);


        dest->S_SUPPKEY = _supplier.S_SUPPKEY;
    }

    q23_supplier_tscan_filter_t* clone() const {
        return new q23_supplier_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("q23_supplier_tscan_filter_t()");
    }
};



class q23_date_tscan_filter_t : public tuple_filter_t 
{
private:
    ShoreSSBEnv* _ssbdb;
    table_row_t* _prdate;
    rep_row_t _rr;

    ssb_date_tuple _date;


public:

    q23_date_tscan_filter_t(ShoreSSBEnv* ssbdb, q2_3_input_t &in) 
        : tuple_filter_t(ssbdb->date_desc()->maxsize()), _ssbdb(ssbdb)
    {

    	// Get a date tupple from the tuple cache and allocate space
        _prdate = _ssbdb->date_man()->get_tuple();
        _rr.set_ts(_ssbdb->date_man()->ts(),
                   _ssbdb->date_desc()->maxsize());
        _prdate->_rep = &_rr;

    }

    ~q23_date_tscan_filter_t()
    {
        // Give back the date tuple 
        _ssbdb->date_man()->give_tuple(_prdate);
    }


    // Predication
    bool select(const tuple_t &input) {

        // Get next date and read its shipdate
        if (!_ssbdb->date_man()->load(_prdate, input.data)) {
            assert(false); // RC(se_WRONG_DISK_DATA)
        }

		return (true);
    }

    
    // Projection
    void project(tuple_t &d, const tuple_t &s) {        

        q23_d_tuple *dest;
        dest = aligned_cast<q23_d_tuple>(d.data);

        _prdate->get_value(0, _date.D_DATEKEY);
        _prdate->get_value(4, _date.D_YEAR);

        TRACE( TRACE_RECORD_FLOW, "%d|%d --d\n",
               _date.D_DATEKEY,
               _date.D_YEAR);


        dest->D_DATEKEY = _date.D_DATEKEY;
        dest->D_YEAR=_date.D_YEAR;
    }

    q23_date_tscan_filter_t* clone() const {
        return new q23_date_tscan_filter_t(*this);
    }

    c_str to_string() const {
        return c_str("q2_3_date_tscan_filter_t()");
    }
};

//Natural join
// left is lineorder, right is supplier
struct q23_lo_s_join_t : public tuple_join_t {


    q23_lo_s_join_t ()
        : tuple_join_t(sizeof(q23_lo_tuple),
                       offsetof(q23_lo_tuple, LO_SUPPKEY),
                       sizeof(q23_s_tuple),
                       offsetof(q23_s_tuple, S_SUPPKEY),
                       sizeof(int),
                       sizeof(q23_join_s_tuple))
    {
    }


    virtual void join(tuple_t &dest,
                      const tuple_t &left,
                      const tuple_t &right)
    {
        // KLUDGE: this projection should go in a separate filter class
    	q23_lo_tuple* lo = aligned_cast<q23_lo_tuple>(left.data);
    	q23_s_tuple* s = aligned_cast<q23_s_tuple>(right.data);
	q23_join_s_tuple* ret = aligned_cast<q23_join_s_tuple>(dest.data);
	
	ret->LO_ORDERDATE = lo->LO_ORDERDATE;
	ret->LO_PARTKEY = lo->LO_PARTKEY;
	ret->LO_REVENUE = lo->LO_REVENUE;


        TRACE ( TRACE_RECORD_FLOW, "JOIN %d %d %d\n",ret->LO_ORDERDATE, ret->LO_PARTKEY, ret->LO_REVENUE);

    }

    virtual q23_lo_s_join_t*  clone() const {
        return new q23_lo_s_join_t(*this);
    }

    virtual c_str to_string() const {
        return "join LINEORDER and SUPPLIER, select LO_ORDERDATE, LO_PARTKEY, LO_REVENUE";
    }
};

//Natural join
// left is lineorder, supplier right is part
struct q23_lo_s_p_join_t : public tuple_join_t {


    q23_lo_s_p_join_t ()
        : tuple_join_t(sizeof(q23_join_s_tuple),
                       offsetof(q23_join_s_tuple, LO_PARTKEY),
                       sizeof(q23_p_tuple),
                       offsetof(q23_p_tuple, P_PARTKEY),
                       sizeof(int),
                       sizeof(q23_join_s_p_tuple))
    {
    }


    virtual void join(tuple_t &dest,
                      const tuple_t &left,
                      const tuple_t &right)
    {
        // KLUDGE: this projection should go in a separate filter class
    	q23_join_s_tuple* jo = aligned_cast<q23_join_s_tuple>(left.data);
    	q23_p_tuple* p = aligned_cast<q23_p_tuple>(right.data);
	q23_join_s_p_tuple* ret = aligned_cast<q23_join_s_p_tuple>(dest.data);
	
	ret->LO_ORDERDATE = jo->LO_ORDERDATE;
	ret->LO_REVENUE = jo->LO_REVENUE;
	strcpy(ret->P_BRAND,p->P_BRAND);
	
        TRACE ( TRACE_RECORD_FLOW, "JOIN %d {%s} %d\n",ret->LO_ORDERDATE, ret->P_BRAND, ret->LO_REVENUE);

    }

    virtual q23_lo_s_p_join_t* clone() const {
        return new q23_lo_s_p_join_t(*this);
    }

    virtual c_str to_string() const {
        return "join LINEORDER and SUPPLIER and PART, select LO_ORDERDATE, P_BRAND, LO_REVENUE";
    }
};

//Natural join
// left is lineorder, supplier and part, right is date
struct q23_join_t : public tuple_join_t {


    q23_join_t ()
        : tuple_join_t(sizeof(q23_join_s_p_tuple),
                       offsetof(q23_join_s_p_tuple, LO_ORDERDATE),
                       sizeof(q23_d_tuple),
                       offsetof(q23_d_tuple, D_DATEKEY),
                       sizeof(int),
                       sizeof(q23_join_tuple))
    {
    }


    virtual void join(tuple_t &dest,
                      const tuple_t &left,
                      const tuple_t &right)
    {
        // KLUDGE: this projection should go in a separate filter class
    	q23_join_s_p_tuple* jo = aligned_cast<q23_join_s_p_tuple>(left.data);
    	q23_d_tuple* d = aligned_cast<q23_d_tuple>(right.data);
	q23_join_tuple* ret = aligned_cast<q23_join_tuple>(dest.data);
	
	strcpy(ret->P_BRAND,jo->P_BRAND);
	ret->LO_REVENUE = jo->LO_REVENUE;
	ret->D_YEAR = d->D_YEAR;


        TRACE ( TRACE_RECORD_FLOW, "JOIN {%s} %d %d\n",ret->P_BRAND, ret->D_YEAR, ret->LO_REVENUE);

    }

    virtual q23_join_t* clone() const {
        return new q23_join_t(*this);
    }

    virtual c_str to_string() const {
        return "join LINEORDER and SUPPLIER and PART and DATE, select P_BRAND, S_CITY, D_YEAR, LO_REVENUE";
    }
};




// Key extractor and Comparator for Aggregation filter

struct q23_agg_input_tuple_key {
    
    char P_BRAND[STRSIZE(9)];
    int D_YEAR;
    
    
    int extract_hint() {
       
               return (this->D_YEAR - 1990 << 24) + (this->P_BRAND[5] << 16) + (this->P_BRAND[6] << 8) + (this->P_BRAND[7]);
    }
};

struct q23_agg_input_tuple_key_extractor_t : public key_extractor_t {

    q23_agg_input_tuple_key_extractor_t() : key_extractor_t(sizeof(q23_agg_input_tuple_key), offsetof(q23_join_tuple, P_BRAND)) {
    }

    virtual int extract_hint(const char* key) const {
        q23_agg_input_tuple_key* aligned_key = aligned_cast<q23_agg_input_tuple_key>(key);

        // We assume a 4-byte integer and fill it with: The three first 
        // characters of P_BRAND, and the
        // last digit of year (according to the specification, years
        // can be between 1992-1998). According to this hint,
        // the ordering can be made faster and only in ties, will
        // the key_compare_t be used to extract the full comparison.
        // Keep in mind that by using such a hint, performance is gained,
        // as we don't have to compare full strings if the first
        // characters are different, but we don't gain full ordering
        // if the cities start with the same letters: The resulting groups will be sorted primarily by year, because
        // the hint will only differ in the year. Groups are not affected, as on ties the full comparison is used.
        // If ordering is wanted, you'll need to use a hint
        // that is equal for everyone (like a zero), so that the full
        // comparison is used for everyone. In Q3.2, we don't need
        // full ordering in the aggregator, as in the end
        // the tuples are again ordered differently.
//        int result = (aligned_key->P_BRAND[0] << 24) + (aligned_key->P_BRAND[1] << 16) + (aligned_key->P_BRAND[2] << 8) + (aligned_key->D_YEAR - 1990);
//        return result;
        return aligned_key->extract_hint();
        
        //return 0;
    }

    virtual q23_agg_input_tuple_key_extractor_t * clone() const {
        return new q23_agg_input_tuple_key_extractor_t(*this);
    }
};

struct q23_agg_input_tuple_key_compare_t : public key_compare_t {

    virtual int operator()(const void* key1, const void* key2) const {
        q23_agg_input_tuple_key* a = aligned_cast<q23_agg_input_tuple_key>(key1);
        q23_agg_input_tuple_key* b = aligned_cast<q23_agg_input_tuple_key>(key2);

        int pbrandcomparison = strcmp(a->P_BRAND, b->P_BRAND);
        if (pbrandcomparison != 0) return pbrandcomparison;
        return a->D_YEAR - b->D_YEAR;
    }

    virtual q23_agg_input_tuple_key_compare_t * clone() const {
        return new q23_agg_input_tuple_key_compare_t(*this);
    }
};

// Aggregate's tuple's key is the same as the input tuple's key.
typedef struct q23_agg_input_tuple_key q23_agg_tuple_key;

class q23_agg_aggregate_t : public tuple_aggregate_t {
private:

    struct q23_agg_output_tuple_key_extractor_t : public key_extractor_t {

        q23_agg_output_tuple_key_extractor_t() : key_extractor_t(sizeof (q23_agg_tuple_key), offsetof(q23_aggregate_tuple, P_BRAND)) {
        }

        virtual int extract_hint(const char* key) const {
            q23_agg_tuple_key* aligned_key = aligned_cast<q23_agg_tuple_key > (key);
            return aligned_key->extract_hint();
        }

        virtual q23_agg_output_tuple_key_extractor_t * clone() const {
            return new q23_agg_output_tuple_key_extractor_t(*this);
        }
    };
    
    q23_agg_output_tuple_key_extractor_t _extractor;

public:

    q23_agg_aggregate_t()
    : tuple_aggregate_t(sizeof(q23_aggregate_tuple)) {
    }

    key_extractor_t* key_extractor() {
        return &_extractor;
    }

    void aggregate(char* agg_data, const tuple_t &s) {
        q23_join_tuple *src;
        src = aligned_cast<q23_join_tuple> (s.data);
        q23_aggregate_tuple* tuple = aligned_cast<q23_aggregate_tuple>(agg_data);

        tuple->REVENUE += src->LO_REVENUE;
        TRACE(TRACE_RECORD_FLOW, "%.2f\n", tuple->REVENUE);
    }

    void finish(tuple_t &d, const char* agg_data) {
        q23_aggregate_tuple *dest;
        dest = aligned_cast<q23_aggregate_tuple > (d.data);
        q23_aggregate_tuple* tuple = aligned_cast<q23_aggregate_tuple > (agg_data);

        *dest = *tuple;
        TRACE ( TRACE_RECORD_FLOW, "AGG: %d {%s} %d\n",tuple->D_YEAR, tuple->P_BRAND, tuple->REVENUE);
    }

    q23_agg_aggregate_t* clone() const {
        return new q23_agg_aggregate_t(*this);
    }

    c_str to_string() const {
        return "q23_agg_aggregate_t";
    }
};



// Final Ordering

struct q23_sort_tuple_key {
    char P_BRAND[STRSIZE(9)];
    int D_YEAR;
    
};

struct q23_order_key_extractor_t : public key_extractor_t {

    q23_order_key_extractor_t() : key_extractor_t(sizeof (q23_sort_tuple_key), offsetof(q23_aggregate_tuple, P_BRAND)) {
    }

    virtual int extract_hint(const char* key) const {
        q23_sort_tuple_key* aligned_key = aligned_cast<q23_sort_tuple_key> (key);

        // We assume a 4-byte integer and fill it with: The last digit of year
        // in the 3 most significant bits. The remaining bits are filled with the
        // integer representation of Revenue. But they are subtracted
        // in order to keep a decreasing order.
//        int result = ((aligned_key->D_YEAR - 1992) << 24) + (aligned_key->P_BRAND[5] << 16)+(aligned_key->P_BRAND[6] << 8)+(aligned_key->P_BRAND[7]);
        //int result = ((aligned_key->D_YEAR - 1992) << 24);
        int result=0;
        return result;
        
        //return 0;
    }

    virtual q23_order_key_extractor_t * clone() const {
        return new q23_order_key_extractor_t(*this);
    }
};

struct q23_order_key_compare_t : public key_compare_t {

    virtual int operator()(const void* key1, const void* key2) const {
        q23_sort_tuple_key* a = aligned_cast<q23_sort_tuple_key>(key1);
        q23_sort_tuple_key* b = aligned_cast<q23_sort_tuple_key>(key2);

        int yearcomparison = a->D_YEAR - b->D_YEAR;
        if (yearcomparison != 0) return yearcomparison;

//        int pbrandcomparison = strcmp(a->P_BRAND, b->P_BRAND);
//            return  pbrandcomparison;
//        int pbrandcomparison = strcmp(a->P_BRAND, b->P_BRAND);
//        if (pbrandcomparison != 0) return pbrandcomparison;
    }

    virtual q23_order_key_compare_t * clone() const {
        return new q23_order_key_compare_t(*this);
    }
};






class ssb_q23_process_tuple_t : public process_tuple_t 
{    
public:
        
    void begin() {
        TRACE(TRACE_QUERY_RESULTS, "*** q2_3 ANSWER ...\n");
        TRACE(TRACE_QUERY_RESULTS, "*** ...\n");
    }
    
    virtual void process(const tuple_t& output) {
        projected_tuple *tuple;
        tuple = aligned_cast<projected_tuple>(output.data);
 
        TRACE ( TRACE_QUERY_RESULTS, "PROCESS %d {%s} %d\n",tuple->D_YEAR, tuple->P_BRAND, tuple->REVENUE);

        /*TRACE(TRACE_QUERY_RESULTS, "%d --\n",
	  tuple->KEY);*/
    }
};



/******************************************************************** 
 *
 * QPIPE q2_3 - Packet creation and submission
 *
 ********************************************************************/

w_rc_t ShoreSSBEnv::xct_qpipe_q2_3(const int xct_id, 
                                  q2_3_input_t& in)
{
    TRACE( TRACE_ALWAYS, "********** q2_3 *********\n");

   
    policy_t* dp = this->get_sched_policy();
    xct_t* pxct = smthread_t::me()->xct();
    

    // TSCAN PACKET
    tuple_fifo* lo_out_buffer = new tuple_fifo(sizeof(q23_lo_tuple));
        tscan_packet_t* q23_lo_tscan_packet =
        new tscan_packet_t("TSCAN LINEORDER",
                           lo_out_buffer,
                           new q23_lineorder_tscan_filter_t(this),
                           this->db(),
                           _plineorder_desc.get(),
                           pxct
                           //, SH 
                           );
	//SUPPLIER
	tuple_fifo* s_out_buffer = new tuple_fifo(sizeof(q23_s_tuple));
        tscan_packet_t* q23_s_tscan_packet =
        new tscan_packet_t("TSCAN SUPPLIER",
                           s_out_buffer,
                           new q23_supplier_tscan_filter_t(this,in),
                           this->db(),
                           _psupplier_desc.get(),
                           pxct
                           //, SH 
                           );
        //PART
	tuple_fifo* p_out_buffer = new tuple_fifo(sizeof(q23_p_tuple));
        tscan_packet_t* q23_p_tscan_packet =
        new tscan_packet_t("TSCAN PART",
                           p_out_buffer,
                           new q23_part_tscan_filter_t(this,in),
                           this->db(),
                           _ppart_desc.get(),
                           pxct
                           //, SH 
                           );
	//DATE
	tuple_fifo* d_out_buffer = new tuple_fifo(sizeof(q23_d_tuple));
        tscan_packet_t* q23_d_tscan_packet =
        new tscan_packet_t("TSCAN DATE",
                           d_out_buffer,
                           new q23_date_tscan_filter_t(this,in),
                           this->db(),
                           _pdate_desc.get(),
                           pxct
                           //, SH 
                           );


	//JOIN Lineorder and Supplier
	tuple_fifo* join_lo_s_out = new tuple_fifo(sizeof(q23_join_s_tuple));
	packet_t* q23_join_lo_s_packet =
	    new hash_join_packet_t("Lineorder - Supplier JOIN",
				   join_lo_s_out,
				   new trivial_filter_t(sizeof(q23_join_s_tuple)),
				   q23_lo_tscan_packet,
				   q23_s_tscan_packet,
				   new q23_lo_s_join_t() );

	//JOIN Lineorder and Supplier and Part
	tuple_fifo* join_lo_s_p_out = new tuple_fifo(sizeof(q23_join_s_p_tuple));
	packet_t* q23_join_lo_s_p_packet =
	    new hash_join_packet_t("Lineorder - Supplier - Part JOIN",
				   join_lo_s_p_out,
				   new trivial_filter_t(sizeof(q23_join_s_p_tuple)),
				   q23_join_lo_s_packet,
				   q23_p_tscan_packet,
				   new q23_lo_s_p_join_t() );
	
	//JOIN Lineorder and Supplier and Part and Date
	tuple_fifo* join_out = new tuple_fifo(sizeof(q23_join_tuple));
	packet_t* q23_join_packet =
	    new hash_join_packet_t("Lineorder - Supplier - Part - Date JOIN",
				   join_out,
				   new trivial_filter_t(sizeof(q23_join_tuple)),
				   q23_join_lo_s_p_packet,
				   q23_d_tscan_packet,
				   new q23_join_t() );
        
         // AGG PACKET CREATION

    tuple_fifo* agg_output_buffer =
            new tuple_fifo(sizeof (q23_aggregate_tuple));
    packet_t* q23_agg_packet =
            new partial_aggregate_packet_t("AGG Q2_3",
            agg_output_buffer,
            new trivial_filter_t(agg_output_buffer->tuple_size()),
            q23_join_packet,
            new q23_agg_aggregate_t(),
            new q23_agg_input_tuple_key_extractor_t(),
            new q23_agg_input_tuple_key_compare_t());
  

        tuple_fifo* sort_final_out = new tuple_fifo(sizeof(q23_aggregate_tuple));
	packet_t* q23_sort_final_packet =
	    new sort_packet_t("ORDER BY D_YEAR ASC, REVENUE DESC",
				   sort_final_out,
				   new trivial_filter_t(sizeof(q23_aggregate_tuple)),
                                   new q23_order_key_extractor_t(),
                                   new q23_order_key_compare_t(),
				   q23_agg_packet);
   
    
    qpipe::query_state_t* qs = dp->query_state_create();
    q23_lo_tscan_packet->assign_query_state(qs);
    q23_s_tscan_packet->assign_query_state(qs);
    q23_p_tscan_packet->assign_query_state(qs);
    q23_d_tscan_packet->assign_query_state(qs);
    q23_join_lo_s_packet->assign_query_state(qs);
    q23_join_lo_s_p_packet->assign_query_state(qs);
    q23_join_packet->assign_query_state(qs);
    q23_agg_packet->assign_query_state(qs);
    q23_sort_final_packet->assign_query_state(qs);
        
    // Dispatch packet
    ssb_q23_process_tuple_t pt;
    //process_query(q23_agg_packet, pt);
    process_query(q23_sort_final_packet, pt);
    dp->query_state_destroy(qs);

    return (RCOK); 
}


EXIT_NAMESPACE(ssb);

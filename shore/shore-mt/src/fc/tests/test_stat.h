/*<std-header orig-src='shore' incl-file-exclusion='TEST_STAT_H'>

 $Id: test_stat.h,v 1.1.2.5 2010/03/19 22:17:53 nhall Exp $

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

#ifndef TEST_STAT_H
#define TEST_STAT_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_stat.h>
// DEAD #include "test_stat_def_gen.h"
class test_stat {
    /* add the stats */
#include "test_stat_struct_gen.h"

public:
    test_stat() : 
        b(1),
        f(5.4321),
        i(300),
        j((unsigned)0x333),
        u(3),
        k((float)1.2345),
        l(4),
        v(0xffffffffffffffffull),
        x(5),
        d(6.789),
        sum(0.0) { }

    void inc();
    void dec();
    void compute() {
        sum = (float)(i + j + k);
    }
};

/*<std-footer incl-file-exclusion='TEST_STAT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

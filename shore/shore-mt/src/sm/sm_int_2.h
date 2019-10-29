/*<std-header orig-src='shore' incl-file-exclusion='SM_INT_2_H'>

 $Id: sm_int_2.h,v 1.8.2.4 2010/01/28 04:54:16 nhall Exp $

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

#ifndef SM_INT_2_H
#define SM_INT_2_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 2
#endif

#ifndef SM_INT_1_H
#include "sm_int_1.h"
#endif

class btree_m;
class file_m;
class rtree_m;
class ranges_m;

class smlevel_2 : public smlevel_1 {
public:
    static btree_m* bt;
    static file_m* fi;
    static rtree_m* rt;
    static ranges_m* ra;
};

#if (SM_LEVEL >= 2)
#    include <sdesc.h>
#    ifdef BTREE_C
#    define RTREE_H
#    endif
#    ifdef RTREE_C
#    define BTREE_H
#    endif
#    if defined(FILE_C) || defined(SMFILE_C)
#    define BTREE_H
#    define RTREE_H
#    endif
#    include <btree.h>
#    include <nbox.h>
#    include <rtree.h>
#    include <file.h>
#    include <ranges_p.h>
#endif

/*<std-footer incl-file-exclusion='SM_INT_2_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: stid_t.cpp,v 1.13.2.3 2010/01/28 04:53:23 nhall Exp $

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

#ifdef __GNUC__
#pragma implementation "stid_t.h"
#endif

#define VEC_T_C
#include <cstdlib>
#include <cstring>
#include <w_stream.h>
#include <w_base.h>
#include <w_minmax.h>
#include "basics.h"
#include "stid_t.h"

const stid_t stid_t::null;

ostream& operator<<(ostream& o, const stid_t& stid)
{
    return o << "s(" << stid.vol << '.' << stid.store << ')';
}

istream& operator>>(istream& i, stid_t& stid)
{
    char c[5];
    memset(c, '\0', sizeof(c));
    i >> c[0];
    if(i.good()) 
        i >> c[1];
    if(i.good()) 
        i >> stid.vol;
    if(i.good()) 
        i >> c[2];
    if(i.good()) 
        i >> stid.store;
    if(i.good()) 
        i >> c[3];
    c[4] = '\0';
    if (i) {
        if (strcmp(c, "s(.)")) {
            i.clear(ios::badbit|i.rdstate());  // error
        }
    }
    return i;
}


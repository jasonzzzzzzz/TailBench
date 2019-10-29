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

/*<std-header orig-src='shore' incl-file-exclusion='TEST_ME' no-defines='true'>

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

/* used in mem_block.cpp */

#ifndef __TEST_ME_H
#define __TEST_ME_H

#include "w.h"
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <cstdlib>

#define EXPECT_ASSERT(x) do {				\
	std::fprintf(stdout, "\nExpecting an assertion at line %d...\n", __LINE__);	\
	std::fflush(stdout); \
	pid_t pid = fork();				\
	if(pid == 0) {					\
	    x;						\
		std::fprintf(stdout, "\nExiting 0 at line %d\n", __LINE__);	\
	    std::exit(0);				\
	}						\
	else {						\
	    w_assert0(pid > 0);				\
	    int status;					\
	    int err = wait(&status);			\
	    w_assert0(err != ECHILD);			\
	    w_assert0(err != EINTR);			\
	    w_assert0(err != EINVAL);			\
	    if(!WIFSIGNALED(status)) {			\
			std::fprintf(stdout, "%s:%d: `%s' did not assert as expected\n", \
			     __FILE__, __LINE__, #x);			\
			std::fprintf(stdout, "\naborting...\n");	\
			std::abort();						\
	    }						\
		std::fprintf(stdout, "\nsuccess...\n");	\
	}						\
    } while(0)

#endif

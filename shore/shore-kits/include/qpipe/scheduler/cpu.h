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

/** @file cpu.h
 *
 *  @brief Exports cpu_t datatype. QPIPE worker threads may
 *  invoke cpu_bind_self() to bind themselves to the
 *  specified CPU.
 *
 *  @bug See cpu.cpp.
 */

#ifndef __QPIPE_CPU_H
#define __QPIPE_CPU_H

#include "util/namespace.h"

ENTER_NAMESPACE(qpipe);

/* exported datatypes */

typedef struct cpu_s* cpu_t;


/* exported functions */

void cpu_bind_self(cpu_t cpu);
int  cpu_get_unique_id(cpu_t cpu);

EXIT_NAMESPACE(qpipe);

#endif

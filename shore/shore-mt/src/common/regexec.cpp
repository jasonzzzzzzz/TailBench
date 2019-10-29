/*<std-header orig-src='regex'>

 $Id: regexec.cpp,v 1.18.2.3 2010/03/19 22:19:19 nhall Exp $


*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
Copyright 1992, 1993, 1994, 1997 Henry Spencer.  All rights reserved.
This software is not subject to any license of the American Telephone
and Telegraph Company or of the Regents of the University of California.

Permission is granted to anyone to use this software for any purpose on
any computer system, and to alter it and redistribute it, subject
to the following restrictions:

1. The author is not responsible for the consequences of use of this
   software, no matter how awful, even if they arise from flaws in it.

2. The origin of this software must not be misrepresented, either by
   explicit claim or by omission.  Since few users ever read sources,
   credits must appear in the documentation.

3. Altered versions must be plainly marked as such, and must not be
   misrepresented as being the original software.  Since few users
   ever read sources, credits must appear in the documentation.

4. This notice may not be removed or altered.

*/

/* 
  NOTICE of alterations in Spencer's regex implementation :
  The following alterations were made to Henry Spencer's regular 
  expressions implementation, in order to make it build in the
  Shore configuration scheme:

  1) the generated .ih files are no longer generated. They are
    considered "sources".  Likewise for regex.h.
    *.ih is now called *_i.h, for NT purposes.
  2) names were changed to w_regexex, w_regerror, etc by i
    #define statements in regex.h
  3) all the c sources were protoized and gcc warnings were 
    fixed.
  4) This entire notice was put into the .c, .ih, and .h files
*/

/*
 * the outer shell of regexec()
 *
 * This file includes engine.c *twice*, after muchos fiddling with the
 * macros that code uses.  This lets the same code operate on two different
 * representations for state sets.
 */
#include <os_types.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cctype>
#include <regex.h>

#include "regex_utils.h"
#include "regex2.h"

extern int w_force_shore_regerror; // GROT
int w_force_shore_regexec; // GROT
int &_w_force_shore_regexec=w_force_shore_regerror; // GROT

/* macros for manipulating states, small version */
#define    states    long
#define    states1    states        /* for later use in regexec() decision */
#define    CLEAR(v)    ((v) = 0)
#define    SET0(v, n)    ((v) &= ~(1ul << (n)))
#define    SET1(v, n)    ((v) |= 1ul << (n))
#define    ISSET(v, n)    ((v) & (1ul << (n)))
#define    ASSIGN(d, s)    ((d) = (s))
#define    EQ(a, b)    ((a) == (b))
#define    STATEVARS    int dummy    /* dummy version */
#define    STATESETUP(m, n)    /* nothing */
#define    STATETEARDOWN(m)    /* nothing */
#define    SETUP(v)    ((v) = 0)
#define    onestate    long
#define    INIT(o, n)    ((o) = (unsigned long)1 << (n))
#define    INC(o)    ((o) <<= 1)
#define    ISSTATEIN(v, o)    ((v) & (o))
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define    FWD(dst, src, n)    ((dst) |= ((unsigned long)(src)&(here)) << (n))
#define    BACK(dst, src, n)    ((dst) |= ((unsigned long)(src)&(here)) >> (n))
#define    ISSETBACK(v, n)    ((v) & ((unsigned long)here >> (n)))
/* function names */
#define SNAMES            /* engine.c looks after details */

#include "regex_engine.cpp"

/* now undo things */
#undef    states
#undef    CLEAR
#undef    SET0
#undef    SET1
#undef    ISSET
#undef    ASSIGN
#undef    EQ
#undef    STATEVARS
#undef    STATESETUP
#undef    STATETEARDOWN
#undef    SETUP
#undef    onestate
#undef    INIT
#undef    INC
#undef    ISSTATEIN
#undef    FWD
#undef    BACK
#undef    ISSETBACK
#undef    SNAMES

/* macros for manipulating states, large version */
#define    states    char *
#define    CLEAR(v)    memset(v, 0, m->g->nstates)
#define    SET0(v, n)    ((v)[n] = 0)
#define    SET1(v, n)    ((v)[n] = 1)
#define    ISSET(v, n)    ((v)[n])
#define    ASSIGN(d, s)    memcpy(d, s, m->g->nstates)
#define    EQ(a, b)    (memcmp(a, b, m->g->nstates) == 0)
#define    STATEVARS    int vn; char *space
#define    STATESETUP(m, nv)    { (m)->space = (char *)malloc((nv)*(m)->g->nstates); \
                if ((m)->space == NULL) return(REG_ESPACE); \
                (m)->vn = 0; }
#define    STATETEARDOWN(m)    { free((m)->space); }
#define    SETUP(v)    ((v) = &m->space[m->vn++ * m->g->nstates])
#define    onestate    int
#define    INIT(o, n)    ((o) = (n))
#define    INC(o)    ((o)++)
#define    ISSTATEIN(v, o)    ((v)[o])
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define    FWD(dst, src, n)    ((dst)[here+(n)] |= (src)[here])
#define    BACK(dst, src, n)    ((dst)[here-(n)] |= (src)[here])
#define    ISSETBACK(v, n)    ((v)[here - (n)])
/* function names */
#define    LNAMES            /* flag */

#include "regex_engine.cpp"

/*
 - regexec - interface for matching
 = extern int regexec(const regex_t *, const char *, size_t, \
 =                    regmatch_t [], int);
 = #define    REG_NOTBOL    00001
 = #define    REG_NOTEOL    00002
 = #define    REG_STARTEND    00004
 = #define    REG_TRACE    00400    // tracing of execution
 = #define    REG_LARGE    01000    // force large representation
 = #define    REG_BACKR    02000    // force use of backref code
 *
 * We put this here so we can exploit knowledge of the state representation
 * when choosing which matcher to call.  Also, by this point the matchers
 * have been prototyped.
 */
int                /* 0 success, REG_NOMATCH failure */
regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
    register struct re_guts *g = preg->re_g;
#ifdef REDEBUG
#    define    GOODFLAGS(f)    (f)
#else
#    define    GOODFLAGS(f)    ((f)&(REG_NOTBOL|REG_NOTEOL|REG_STARTEND))
#endif

    if (preg->re_magic != MAGIC1 || g->magic != MAGIC2)
        return(REG_BADPAT);
    re_assert(!(g->iflags&BAD));
    if (g->iflags&BAD)        /* backstop for no-debug case */
        return(REG_BADPAT);
    eflags = GOODFLAGS(eflags);

    if (g->nstates <= (long)(CHAR_BIT*sizeof(states1)) && !(eflags&REG_LARGE))
        return(smatcher(g, (char *)string, nmatch, pmatch, eflags));
    else
        return(lmatcher(g, (char *)string, nmatch, pmatch, eflags));
}


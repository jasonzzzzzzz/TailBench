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

#ifndef __ATOMIC_CONTAINER
#define __ATOMIC_CONTAINER

#include "atomic_templates.h"

// for placement new support, which users need
#include <new>
#include <cassert>
#include <stdlib.h>

/** \brief A thread-safe, lock-free, almost wait-free atomic 
 * container for untyped items.
 *
 * This class takes care of pushing and
 * popping elements from the container for multiple concurrent threads. It
 * is up to the user (client code) to allocate and deallocate the
 * storage for items pushed on this container.
 *
 * The objects being stored here must have an embedded next pointer.
 * The offset given in the constructor tells the container the offset
 * of the "next" pointer in the objects being stored here.  The offset
 * can be + or - from the pointer being given in push().
 *
 * WARNING: in order to avoid the so-called "ABA" problem, the
 * container must begin with and maintain a reasonably large pool. 
 * There is the possibility of recently-freed objects being reused 
 * very quickly, in turn
 * enabling internal corruption from a possible race where a thread
 * begins to allocate an object, but other threads do enough
 * pops and pushes to cycle through 8 version numbers, and all this
 * happens before the first thread finishes.  It's unlikely but 
 * possible.
 */
class atomic_container {
    /// \cond skipdoc
protected:
    struct ptr { ptr* next; }; 
    /// Unions for punning, i.e., type conversion
    union vpn { void* v; ptr* p; long n; };
    union pvn { ptr* p; void* v; long n; };
    /// \endcond skipdoc

public:
    typedef long int offset_typ;

    atomic_container(offset_typ offset)
        : _offset(offset), _locked(0), _active(0), _backup(0)
    { }
    
    /// Pop an item off the stack.
    ///  If we don't find any to pop, return a null ptr.
    ///   We do not go to the global heap. The client must do that.
    void* pop() {
        pvn old_value = {*&_active};
        while(pointer(old_value)) {
            // swap if the head's pointer and version are unchanged
            pvn new_value = next_pointer(old_value);
            void* cur_value = atomic_cas_ptr(&_active, old_value.v, new_value.v);
            if(old_value.v == cur_value)
                break;

            // try again...
            old_value.v = cur_value;
        }

        // slow alloc?
        return pointer(old_value)? prepare(old_value) : slow_pop();
    }

    /// Push an item onto the stack
    void push(void* v) {
        // back up to the real start of this allocation
        vpn u = {v};        
        u.n += _offset;

        // enqueue it
        pvn old_value = { _backup };
        while(1) {
            u.p->next = old_value.p;
            membar_producer();
            void* cur_value = atomic_cas_ptr(&_backup, old_value.v, u.v);
            if(old_value.v == cur_value)
                break;

            // try again...
            old_value.v = cur_value;
        }
    }
    /// Only for debugging.
    offset_typ offset() const { return  _offset; } 

    ~atomic_container() {  // for shutdown/restart purposes:
             _locked = 0; _active = 0; _backup = 0;
    }
    
protected:
    /// Strip off the pointer's version number and hide the header.
    template<class Union>
    void* prepare(Union rval) {
        rval.n = pointer(rval) - _offset;
        return rval.v;
    }
    
    /// Return a null pointer (i.e., it contains the offset only).
    void* null() { 
        union {
            offset_typ  i;
            void *v;
        } _pun = { _offset };
        return _pun.v; 
    } 

    offset_typ const _offset;
    
private:
    unsigned volatile _locked;
    /// The list of active stuff.
    /// Pop will pull things off this list until it's empty.
    ptr* volatile _active;
    /// The list of things recently pushed.
    ///  Push uses this list to avoid interference with pop.
    ptr* volatile _backup;

#ifdef ARCH_LP64
    enum { VERSION_MASK=0x7 };
#else
    enum { VERSION_MASK=0x3 }; //alas. few versions 
#endif
    
    ///Return the pointer with the version mask removed.
    template<class Union>
    static long pointer(Union value) { return value.n &~VERSION_MASK; }
    
    ///Return the version mask that's embedded in the pointer.
    static long version(long value) { return value & VERSION_MASK; }

    ///Return a non-dereferencable pointer to the next item after the given one.
    /// The given value might have the version number embedded (or not).
    /// The returned ptr will have the same version as that in the ptr. 
    static pvn next_pointer(pvn value) {
        long ver = version(value.n);
        value.n -= ver; // subtract out the version
        value.p = value.p->next; // get ptr to the next item
        value.n += ver; // add back in the version
        return value;
    }
    
    /// Spin until acquiring the lock is free or noticing that _active
    ///   has become non-null. 
    bool attempt_lock() {
        bool mine = false;
        pvn active = { *&_active };
        while(!mine) {
            while(*&_locked && !pointer(active)) active.p = *&_active;
            if(pointer(active)) return false;
            mine = !atomic_swap_32(&_locked, true);
        }
        if(mine) {
            membar_enter();
            active.p = *&_active;
            if(!pointer(active))
                return true;
            
            release_lock();
        }
        return false;
    }
    ///Release the lock.
    void release_lock() {
        membar_exit();
        _locked = false;
    }
    
    /// Grab a lock, swap active and backup lists,
    ///  and try again to pop.
    void* slow_pop() {
        if(!attempt_lock())
            return pop(); // try again

        /* At this point (holding the lock) the _active list will
           not change to non-null on us. The _backup list may
           continue to grow so we atomically cut it loose
        */
        vpn rval = { atomic_swap_ptr(&_backup, NULL) };
        if(rval.p) {
            // keep head for ourselves, rest becomes new _active
            pvn old_list = { _active };
            pvn new_list = {rval.p->next};
            new_list.n += version(old_list.n+1);
            _active = new_list.p;
        }
        else {
            rval.v = null();
        }
        
        release_lock();
        return prepare(rval);
    }
    
};

#endif

/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2013 President and Fellows of Harvard College
 * Copyright-2013 (c) 2012 Massachusetts Institute of Technology
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
#ifndef MASSTREE_GET_HH
#define MASSTREE_GET_HH 1
#include "masstree_tcursor.hh"
#include "masstree_traverse.hh"
namespace Masstree {

template <typename P>
bool unlocked_tcursor<P>::find_unlocked(threadinfo *ti)
{
    leafvalue<P> entry = leafvalue<P>::make_empty();
    bool ksuf_match = false;
    int kp, keylenx = 0;
    leaf<P> *n;
    typename leaf<P>::nodeversion_type v;
    node_base<P> *root = tablep_->root_;

 retry:
    n = reach_leaf(root, ka_, ti, v);

 forward:
    if (v.deleted())
	goto retry;

    n->prefetch();
    kp = leaf<P>::bound_type::lower_check(ka_, *n);
    if (kp >= 0) {
	keylenx = n->keylenx_[kp];
	fence();		// see note in check_leaf_insert()
	entry = n->lv_[kp];
	entry.prefetch(keylenx);
	ksuf_match = n->ksuf_equals(kp, ka_, keylenx);
    }
    if (n->has_changed(v)) {
	ti->mark(threadcounter(tc_stable_leaf_insert + n->simple_has_split(v)));
	n = forward_at_leaf(n, v, ka_, ti);
	goto forward;
    }

    if (kp < 0)
	return false;
    else if (n->keylenx_is_node(keylenx)) {
	if (likely(n->keylenx_is_stable_node(keylenx))) {
	    ka_.shift();
	    root = entry.node();
	    goto retry;
	} else
	    goto forward;
    } else if (ksuf_match) {
	datum_ = entry.value();
	return true;
    } else
	return false;
}

template <typename P>
inline bool basic_table<P>::get(Str key, value_type &value,
                                threadinfo *ti) const
{
    unlocked_tcursor<P> lp(*this, key);
    bool found = lp.find_unlocked(ti);
    if (found)
	value = lp.datum_;
    return found;
}

template <typename P>
inline node_base<P> *tcursor<P>::get_leaf_locked(node_type *root,
                                                 nodeversion_type &v,
                                                 threadinfo *ti)
{
    nodeversion_type oldv = v;
    typename permuter_type::storage_type old_perm;
    leaf_type *next;

    n_->prefetch();

    if (!ka_.has_suffix())
	v = n_->lock(oldv, ti->lock_fence(tc_leaf_lock));
    else {
	// First, look up without locking.
	// The goal is to avoid dirtying cache lines on upper layers of a long
	// key walk. But we do lock if the next layer has split.
	old_perm = n_->permutation_;
	ki_ = leaf_type::bound_type::lower_with_position(ka_, *n_, kp_);
	if (kp_ >= 0 && n_->is_stable_node(kp_)) {
	    fence();
	    leafvalue_type entry(n_->lv_[kp_]);
	    entry.node()->prefetch_full();
	    fence();
	    if (likely(!v.deleted()) && !n_->has_changed(oldv)
		&& old_perm == n_->permutation_
		&& !entry.node()->has_split()) {
		ka_.shift();
		return entry.node();
	    }
	}

	// Otherwise lock.
	v = n_->lock(oldv, ti->lock_fence(tc_leaf_lock));

	// Maybe the old position works.
	if (likely(!v.deleted()) && !n_->has_changed(oldv)
	    && old_perm == n_->permutation_) {
	found:
	    if (kp_ >= 0 && n_->is_stable_node(kp_)) {
		root = n_->lv_[kp_].node();
		if (root->has_split())
		    n_->lv_[kp_] = root = root->unsplit_ancestor();
		n_->unlock(v);
		ka_.shift();
		return root;
	    } else
		return 0;
	}
    }


    // Walk along leaves.
    while (1) {
	if (unlikely(v.deleted())) {
	    n_->unlock(v);
	    return root;
	}
	ki_ = leaf_type::bound_type::lower_with_position(ka_, *n_, kp_);
	if (kp_ >= 0) {
	    n_->lv_[kp_].prefetch(n_->keylenx_[kp_]);
	    goto found;
	} else if (likely(ki_ != n_->size() || !v.has_split(oldv))
		   || !(next = n_->safe_next())
		   || compare(ka_.ikey(), next->ikey_bound()) < 0)
	    goto found;
	n_->unlock(v);
	ti->mark(tc_leaf_retry);
	ti->mark(tc_leaf_walk);
	do {
	    n_ = next;
	    oldv = n_->stable();
	} while (!unlikely(oldv.deleted()) && (next = n_->safe_next())
		 && compare(ka_.ikey(), next->ikey_bound()) >= 0);
	n_->prefetch();
	v = n_->lock(oldv, ti->lock_fence(tc_leaf_lock));
    }
}

template <typename P>
inline node_base<P> *tcursor<P>::check_leaf_locked(node_type *root,
                                                   nodeversion_type v,
                                                   threadinfo *ti)
{
    if (node_type *next_root = get_leaf_locked(root, v, ti))
	return next_root;
    if (kp_ >= 0) {
	if (!n_->ksuf_equals(kp_, ka_))
	    kp_ = -1;
    } else if (ki_ == 0 && unlikely(n_->deleted_layer())) {
	n_->unlock();
	return reset_retry();
    }
    return 0;
}

template <typename P>
bool tcursor<P>::find_locked(threadinfo *ti)
{
    nodeversion_type v;
    node_type *root = tablep_->root_;
    while (1) {
	n_ = reach_leaf(root, ka_, ti, v);
	root = check_leaf_locked(root, v, ti);
	if (!root) {
            state_ = kp_ >= 0;
	    return kp_ >= 0;
        }
    }
}

} // namespace Masstree
#endif

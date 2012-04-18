/** $Id: lock.h 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file lock.h
	@defgroup locking Locking memory
	@ingroup module_api

	Memory locking is implemented using compare and exchange methods.  
	Any time more than one object can concurrently write to the same
	region of memory, it is necessary to implement locking to prevent
	one object from overwriting the changes made by another.  
	For example, more than one link can simultaneously update the 
	admittance and current injection accumulators in nodes.  Thus
	the following code is required to prevent two objects from simultaneously
	reading the same accumulator and posting their modifications without
	considering the other's contribution.  
	
	@code
	complex I = to->V * Y;
	LOCK_OBJECT(from);
	from->Ys += Y;
	from->YVs -= I;
	UNLOCK_OBJECT(from);
	@endcode

	Without locking, only the second one's contribution would be counted
	and the first one's would be lost.
 @{
 **/

#ifndef _LOCK_H
#define _LOCK_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(__APPLE__)
	#include <libkern/OSAtomic.h>
	#define atomic_compare_and_swap(dest, comp, xchg) OSAtomicCompareAndSwap32Barrier(comp, xchg, (volatile int32_t *) dest)
	#define atomic_increment(ptr) OSAtomicIncrement32Barrier((volatile int32_t *) ptr)
#elif defined(WIN32)
	#include <intrin.h>
	#pragma intrinsic(_InterlockedCompareExchange)
	#pragma intrinsic(_InterlockedIncrement)
	#define atomic_compare_and_swap(dest, comp, xchg) (_InterlockedCompareExchange((long *) dest, xchg, comp) == comp)
	#define atomic_increment(ptr) _InterlockedIncrement((long *) ptr)
	#ifndef inline
		#define inline __inline
	#endif
#elif defined HAVE___SYNC_BOOL_COMPARE_AND_SWAP
	#define atomic_compare_and_swap __sync_bool_compare_and_swap
	#ifdef HAVE___SYNC_ADD_AND_FETCH
		#define atomic_increment(ptr) __sync_add_and_fetch(ptr, 1)
	#else
		static inline unsigned int atomic_increment(unsigned int *ptr)
		{
			unsigned int value;
			do {
				value = *ptr;
			} while (!__sync_bool_compare_and_swap(ptr, value, value + 1));
			return value;
		}
	#endif
#else
	#error "Locking is not supported on this system"
#endif

static inline void _lock(unsigned int *lock)
{
	unsigned int value;

	do {
		value = (*lock);
	} while ((value&1) || !atomic_compare_and_swap((volatile long*)lock, value, value + 1));
}
static inline void unlock(unsigned int *lock)
{
	unsigned int value = *lock;
	atomic_increment((volatile long*)lock);
}
#define rlock _lock /** @todo implement read lock */
#define wlock _lock /** @todo implement write lock */

#define LOCK(X) wlock(X) /**< Locks an item */
#define READLOCK(X) rlock(X) /**< @todo Locks an item for reading (allows other reads but blocks write) */
#define WRITELOCK(X) wlock(X) /**< @todo Locks an item for writing (blocks all operations) */
#define UNLOCK(lock) unlock(lock) /**< Unlocks an item */
#define LOCK_OBJECT(obj) wlock(&((obj)->lock)) /**< Locks an object */
#define READLOCK_OBJECT(obj) rlock(&((obj)->lock)) /**< @todo Locks an object for reading */
#define WRITELOCK_OBJECT(obj) wlock(&((obj)->lock)) /**< @todo Locks an object for writing */
#define UNLOCK_OBJECT(obj) unlock(&((obj)->lock)) /**< Unlocks an object */
#define LOCKED(obj,command) (LOCK_OBJECT(obj),(command),UNLOCK_OBJECT(obj))

#endif /* _LOCK_H */

/**@}**/


/*

    ncdd

    Copyright (c) 2002 Cesar Hernandez <chernandezba@hotmail.com>

    This file is part of ncdd

    ncdd is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


*/


#ifndef NCDD_ATOMIC_H
#define NCDD_ATOMIC_H

#ifdef NCDD_USES_KERNEL_ATOMIC
//Use Kernel builtin functions
//http://golubenco.org/blog/atomic-operations
	#include <asm/alternative.h>
	#include <asm/atomic.h>
#else
//Use gcc functions

 
/**
 * Atomic type.
 */
 
typedef struct {
volatile int counter;
} atomic_t;
 
#define ATOMIC_INIT(i)  { (i) }
 
/**
 * Read atomic variable
 * @param v pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v) ((v)->counter)
 
/**
 * Set atomic variable
 * @param v pointer of type atomic_t
 * @param i required value
 */
#define atomic_set(v,i) (((v)->counter) = (i))
 
/**
 * Add to the atomic variable
 * @param i integer value to add
 * @param v pointer of type atomic_t
 */
static inline void atomic_add( int i, atomic_t *v ) 
{
         (void)__sync_add_and_fetch(&v->counter, i);
}
 
/**
 * Subtract the atomic variable
 * @param i integer value to subtract
 * @param v pointer of type atomic_t
 *
 * Atomically subtracts @i from @v.
 */
static inline void atomic_sub( int i, atomic_t *v ) 
{
        (void)__sync_sub_and_fetch(&v->counter, i);
}
 
/**
 * Subtract value from variable and test result
 * @param i integer value to subtract
 * @param v pointer of type atomic_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static inline int atomic_sub_and_test( int i, atomic_t *v ) 
{
        return !(__sync_sub_and_fetch(&v->counter, i));
}
 
/**
 * Increment atomic variable
 * @param v pointer of type atomic_t
 *
 * Atomically increments @v by 1.
 */
static inline void atomic_inc( atomic_t *v ) 
{
       (void)__sync_fetch_and_add(&v->counter, 1);
}
 
/**
 * @brief decrement atomic variable
 * @param v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.  Note that the guaranteed
 * useful range of an atomic_t is only 24 bits.
 */
static inline void atomic_dec( atomic_t *v ) 
{
       (void)__sync_fetch_and_sub(&v->counter, 1);
}
 
/**
 * @brief Decrement and test
 * @param v pointer of type atomic_t
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */
static inline int atomic_dec_and_test( atomic_t *v ) 
{
       return !(__sync_sub_and_fetch(&v->counter, 1));
}
 
/**
 * @brief Increment and test
 * @param v pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
static inline int atomic_inc_and_test( atomic_t *v ) 
{
      return !(__sync_add_and_fetch(&v->counter, 1));
}
 
/**
 * @brief add and test if negative
 * @param v pointer of type atomic_t
 * @param i integer value to add
 *
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */
static inline int atomic_add_negative( int i, atomic_t *v ) 
{
       return (__sync_add_and_fetch(&v->counter, i) < 0);
}

#endif
 
#endif

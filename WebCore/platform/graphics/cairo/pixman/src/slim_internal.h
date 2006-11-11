/* slim - Shared Library Interface Macros
 *
 * Copyright © 2003 Richard Henderson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Richard Henderson
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Richard Henderson makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * RICHARD HENDERSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL RICHARD HENDERSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Richard Henderson <rth@twiddle.net>
 */

#ifndef _SLIM_INTERNAL_H_
#define _SLIM_INTERNAL_H_ 1

/* This macro marks a symbol as STV_HIDDEN, which prevents it from being
   added to the dynamic symbol table of the shared library.  This prevents
   users of the library from knowingly or unknowingly accessing library
   internals that may change in future releases.  It also allows the 
   compiler to generate slightly more efficient code in some cases.

   The macro should be placed either immediately before the return type in
   a function declaration:

	pixman_private int
	somefunction(void);

   or after a data name,

	extern int somedata pixman_private;

   The ELF visibility attribute did not exist before gcc 3.3.  */
/* ??? Not marked with "slim" because that makes it look too much
   like the function name instead of just an attribute.  */

#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)) && defined(__ELF__)
#define pixman_private	__attribute__((__visibility__("hidden")))
#else
#define pixman_private
#endif

/* The following macros are used for PLT bypassing.  First of all,
   you need to have the function prototyped somewhere, say in foo.h:

	int foo (int __bar);

   If calls to foo within libfoo.so should always go to foo defined
   in libfoo.so, then in fooint.h you add:

	slim_hidden_proto (foo)

   and after the foo function definition:

	int foo (int __bar)
	{
	  return __bar;
	}
	slim_hidden_def (foo);

   This works by arranging for the C symbol "foo" to be renamed to
   "INT_foo" at the assembly level, which is marked pixman_private.
   We then create another symbol at the same address (an alias) with
   the C symbol "EXT_foo", which is renamed to "foo" at the assembly
   level.  */

#if __GNUC__ >= 3 && defined(__ELF__)
# define slim_hidden_proto(name)	slim_hidden_proto1(name, INT_##name)
# define slim_hidden_def(name)		slim_hidden_def1(name, INT_##name)
# define slim_hidden_proto1(name, internal)				\
  extern __typeof (name) name						\
	__asm__ (slim_hidden_asmname (internal))			\
	pixman_private;
# define slim_hidden_def1(name, internal)				\
  extern __typeof (name) EXT_##name __asm__(slim_hidden_asmname(name))	\
	__attribute__((__alias__(slim_hidden_asmname(internal))))
# define slim_hidden_ulp		slim_hidden_ulp1(__USER_LABEL_PREFIX__)
# define slim_hidden_ulp1(x)		slim_hidden_ulp2(x)
# define slim_hidden_ulp2(x)		#x
# define slim_hidden_asmname(name)	slim_hidden_asmname1(name)
# define slim_hidden_asmname1(name)	slim_hidden_ulp #name
#else
# define slim_hidden_proto(name)
# define slim_hidden_def(name)
#endif

#endif /* _SLIM_INTERNAL_H_ */

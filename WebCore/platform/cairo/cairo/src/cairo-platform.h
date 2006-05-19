/* cairo - a vector graphics library with display and print output
 *
 * Copyright Å¬Å© 2005 Mozilla Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Stuart Parmenter <stuart@mozilla.com>
 */

#ifndef CAIRO_PLATFORM_H
#define CAIRO_PLATFORM_H

#if defined(MOZ_ENABLE_LIBXUL)

#ifdef HAVE_VISIBILITY_HIDDEN_ATTRIBUTE
#define CVISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#define CVISIBILITY_HIDDEN
#endif

// In libxul builds we don't ever want to export cairo symbols
#define cairo_public extern CVISIBILITY_HIDDEN
#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#elif defined(XP_WIN)

#define cairo_public extern __declspec(dllexport)
#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#elif defined(XP_BEOS)

#define cairo_public extern __declspec(dllexport)
#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#elif defined(XP_MAC)

#define cairo_public extern __declspec(export)
#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#elif defined(XP_OS2_VACPP) 

#define cairo_public extern
#define CCALLBACK _Optlink
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x CCALLBACK

#else /* Unix */

#ifdef HAVE_VISIBILITY_ATTRIBUTE
#define CVISIBILITY_DEFAULT __attribute__((visibility("default")))
#else
#define CVISIBILITY_DEFAULT
#endif

#define cairo_public extern CVISIBILITY_DEFAULT
#define CCALLBACK
#define CCALLBACK_DECL
#define CSTATIC_CALLBACK(__x) static __x

#endif

#ifdef MOZILLA_VERSION
#include "cairo-rename.h"
#endif

#endif /* CAIRO_PLATFORM_H */

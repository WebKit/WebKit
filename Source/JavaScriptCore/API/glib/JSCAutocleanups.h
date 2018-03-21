/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__JSC_H_INSIDE__) && !defined(JSC_COMPILATION) && !defined(WEBKIT2_COMPILATION)
#error "Only <jsc/jsc.h> can be included directly."
#endif

#ifndef JSCAutoPtr_h
#define JSCAutoPtr_h

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (JSCClass, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JSCContext, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JSCException, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JSCValue, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (JSCVirtualMachine, g_object_unref)

#endif /* __GI_SCANNER__ */
#endif /* G_DEFINE_AUTOPTR_CLEANUP_FUNC */

#endif /* JSCAutoPtr_h */

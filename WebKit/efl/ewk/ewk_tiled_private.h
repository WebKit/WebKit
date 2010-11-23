/*
    Copyright (C) 2009-2010 Samsung Electronics
    Copyright (C) 2009-2010 ProFUSION embedded systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ewk_tiled_private_h
#define ewk_tiled_private_h

/* logging */
extern int _ewk_tiled_log_dom;

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_ewk_tiled_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_ewk_tiled_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_ewk_tiled_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_ewk_tiled_log_dom, __VA_ARGS__)
#define DBG(...) EINA_LOG_DOM_DBG(_ewk_tiled_log_dom, __VA_ARGS__)
#define OOM(op, size) CRITICAL("could not %s %zd bytes: %s", op, size, strerror(errno))
#define MALLOC_OR_OOM_RET(ptr, size, ...)       \
    do {                                        \
        ptr = malloc(size);                     \
        if (!ptr && (size) > 0) {               \
            OOM("malloc", (size));              \
            return __VA_ARGS__;                 \
        }                                       \
    } while (0)

#define CALLOC_OR_OOM_RET(ptr, size, ...)       \
    do {                                        \
        ptr = calloc(1, size);                  \
        if (!ptr && (size) > 0) {               \
            OOM("calloc", (size));              \
            return __VA_ARGS__;                 \
        }                                       \
    } while (0)

#define REALLOC_OR_OOM_RET(ptr, size, ...)      \
    do {                                        \
        void *__tmp_ptr;                        \
        __tmp_ptr = realloc(ptr, size);         \
        if (!__tmp_ptr && (size) > 0) {         \
            OOM("realloc", (size));             \
            return __VA_ARGS__;                 \
        }                                       \
        ptr = __tmp_ptr;                        \
    } while (0)

#endif // ewk_tiled_private_h

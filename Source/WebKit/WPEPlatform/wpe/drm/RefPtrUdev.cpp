/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RefPtrUdev.h"

#include <libudev.h>

namespace WTF {

struct udev* DefaultRefDerefTraits<struct udev>::refIfNotNull(struct udev* ptr)
{
    if (LIKELY(ptr))
        udev_ref(ptr);
    return ptr;
}

void DefaultRefDerefTraits<struct udev>::derefIfNotNull(struct udev* ptr)
{
    if (LIKELY(ptr))
        udev_unref(ptr);
}

struct udev_device* DefaultRefDerefTraits<struct udev_device>::refIfNotNull(struct udev_device* ptr)
{
    if (LIKELY(ptr))
        udev_device_ref(ptr);
    return ptr;
}

void DefaultRefDerefTraits<struct udev_device>::derefIfNotNull(struct udev_device* ptr)
{
    if (LIKELY(ptr))
        udev_device_unref(ptr);
}

struct udev_enumerate* DefaultRefDerefTraits<struct udev_enumerate>::refIfNotNull(struct udev_enumerate* ptr)
{
    if (LIKELY(ptr))
        udev_enumerate_ref(ptr);
    return ptr;
}

void DefaultRefDerefTraits<struct udev_enumerate>::derefIfNotNull(struct udev_enumerate* ptr)
{
    if (LIKELY(ptr))
        udev_enumerate_unref(ptr);
}

} // namespace WTF

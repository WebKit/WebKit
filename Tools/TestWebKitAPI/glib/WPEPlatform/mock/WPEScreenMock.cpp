/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WPEScreenMock.h"

struct _WPEScreenMock {
    WPEScreen parent;

    gboolean isInvalid;
};
G_DEFINE_FINAL_TYPE(WPEScreenMock, wpe_screen_mock, WPE_TYPE_SCREEN)

static void wpeScreenMockInvalidate(WPEScreen* screen)
{
    auto* screenMock = WPE_SCREEN_MOCK(screen);
    screenMock->isInvalid = TRUE;

    WPE_SCREEN_CLASS(wpe_screen_mock_parent_class)->invalidate(screen);
}

static void wpe_screen_mock_class_init(WPEScreenMockClass* screenMockClass)
{
    WPEScreenClass* screenClass = WPE_SCREEN_CLASS(screenMockClass);
    screenClass->invalidate = wpeScreenMockInvalidate;
}

static void wpe_screen_mock_init(WPEScreenMock*)
{
}

gboolean wpeScreenMockIsInvalid(WPEScreenMock* screenMock)
{
    return screenMock->isInvalid;
}

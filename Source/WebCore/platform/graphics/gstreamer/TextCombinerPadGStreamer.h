/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"

#define WEBKIT_TYPE_TEXT_COMBINER_PAD webkit_text_combiner_pad_get_type()

#define WEBKIT_TEXT_COMBINER_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPad))
#define WEBKIT_TEXT_COMBINER_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPadClass))
#define WEBKIT_IS_TEXT_COMBINER_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD))
#define WEBKIT_IS_TEXT_COMBINER_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_TEXT_COMBINER_PAD))
#define WEBKIT_TEXT_COMBINER_PAD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPadClass))

typedef struct _WebKitTextCombinerPadPrivate WebKitTextCombinerPadPrivate;

struct WebKitTextCombinerPad {
    GstGhostPad parent;
    WebKitTextCombinerPadPrivate* priv;
};

struct WebKitTextCombinerPadClass {
    GstGhostPadClass parent;
};

GType webkit_text_combiner_pad_get_type(void);

GstPad* webKitTextCombinerPadLeakInternalPadRef(WebKitTextCombinerPad*);

#endif

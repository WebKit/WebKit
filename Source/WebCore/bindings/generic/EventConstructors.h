/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventConstructors_h
#define EventConstructors_h

namespace WebCore {

#define SETUP_EVENT_CONFIGURATION_FOR_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY) \
    DICTIONARY_START(Event) \
    FILL_PROPERTY(bubbles) \
    FILL_PROPERTY(cancelable) \
    FILL_PROPERTY(defaultPrevented) \
    FILL_PROPERTY(timeStamp) \
    DICTIONARY_END(Event)

#define SETUP_EVENT_CONFIGURATION_FOR_ALL_EVENTS(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY) \
    SETUP_EVENT_CONFIGURATION_FOR_EVENT(DICTIONARY_START, DICTIONARY_END, FILL_PARENT_PROPERTIES, FILL_PROPERTY)

} // namespace WebCore

#endif // EventConstructors_h

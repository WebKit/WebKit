/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef Gamepad_h
#define Gamepad_h

#if ENABLE(GAMEPAD)

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GamepadButton;
class PlatformGamepad;

class Gamepad: public RefCounted<Gamepad> {
public:
    static PassRefPtr<Gamepad> create(const PlatformGamepad& platformGamepad)
    {
        return adoptRef(new Gamepad(platformGamepad));
    }
    ~Gamepad();

    const String& id() const { return m_id; }
    unsigned index() const { return m_index; }
    const String& mapping() const { return m_mapping; }

    bool connected() const { return m_connected; }
    double timestamp() const { return m_timestamp; }
    const Vector<double>& axes() const;
    const Vector<Ref<GamepadButton>>& buttons() const;

    void updateFromPlatformGamepad(const PlatformGamepad&);

private:
    explicit Gamepad(const PlatformGamepad&);
    String m_id;
    unsigned m_index;
    bool m_connected;
    double m_timestamp;
    String m_mapping;

    Vector<double> m_axes;
    Vector<Ref<GamepadButton>> m_buttons;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD)

#endif // Gamepad_h

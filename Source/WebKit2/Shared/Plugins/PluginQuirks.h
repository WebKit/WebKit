/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef PluginQuirks_h
#define PluginQuirks_h

namespace WebKit {

class PluginQuirks {
public:
    enum PluginQuirk {
        // Mac specific quirks:
#if PLUGIN_ARCHITECTURE(MAC)
        // The plug-in wants the call to getprogame() to return "WebKitPluginHost".
        // Adobe Flash Will not handle key down events otherwise.
        PrognameShouldBeWebKitPluginHost,
        // Supports receiving a paint event, even when using CoreAnimation rendering.
        SupportsSnapshotting,
#elif PLUGIN_ARCHITECTURE(X11)
        // Flash and npwrapper ask the browser about which GTK version does it use
        // and refuse to load and work if it is not GTK 2 so we need to fake it in
        // NPN_GetValue even when it is a lie.
        RequiresGTKToolKit,
#endif
        NumPluginQuirks
    };
    
    PluginQuirks()
        : m_quirks(0)
    {
        COMPILE_ASSERT(sizeof(m_quirks) * 8 >= NumPluginQuirks, not_enough_room_for_quirks);
    }
    
    void add(PluginQuirk quirk)
    {
        ASSERT(quirk >= 0);
        ASSERT(quirk < NumPluginQuirks);
        
        m_quirks |= (1 << quirk);
    }
    
    bool contains(PluginQuirk quirk) const
    {
        return m_quirks & (1 << quirk);
    }

private:
    uint32_t m_quirks;
};

} // namespace WebKit

#endif // PluginQuirkSet_h

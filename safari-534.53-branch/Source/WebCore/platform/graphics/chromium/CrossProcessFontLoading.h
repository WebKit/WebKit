/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CrossProcessFontLoading_h
#define CrossProcessFontLoading_h

#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>

typedef struct CGFont* CGFontRef;
typedef UInt32 ATSFontContainerRef;
typedef UInt32 ATSFontRef;

namespace WebCore {

// MemoryActivatedFont encapsulates a font loaded from another process and
// activated from memory.
//
// Responsibilities:
// * Holder for the CGFontRef & ATSFontRef belonging to the activated font.
// * Responsible for unloading the font container when done.
//
// Memory Management:
// The class is reference counted, with each instance of FontPlatformData that
// uses this class holding a reference to it.
// Entries are kept track of internally in a hash to allow quick lookup
// of existing instances for reuse:
// - fontCacheBySrcFontContainerRef() - key is the ATSFontContainerRef
//   corresponding to the *original in-process NSFont* whose loading was blocked
//   by the sandbox.
//   This is needed to allow lookup of a pre-existing MemoryActivatedFont when
//   creating a new FontPlatformData object.
//
// Assumptions:
// This code assumes that an ATSFontRef is a unique identifier tied to an
// activated font.  i.e. After we activate a font, its ATSFontRef doesn't
// change.
// It also assumes that the ATSFoncontainerRef for two in-memory NSFonts that
// correspond to the same on-disk font file are always the same and don't change
// with time.
//
// Flushing caches:
// When the system notifies us of a system font cache flush, all FontDataCache
// objects are destroyed.  This should in turn dereference all
// MemoryActivatedFonts and thus unload all in-memory fonts.
class MemoryActivatedFont : public RefCounted<MemoryActivatedFont> {
public:
    // Use to create a new object, see docs on constructor below.
    static PassRefPtr<MemoryActivatedFont> create(ATSFontContainerRef srcFontContainerRef, ATSFontContainerRef container);
    ~MemoryActivatedFont();
    
    // Get cached CGFontRef corresponding to the in-memory font.
    CGFontRef cgFont() { return m_cgFont.get(); }
    
    // Get cached ATSFontRef corresponding to the in-memory font.
    ATSFontRef atsFontRef() { return m_atsFontRef; }

private:
    // srcFontRef - ATSFontRef belonging to the NSFont object that failed to
    // load in-process.
    // container - a font container corresponding to an identical font that
    // we loaded cross-process.
    MemoryActivatedFont(ATSFontContainerRef srcFontContainerRef, ATSFontContainerRef container);

    ATSFontContainerRef m_fontContainer;
    WTF::RetainPtr<CGFontRef> m_cgFont;
    ATSFontRef m_atsFontRef;
    ATSFontContainerRef m_srcFontContainerRef;
};

} // namespace WebCore

#endif // CrossProcessFontLoading_h

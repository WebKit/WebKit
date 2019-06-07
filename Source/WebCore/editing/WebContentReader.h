/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "DocumentFragment.h"
#include "Frame.h"
#include "Pasteboard.h"
#include "Range.h"
#include "markup.h"

namespace WebCore {

class ArchiveResource;
class Blob;

class FrameWebContentReader : public PasteboardWebContentReader {
public:
    Frame& frame;

    FrameWebContentReader(Frame& frame)
        : frame(frame)
    {
    }

protected:
    bool shouldSanitize() const;
    MSOListQuirks msoListQuirksForMarkup() const;
};

class WebContentReader final : public FrameWebContentReader {
public:
    Range& context;
    const bool allowPlainText;

    RefPtr<DocumentFragment> fragment;
    bool madeFragmentFromPlainText;

    WebContentReader(Frame& frame, Range& context, bool allowPlainText)
        : FrameWebContentReader(frame)
        , context(context)
        , allowPlainText(allowPlainText)
        , madeFragmentFromPlainText(false)
    {
    }

    void addFragment(Ref<DocumentFragment>&&);

private:
#if PLATFORM(COCOA)
    bool readWebArchive(SharedBuffer&) override;
    bool readFilePath(const String&, PresentationSize preferredPresentationSize = { }, const String& contentType = { }) override;
    bool readFilePaths(const Vector<String>&) override;
    bool readHTML(const String&) override;
    bool readRTFD(SharedBuffer&) override;
    bool readRTF(SharedBuffer&) override;
    bool readImage(Ref<SharedBuffer>&&, const String& type, PresentationSize preferredPresentationSize = { }) override;
    bool readURL(const URL&, const String& title) override;
    bool readDataBuffer(SharedBuffer&, const String& type, const String& name, PresentationSize preferredPresentationSize = { }) override;
#endif
    bool readPlainText(const String&) override;
};

class WebContentMarkupReader final : public FrameWebContentReader {
public:
    String markup;

    explicit WebContentMarkupReader(Frame& frame)
        : FrameWebContentReader(frame)
    {
    }

private:
#if PLATFORM(COCOA)
    bool readWebArchive(SharedBuffer&) override;
    bool readFilePath(const String&, PresentationSize = { }, const String& = { }) override { return false; }
    bool readFilePaths(const Vector<String>&) override { return false; }
    bool readHTML(const String&) override;
    bool readRTFD(SharedBuffer&) override;
    bool readRTF(SharedBuffer&) override;
    bool readImage(Ref<SharedBuffer>&&, const String&, PresentationSize = { }) override { return false; }
    bool readURL(const URL&, const String&) override { return false; }
    bool readDataBuffer(SharedBuffer&, const String&, const String&, PresentationSize = { }) override { return false; }
#endif
    bool readPlainText(const String&) override { return false; }
};

#if PLATFORM(COCOA) && defined(__OBJC__)
struct FragmentAndResources {
    RefPtr<DocumentFragment> fragment;
    Vector<Ref<ArchiveResource>> resources;
};

RefPtr<DocumentFragment> createFragmentAndAddResources(Frame&, NSAttributedString *);
#endif

}

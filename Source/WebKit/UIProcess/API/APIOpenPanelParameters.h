/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/FileChooser.h>

namespace API {

class Array;

class OpenPanelParameters : public API::ObjectImpl<API::Object::Type::OpenPanelParameters> {
public:
    static Ref<OpenPanelParameters> create(const WebCore::FileChooserSettings&);
    ~OpenPanelParameters();

    bool allowDirectories() const { return m_settings.allowsDirectories; }
    bool allowMultipleFiles() const { return m_settings.allowsMultipleFiles; }
    Ref<API::Array> acceptMIMETypes() const;
    Ref<API::Array> acceptFileExtensions() const;
    Ref<API::Array> allowedMIMETypes() const;
    Ref<API::Array> allowedFileExtensions() const;
    Ref<API::Array> selectedFileNames() const;
#if ENABLE(MEDIA_CAPTURE)
    WebCore::MediaCaptureType mediaCaptureType() const { return m_settings.mediaCaptureType; }
#endif

private:
    explicit OpenPanelParameters(const WebCore::FileChooserSettings&);

    WebCore::FileChooserSettings m_settings;
};

} // namespace API

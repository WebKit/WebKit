/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(CONNECTED_VOLUMETRIC_SCENE)

#include <WebCore/ModelPresentationMode.h>
#include <optional>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Element;
class ModelPlayer;
template<typename IDLType> class DOMPromiseDeferred;

// Presents an element's model content in a volumetric scene that coexists with the page.
class ElementVolumetricScene {
public:
    static void requestVolumetricScene(Element&, DOMPromiseDeferred<void>&&);
    static void exitVolumetricScene(Element&);

    static void documentVisibilityDidChange(Element&);

    // Not on the exit request: the volume still holds the hosting claim until it actually closes.
    WEBCORE_EXPORT static void volumetricSceneDidClose(Element&);

    WEBCORE_EXPORT static RefPtr<ModelPlayer> playerForElement(Element&);

    WEBCORE_EXPORT static bool isPresentedInVolumetricScene(Element&);

    WEBCORE_EXPORT static String presentationModeForTesting(Element&);

private:
    static std::optional<ModelPresentationMode> presentationMode(Element&);
    static void setPresentationMode(Element&, ModelPresentationMode);
};

} // namespace WebCore

#endif // ENABLE(CONNECTED_VOLUMETRIC_SCENE)

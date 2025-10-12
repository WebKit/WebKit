/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(SCENEKIT)

#import "SceneKitModelLoader.h"

#import "MIMETypeRegistry.h"
#import "Model.h"
#import "ResourceError.h"
#import "SceneKitModelLoaderClient.h"
#import "SceneKitModelLoaderUSD.h"

namespace WebCore {

// Defining trivial virtual destructor in implementation to pin vtable.
SceneKitModelLoader::~SceneKitModelLoader() = default;

// No-op loader used for the cases where no loading actually happens (such as unsupported MIME type).
class SceneKitModelLoaderFailure final : public SceneKitModelLoader {
public:
    static Ref<SceneKitModelLoaderFailure> create(ResourceError error)
    {
        return adoptRef(*new SceneKitModelLoaderFailure(WTFMove(error)));
    }

    virtual ~SceneKitModelLoaderFailure() = default;
    virtual void cancel() override { }

    const ResourceError& error() const
    {
        return m_error;
    }

private:
    SceneKitModelLoaderFailure(ResourceError error)
        : m_error { WTFMove(error) }
    {
    }

    ResourceError m_error;
};


}

namespace WebCore {

static String mimeTypeUtilizingFileExtensionOverridingForLocalFiles(const Model& modelSource)
{
    if (modelSource.url().protocolIsFile() && (modelSource.mimeType().isEmpty() || modelSource.mimeType() == WebCore::defaultMIMEType())) {
        // FIXME: Getting the file extension from a URL seems like it should be in shared code.
        auto lastPathComponent = modelSource.url().lastPathComponent();
        auto position = lastPathComponent.reverseFind('.');
        if (position != WTF::notFound) {
            auto extension = lastPathComponent.substring(position + 1);

            return MIMETypeRegistry::mediaMIMETypeForExtension(extension);
        }
    }

    return modelSource.mimeType();
}

enum class ModelType {
    USD,
    Unknown
};

static ModelType modelType(Model& modelSource)
{
    auto mimeType = mimeTypeUtilizingFileExtensionOverridingForLocalFiles(modelSource);

    if (WebCore::MIMETypeRegistry::isUSDMIMEType(mimeType))
        return ModelType::USD;

    return ModelType::Unknown;
}

Ref<SceneKitModelLoader> loadSceneKitModel(Model& modelSource, SceneKitModelLoaderClient& client)
{
    switch (modelType(modelSource)) {
    case ModelType::USD:
        return loadSceneKitModelUsingUSDLoader(modelSource, client);
    case ModelType::Unknown:
        break;
    }

    auto loader = SceneKitModelLoaderFailure::create([NSError errorWithDomain:@"SceneKitModelLoader" code:-1 userInfo:@{
        NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Unsupported MIME type: %s.", modelSource.mimeType().utf8().data()],
        NSURLErrorFailingURLErrorKey: modelSource.url().createNSURL().get()
    }]);

    dispatch_async(mainDispatchQueueSingleton(), [weakClient = WeakPtr { client }, loader] {
        auto strongClient = weakClient.get();
        if (!strongClient)
            return;

        strongClient->didFailLoading(loader.get(), loader->error());
    });

    return loader;
}

}

#endif

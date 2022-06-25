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

#import "SceneKitModelLoaderUSD.h"

#import "Model.h"
#import "ResourceError.h"
#import "SceneKitModel.h"
#import "SceneKitModelLoader.h"
#import "SceneKitModelLoaderClient.h"
#import <pal/spi/cocoa/SceneKitSPI.h>

namespace WebCore {

class SceneKitModelLoaderUSD final : public SceneKitModelLoader {
public:
    static Ref<SceneKitModelLoaderUSD> create()
    {
        return adoptRef(*new SceneKitModelLoaderUSD());
    }

    virtual ~SceneKitModelLoaderUSD() = default;
    virtual void cancel() final { m_canceled = true; }

    bool isCanceled() const { return m_canceled; }

private:
    SceneKitModelLoaderUSD()
        : m_canceled { false }
    {
    }

    bool m_canceled;
};

class SceneKitModelUSD final : public SceneKitModel {
public:
    static Ref<SceneKitModelUSD> create(Ref<Model> modelSource, RetainPtr<SCNScene> scene)
    {
        return adoptRef(*new SceneKitModelUSD(WTFMove(modelSource), WTFMove(scene)));
    }

    virtual ~SceneKitModelUSD() = default;

private:
    SceneKitModelUSD(Ref<Model> modelSource, RetainPtr<SCNScene> scene)
        : m_modelSource { WTFMove(modelSource) }
        , m_scene { WTFMove(scene) }
    {
    }

    // SceneKitModel overrides.
    virtual const Model& modelSource() const override
    {
        return m_modelSource.get();
    }

    virtual SCNScene *defaultScene() const override
    {
        return m_scene.get();
    }

    virtual NSArray<SCNScene *> *scenes() const override
    {
        return @[ m_scene.get() ];
    }

    Ref<Model> m_modelSource;
    RetainPtr<SCNScene> m_scene;
};

static RetainPtr<NSURL> writeToTemporaryFile(WebCore::Model& modelSource)
{
    // FIXME: DO NOT SHIP!!! We must not write these to disk; we need SceneKit
    // to support reading USD files from its [SCNSceneSource initWithData:options:],
    // initializer but currently that does not work.

    FileSystem::PlatformFileHandle fileHandle;
    auto filePath = FileSystem::openTemporaryFile("ModelFile"_s, fileHandle, ".usdz"_s);
    ASSERT(FileSystem::isHandleValid(fileHandle));

    size_t byteCount = FileSystem::writeToFile(fileHandle, modelSource.data()->makeContiguous()->data(), modelSource.data()->size());
    ASSERT_UNUSED(byteCount, byteCount == modelSource.data()->size());
    FileSystem::closeFile(fileHandle);

    return adoptNS([[NSURL alloc] initFileURLWithPath:filePath]);
}

Ref<SceneKitModelLoader> loadSceneKitModelUsingUSDLoader(Model& modelSource, SceneKitModelLoaderClient& client)
{
    auto loader = SceneKitModelLoaderUSD::create();
    
    dispatch_async(dispatch_get_main_queue(), [weakClient = WeakPtr { client }, loader, modelSource = Ref { modelSource }] () mutable {
        // If the client has gone away, there is no reason to do any work.
        auto strongClient = weakClient.get();
        if (!strongClient)
            return;

        // If the caller has canceled the load, there is no reason to do any work.
        if (loader->isCanceled())
            return;

        auto url = writeToTemporaryFile(modelSource.get());

        auto source = adoptNS([[SCNSceneSource alloc] initWithURL:url.get() options:nil]);
        NSError *error = nil;
        RetainPtr scene = [source sceneWithOptions:@{ } error:&error];

        if (error) {
            strongClient->didFailLoading(loader.get(), ResourceError(error));
            [error release];
            return;
        }

        ASSERT(scene);

        strongClient->didFinishLoading(loader.get(), SceneKitModelUSD::create(WTFMove(modelSource), WTFMove(scene)));
    });

    return loader;
}

}

#endif

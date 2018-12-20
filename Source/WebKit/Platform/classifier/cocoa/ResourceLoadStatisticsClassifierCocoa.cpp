/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ResourceLoadStatisticsClassifierCocoa.h"

#if HAVE(CORE_PREDICTION)

#include "CorePredictionSPI.h"
#include "Logging.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/darwin/WeakLinking.h>

namespace WebKit {

bool ResourceLoadStatisticsClassifierCocoa::classify(unsigned subresourceUnderTopFrameOriginsCount, unsigned subresourceUniqueRedirectsToCount, unsigned subframeUnderTopFrameOriginsCount)
{
    if (!canUseCorePrediction())
        return classifyWithVectorThreshold(subresourceUnderTopFrameOriginsCount, subresourceUniqueRedirectsToCount, subframeUnderTopFrameOriginsCount);

    Vector<svm_node> features;

    if (subresourceUnderTopFrameOriginsCount)
        features.append({1, static_cast<double>(subresourceUnderTopFrameOriginsCount)});
    if (subresourceUniqueRedirectsToCount)
        features.append({2, static_cast<double>(subresourceUniqueRedirectsToCount)});
    if (subframeUnderTopFrameOriginsCount)
        features.append({3, static_cast<double>(subframeUnderTopFrameOriginsCount)});

    // Add termination node with index -1.
    features.append({-1, -1});

    double score;
    int classification = svm_predict_values(singletonPredictionModel(), features.data(), &score);
    LOG(ResourceLoadStatistics, "ResourceLoadStatisticsClassifierCocoa::classify(): Classified with CorePrediction.");
    return classification < 0;
}

String ResourceLoadStatisticsClassifierCocoa::storagePath()
{
    CFBundleRef webKitBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit"));
    RetainPtr<CFURLRef> resourceUrl = adoptCF(CFBundleCopyResourcesDirectoryURL(webKitBundle));
    resourceUrl = adoptCF(CFURLCreateCopyAppendingPathComponent(nullptr, resourceUrl.get(), CFSTR("corePrediction_model"), false));
    CFErrorRef error = nullptr;
    resourceUrl = adoptCF(CFURLCreateFilePathURL(nullptr, resourceUrl.get(), &error));

    if (error || !resourceUrl)
        return String();

    RetainPtr<CFStringRef> resourceUrlString = adoptCF(CFURLCopyFileSystemPath(resourceUrl.get(), kCFURLPOSIXPathStyle));
    return String(resourceUrlString.get());
}

bool ResourceLoadStatisticsClassifierCocoa::canUseCorePrediction()
{
    if (m_haveLoadedModel)
        return true;

    if (!m_useCorePrediction)
        return false;

    if (isNullFunctionPointer(svm_load_model)) {
        m_useCorePrediction = false;
        return false;
    }

    String storagePathStr = storagePath();
    if (storagePathStr.isNull() || storagePathStr.isEmpty()) {
        m_useCorePrediction = false;
        return false;
    }

    if (singletonPredictionModel()) {
        m_haveLoadedModel = true;
        return true;
    }

    m_useCorePrediction = false;
    return false;
}

const struct svm_model* ResourceLoadStatisticsClassifierCocoa::singletonPredictionModel()
{
    static Optional<struct svm_model*> corePredictionModel;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        auto path = storagePath();
        if (path.isEmpty())
            return;

        corePredictionModel = svm_load_model(path.utf8().data());
    });

    if (corePredictionModel && corePredictionModel.value())
        return corePredictionModel.value();

    WTFLogAlways("ResourceLoadStatisticsClassifierCocoa::singletonPredictionModel(): Couldn't load model file at path %s.", storagePath().utf8().data());
    m_useCorePrediction = false;
    return nullptr;
}
}

#endif

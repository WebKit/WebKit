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

#include "Logging.h"

namespace WebKit {

bool ResourceLoadStatisticsClassifier::classify(const unsigned subresourceUnderTopFrameOriginsCount, const unsigned subresourceUniqueRedirectsToCount, const unsigned subframeUnderTopFrameOriginsCount)
{
    if (!shouldUseCorePrediction())
        return classifyWithVectorThreshold(subresourceUnderTopFrameOriginsCount, subresourceUniqueRedirectsToCount, subframeUnderTopFrameOriginsCount);

#if HAVE(CORE_PREDICTION)
    Vector<unsigned> nonZeroFeatures;
    Vector<unsigned> indices;

    if (subresourceUnderTopFrameOriginsCount) {
        nonZeroFeatures.append(subresourceUnderTopFrameOriginsCount);
        indices.append(1);
    }
    if (subresourceUniqueRedirectsToCount) {
        nonZeroFeatures.append(subresourceUniqueRedirectsToCount);
        indices.append(2);
    }
    if (subframeUnderTopFrameOriginsCount) {
        nonZeroFeatures.append(subframeUnderTopFrameOriginsCount);
        indices.append(3);
    }

    svm_node* exampleVector = new svm_node[nonZeroFeatures.size() + 1];
    for (size_t i = 0; i < nonZeroFeatures.size(); i++) {
        exampleVector[i].index = indices.at(i);
        exampleVector[i].value = nonZeroFeatures.at(i);
    }
    // Add termination node with index -1.
    exampleVector[nonZeroFeatures.size()].index = -1;
    exampleVector[nonZeroFeatures.size()].value = -1;

    int classification;
    double score;
    classification = svm_predict_values(m_corePredictionModel, exampleVector, &score);
    delete[] exampleVector;
    return classification < 0;
#endif
    return false;
}

String ResourceLoadStatisticsClassifier::storagePath()
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

bool ResourceLoadStatisticsClassifier::shouldUseCorePrediction()
{
#if HAVE(CORE_PREDICTION)
    if (m_corePredictionModel)
        return true;

    if (!m_useCorePrediction)
        return false;

    String storagePathStr = storagePath();
    if (storagePathStr.isNull() || storagePathStr.isEmpty()) {
        m_useCorePrediction = false;
        return false;
    }

    m_corePredictionModel = svm_load_model((storagePathStr).utf8().data());
    
    if (m_corePredictionModel)
        return true;

    WTFLogAlways("ResourceLoadStatisticsClassifier::shouldUseCorePrediction(): Couldn't load model file at path %s.", storagePathStr.utf8().data());
    m_useCorePrediction = false;
#endif
    return false;
}

}

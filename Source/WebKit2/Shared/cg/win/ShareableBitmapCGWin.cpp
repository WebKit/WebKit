/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "ShareableBitmap.h"

namespace WebKit {

static void derefSharedMemory(void* typelessSharedMemory, const void*, size_t)
{
    ASSERT_ARG(typelessSharedMemory, typelessSharedMemory);

    // Balanced by leakRef in makeCGImageCopy.
    RefPtr<SharedMemory> memory = adoptRef(static_cast<SharedMemory*>(typelessSharedMemory));
}

RetainPtr<CGImageRef> ShareableBitmap::makeCGImageCopy()
{
    RetainPtr<CGDataProviderRef> dataProvider;
    if (isBackedBySharedMemory()) {
        if (RefPtr<SharedMemory> copyOnWriteMemory = m_sharedMemory->createCopyOnWriteCopy(sizeInBytes())) {
            RefPtr<SharedMemory> originalMemory = m_sharedMemory.release();
            // Writes to originalMemory will affect copyOnWriteMemory (until copyOnWriteMemory is
            // written to for the first time), but writes to copyOnWriteMemory will not affect
            // originalMemory. So we wrap originalMemory in a CGImage (which will never write to
            // it) and use copyOnWriteMemory to back this ShareableBitmap from here on out. That
            // way, writes to this ShareableBitmap will not affect the CGImage.
            m_sharedMemory = copyOnWriteMemory.release();
            dataProvider.adoptCF(CGDataProviderCreateWithData(originalMemory.get(), originalMemory->data(), sizeInBytes(), derefSharedMemory));
            // Balanced by adoptRef in derefSharedMemory.
            originalMemory.release().leakRef();
        }
    }
    if (!dataProvider) {
        // We weren't able to make a copy-on-write copy, so we'll just have to fall back to a
        // normal copy (which is more expensive, but the best we can do).
        RetainPtr<CFDataRef> cfData(AdoptCF, CFDataCreate(0, static_cast<UInt8*>(data()), sizeInBytes()));
        dataProvider.adoptCF(CGDataProviderCreateWithCFData(cfData.get()));
    }
    ASSERT(dataProvider);
    return createCGImage(dataProvider.get());
}

} // namespace WebKit

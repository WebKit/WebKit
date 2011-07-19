/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WaveShaperDSPKernel_h
#define WaveShaperDSPKernel_h

#include "AudioDSPKernel.h"
#include "WaveShaperProcessor.h"

namespace WebCore {

class WaveShaperProcessor;

// WaveShaperDSPKernel is an AudioDSPKernel and is responsible for non-linear distortion on one channel.

class WaveShaperDSPKernel : public AudioDSPKernel {
public:  
    WaveShaperDSPKernel(WaveShaperProcessor* processor)
    : AudioDSPKernel(processor)
    {
    }
    
    // AudioDSPKernel
    virtual void process(const float* source, float* dest, size_t framesToProcess);
    virtual void reset() { }
    
protected:
    WaveShaperProcessor* waveShaperProcessor() { return static_cast<WaveShaperProcessor*>(processor()); }
};

} // namespace WebCore

#endif // WaveShaperDSPKernel_h

/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "src/gpu/ganesh/GrRenderTarget.h"

#include "src/core/SkRectPriv.h"
#include "src/gpu/ganesh/GrAttachment.h"
#include "src/gpu/ganesh/GrBackendUtils.h"
#include "src/gpu/ganesh/GrGpu.h"
#include "src/gpu/ganesh/GrStencilSettings.h"

GrRenderTarget::GrRenderTarget(GrGpu* gpu,
                               const SkISize& dimensions,
                               int sampleCount,
                               GrProtected isProtected,
                               std::string_view label,
                               sk_sp<GrAttachment> stencil)
        : INHERITED(gpu, dimensions, isProtected, label)
        , fSampleCnt(sampleCount) {
    if (this->numSamples() > 1) {
        fMSAAStencilAttachment = std::move(stencil);
    } else {
        fStencilAttachment = std::move(stencil);
    }
}

GrRenderTarget::~GrRenderTarget() = default;

void GrRenderTarget::onRelease() {
    fStencilAttachment = nullptr;
    fMSAAStencilAttachment = nullptr;

    INHERITED::onRelease();
}

void GrRenderTarget::onAbandon() {
    fStencilAttachment = nullptr;
    fMSAAStencilAttachment = nullptr;

    INHERITED::onAbandon();
}

void GrRenderTarget::attachStencilAttachment(sk_sp<GrAttachment> stencil, bool useMSAASurface) {
    auto stencilAttachment = (useMSAASurface) ? &GrRenderTarget::fMSAAStencilAttachment
                                              : &GrRenderTarget::fStencilAttachment;
    if (!stencil && !(this->*stencilAttachment)) {
        // No need to do any work since we currently don't have a stencil attachment and
        // we're not actually adding one.
        return;
    }

    if (!this->completeStencilAttachment(stencil.get(), useMSAASurface)) {
        return;
    }

    this->*stencilAttachment = std::move(stencil);
}

int GrRenderTarget::numStencilBits(bool useMSAASurface) const {
    return GrBackendFormatStencilBits(this->getStencilAttachment(useMSAASurface)->backendFormat());
}

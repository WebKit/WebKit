/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ElementContext.h"
#include "IntRect.h"
#include "ProcessIdentifier.h"
#include <wtf/EnumTraits.h>

namespace WebCore {

enum FrameState {
    FrameStateProvisional,
    // This state indicates we are ready to commit to a page,
    // which means the view will transition to use the new data source.
    FrameStateCommittedPage,
    FrameStateComplete
};

enum class PolicyAction : uint8_t {
    Use,
    Download,
    Ignore,
    StopAllLoads
};

enum class ReloadOption : uint8_t {
    ExpiredOnly = 1 << 0,
    FromOrigin  = 1 << 1,
    DisableContentBlockers = 1 << 2,
};

enum class FrameLoadType : uint8_t {
    Standard,
    Back,
    Forward,
    IndexedBackForward, // a multi-item hop in the backforward list
    Reload,
    Same, // user loads same URL again (but not reload button)
    RedirectWithLockedBackForwardList, // FIXME: Merge "lockBackForwardList", "lockHistory", "quickRedirect" and "clientRedirect" into a single concept of redirect.
    Replace,
    ReloadFromOrigin,
    ReloadExpiredOnly
};

enum class WillContinueLoading : bool { No, Yes };

class PolicyCheckIdentifier {
public:
    PolicyCheckIdentifier() = default;

    static PolicyCheckIdentifier create();

    bool isValidFor(PolicyCheckIdentifier);
    bool operator==(const PolicyCheckIdentifier& other) const { return m_process == other.m_process && m_policyCheck == other.m_policyCheck; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PolicyCheckIdentifier> decode(Decoder&);

private:
    PolicyCheckIdentifier(ProcessIdentifier process, uint64_t policyCheck)
        : m_process(process)
        , m_policyCheck(policyCheck)
    { }

    ProcessIdentifier m_process;
    uint64_t m_policyCheck { 0 };
};

template<class Encoder>
void PolicyCheckIdentifier::encode(Encoder& encoder) const
{
    encoder << m_process << m_policyCheck;
}

template<class Decoder>
Optional<PolicyCheckIdentifier> PolicyCheckIdentifier::decode(Decoder& decoder)
{
    auto process = ProcessIdentifier::decode(decoder);
    if (!process)
        return WTF::nullopt;

    uint64_t policyCheck;
    if (!decoder.decode(policyCheck))
        return WTF::nullopt;

    return PolicyCheckIdentifier { *process, policyCheck };
}

enum class ShouldContinuePolicyCheck : bool {
    Yes,
    No
};

enum class NewFrameOpenerPolicy : uint8_t {
    Suppress,
    Allow
};

enum class NavigationType : uint8_t {
    LinkClicked,
    FormSubmitted,
    BackForward,
    Reload,
    FormResubmitted,
    Other
};

enum class ShouldOpenExternalURLsPolicy : uint8_t {
    ShouldNotAllow,
    ShouldAllowExternalSchemesButNotAppLinks,
    ShouldAllow,
};

enum class InitiatedByMainFrame : uint8_t {
    Yes,
    Unknown,
};

enum ClearProvisionalItemPolicy {
    ShouldClearProvisionalItem,
    ShouldNotClearProvisionalItem
};

enum class StopLoadingPolicy {
    PreventDuringUnloadEvents,
    AlwaysStopLoading
};

enum class ObjectContentType : uint8_t {
    None,
    Image,
    Frame,
    PlugIn,
};

enum UnloadEventPolicy {
    UnloadEventPolicyNone,
    UnloadEventPolicyUnloadOnly,
    UnloadEventPolicyUnloadAndPageHide
};

// Passed to FrameLoader::urlSelected() and ScriptController::executeIfJavaScriptURL()
// to control whether, in the case of a JavaScript URL, executeIfJavaScriptURL() should
// replace the document. It is a FIXME to eliminate this extra parameter from
// executeIfJavaScriptURL(), in which case this enum can go away.
enum ShouldReplaceDocumentIfJavaScriptURL {
    ReplaceDocumentIfJavaScriptURL,
    DoNotReplaceDocumentIfJavaScriptURL
};

enum class WebGLLoadPolicy : uint8_t {
    WebGLBlockCreation,
    WebGLAllowCreation,
    WebGLPendingCreation
};

enum class LockHistory : bool { No, Yes };
enum class LockBackForwardList : bool { No, Yes };
enum class AllowNavigationToInvalidURL : bool { No, Yes };
enum class HasInsecureContent : bool { No, Yes };

struct SystemPreviewInfo {
    ElementContext element;

    IntRect previewRect;
    bool isPreview { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<SystemPreviewInfo> decode(Decoder&);
};

template<class Encoder>
void SystemPreviewInfo::encode(Encoder& encoder) const
{
    encoder << element << previewRect << isPreview;
}

template<class Decoder>
Optional<SystemPreviewInfo> SystemPreviewInfo::decode(Decoder& decoder)
{
    Optional<ElementContext> element;
    decoder >> element;
    if (!element)
        return WTF::nullopt;

    Optional<IntRect> previewRect;
    decoder >> previewRect;
    if (!previewRect)
        return WTF::nullopt;

    Optional<bool> isPreview;
    decoder >> isPreview;
    if (!isPreview)
        return WTF::nullopt;

    return { { WTFMove(*element), WTFMove(*previewRect), WTFMove(*isPreview) } };
}

enum class LoadCompletionType : bool {
    Finish,
    Cancel
};

enum class AllowsContentJavaScript : bool {
    No,
    Yes,
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::FrameLoadType> {
    using values = EnumValues<
        WebCore::FrameLoadType,
        WebCore::FrameLoadType::Standard,
        WebCore::FrameLoadType::Back,
        WebCore::FrameLoadType::Forward,
        WebCore::FrameLoadType::IndexedBackForward,
        WebCore::FrameLoadType::Reload,
        WebCore::FrameLoadType::Same,
        WebCore::FrameLoadType::RedirectWithLockedBackForwardList,
        WebCore::FrameLoadType::Replace,
        WebCore::FrameLoadType::ReloadFromOrigin,
        WebCore::FrameLoadType::ReloadExpiredOnly
    >;
};

template<> struct EnumTraits<WebCore::NavigationType> {
    using values = EnumValues<
        WebCore::NavigationType,
        WebCore::NavigationType::LinkClicked,
        WebCore::NavigationType::FormSubmitted,
        WebCore::NavigationType::BackForward,
        WebCore::NavigationType::Reload,
        WebCore::NavigationType::FormResubmitted,
        WebCore::NavigationType::Other
    >;
};

template<> struct EnumTraits<WebCore::PolicyAction> {
    using values = EnumValues<
        WebCore::PolicyAction,
        WebCore::PolicyAction::Use,
        WebCore::PolicyAction::Download,
        WebCore::PolicyAction::Ignore,
        WebCore::PolicyAction::StopAllLoads
    >;
};

template<> struct EnumTraits<WebCore::ShouldOpenExternalURLsPolicy> {
    using values = EnumValues<
        WebCore::ShouldOpenExternalURLsPolicy,
        WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow,
        WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks,
        WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow
    >;
};

template<> struct EnumTraits<WebCore::WebGLLoadPolicy> {
    using values = EnumValues<
        WebCore::WebGLLoadPolicy,
        WebCore::WebGLLoadPolicy::WebGLBlockCreation,
        WebCore::WebGLLoadPolicy::WebGLAllowCreation,
        WebCore::WebGLLoadPolicy::WebGLPendingCreation
    >;
};

} // namespace WTF

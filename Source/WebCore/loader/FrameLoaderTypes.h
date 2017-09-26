/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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

namespace WebCore {

enum FrameState {
    FrameStateProvisional,
    // This state indicates we are ready to commit to a page,
    // which means the view will transition to use the new data source.
    FrameStateCommittedPage,
    FrameStateComplete
};

enum class PolicyAction {
    Use,
    Download,
    Ignore
};

enum class ReloadOption {
    ExpiredOnly = 1 << 0,
    FromOrigin  = 1 << 1,
    DisableContentBlockers = 1 << 2,
};

enum class FrameLoadType {
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

enum class NewFrameOpenerPolicy {
    Suppress,
    Allow
};

enum class NavigationType {
    LinkClicked,
    FormSubmitted,
    BackForward,
    Reload,
    FormResubmitted,
    Other
};

enum class ShouldOpenExternalURLsPolicy {
    ShouldNotAllow,
    ShouldAllowExternalSchemes,
    ShouldAllow,
};

enum class InitiatedByMainFrame {
    Yes,
    Unknown,
};

enum ClearProvisionalItemPolicy {
    ShouldClearProvisionalItem,
    ShouldNotClearProvisionalItem
};

enum class ObjectContentType {
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

enum ShouldSendReferrer {
    MaybeSendReferrer,
    NeverSendReferrer
};

// Passed to FrameLoader::urlSelected() and ScriptController::executeIfJavaScriptURL()
// to control whether, in the case of a JavaScript URL, executeIfJavaScriptURL() should
// replace the document. It is a FIXME to eliminate this extra parameter from
// executeIfJavaScriptURL(), in which case this enum can go away.
enum ShouldReplaceDocumentIfJavaScriptURL {
    ReplaceDocumentIfJavaScriptURL,
    DoNotReplaceDocumentIfJavaScriptURL
};

enum WebGLLoadPolicy {
    WebGLBlockCreation,
    WebGLAllowCreation,
    WebGLPendingCreation
};

enum class LockHistory {
    Yes,
    No
};

enum class LockBackForwardList {
    Yes,
    No
};

enum class AllowNavigationToInvalidURL {
    Yes,
    No
};

enum class HasInsecureContent {
    Yes,
    No,
};

} // namespace WebCore

namespace WTF {

template<typename> struct EnumTraits;
template<typename E, E...> struct EnumValues;

template<> struct EnumTraits<WebCore::PolicyAction> {
    using values = EnumValues<
        WebCore::PolicyAction,
        WebCore::PolicyAction::Use,
        WebCore::PolicyAction::Download,
        WebCore::PolicyAction::Ignore
    >;
};

} // namespace WTF

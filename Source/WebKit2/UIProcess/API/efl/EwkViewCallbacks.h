/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef EwkViewCallbacks_h
#define EwkViewCallbacks_h

#include "ewk_private.h"
#include <Evas.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

struct Ewk_Download_Job;
struct Ewk_Download_Job_Error;
struct Ewk_File_Chooser_Request;
struct Ewk_Form_Submission_Request;
struct Ewk_Error;
struct Ewk_Resource_Request;
struct Ewk_Resource_Load_Response;
struct Ewk_Resource_Load_Error;
struct Ewk_Resource;
struct Ewk_Navigation_Policy_Decision;
#if ENABLE(WEB_INTENTS)
struct Ewk_Intent;
#endif
#if ENABLE(WEB_INTENTS_TAG)
struct Ewk_Intent_Service;
#endif

namespace EwkViewCallbacks {

enum CallbackType {
    BackForwardListChange,
    DownloadJobCancelled,
    DownloadJobFailed,
    DownloadJobFinished,
    DownloadJobRequested,
    FileChooserRequest,
    NewFormSubmissionRequest,
    LoadError,
    LoadFinished,
    LoadProgress,
    ProvisionalLoadFailed,
    ProvisionalLoadRedirect,
    ProvisionalLoadStarted,
    ResourceLoadStarted,
    ResourceLoadResponse,
    ResourceLoadFailed,
    ResourceLoadFinished,
    ResourceRequestSent,
    NavigationPolicyDecision,
    NewWindowPolicyDecision,
    TextFound,
    TitleChange,
    TooltipTextUnset,
    TooltipTextSet,
    WebProcessCrashed,
#if ENABLE(WEB_INTENTS)
    IntentRequest,
#endif
#if ENABLE(WEB_INTENTS_TAG)
    IntentServiceRegistration,
#endif
};

template<typename T>
inline bool callbackArgumentsExpected()
{
    return true;
}

template<>
inline bool callbackArgumentsExpected<void>()
{
    return false;
}

template <CallbackType>
struct CallBackInfo {
    typedef void* Type;

    static inline const char* name()
    {
        ASSERT_NOT_REACHED();
        return "";
    }
};

template <CallbackType callbackType>
class CallBack {
public:
    typedef typename CallBackInfo<callbackType>::Type ArgType;

    explicit CallBack(Evas_Object* view)
        : m_view(view)
    {
        ASSERT(m_view);
    }

    void call(ArgType* argument = 0)
    {
        if (argument && !callbackArgumentsExpected<ArgType>()) {
            CRITICAL("should not pass arguments for this callback!");
            ASSERT_NOT_REACHED();
            return;
        }

        evas_object_smart_callback_call(m_view, CallBackInfo<callbackType>::name(), static_cast<void*>(argument));
    }

    void call(const String& arg)
    {
        call(const_cast<char*>(arg.utf8().data()));
    }

private:
    Evas_Object* m_view;
};

#define DECLARE_EWK_VIEW_CALLBACK(callbackType, string, type) \
template <>                                                   \
struct CallBackInfo<callbackType> {                           \
    typedef type Type;                                        \
    static inline const char* name() { return string; }       \
}

// Note: type 'void' means that no arguments are expected.
DECLARE_EWK_VIEW_CALLBACK(BackForwardListChange, "back,forward,list,changed", void);
DECLARE_EWK_VIEW_CALLBACK(DownloadJobCancelled, "download,cancelled", Ewk_Download_Job);
DECLARE_EWK_VIEW_CALLBACK(DownloadJobFailed, "download,failed", Ewk_Download_Job_Error);
DECLARE_EWK_VIEW_CALLBACK(DownloadJobFinished, "download,finished", Ewk_Download_Job);
DECLARE_EWK_VIEW_CALLBACK(DownloadJobRequested, "download,request", Ewk_Download_Job);
DECLARE_EWK_VIEW_CALLBACK(FileChooserRequest, "file,chooser,request", Ewk_File_Chooser_Request);
DECLARE_EWK_VIEW_CALLBACK(NewFormSubmissionRequest, "form,submission,request", Ewk_Form_Submission_Request);
DECLARE_EWK_VIEW_CALLBACK(LoadError, "load,error", Ewk_Error);
DECLARE_EWK_VIEW_CALLBACK(LoadFinished, "load,finished", void);
DECLARE_EWK_VIEW_CALLBACK(LoadProgress, "load,progress", double);
DECLARE_EWK_VIEW_CALLBACK(ProvisionalLoadFailed, "load,provisional,failed", Ewk_Error);
DECLARE_EWK_VIEW_CALLBACK(ProvisionalLoadRedirect, "load,provisional,redirect", void);
DECLARE_EWK_VIEW_CALLBACK(ProvisionalLoadStarted, "load,provisional,started", void);
DECLARE_EWK_VIEW_CALLBACK(NavigationPolicyDecision, "policy,decision,navigation", Ewk_Navigation_Policy_Decision);
DECLARE_EWK_VIEW_CALLBACK(NewWindowPolicyDecision, "policy,decision,new,window", Ewk_Navigation_Policy_Decision);
DECLARE_EWK_VIEW_CALLBACK(ResourceLoadStarted, "resource,request,new", Ewk_Resource_Request);
DECLARE_EWK_VIEW_CALLBACK(ResourceLoadResponse, "resource,request,response", Ewk_Resource_Load_Response);
DECLARE_EWK_VIEW_CALLBACK(ResourceLoadFailed, "resource,request,failed", Ewk_Resource_Load_Error);
DECLARE_EWK_VIEW_CALLBACK(ResourceLoadFinished, "resource,request,finished", Ewk_Resource);
DECLARE_EWK_VIEW_CALLBACK(ResourceRequestSent, "resource,request,sent", Ewk_Resource_Request);
DECLARE_EWK_VIEW_CALLBACK(TextFound, "text,found", unsigned);
DECLARE_EWK_VIEW_CALLBACK(TitleChange, "title,changed", char);
DECLARE_EWK_VIEW_CALLBACK(TooltipTextUnset, "tooltip,text,unset", void);
DECLARE_EWK_VIEW_CALLBACK(TooltipTextSet, "tooltip,text,set", char);
DECLARE_EWK_VIEW_CALLBACK(WebProcessCrashed, "webprocess,crashed", bool);
#if ENABLE(WEB_INTENTS)
DECLARE_EWK_VIEW_CALLBACK(IntentRequest, "intent,request,new", Ewk_Intent);
#endif
#if ENABLE(WEB_INTENTS_TAG)
DECLARE_EWK_VIEW_CALLBACK(IntentServiceRegistration, "intent,service,register", Ewk_Intent_Service);
#endif

}

#endif // #ifndef EwkViewCallbacks_h

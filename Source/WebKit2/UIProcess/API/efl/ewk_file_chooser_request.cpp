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

#include "config.h"
#include "ewk_file_chooser_request.h"

#include "ImmutableArray.h"
#include "MutableArray.h"
#include "WebOpenPanelParameters.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebString.h"
#include "WebURL.h"
#include "ewk_file_chooser_request_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

EwkFileChooserRequest::EwkFileChooserRequest(WebOpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
    : m_parameters(parameters)
    , m_listener(listener)
    , m_wasRequestHandled(false)
{
    ASSERT(parameters);
    ASSERT(listener);
}

EwkFileChooserRequest::~EwkFileChooserRequest()
{
    if (!m_wasRequestHandled)
        m_listener->cancel();
}

bool EwkFileChooserRequest::allowMultipleFiles() const
{
    return m_parameters->allowMultipleFiles();
}

PassRefPtr<ImmutableArray> EwkFileChooserRequest::acceptedMIMETypes() const
{
    return m_parameters->acceptMIMETypes();
}

void EwkFileChooserRequest::cancel()
{
    m_wasRequestHandled = true;

    return m_listener->cancel();
}

void EwkFileChooserRequest::chooseFiles(Vector< RefPtr<APIObject> >& fileURLs)
{
    ASSERT(!fileURLs.isEmpty());
    ASSERT(fileURLs.size() == 1 || m_parameters->allowMultipleFiles());

    m_wasRequestHandled = true;
    m_listener->chooseFiles(ImmutableArray::adopt(fileURLs).get());
}

Eina_Bool ewk_file_chooser_request_allow_multiple_files_get(const Ewk_File_Chooser_Request* request)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkFileChooserRequest, request, impl, false);

    return impl->allowMultipleFiles();
}

Eina_List* ewk_file_chooser_request_accepted_mimetypes_get(const Ewk_File_Chooser_Request* request)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkFileChooserRequest, request, impl, 0);

    Eina_List* mimeTypeList = 0;
    RefPtr<ImmutableArray> mimeTypes = impl->acceptedMIMETypes();

    const size_t size = mimeTypes->size();
    for (size_t i = 0; i < size; ++i) {
        String mimeTypeString = static_cast<WebString*>(mimeTypes->at(i))->string();
        if (mimeTypeString.isEmpty())
            continue;
        mimeTypeList = eina_list_append(mimeTypeList, eina_stringshare_add(mimeTypeString.utf8().data()));
    }

    return mimeTypeList;
}

Eina_Bool ewk_file_chooser_request_cancel(Ewk_File_Chooser_Request* request)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFileChooserRequest, request, impl, false);
    EINA_SAFETY_ON_TRUE_RETURN_VAL(impl->wasHandled(), false);

    impl->cancel();

    return true;
}

Eina_Bool ewk_file_chooser_request_files_choose(Ewk_File_Chooser_Request* request, const Eina_List* chosenFiles)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFileChooserRequest, request, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(chosenFiles, false);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(eina_list_count(chosenFiles) == 1 || impl->allowMultipleFiles(), false);
    EINA_SAFETY_ON_TRUE_RETURN_VAL(impl->wasHandled(), false);

    Vector< RefPtr<APIObject> > fileURLs;

    const Eina_List* l;
    void* data;
    EINA_LIST_FOREACH(chosenFiles, l, data) {
        EINA_SAFETY_ON_NULL_RETURN_VAL(data, false);
        String fileURL = "file://" + String::fromUTF8(static_cast<char*>(data));
        fileURLs.append(WebURL::create(fileURL));
    }

    impl->chooseFiles(fileURLs);

    return true;
}

Eina_Bool ewk_file_chooser_request_file_choose(Ewk_File_Chooser_Request* request, const char* chosenFile)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFileChooserRequest, request, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(chosenFile, false);
    EINA_SAFETY_ON_TRUE_RETURN_VAL(impl->wasHandled(), false);

    Vector< RefPtr<APIObject> > fileURLs;
    String fileURL = "file://" + String::fromUTF8(chosenFile);
    fileURLs.append(WebURL::create(fileURL));

    impl->chooseFiles(fileURLs);

    return true;
}

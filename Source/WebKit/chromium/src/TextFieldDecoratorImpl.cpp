/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextFieldDecoratorImpl.h"

#include "CachedImage.h"
#include "HTMLInputElement.h"
#include "Image.h"
#include "WebInputElement.h"
#include "WebTextFieldDecoratorClient.h"

namespace WebKit {

using namespace WebCore;

inline TextFieldDecoratorImpl::TextFieldDecoratorImpl(WebTextFieldDecoratorClient* client)
    : m_client(client)
{
    ASSERT(client);
}

PassOwnPtr<TextFieldDecoratorImpl> TextFieldDecoratorImpl::create(WebTextFieldDecoratorClient* client)
{
    return adoptPtr(new TextFieldDecoratorImpl(client));
}

TextFieldDecoratorImpl::~TextFieldDecoratorImpl()
{
}

bool TextFieldDecoratorImpl::willAddDecorationTo(HTMLInputElement* input)
{
    ASSERT(input);
    return m_client->shouldAddDecorationTo(WebInputElement(input));
}

CachedImage* TextFieldDecoratorImpl::imageForNormalState()
{
    if (!m_cachedImageForNormalState) {
        WebCString imageName = m_client->imageNameForNormalState();
        ASSERT(!imageName.isEmpty());
        RefPtr<Image> image = Image::loadPlatformResource(imageName.data());
        m_cachedImageForNormalState = new CachedImage(image.get());
        // The CachedImage owns a RefPtr to the Image.
    }
    return m_cachedImageForNormalState.get();
}

CachedImage* TextFieldDecoratorImpl::imageForDisabledState()
{
    if (!m_cachedImageForDisabledState) {
        WebCString imageName = m_client->imageNameForDisabledState();
        if (imageName.isEmpty())
            m_cachedImageForDisabledState = imageForNormalState();
        else {
            RefPtr<Image> image = Image::loadPlatformResource(imageName.data());
            m_cachedImageForDisabledState = new CachedImage(image.get());
        }
    }
    return m_cachedImageForDisabledState.get();
}

CachedImage* TextFieldDecoratorImpl::imageForReadonlyState()
{
    if (!m_cachedImageForReadonlyState) {
        WebCString imageName = m_client->imageNameForReadOnlyState();
        if (imageName.isEmpty())
            m_cachedImageForDisabledState = imageForDisabledState();
        else {
            RefPtr<Image> image = Image::loadPlatformResource(imageName.data());
            m_cachedImageForReadonlyState = new CachedImage(image.get());
        }
    }
    return m_cachedImageForReadonlyState.get();
}

void TextFieldDecoratorImpl::handleClick(HTMLInputElement* input)
{
    ASSERT(input);
    WebInputElement webInput(input);
    m_client->handleClick(webInput);
}

void TextFieldDecoratorImpl::willDetach(HTMLInputElement* input)
{
    ASSERT(input);
    WebInputElement webInput(input);
    m_client->willDetach(webInput);
}

}

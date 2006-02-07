/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <kxmlcore/Vector.h>
#include "Array.h"
#include "Image.h"
#include "ImageDecoder.h"
#include "IntRect.h"
#include "ImageData.h"

namespace WebCore {

// ================================================
// Image Class
// ================================================

Image::Image()
:m_data(0), m_animationObserver(0)
{
}

Image::Image(ImageAnimationObserver* observer, bool isPDF)
{
    m_data = new ImageData(this);
#if __APPLE__
    m_data->setIsPDF(isPDF);
#endif
    m_animationObserver = observer;
}

Image::~Image()
{
    delete m_data;
}

void Image::resetAnimation() const
{
    if (m_data)
        m_data->resetAnimation();
}

bool Image::setData(const ByteArray& bytes, bool allDataReceived)
{
    // Make sure we have some data.
    if (!m_data)
        return true;
    
    return m_data->setData(bytes, allDataReceived);
}

bool Image::isNull() const
{
    return m_data ? m_data->isNull() : true;
}

IntSize Image::size() const
{
    return m_data ? m_data->size() : IntSize();
}

IntRect Image::rect() const
{
    return m_data ? IntRect(IntPoint(), m_data->size()) : IntRect();
}

int Image::width() const
{
    return size().width();
}

int Image::height() const
{
    return size().height();
}

const int NUM_COMPOSITE_OPERATORS = 14;

struct CompositeOperatorEntry
{
    const char *name;
    Image::CompositeOperator value;
};

struct CompositeOperatorEntry compositeOperators[NUM_COMPOSITE_OPERATORS] = {
    { "clear", Image::CompositeClear },
    { "copy", Image::CompositeCopy },
    { "source-over", Image::CompositeSourceOver },
    { "source-in", Image::CompositeSourceIn },
    { "source-out", Image::CompositeSourceOut },
    { "source-atop", Image::CompositeSourceAtop },
    { "destination-over", Image::CompositeDestinationOver },
    { "destination-in", Image::CompositeDestinationIn },
    { "destination-out", Image::CompositeDestinationOut },
    { "destination-atop", Image::CompositeDestinationAtop },
    { "xor", Image::CompositeXOR },
    { "darker", Image::CompositePlusDarker },
    { "highlight", Image::CompositeHighlight },
    { "lighter", Image::CompositePlusLighter }
};

Image::CompositeOperator Image::compositeOperatorFromString(const char* aString)
{
    CompositeOperator op = CompositeSourceOver;
    
    if (strlen(aString)) {
        for (int i = 0; i < NUM_COMPOSITE_OPERATORS; i++) {
#if WIN32
            // FIXME: Use the new String class 
            if (strcmp(aString, compositeOperators[i].name) == 0) {
#else
            if (strcasecmp(aString, compositeOperators[i].name) == 0) {
#endif
                return compositeOperators[i].value;
            }
        }
    }
    return op;
}

}

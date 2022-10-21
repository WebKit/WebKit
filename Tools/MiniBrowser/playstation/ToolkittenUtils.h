/*
* Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#pragma once

#include <WebKit/WKGeometry.h>
#include <toolkitten/IntRect.h>

inline toolkitten::IntRect toTKRect(WKRect rect)
{
    return {
        toolkitten::IntPoint {
            static_cast<int>(rect.origin.x),
            static_cast<int>(rect.origin.y)
        },
        toolkitten::IntSize {
            static_cast<unsigned>(rect.size.width),
            static_cast<unsigned>(rect.size.height)
        }
    };
}

inline toolkitten::IntPoint toTKPoint(WKPoint point)
{
    return {
        toolkitten::IntPoint {
            static_cast<int>(point.x),
            static_cast<int>(point.y)
        }
    };
}

inline WKRect toWKRect(toolkitten::IntRect rect)
{
    return WKRectMake(rect.position.x, rect.position.y, rect.size.w, rect.size.h);
}

inline WKSize toWKSize(toolkitten::IntSize size)
{
    return WKSizeMake(size.w, size.h);
}

inline WKPoint toWKPoint(toolkitten::IntPoint point)
{
    return WKPointMake(point.x, point.y);
}

template<typename WidgetT>
WidgetT* createAndAppendChild(toolkitten::Widget* parent, const toolkitten::IntSize size, const toolkitten::IntPoint& position)
{
    auto widget = std::make_unique<WidgetT>();
    WidgetT* ptr = widget.get();
    ptr->setSize(size);
    ptr->setPosition(position);
    parent->appendChild(std::move(widget));
    return ptr;
}

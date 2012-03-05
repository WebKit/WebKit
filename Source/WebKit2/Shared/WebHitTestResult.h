/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WebHitTestResult_h
#define WebHitTestResult_h

#include "APIObject.h"
#include <WebCore/HitTestResult.h>
#include <WebCore/KURL.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class WebFrame;

class WebHitTestResult : public APIObject {
public:
    static const Type APIType = TypeHitTestResult;

    struct Data {
        String absoluteImageURL;
        String absolutePDFURL;
        String absoluteLinkURL;
        String absoluteMediaURL;
        String linkLabel;
        String linkTitle;

        Data()
        {
        }

        explicit Data(const WebCore::HitTestResult& hitTestResult)
            : absoluteImageURL(hitTestResult.absoluteImageURL().string())
            , absolutePDFURL(hitTestResult.absolutePDFURL().string())
            , absoluteLinkURL(hitTestResult.absoluteLinkURL().string())
            , absoluteMediaURL(hitTestResult.absoluteMediaURL().string())
            , linkLabel(hitTestResult.textContent())
            , linkTitle(hitTestResult.titleDisplayString())
        {
        }

        void encode(CoreIPC::ArgumentEncoder*) const;
        static bool decode(CoreIPC::ArgumentDecoder*, WebHitTestResult::Data&);
    };

    static PassRefPtr<WebHitTestResult> create(const WebHitTestResult::Data&);

    String absoluteImageURL() const { return m_data.absoluteImageURL; }
    String absolutePDFURL() const { return m_data.absolutePDFURL; }
    String absoluteLinkURL() const { return m_data.absoluteLinkURL; }
    String absoluteMediaURL() const { return m_data.absoluteMediaURL; }

    String linkLabel() const { return m_data.linkLabel; }
    String linkTitle() const { return m_data.linkTitle; }

private:
    explicit WebHitTestResult(const WebHitTestResult::Data& hitTestResultData)
        : m_data(hitTestResultData)
    {
    }

    virtual Type type() const { return APIType; }

    Data m_data;
};

} // namespace WebKit

#endif // WebHitTestResult_h

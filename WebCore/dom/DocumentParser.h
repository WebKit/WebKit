/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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
 *
 */

#ifndef DocumentParser_h
#define DocumentParser_h

namespace WebCore {

    class HTMLParser;
    class HTMLDocumentParser;
    class SegmentedString;
    class XSSAuditor;

    class DocumentParser : public Noncopyable {
    public:
        virtual ~DocumentParser() { }

        // Script output must be prepended, while new data
        // received during executing a script must be appended, hence the
        // extra bool to be able to distinguish between both cases.
        // document.write() always uses false, while the loader uses true.
        virtual void write(const SegmentedString&, bool appendData) = 0;
        virtual void finish() = 0;
        virtual bool isWaitingForScripts() const = 0;
        virtual void stopParsing() { m_parserStopped = true; }
        virtual bool processingData() const { return false; }
        virtual int executingScript() const { return 0; }

        virtual bool wantsRawData() const { return false; }
        virtual bool writeRawData(const char* /*data*/, int /*length*/) { return false; }

        bool inViewSourceMode() const { return m_inViewSourceMode; }
        void setInViewSourceMode(bool mode) { m_inViewSourceMode = mode; }

        virtual bool wellFormed() const { return true; }

        virtual int lineNumber() const { return -1; }
        virtual int columnNumber() const { return -1; }

        virtual void executeScriptsWaitingForStylesheets() {}

        virtual HTMLParser* htmlParser() const { return 0; }
        virtual HTMLDocumentParser* asHTMLTokenizer() { return 0; }

        XSSAuditor* xssAuditor() const { return m_XSSAuditor; }
        void setXSSAuditor(XSSAuditor* auditor) { m_XSSAuditor = auditor; }

    protected:
        DocumentParser(bool viewSourceMode = false)
            : m_parserStopped(false)
            , m_inViewSourceMode(viewSourceMode)
            , m_XSSAuditor(0)
        {
        }

        // The parser has buffers, so parsing may continue even after
        // it stops receiving data. We use m_parserStopped to stop the parser
        // even when it has buffered data.
        bool m_parserStopped;
        bool m_inViewSourceMode;

        // The XSSAuditor associated with this document parser.
        XSSAuditor* m_XSSAuditor;
    };

} // namespace WebCore

#endif // DocumentParser_h

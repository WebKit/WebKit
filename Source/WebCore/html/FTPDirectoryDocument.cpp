/*
 * Copyright (C) 2007-2008, 2014-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FTPDirectoryDocument.h"

#if ENABLE(FTPDIR)

#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDocumentParser.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "FTPDirectoryParser.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "Text.h"
#include <wtf/GregorianDateTime.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FTPDirectoryDocument);

using namespace HTMLNames;
    
class FTPDirectoryDocumentParser final : public HTMLDocumentParser {
public:
    static Ref<FTPDirectoryDocumentParser> create(HTMLDocument& document)
    {
        return adoptRef(*new FTPDirectoryDocumentParser(document));
    }

private:
    void append(RefPtr<StringImpl>&&) override;
    void finish() override;

    void checkBuffer(int len = 10)
    {
        if ((m_dest - m_buffer) > m_size - len) {
            // Enlarge buffer
            int newSize = std::max(m_size * 2, m_size + len);
            int oldOffset = m_dest - m_buffer;
            m_buffer = static_cast<UChar*>(fastRealloc(m_buffer, newSize * sizeof(UChar)));
            m_dest = m_buffer + oldOffset;
            m_size = newSize;
        }
    }

    FTPDirectoryDocumentParser(HTMLDocument&);

    // The parser will attempt to load the document template specified via the preference
    // Failing that, it will fall back and create the basic document which will have a minimal
    // table for presenting the FTP directory in a useful manner
    bool loadDocumentTemplate();
    void createBasicDocument();

    void parseAndAppendOneLine(const String&);
    void appendEntry(const String& name, const String& size, const String& date, bool isDirectory);    
    Ref<Element> createTDForFilename(const String&);

    RefPtr<HTMLTableElement> m_tableElement;

    bool m_skipLF { false };
    
    int m_size { 254 };
    UChar* m_buffer;
    UChar* m_dest;
    String m_carryOver;
    
    ListState m_listState;
};

FTPDirectoryDocumentParser::FTPDirectoryDocumentParser(HTMLDocument& document)
    : HTMLDocumentParser(document)
    , m_buffer(static_cast<UChar*>(fastMalloc(sizeof(UChar) * m_size)))
    , m_dest(m_buffer)
{
}

void FTPDirectoryDocumentParser::appendEntry(const String& filename, const String& size, const String& date, bool isDirectory)
{
    auto& document = *this->document();

    auto rowElement = m_tableElement->insertRow(-1).releaseReturnValue();
    rowElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryEntryRow", AtomString::ConstructFromLiteral));

    auto typeElement = HTMLTableCellElement::create(tdTag, document);
    typeElement->appendChild(Text::create(document, String(&noBreakSpace, 1)));
    if (isDirectory)
        typeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryIcon ftpDirectoryTypeDirectory", AtomString::ConstructFromLiteral));
    else
        typeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryIcon ftpDirectoryTypeFile", AtomString::ConstructFromLiteral));
    rowElement->appendChild(typeElement);

    auto nameElement = createTDForFilename(filename);
    nameElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryFileName", AtomString::ConstructFromLiteral));
    rowElement->appendChild(nameElement);

    auto dateElement = HTMLTableCellElement::create(tdTag, document);
    dateElement->appendChild(Text::create(document, date));
    dateElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryFileDate", AtomString::ConstructFromLiteral));
    rowElement->appendChild(dateElement);

    auto sizeElement = HTMLTableCellElement::create(tdTag, document);
    sizeElement->appendChild(Text::create(document, size));
    sizeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("ftpDirectoryFileSize", AtomString::ConstructFromLiteral));
    rowElement->appendChild(sizeElement);
    document.setHasVisuallyNonEmptyCustomContent();
}

Ref<Element> FTPDirectoryDocumentParser::createTDForFilename(const String& filename)
{
    auto& document = *this->document();

    String fullURL = document.baseURL().string();
    if (fullURL.endsWith('/'))
        fullURL = fullURL + filename;
    else
        fullURL = fullURL + '/' + filename;

    auto anchorElement = HTMLAnchorElement::create(document);
    anchorElement->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, fullURL);
    anchorElement->appendChild(Text::create(document, filename));

    auto tdElement = HTMLTableCellElement::create(tdTag, document);
    tdElement->appendChild(anchorElement);

    return tdElement;
}

static String processFilesizeString(const String& size, bool isDirectory)
{
    if (isDirectory)
        return "--"_s;

    auto bytes = parseIntegerAllowingTrailingJunk<uint64_t>(size);
    if (!bytes)
        return unknownFileSizeText();

    if (*bytes < 1000000)
        return makeString(FormattedNumber::fixedWidth(*bytes / 1000.0, 2), " KB");
    if (*bytes < 1000000000)
        return makeString(FormattedNumber::fixedWidth(*bytes / 1000000.0, 2), " MB");
    return makeString(FormattedNumber::fixedWidth(*bytes / 1000000000.0, 2), " GB");
}

static bool wasLastDayOfMonth(int year, int month, int day)
{
    static const int lastDays[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 0 || month > 11)
        return false;

    if (month == 2) {
        if (year % 4 == 0 && (year % 100 || year % 400 == 0)) {
            if (day == 29)
                return true;
            return false;
        }

        if (day == 28)
            return true;
        return false;
    }

    return lastDays[month] == day;
}

static String processFileDateString(const FTPTime& fileTime)
{
    // FIXME: Need to localize this string?

    String timeOfDay;

    if (!(fileTime.tm_hour == 0 && fileTime.tm_min == 0 && fileTime.tm_sec == 0)) {
        int hour = fileTime.tm_hour;
        ASSERT(hour >= 0 && hour < 24);

        if (hour < 12) {
            if (hour == 0)
                hour = 12;
            timeOfDay = makeString(", ", hour, ':', pad('0', 2, fileTime.tm_min), " AM");
        } else {
            hour = hour - 12;
            if (hour == 0)
                hour = 12;
            timeOfDay = makeString(", ", hour, ':', pad('0', 2, fileTime.tm_min), " PM");
        }
    }

    // If it was today or yesterday, lets just do that - but we have to compare to the current time
    GregorianDateTime now;
    now.setToCurrentLocalTime();

    if (fileTime.tm_year == now.year()) {
        if (fileTime.tm_mon == now.month()) {
            if (fileTime.tm_mday == now.monthDay())
                return "Today" + timeOfDay;
            if (fileTime.tm_mday == now.monthDay() - 1)
                return "Yesterday" + timeOfDay;
        }
        
        if (now.monthDay() == 1 && (now.month() == fileTime.tm_mon + 1 || (now.month() == 0 && fileTime.tm_mon == 11)) &&
            wasLastDayOfMonth(fileTime.tm_year, fileTime.tm_mon, fileTime.tm_mday))
                return "Yesterday" + timeOfDay;
    }

    if (fileTime.tm_year == now.year() - 1 && fileTime.tm_mon == 12 && fileTime.tm_mday == 31 && now.month() == 1 && now.monthDay() == 1)
        return "Yesterday" + timeOfDay;

    static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "???" };

    int month = fileTime.tm_mon;
    if (month < 0 || month > 11)
        month = 12;

    String dateString;

    if (fileTime.tm_year > -1)
        dateString = makeString(months[month], ' ', fileTime.tm_mday, ", ", fileTime.tm_year);
    else
        dateString = makeString(months[month], ' ', fileTime.tm_mday, ", ", now.year());

    return dateString + timeOfDay;
}

void FTPDirectoryDocumentParser::parseAndAppendOneLine(const String& inputLine)
{
    ListResult result;
    CString latin1Input = inputLine.latin1();

    FTPEntryType typeResult = parseOneFTPLine(latin1Input.data(), m_listState, result);

    // FTPMiscEntry is a comment or usage statistic which we don't care about, and junk is invalid data - bail in these 2 cases
    if (typeResult == FTPMiscEntry || typeResult == FTPJunkEntry)
        return;

    String filename(result.filename, result.filenameLength);
    if (result.type == FTPDirectoryEntry) {
        filename.append('/');

        // We have no interest in linking to "current directory"
        if (filename == "./")
            return;
    }

    LOG(FTP, "Appending entry - %s, %s", filename.ascii().data(), result.fileSize.ascii().data());

    appendEntry(filename, processFilesizeString(result.fileSize, result.type == FTPDirectoryEntry), processFileDateString(result.modifiedTime), result.type == FTPDirectoryEntry);
}

static inline RefPtr<SharedBuffer> createTemplateDocumentData(const Settings& settings)
{
    auto buffer = SharedBuffer::createWithContentsOfFile(settings.ftpDirectoryTemplatePath());
    if (buffer)
        LOG(FTP, "Loaded FTPDirectoryTemplate of length %zu\n", buffer->size());
    return buffer;
}
    
bool FTPDirectoryDocumentParser::loadDocumentTemplate()
{
    static SharedBuffer* templateDocumentData = createTemplateDocumentData(document()->settings()).leakRef();
    // FIXME: Instead of storing the data, it would be more efficient if we could parse the template data into the
    // template Document once, store that document, then "copy" it whenever we get an FTP directory listing.
    
    if (!templateDocumentData) {
        LOG_ERROR("Could not load templateData");
        return false;
    }

    HTMLDocumentParser::insert(String(templateDocumentData->data(), templateDocumentData->size()));

    auto& document = *this->document();

    auto foundElement = makeRefPtr(document.getElementById(String("ftpDirectoryTable"_s)));
    if (!foundElement)
        LOG_ERROR("Unable to find element by id \"ftpDirectoryTable\" in the template document.");
    else if (!is<HTMLTableElement>(foundElement))
        LOG_ERROR("Element of id \"ftpDirectoryTable\" is not a table element");
    else {
        m_tableElement = downcast<HTMLTableElement>(foundElement.get());
        return true;
    }

    m_tableElement = HTMLTableElement::create(document);
    m_tableElement->setAttributeWithoutSynchronization(HTMLNames::idAttr, AtomString("ftpDirectoryTable", AtomString::ConstructFromLiteral));

    // If we didn't find the table element, lets try to append our own to the body.
    // If that fails for some reason, cram it on the end of the document as a last ditch effort.
    if (auto body = makeRefPtr(document.bodyOrFrameset()))
        body->appendChild(*m_tableElement);
    else
        document.appendChild(*m_tableElement);

    return true;
}

void FTPDirectoryDocumentParser::createBasicDocument()
{
    LOG(FTP, "Creating a basic FTP document structure as no template was loaded");

    auto& document = *this->document();

    auto bodyElement = HTMLBodyElement::create(document);
    document.appendChild(bodyElement);

    m_tableElement = HTMLTableElement::create(document);
    m_tableElement->setAttributeWithoutSynchronization(HTMLNames::idAttr, AtomString("ftpDirectoryTable", AtomString::ConstructFromLiteral));
    m_tableElement->setAttribute(HTMLNames::styleAttr, AtomString("width:100%", AtomString::ConstructFromLiteral));

    bodyElement->appendChild(*m_tableElement);

    document.processViewport("width=device-width", ViewportArguments::ViewportMeta);
}

void FTPDirectoryDocumentParser::append(RefPtr<StringImpl>&& inputSource)
{
    // Make sure we have the table element to append to by loading the template set in the pref, or
    // creating a very basic document with the appropriate table
    if (!m_tableElement) {
        if (!loadDocumentTemplate())
            createBasicDocument();
        ASSERT(m_tableElement);
    }

    bool foundNewLine = false;

    m_dest = m_buffer;
    SegmentedString string { String { WTFMove(inputSource) } };
    while (!string.isEmpty()) {
        UChar c = string.currentCharacter();

        if (c == '\r') {
            *m_dest++ = '\n';
            foundNewLine = true;
            // possibly skip an LF in the case of an CRLF sequence
            m_skipLF = true;
        } else if (c == '\n') {
            if (!m_skipLF)
                *m_dest++ = c;
            else
                m_skipLF = false;
        } else {
            *m_dest++ = c;
            m_skipLF = false;
        }

        string.advance();

        // Maybe enlarge the buffer
        checkBuffer();
    }

    if (!foundNewLine) {
        m_dest = m_buffer;
        return;
    }

    UChar* start = m_buffer;
    UChar* cursor = start;

    while (cursor < m_dest) {
        if (*cursor == '\n') {
            m_carryOver.append(String(start, cursor - start));
            LOG(FTP, "%s", m_carryOver.ascii().data());
            parseAndAppendOneLine(m_carryOver);
            m_carryOver = String();

            start = ++cursor;
        } else 
            cursor++;
    }

    // Copy the partial line we have left to the carryover buffer
    if (cursor - start > 1)
        m_carryOver.append(String(start, cursor - start - 1));
}

void FTPDirectoryDocumentParser::finish()
{
    // Possible the last line in the listing had no newline, so try to parse it now
    if (!m_carryOver.isEmpty()) {
        parseAndAppendOneLine(m_carryOver);
        m_carryOver = String();
    }

    m_tableElement = nullptr;
    fastFree(m_buffer);

    HTMLDocumentParser::finish();
}

FTPDirectoryDocument::FTPDirectoryDocument(Frame* frame, const Settings& settings, const URL& url)
    : HTMLDocument(frame, settings, url)
{
#if !LOG_DISABLED
    LogFTP.state = WTFLogChannelState::On;
#endif
}

Ref<DocumentParser> FTPDirectoryDocument::createParser()
{
    return FTPDirectoryDocumentParser::create(*this);
}

}

#endif // ENABLE(FTPDIR)

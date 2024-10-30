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
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FTPDirectoryDocument);

using namespace HTMLNames;
    
class FTPDirectoryDocumentParser final : public HTMLDocumentParser {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(HTMLDocumentParser);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FTPDirectoryDocumentParser);
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
    void appendEntry(String&& name, String&& size, String&& date, bool isDirectory);
    Ref<Element> createTDForFilename(String&&);

    RefPtr<HTMLTableElement> m_tableElement;

    bool m_skipLF { false };
    
    int m_size { 254 };
    UChar* m_buffer;
    UChar* m_dest;
    StringBuilder m_carryOver;
    
    ListState m_listState;
};

FTPDirectoryDocumentParser::FTPDirectoryDocumentParser(HTMLDocument& document)
    : HTMLDocumentParser(document)
    , m_buffer(static_cast<UChar*>(fastMalloc(sizeof(UChar) * m_size)))
    , m_dest(m_buffer)
{
}

void FTPDirectoryDocumentParser::appendEntry(String&& filename, String&& size, String&& date, bool isDirectory)
{
    Ref document = *this->document();

    Ref rowElement = m_tableElement->insertRow(-1).releaseReturnValue();
    rowElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryEntryRow"_s);

    Ref typeElement = HTMLTableCellElement::create(tdTag, document);
    typeElement->appendChild(Text::create(document, span(noBreakSpace)));
    if (isDirectory)
        typeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryIcon ftpDirectoryTypeDirectory"_s);
    else
        typeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryIcon ftpDirectoryTypeFile"_s);
    rowElement->appendChild(typeElement);

    Ref nameElement = createTDForFilename(WTFMove(filename));
    nameElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryFileName"_s);
    rowElement->appendChild(nameElement);

    Ref dateElement = HTMLTableCellElement::create(tdTag, document);
    dateElement->appendChild(Text::create(document, WTFMove(date)));
    dateElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryFileDate"_s);
    rowElement->appendChild(dateElement);

    Ref sizeElement = HTMLTableCellElement::create(tdTag, document);
    sizeElement->appendChild(Text::create(document, WTFMove(size)));
    sizeElement->setAttributeWithoutSynchronization(HTMLNames::classAttr, "ftpDirectoryFileSize"_s);
    rowElement->appendChild(sizeElement);
    document->setHasVisuallyNonEmptyCustomContent();
}

Ref<Element> FTPDirectoryDocumentParser::createTDForFilename(String&& filename)
{
    Ref document = *this->document();

    auto baseURL = document->baseURL().string();
    AtomString fullURL;
    if (baseURL.endsWith('/'))
        fullURL = makeAtomString(baseURL, filename);
    else
        fullURL = makeAtomString(baseURL, '/', filename);

    Ref anchorElement = HTMLAnchorElement::create(document);
    anchorElement->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, WTFMove(fullURL));
    anchorElement->appendChild(Text::create(document, WTFMove(filename)));

    Ref tdElement = HTMLTableCellElement::create(tdTag, document);
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
        return makeString(FormattedNumber::fixedWidth(*bytes / 1000.0, 2), " KB"_s);
    if (*bytes < 1000000000)
        return makeString(FormattedNumber::fixedWidth(*bytes / 1000000.0, 2), " MB"_s);
    return makeString(FormattedNumber::fixedWidth(*bytes / 1000000000.0, 2), " GB"_s);
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
            timeOfDay = makeString(", "_s, hour, ':', pad('0', 2, fileTime.tm_min), " AM"_s);
        } else {
            hour = hour - 12;
            if (hour == 0)
                hour = 12;
            timeOfDay = makeString(", "_s, hour, ':', pad('0', 2, fileTime.tm_min), " PM"_s);
        }
    }

    // If it was today or yesterday, lets just do that - but we have to compare to the current time
    GregorianDateTime now;
    now.setToCurrentLocalTime();

    if (fileTime.tm_year == now.year()) {
        if (fileTime.tm_mon == now.month()) {
            if (fileTime.tm_mday == now.monthDay())
                return makeString("Today"_s, timeOfDay);
            if (fileTime.tm_mday == now.monthDay() - 1)
                return makeString("Yesterday"_s, timeOfDay);
        }
        
        if (now.monthDay() == 1 && (now.month() == fileTime.tm_mon + 1 || (now.month() == 0 && fileTime.tm_mon == 11)) &&
            wasLastDayOfMonth(fileTime.tm_year, fileTime.tm_mon, fileTime.tm_mday))
                return makeString("Yesterday"_s, timeOfDay);
    }

    if (fileTime.tm_year == now.year() - 1 && fileTime.tm_mon == 12 && fileTime.tm_mday == 31 && now.month() == 1 && now.monthDay() == 1)
        return makeString("Yesterday"_s, timeOfDay);

    static constexpr std::array months = { "Jan"_s, "Feb"_s, "Mar"_s, "Apr"_s, "May"_s, "Jun"_s, "Jul"_s, "Aug"_s, "Sep"_s, "Oct"_s, "Nov"_s, "Dec"_s, "???"_s };

    int month = fileTime.tm_mon;
    if (month < 0 || month > 11)
        month = 12;

    if (fileTime.tm_year > -1)
        return makeString(months[month], ' ', fileTime.tm_mday, ", "_s, fileTime.tm_year, timeOfDay);
    return makeString(months[month], ' ', fileTime.tm_mday, ", "_s, now.year(), timeOfDay);
}

void FTPDirectoryDocumentParser::parseAndAppendOneLine(const String& inputLine)
{
    ListResult result;
    CString latin1Input = inputLine.latin1();

    FTPEntryType typeResult = parseOneFTPLine(latin1Input.data(), m_listState, result);

    // FTPMiscEntry is a comment or usage statistic which we don't care about, and junk is invalid data - bail in these 2 cases
    if (typeResult == FTPMiscEntry || typeResult == FTPJunkEntry)
        return;

    String filename;
    if (result.type == FTPDirectoryEntry) {
        filename = makeString(result.filenameSpan(), '/');

        // We have no interest in linking to "current directory"
        if (filename == "./"_s)
            return;
    } else
        filename = String(result.filenameSpan());

    LOG(FTP, "Appending entry - %s, %s", filename.ascii().data(), result.fileSize.ascii().data());

    appendEntry(WTFMove(filename), processFilesizeString(result.fileSize, result.type == FTPDirectoryEntry), processFileDateString(result.modifiedTime), result.type == FTPDirectoryEntry);
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
    static NeverDestroyed<RefPtr<SharedBuffer>> templateDocumentData = createTemplateDocumentData(document()->settings());
    // FIXME: Instead of storing the data, it would be more efficient if we could parse the template data into the
    // template Document once, store that document, then "copy" it whenever we get an FTP directory listing.
    
    if (!templateDocumentData.get()) {
        LOG_ERROR("Could not load templateData");
        return false;
    }

    HTMLDocumentParser::insert(String(templateDocumentData.get()->span()));

    Ref document = *this->document();

    RefPtr foundElement = document->getElementById(StringView { "ftpDirectoryTable"_s });
    if (!foundElement)
        LOG_ERROR("Unable to find element by id \"ftpDirectoryTable\" in the template document.");
    else if (RefPtr tableElement = dynamicDowncast<HTMLTableElement>(*foundElement)) {
        m_tableElement = WTFMove(tableElement);
        return true;
    } else
        LOG_ERROR("Element of id \"ftpDirectoryTable\" is not a table element");

    Ref tableElement = HTMLTableElement::create(document);
    m_tableElement = tableElement.copyRef();
    tableElement->setAttributeWithoutSynchronization(HTMLNames::idAttr, "ftpDirectoryTable"_s);

    // If we didn't find the table element, lets try to append our own to the body.
    // If that fails for some reason, cram it on the end of the document as a last ditch effort.
    if (RefPtr body = document->bodyOrFrameset())
        body->appendChild(tableElement);
    else
        document->appendChild(tableElement);

    return true;
}

void FTPDirectoryDocumentParser::createBasicDocument()
{
    LOG(FTP, "Creating a basic FTP document structure as no template was loaded");

    Ref document = *this->document();

    Ref bodyElement = HTMLBodyElement::create(document);
    document->appendChild(bodyElement);

    Ref tableElement = HTMLTableElement::create(document);
    m_tableElement = tableElement.copyRef();
    tableElement->setAttributeWithoutSynchronization(HTMLNames::idAttr, "ftpDirectoryTable"_s);
    tableElement->setAttribute(HTMLNames::styleAttr, "width:100%"_s);

    bodyElement->appendChild(tableElement);

    document->processViewport("width=device-width"_s, ViewportArguments::Type::ViewportMeta);
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
            m_carryOver.append(StringView(std::span(start, cursor - start)));
            LOG(FTP, "%s", m_carryOver.toString().ascii().data());
            parseAndAppendOneLine(m_carryOver.toString());
            m_carryOver.clear();

            start = ++cursor;
        } else 
            cursor++;
    }

    // Copy the partial line we have left to the carryover buffer
    if (cursor - start > 1)
        m_carryOver.append(StringView(std::span(start, cursor - start - 1)));
}

void FTPDirectoryDocumentParser::finish()
{
    // Possible the last line in the listing had no newline, so try to parse it now
    if (!m_carryOver.isEmpty()) {
        parseAndAppendOneLine(m_carryOver.toString());
        m_carryOver.clear();
    }

    m_tableElement = nullptr;
    fastFree(m_buffer);

    HTMLDocumentParser::finish();
}

FTPDirectoryDocument::FTPDirectoryDocument(LocalFrame* frame, const Settings& settings, const URL& url)
    : HTMLDocument(frame, settings, url, { })
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(FTPDIR)

/*
 * Copyright (C) 2009, Martin Robinson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "SelectionData.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringCommon.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// g_filename_from_uri() discards the authority when the hostname out parameter is
// null, so file://attacker.example/etc/passwd would resolve to /etc/passwd. RFC 8089
// only lets an empty authority or "localhost" mean this machine.
String SelectionData::localPathFromURIListLine(const String& line)
{
    URL url { line };
    if (!url.isValid())
        return { };

    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<gchar> hostname;
    GUniquePtr<gchar> filename(g_filename_from_uri(line.utf8().data(), &hostname.outPtr(), &error.outPtr()));
    if (error || !filename)
        return { };

    if (hostname && hostname.get()[0] && !equalLettersIgnoringASCIICase(String::fromUTF8(hostname.get()), "localhost"_s))
        return { };

    return String::fromUTF8(filename.get());
}

// Broader than localPathFromURIListLine() on purpose. A receiving application may
// resolve a remote-host file URI locally, so export strips anything file shaped.
static bool isFileURIListLine(const String& line)
{
    URL url { line };
    if (!url.isValid())
        return false;

    if (url.protocolIs("file"_s))
        return true;

    GUniqueOutPtr<GError> error;
    GUniquePtr<gchar> filename(g_filename_from_uri(line.utf8().data(), nullptr, &error.outPtr()));
    return !error && filename;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(SelectionData);

SelectionData::SelectionData(const String& text, const String& markup, const URL& url, const String& uriList, Vector<String>&& filenames, RefPtr<WebCore::Image>&& image, RefPtr<WebCore::SharedBuffer>&& buffer, bool canSmartReplace)
{
    if (!text.isEmpty())
        setText(text);
    if (!markup.isEmpty())
        setMarkup(markup);
    if (!url.isEmpty())
        setURL(url, String());
    if (!uriList.isEmpty())
        setURIList(uriList);
    // After setURIList(), which must not touch m_filenames.
    if (!filenames.isEmpty())
        setFilenames(WTF::move(filenames));
    if (image)
        setImage(WTF::move(image));
    if (buffer)
        setCustomData(buffer.releaseNonNull());
    setCanSmartReplace(canSmartReplace);
}

static void replaceNonBreakingSpaceWithSpace(String& string)
{
    string = makeStringByReplacingAll(string, noBreakSpace, space);
}

void SelectionData::setText(const String& newText)
{
    m_text = newText;
    replaceNonBreakingSpaceWithSpace(m_text);
}

static void updateURLFromURIList(const String& uriListString, URL& url, bool& urlIsSet)
{
    if (urlIsSet)
        return;

    // Line separator is \r\n per RFC 2483 - however, for compatibility
    // reasons we also allow just \n here.
    for (auto& line : uriListString.split('\n')) {
        line = line.trim(deprecatedIsSpaceOrNewline);
        if (line.isEmpty())
            continue;
        if (line[0] == '#')
            continue;

        URL parsed { line };
        if (!parsed.isValid())
            continue;

        url = WTF::move(parsed);
        urlIsSet = true;
        return;
    }
}

Vector<String> SelectionData::filenamesFromURIList(const String& uriListString)
{
    // This code is originally from: platform/chromium/ChromiumDataObject.cpp.
    Vector<String> filenames;
    for (auto& line : uriListString.split('\n')) {
        line = line.trim(deprecatedIsSpaceOrNewline);
        if (line.isEmpty())
            continue;
        if (line[0] == '#')
            continue;

        auto path = localPathFromURIListLine(line);
        if (!path.isNull())
            filenames.append(WTF::move(path));
    }
    return filenames;
}

void SelectionData::setURIList(const String& uriListString)
{
    m_uriList = uriListString;

    // Process the input and copy the first valid URL into the url member.
    // In case no URLs can be found, subsequent calls to getData("URL")
    // will get an empty string. This is in line with the HTML5 spec (see
    // "The DragEvent and DataTransfer interfaces").
    //
    // Script can write text/uri-list through DataTransfer.setData(), so file:// lines
    // are not promoted into m_filenames here. See CVE-2025-13947.
    bool setURL = hasURL();
    updateURLFromURIList(uriListString, m_url, setURL);
}

void SelectionData::setFilenames(Vector<String>&& filenames)
{
    m_filenames = WTF::move(filenames);
}

void SelectionData::setFilenamesFromURIList(const String& uriListString)
{
    m_filenames = filenamesFromURIList(uriListString);
}

void SelectionData::setTrustedDrop(const String& uriListString, Vector<String>&& portalFilenames)
{
    setURIList(uriListString);

    // The portal list is the grant when present. A parallel uri-list could only widen it.
    if (!portalFilenames.isEmpty()) {
        setFilenames(WTF::move(portalFilenames));
        return;
    }

    setFilenamesFromURIList(uriListString);
}

String SelectionData::uriListWithoutFilenames(const String& uriListString)
{
    StringBuilder builder;
    for (auto& line : uriListString.split('\n')) {
        auto trimmed = line.trim(deprecatedIsSpaceOrNewline);
        if (trimmed.isEmpty())
            continue;
        if (trimmed[0] == '#')
            continue;

        if (isFileURIListLine(trimmed))
            continue;

        if (!builder.isEmpty())
            builder.append("\r\n"_s);
        builder.append(trimmed);
    }
    return builder.toString();
}

// Answers from web-writable text, so callers may only narrow on a true result.
bool SelectionData::uriListContainsFileURI(const String& uriListString)
{
    for (auto& line : uriListString.split('\n')) {
        auto trimmed = line.trim(deprecatedIsSpaceOrNewline);
        if (trimmed.isEmpty())
            continue;
        if (trimmed[0] == '#')
            continue;

        if (isFileURIListLine(trimmed))
            return true;
    }
    return false;
}

void SelectionData::setURL(const URL& url, const String& label)
{
    m_url = url;
    if (m_uriList.isEmpty())
        m_uriList = url.string();

    if (!hasText())
        setText(url.string());

    if (hasMarkup())
        return;

    String actualLabel = label.isEmpty() ? url.string() : label;
    GUniquePtr<gchar> escaped(g_markup_escape_text(actualLabel.utf8().data(), -1));

    setMarkup(makeString("<a href=\""_s, url.string(), "\">"_s,
        String::fromUTF8(escaped.get()), "</a>"_s));
}

const String& SelectionData::urlLabel() const
{
    if (hasText())
        return text();

    if (hasURL())
        return url().string();

    return emptyString();
}

void SelectionData::clearAllExceptFilenames()
{
    clearText();
    clearMarkup();
    clearURIList();
    clearURL();
    clearImage();
    clearCustomData();
    clearBuffers();

    m_canSmartReplace = false;
}

void SelectionData::clearAll()
{
    clearAllExceptFilenames();
    m_filenames.clear();
}

} // namespace WebCore

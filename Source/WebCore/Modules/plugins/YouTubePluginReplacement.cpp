/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "YouTubePluginReplacement.h"

#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLPlugInElement.h"
#include "RenderElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "YouTubeEmbedShadowElement.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void YouTubePluginReplacement::registerPluginReplacement(PluginReplacementRegistrar registrar)
{
    registrar(ReplacementPlugin(create, supportsMIMEType, supportsFileExtension, supportsURL, isEnabledBySettings));
}

Ref<PluginReplacement> YouTubePluginReplacement::create(HTMLPlugInElement& plugin, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues)
{
    return adoptRef(*new YouTubePluginReplacement(plugin, paramNames, paramValues));
}

bool YouTubePluginReplacement::supportsMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "application/x-shockwave-flash"_s)
        || equalLettersIgnoringASCIICase(mimeType, "application/futuresplash"_s);
}

bool YouTubePluginReplacement::supportsFileExtension(StringView extension)
{
    return equalLettersIgnoringASCIICase(extension, "spl"_s) || equalLettersIgnoringASCIICase(extension, "swf"_s);
}

YouTubePluginReplacement::YouTubePluginReplacement(HTMLPlugInElement& plugin, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues)
    : m_parentElement(plugin)
{
    ASSERT(paramNames.size() == paramValues.size());
    for (size_t i = 0; i < paramNames.size(); ++i)
        m_attributes.add(paramNames[i], paramValues[i]);
}

RenderPtr<RenderElement> YouTubePluginReplacement::createElementRenderer(HTMLPlugInElement& plugin, RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    ASSERT_UNUSED(plugin, m_parentElement == &plugin);

    if (!m_embedShadowElement)
        return nullptr;
    
    return m_embedShadowElement->createElementRenderer(WTFMove(style), insertionPosition);
}

void YouTubePluginReplacement::installReplacement(ShadowRoot& root)
{
    m_embedShadowElement = YouTubeEmbedShadowElement::create(m_parentElement->document());

    root.appendChild(*m_embedShadowElement);

    auto iframeElement = HTMLIFrameElement::create(HTMLNames::iframeTag, m_parentElement->document());
    if (m_attributes.contains<HashTranslatorASCIILiteral>("width"_s))
        iframeElement->setAttributeWithoutSynchronization(HTMLNames::widthAttr, "100%"_s);

    const auto& heightValue = m_attributes.find<HashTranslatorASCIILiteral>("height"_s);
    if (heightValue != m_attributes.end()) {
        iframeElement->setAttribute(HTMLNames::styleAttr, "max-height: 100%"_s);
        iframeElement->setAttributeWithoutSynchronization(HTMLNames::heightAttr, heightValue->value);
    }

    iframeElement->setAttributeWithoutSynchronization(HTMLNames::srcAttr, youTubeURL(m_attributes.get<HashTranslatorASCIILiteral>("src"_s)));
    iframeElement->setAttributeWithoutSynchronization(HTMLNames::frameborderAttr, "0"_s);
    
    // Disable frame flattening for this iframe.
    iframeElement->setAttributeWithoutSynchronization(HTMLNames::scrollingAttr, "no"_s);
    m_embedShadowElement->appendChild(iframeElement);
}
    
static URL createYouTubeURL(StringView videoID, StringView timeID)
{
    ASSERT(!videoID.isEmpty());
    ASSERT(videoID != "/"_s);
    return URL(URL(), makeString("youtube:"_s, videoID, timeID.isEmpty() ? ""_s : "t="_s, timeID));
}

static HashMap<String, String> queryKeysAndValues(StringView queryString)
{
    HashMap<String, String> queryDictionary;
    
    size_t queryLength = queryString.length();
    if (!queryLength)
        return queryDictionary;
    
    size_t equalSearchLocation = 0;
    size_t equalSearchLength = queryLength;
    
    while (equalSearchLocation < queryLength - 1 && equalSearchLength) {
        
        // Search for "=".
        size_t equalLocation = queryString.find('=', equalSearchLocation);
        if (equalLocation == notFound)
            break;
        
        size_t indexAfterEqual = equalLocation + 1;
        if (indexAfterEqual > queryLength - 1)
            break;
        
        // Get the key before the "=".
        size_t keyLocation = equalSearchLocation;
        size_t keyLength = equalLocation - equalSearchLocation;
        
        // Seach for the ampersand.
        size_t ampersandLocation = queryString.find('&', indexAfterEqual);
        
        // Get the value after the "=", before the ampersand.
        size_t valueLocation = indexAfterEqual;
        size_t valueLength;
        if (ampersandLocation != notFound)
            valueLength = ampersandLocation - indexAfterEqual;
        else
            valueLength = queryLength - indexAfterEqual;
        
        // Save the key and the value.
        if (keyLength && valueLength) {
            String key = queryString.substring(keyLocation, keyLength).convertToASCIILowercase();
            auto value = makeStringByReplacingAll(queryString.substring(valueLocation, valueLength), '+', ' ');

            if (!key.isEmpty() && !value.isEmpty())
                queryDictionary.add(key, value);
        }
        
        if (ampersandLocation == notFound)
            break;
        
        // Continue searching after the ampersand.
        size_t indexAfterAmpersand = ampersandLocation + 1;
        equalSearchLocation = indexAfterAmpersand;
        equalSearchLength = queryLength - indexAfterAmpersand;
    }
    
    return queryDictionary;
}
    
static bool isYouTubeURL(const URL& url)
{
    if (!url.protocolIsInHTTPFamily())
        return false;

    auto hostName = url.host();
    return equalLettersIgnoringASCIICase(hostName, "m.youtube.com"_s)
        || equalLettersIgnoringASCIICase(hostName, "youtu.be"_s)
        || equalLettersIgnoringASCIICase(hostName, "www.youtube.com"_s)
        || equalLettersIgnoringASCIICase(hostName, "youtube.com"_s)
        || equalLettersIgnoringASCIICase(hostName, "www.youtube-nocookie.com"_s)
        || equalLettersIgnoringASCIICase(hostName, "youtube-nocookie.com"_s);
}

static const String& valueForKey(const HashMap<String, String>& dictionary, const String& key)
{
    const auto& value = dictionary.find(key);
    if (value == dictionary.end())
        return emptyString();

    return value->value;
}

static URL processAndCreateYouTubeURL(const URL& url, bool& isYouTubeShortenedURL, String& outPathAfterFirstAmpersand)
{
    ASSERT(isYouTubeURL(url));

    // Bail out early if we aren't even on www.youtube.com or youtube.com.
    if (!isYouTubeURL(url))
        return URL();

    auto hostName = url.host();
    bool isYouTubeMobileWebAppURL = equalLettersIgnoringASCIICase(hostName, "m.youtube.com"_s);
    isYouTubeShortenedURL = equalLettersIgnoringASCIICase(hostName, "youtu.be"_s);

    // Short URL of the form: http://youtu.be/v1d301D
    if (isYouTubeShortenedURL) {
        auto videoID = url.lastPathComponent();
        if (videoID.isEmpty() || videoID == "/"_s)
            return URL();
        return createYouTubeURL(videoID, { });
    }

    auto path = url.path();
    auto query = url.query();
    auto fragment = url.fragmentIdentifier();

    // On the YouTube mobile web app, the path and query string are put into the
    // fragment so that one web page is only ever loaded (see <rdar://problem/9550639>).
    if (isYouTubeMobileWebAppURL) {
        size_t location = fragment.find('?');
        if (location == notFound) {
            path = fragment;
            query = emptyString();
        } else {
            path = fragment.left(location);
            query = fragment.substring(location + 1);
        }
        fragment = emptyString();
    }
    
    if (equalLettersIgnoringASCIICase(path, "/watch"_s)) {
        if (!query.isEmpty()) {
            const auto& queryDictionary = queryKeysAndValues(query);
            String videoID = valueForKey(queryDictionary, "v"_s);
            
            if (!videoID.isEmpty()) {
                const auto& fragmentDictionary = queryKeysAndValues(url.fragmentIdentifier());
                String timeID = valueForKey(fragmentDictionary, "t"_s);
                return createYouTubeURL(videoID, timeID);
            }
        }
        
        // May be a new-style link (see <rdar://problem/7733692>).
        if (fragment.startsWith('!')) {
            query = fragment.substring(1);
            
            if (!query.isEmpty()) {
                const auto& queryDictionary = queryKeysAndValues(query);
                String videoID = valueForKey(queryDictionary, "v"_s);
                
                if (!videoID.isEmpty()) {
                    String timeID = valueForKey(queryDictionary, "t"_s);
                    return createYouTubeURL(videoID, timeID);
                }
            }
        }
    } else if (startsWithLettersIgnoringASCIICase(path, "/v/"_s) || startsWithLettersIgnoringASCIICase(path, "/e/"_s)) {
        StringView videoID;
        StringView pathAfterFirstAmpersand;

        auto lastPathComponent = url.lastPathComponent();
        size_t ampersandLocation = lastPathComponent.find('&');
        if (ampersandLocation != notFound) {
            // Some URLs we care about use & in place of ? for the first query parameter.
            videoID = lastPathComponent.left(ampersandLocation);
            pathAfterFirstAmpersand = lastPathComponent.substring(ampersandLocation + 1, lastPathComponent.length() - ampersandLocation);
        } else
            videoID = lastPathComponent;

        if (!videoID.isEmpty()) {
            outPathAfterFirstAmpersand = pathAfterFirstAmpersand.toString();
            return createYouTubeURL(videoID, emptyString());
        }
    }

    return URL();
}

AtomString YouTubePluginReplacement::youTubeURL(const AtomString& srcString)
{
    URL srcURL = m_parentElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(srcString));
    return youTubeURLFromAbsoluteURL(srcURL, srcString);
}

AtomString YouTubePluginReplacement::youTubeURLFromAbsoluteURL(const URL& srcURL, const AtomString& srcString)
{
    // Validate URL to make sure it is a Youtube URL.
    if (!isYouTubeURL(srcURL))
        return emptyAtom();

    bool isYouTubeShortenedURL = false;
    String possiblyMalformedQuery;
    URL youTubeURL = processAndCreateYouTubeURL(srcURL, isYouTubeShortenedURL, possiblyMalformedQuery);
    if (youTubeURL.isEmpty())
        return srcString;

    // Transform the youtubeURL (youtube:VideoID) to iframe embed url which has the format: http://www.youtube.com/embed/VideoID
    auto srcPath = srcURL.path();
    const String& videoID = youTubeURL.string().substring(youTubeURL.protocol().length() + 1);
    size_t locationOfVideoIDInPath = srcPath.find(videoID);

    size_t locationOfPathBeforeVideoID = notFound;
    if (locationOfVideoIDInPath != notFound) {
        ASSERT(locationOfVideoIDInPath);
    
        // From the original URL, we need to get the part before /path/VideoId.
        locationOfPathBeforeVideoID = StringView(srcString).find(srcPath.left(locationOfVideoIDInPath));
    } else if (equalLettersIgnoringASCIICase(srcPath, "/watch"_s)) {
        // From the original URL, we need to get the part before /watch/#!v=VideoID
        // FIXME: Shouldn't this be ASCII case-insensitive?
        locationOfPathBeforeVideoID = srcString.find("/watch"_s);
    } else
        return srcString;

    ASSERT(locationOfPathBeforeVideoID != notFound);

    auto srcURLPrefix = StringView(srcString).left(locationOfPathBeforeVideoID);
    auto query = srcURL.query();

    // If the URL has no query, use the possibly malformed query we found.
    if (query.isEmpty())
        query = possiblyMalformedQuery;

    // Append the query string if it is valid.
    return makeAtomString(
        isYouTubeShortenedURL ? "http://www.youtube.com"_s : srcURLPrefix,
        "/embed/"_s,
        videoID,
        query.isEmpty() ? ""_s : "?"_s,
        query
    );
}
    
bool YouTubePluginReplacement::supportsURL(const URL& url)
{
    return isYouTubeURL(url);
}

bool YouTubePluginReplacement::isEnabledBySettings(const Settings& settings)
{
    return settings.youTubeFlashPluginReplacementEnabled();
}
    
}

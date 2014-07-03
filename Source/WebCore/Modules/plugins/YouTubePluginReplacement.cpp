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
#include "HTMLPlugInElement.h"
#include "Page.h"
#include "RenderElement.h"
#include "ShadowRoot.h"
#include "YouTubeEmbedShadowElement.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void YouTubePluginReplacement::registerPluginReplacement(PluginReplacementRegistrar registrar)
{
    registrar(ReplacementPlugin(create, supportsMimeType, supportsFileExtension, supportsURL));
}

PassRefPtr<PluginReplacement> YouTubePluginReplacement::create(HTMLPlugInElement& plugin, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    return adoptRef(new YouTubePluginReplacement(plugin, paramNames, paramValues));
}

bool YouTubePluginReplacement::supportsMimeType(const String& mimeType)
{
    return equalIgnoringCase(mimeType, "application/x-shockwave-flash")
        || equalIgnoringCase(mimeType, "application/futuresplash");
}

bool YouTubePluginReplacement::supportsFileExtension(const String& extension)
{
    return equalIgnoringCase(extension, "spl") || equalIgnoringCase(extension, "swf");
}

YouTubePluginReplacement::YouTubePluginReplacement(HTMLPlugInElement& plugin, const Vector<String>& paramNames, const Vector<String>& paramValues)
    : m_parentElement(&plugin)
{
    ASSERT(paramNames.size() == paramValues.size());
    for (size_t i = 0; i < paramNames.size(); ++i)
        m_attributes.add(paramNames[i], paramValues[i]);
}

RenderPtr<RenderElement> YouTubePluginReplacement::createElementRenderer(HTMLPlugInElement& plugin, PassRef<RenderStyle> style)
{
    ASSERT_UNUSED(plugin, m_parentElement == &plugin);

    if (!m_embedShadowElement)
        return nullptr;
    
    return m_embedShadowElement->createElementRenderer(WTF::move(style));
}

bool YouTubePluginReplacement::installReplacement(ShadowRoot* root)
{
    m_embedShadowElement = YouTubeEmbedShadowElement::create(m_parentElement->document());

    root->appendChild(m_embedShadowElement.get());

    RefPtr<HTMLIFrameElement> iframeElement = HTMLIFrameElement::create(HTMLNames::iframeTag, m_parentElement->document());
    if (m_attributes.contains("width"))
        iframeElement->setAttribute(HTMLNames::widthAttr, AtomicString("100%", AtomicString::ConstructFromLiteral));
    
    const auto& heightValue = m_attributes.find("height");
    if (heightValue != m_attributes.end()) {
        iframeElement->setAttribute(HTMLNames::styleAttr, AtomicString("max-height: 100%", AtomicString::ConstructFromLiteral));
        iframeElement->setAttribute(HTMLNames::heightAttr, heightValue->value);
    }

    iframeElement->setAttribute(HTMLNames::srcAttr, youTubeURL(m_attributes.get("src")));
    iframeElement->setAttribute(HTMLNames::frameborderAttr, AtomicString("0", AtomicString::ConstructFromLiteral));
    
    // Disable frame flattening for this iframe.
    iframeElement->setAttribute(HTMLNames::scrollingAttr, AtomicString("no", AtomicString::ConstructFromLiteral));
    m_embedShadowElement->appendChild(iframeElement);

    return true;
}
    
static inline URL createYouTubeURL(const String& videoID, const String& timeID)
{
    ASSERT(!videoID.isEmpty());
    ASSERT(videoID != "/");
    
    URL result(URL(), "youtube:" + videoID);
    if (!timeID.isEmpty())
        result.setQuery("t=" + timeID);
    
    return result;
}
    
static YouTubePluginReplacement::KeyValueMap queryKeysAndValues(const String& queryString)
{
    YouTubePluginReplacement::KeyValueMap queryDictionary;
    
    size_t queryLength = queryString.length();
    if (!queryLength)
        return queryDictionary;
    
    size_t equalSearchLocation = 0;
    size_t equalSearchLength = queryLength;
    
    while (equalSearchLocation < queryLength - 1 && equalSearchLength) {
        
        // Search for "=".
        size_t equalLocation = queryString.find("=", equalSearchLocation);
        if (equalLocation == notFound)
            break;
        
        size_t indexAfterEqual = equalLocation + 1;
        if (indexAfterEqual > queryLength - 1)
            break;
        
        // Get the key before the "=".
        size_t keyLocation = equalSearchLocation;
        size_t keyLength = equalLocation - equalSearchLocation;
        
        // Seach for the ampersand.
        size_t ampersandLocation = queryString.find("&", indexAfterEqual);
        
        // Get the value after the "=", before the ampersand.
        size_t valueLocation = indexAfterEqual;
        size_t valueLength;
        if (ampersandLocation != notFound)
            valueLength = ampersandLocation - indexAfterEqual;
        else
            valueLength = queryLength - indexAfterEqual;
        
        // Save the key and the value.
        if (keyLength && valueLength) {
            const String& key = queryString.substring(keyLocation, keyLength).lower();
            String value = queryString.substring(valueLocation, valueLength);
            value.replace('+', ' ');

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
    
static bool hasCaseInsensitivePrefix(const String& input, const String& prefix)
{
    return input.startsWith(prefix, false);
}
    
static bool isYouTubeURL(const URL& url)
{
    const String& hostName = url.host().lower();
    
    return hostName == "m.youtube.com"
        || hostName == "youtu.be"
        || hostName == "www.youtube.com"
        || hostName == "youtube.com";
}

static const String& valueForKey(const YouTubePluginReplacement::KeyValueMap& dictionary, const String& key)
{
    const auto& value = dictionary.find(key);
    if (value == dictionary.end())
        return emptyString();

    return value->value;
}

static URL processAndCreateYouTubeURL(const URL& url, bool& isYouTubeShortenedURL)
{
    if (!url.protocolIs("http") && !url.protocolIs("https"))
        return URL();
    
    // Bail out early if we aren't even on www.youtube.com or youtube.com.
    if (!isYouTubeURL(url))
        return URL();
    
    const String& hostName = url.host().lower();
    
    bool isYouTubeMobileWebAppURL = hostName == "m.youtube.com";
    isYouTubeShortenedURL = hostName == "youtu.be";
    
    // Short URL of the form: http://youtu.be/v1d301D
    if (isYouTubeShortenedURL) {
        const String& videoID = url.lastPathComponent();
        if (videoID.isEmpty() || videoID == "/")
            return URL();
        
        return createYouTubeURL(videoID, emptyString());
    }
    
    String path = url.path();
    String query = url.query();
    String fragment = url.fragmentIdentifier();
    
    // On the YouTube mobile web app, the path and query string are put into the
    // fragment so that one web page is only ever loaded (see <rdar://problem/9550639>).
    if (isYouTubeMobileWebAppURL) {
        size_t location = fragment.find('?');
        if (location == notFound) {
            path = fragment;
            query = emptyString();
        } else {
            path = fragment.substring(0, location);
            query = fragment.substring(location + 1);
        }
        fragment = emptyString();
    }
    
    if (path.lower() == "/watch") {
        if (!query.isEmpty()) {
            const auto& queryDictionary = queryKeysAndValues(query);
            String videoID = valueForKey(queryDictionary, "v");
            
            if (!videoID.isEmpty()) {
                const auto& fragmentDictionary = queryKeysAndValues(url.fragmentIdentifier());
                String timeID = valueForKey(fragmentDictionary, "t");
                return createYouTubeURL(videoID, timeID);
            }
        }
        
        // May be a new-style link (see <rdar://problem/7733692>).
        if (fragment.startsWith('!')) {
            query = fragment.substring(1);
            
            if (!query.isEmpty()) {
                const auto& queryDictionary = queryKeysAndValues(query);
                String videoID = valueForKey(queryDictionary, "v");
                
                if (!videoID.isEmpty()) {
                    String timeID = valueForKey(queryDictionary, "t");
                    return createYouTubeURL(videoID, timeID);
                }
            }
        }
    } else if (hasCaseInsensitivePrefix(path, "/v/") || hasCaseInsensitivePrefix(path, "/e/")) {
        String videoID = url.lastPathComponent();
        
        // These URLs are funny - they don't have a ? for the first query parameter.
        // Strip all characters after and including '&' to remove extraneous parameters after the video ID.
        size_t ampersand = videoID.find('&');
        if (ampersand != notFound)
            videoID = videoID.substring(0, ampersand);
        
        if (!videoID.isEmpty())
            return createYouTubeURL(videoID, emptyString());
    }
    
    return URL();
}

String YouTubePluginReplacement::youTubeURL(const String& srcString)
{
    URL srcURL(URL(), srcString);

    bool isYouTubeShortenedURL = false;
    URL youTubeURL = processAndCreateYouTubeURL(srcURL, isYouTubeShortenedURL);
    if (srcURL.isEmpty() || youTubeURL.isEmpty())
        return srcString;

    // Transform the youtubeURL (youtube:VideoID) to iframe embed url which has the format: http://www.youtube.com/embed/VideoID
    const String& srcPath = srcURL.path();
    const String& videoID = youTubeURL.string().substring(youTubeURL.protocol().length() + 1);
    size_t locationOfVideoIDInPath = srcPath.find(videoID);

    size_t locationOfPathBeforeVideoID = notFound;
    if (locationOfVideoIDInPath != notFound) {
        ASSERT(locationOfVideoIDInPath);
    
        // From the original URL, we need to get the part before /path/VideoId.
        locationOfPathBeforeVideoID = srcString.find(srcPath.substring(0, locationOfVideoIDInPath));
    } else if (srcPath.lower() == "/watch") {
        // From the original URL, we need to get the part before /watch/#!v=VideoID
        locationOfPathBeforeVideoID = srcString.find("/watch");
    } else
        return srcString;

    ASSERT(locationOfPathBeforeVideoID != notFound);

    const String& srcURLPrefix = srcString.substring(0, locationOfPathBeforeVideoID);
    String query = srcURL.query();

    // By default, the iframe will display information like the video title and uploader on top of the video. Don't display
    // them if the embeding html doesn't specify it.
    if (!query.isEmpty() && !query.contains("showinfo"))
        query.append("&showinfo=0");
    else
        query = "showinfo=0";
    
    // Append the query string if it is valid. Some sites apparently forget to add "?" for the query string, in that case,
    // we will discard the parameters in the url.
    // See: <rdar://problem/11535155>
    StringBuilder finalURL;
    if (isYouTubeShortenedURL)
        finalURL.append("http://www.youtube.com");
    else
        finalURL.append(srcURLPrefix);
    finalURL.appendLiteral("/embed/");
    finalURL.append(videoID);
    if (!query.isEmpty()) {
        finalURL.appendLiteral("?");
        finalURL.append(query);
    }
    return finalURL.toString();
}
    
bool YouTubePluginReplacement::supportsURL(const URL& url)
{
    return isYouTubeURL(url);
}
    
}

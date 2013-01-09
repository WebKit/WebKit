/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebPageSerializer_h
#define WebPageSerializer_h

#include <public/WebCString.h>
#include <public/WebCommon.h>
#include <public/WebURL.h>

namespace WebKit {
class WebFrame;
class WebPageSerializerClient;
class WebString;
class WebView;
template <typename T> class WebVector;

// Get html data by serializing all frames of current page with lists
// which contain all resource links that have local copy.
class WebPageSerializer {
public:
    struct Resource {
        WebURL url;
        WebCString mimeType;
        WebCString data;
    };

    // Serializes all the frames from the WebView, retrieves the page's
    // resources (such as images and CSS) and adds them to the passed vector.
    // The first resource in that vector is the top frame contents.
    // Note that this also strips-out any script tag or link to JavaScript.
    WEBKIT_EXPORT static void serialize(WebView*, WebVector<Resource>*);

    // Serializes the WebView contents to a MHTML representation.
    WEBKIT_EXPORT static WebCString serializeToMHTML(WebView*);

    // Similar to serializeToMHTML but uses binary encoding for the MHTML parts.
    // This results in a smaller MHTML file but it might not be supported by other browsers.
    WEBKIT_EXPORT static WebCString serializeToMHTMLUsingBinaryEncoding(WebView*);

    // IMPORTANT:
    // The API below is an older implementation of a pageserialization that
    // will be removed soon.


    // This function will find out all frames and serialize them to HTML data.
    // We have a data buffer to temporary saving generated html data. We will
    // sequentially call WebPageSeriazlierClient once the data buffer is full.
    //
    // Return false means no available frame has been serialized, otherwise
    // return true.
    //
    // The parameter frame specifies which frame need to be serialized.
    // The parameter recursive specifies whether we need to
    // serialize all sub frames of the specified frame or not.
    // The parameter client specifies the pointer of interface
    // WebPageSerializerClient providing a sink interface to receive the
    // individual chunks of data to be saved.
    // The parameter links contain original URLs of all saved links.
    // The parameter localPaths contain corresponding local file paths of all
    // saved links, which matched with vector:links one by one.
    // The parameter localDirectoryName is relative path of directory which
    // contain all saved auxiliary files included all sub frames and resources.
    WEBKIT_EXPORT static bool serialize(WebFrame*,
                                        bool recursive,
                                        WebPageSerializerClient*,
                                        const WebVector<WebURL>& links,
                                        const WebVector<WebString>& localPaths,
                                        const WebString& localDirectoryName);

    // Retrieve all the resource for the passed view, including the main frame
    // and sub-frames. Returns true if all resources were retrieved
    // successfully.
    WEBKIT_EXPORT static bool retrieveAllResources(WebView*,
                                                   const WebVector<WebCString>& supportedSchemes,
                                                   WebVector<WebURL>* resources,
                                                   WebVector<WebURL>* frames);

    // FIXME: The following are here for unit testing purposes. Consider
    // changing the unit tests instead.

    // Generate the META for charset declaration.
    WEBKIT_EXPORT static WebString generateMetaCharsetDeclaration(const WebString& charset);
    // Generate the MOTW declaration.
    WEBKIT_EXPORT static WebString generateMarkOfTheWebDeclaration(const WebURL&);
    // Generate the default base tag declaration.
    WEBKIT_EXPORT static WebString generateBaseTagDeclaration(const WebString& baseTarget);
};

} // namespace WebKit

#endif

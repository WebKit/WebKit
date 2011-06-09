/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

WebInspector.ResourcePreviewView = function(resource, sourceView)
{
    WebInspector.ResourceView.call(this, resource);
    this._sourceView = sourceView;
}

WebInspector.ResourcePreviewView.prototype = {
    hasContent: function()
    {
        return true;
    },

    show: function(parentElement)
    {
        WebInspector.ResourceView.prototype.show.call(this, parentElement);
        this._ensureInnerViewShown();
    },

    _ensureInnerViewShown: function()
    {
        if (this._innerViewShowRequested)
            return;
        this._innerViewShowRequested = true;

        function callback()
        {
            this._createInnerView(callback).show(this.element);
            this._innerViewShowRequested = false;
        }

        this.resource.requestContent(callback.bind(this));
    },

    _createInnerView: function(callback)
    {
        if (this.resource.hasErrorStatusCode() && this.resource.content)
            return new WebInspector.ResourceHTMLView(this.resource);

        if (this.resource.category === WebInspector.resourceCategories.xhr && this.resource.content) {
            var parsedJSON = WebInspector.ResourceJSONView.parseJSON(this.resource.content);
            if (parsedJSON)
                return new WebInspector.ResourceJSONView(this.resource, parsedJSON);
        }

        if (WebInspector.ResourceView.hasTextContent(this.resource))
            return this._sourceView;

        return WebInspector.ResourceView.nonSourceViewForResource(this.resource);
    },
    
    
}

WebInspector.ResourcePreviewView.prototype.__proto__ = WebInspector.ResourceView.prototype;

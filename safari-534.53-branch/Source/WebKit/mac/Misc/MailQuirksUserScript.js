/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

(function() {
    function childrenBefore(parent, stopAt)
    {
        var children = [];
        for (var child = parent.firstChild; child != stopAt; child = child.nextSibling)
            children.push(child);
        return children;
    }

    // If html or body is missing, Mail.app's assumption that
    // document.firstChild.firstChild == document.body is wrong anyway,
    // so return null to not move anything.
    if (!document.documentElement || !document.body)
        return;

    var children = childrenBefore(document, document.documentElement);
    children = children.concat(childrenBefore(document.documentElement, document.body));

    for (var i = children.length - 1; i >= 0; i--) {
        var child = children[i];
        // It's not possible to move doctype nodes into the body, so just remove them.
        if (child.nodeType == child.DOCUMENT_TYPE_NODE)
            child.parentNode.removeChild(child);
        else
            document.body.insertBefore(child, document.body.firstChild);
    }
})();

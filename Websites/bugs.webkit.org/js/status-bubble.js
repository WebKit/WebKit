/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

function handleStatusBubbleMessage(event) {
    if ((event.origin !== 'https://webkit-queues.webkit.org' && event.origin !== 'https://ews.webkit.org') || !event.data.height)
        return;

    for (const iframe of document.querySelectorAll('.statusBubble > iframe')) {
        if (iframe.contentWindow === event.source) {
            iframe.style.height = event.data.height + 'px';
            iframe.style.width = event.data.width + 'px';
            break;
        }
    }
}

function handleStatusBubbleLoad(iframe) {
    iframe.contentWindow.postMessage('containerMetrics', 'https://webkit-queues.webkit.org');
}

function handleStatusBubbleLoadNewEWS(iframe) {
    iframe.contentWindow.postMessage('containerMetrics', 'https://ews.webkit.org');
}

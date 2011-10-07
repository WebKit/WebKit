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

var console = {}
console.warn = function(message) {}
/** @param {string=} message */
console.assert = function(expr, message) {}

var WebInspector = {}

/**
 * @param {string} url
 * @param {string=} linkText
 * @param {string=} classes
 * @param {boolean=} isExternal
 * @param {string=} tooltipText
 */
WebInspector.linkifyURLAsNode = function(url, linkText, classes, isExternal, tooltipText) {}

/**
 * @param {string} url
 * @param {string=} linkText
 * @param {string=} classes
 * @param {boolean=} isExternal
 * @param {string=} tooltipText
 */
WebInspector.linkifyURL = function(url, linkText, classes, isExternal, tooltipText) {}

/** @param {string} url */
WebInspector.linkifyStringAsFragment = function(url) {}
/** @param {string} url */
WebInspector.linkifyStringAsFragmentWithCustomLinkifier = function(url, linkifier) {}

/**
 * @param {string=} classes
 * @param {string=} tooltipText
 */
WebInspector.linkifyResourceAsNode = function(url, lineNumber, classes, tooltipText) {}

/**
 * @param {string} url
 * @param {number} lineNumber
 */
WebInspector.formatLinkText = function(url, lineNumber) {}

/**
 * @param {string} url
 */
WebInspector.displayNameForURL = function(url) {}

/**
 * @param {string} url
 * @param {string} href
 */
WebInspector.completeURL = function(url, href) {}

/**
 * @param {string} str
 * @param {...*} varArgs
 */
WebInspector.UIString = function(str, varArgs) {}
var InspectorBackend = {}

var JSON = {}
/** @param {string} str */
JSON.parse = function(str) {}

/** @param {Object} obj */
/** @return {string} */
JSON.stringify = function(str) {}

/**
 * @param {boolean=} param
 */
Element.prototype.scrollIntoViewIfNeeded = function(param) {}

/**
 * @return {number}
 */
Element.prototype.selectionLeftOffset = function() {}


/**
 * @param {boolean=} onlyFirst
 */
Array.prototype.remove = function(obj, onlyFirst) {}

Array.prototype.keySet = function() {}

WebInspector.extensionServer = {}
WebInspector.extensionServer.notifyResourceContentCommitted = function(resource, content) {}
WebInspector.extensionServer.notifyPanelShown = function(panel) {}
WebInspector.extensionServer.notifyPanelHidden = function(panel) {}
WebInspector.extensionServer.notifyObjectSelected = function(object) {}

/**
 * @param {string} url
 * @return {?WebInspector.Resource}
 */
WebInspector.resourceForURL = function(url) {}

WebInspector.debuggerPresentationModel = {
    linkifyLocation: function(url, lineNumber, columnNumber, className) {}
}

/**
 * @param {NetworkAgent.RequestId} requestId
 * @return {?WebInspector.Resource}
 */
WebInspector.networkResourceById = function(requestId)
{
}

/**
 * @param {WebInspector.ContextMenu} contextMenu
 * @param {Node} contextNode
 * @param {Event} event
 */
WebInspector.populateHrefContextMenu = function(contextMenu, contextNode, event)
{
}

/**
 * @return {boolean}
 */
WebInspector.useLowerCaseMenuTitles = function()
{
}

WebInspector.panels = {};

/**
 * @type {WebInspector.ScriptsPanel}
 */
WebInspector.panels.scripts = null;

/**
 * @return {WebInspector.Panel} 
 */
WebInspector.currentPanel = function() {}

/**
 * @param {WebInspector.Panel} panel
 */
WebInspector.setCurrentPanel = function(panel) {}

/**
 * @type {?Element}
 */
WebInspector.currentFocusElement = null;

/**
 * @type {?Element}
 */
WebInspector.previousFocusElement = null;

/**
 * @type {string}
 */
WebInspector.platformFlavor = "";

/** @param {Element} element */
window.getComputedStyle = function(element) {}

/**
 * @type {boolean}
 */
Event.prototype.isMetaOrCtrlForTest = false;

/**
 * @param {...*} vararg
 */
Event.prototype.initWebKitWheelEvent = function(vararg) {}

/**
 * @param {string=} messageLevel
 * @param {boolean=} showConsole
 */
WebInspector.log = function(message, messageLevel, showConsole) {}

WebInspector.resourceURLForRelatedNode = function(node, url) {}

WebInspector.addMainEventListeners = function(doc) {}

WebInspector.openResource = function(url, external) {}

DOMApplicationCache.prototype.UNCACHED = 0;
DOMApplicationCache.prototype.IDLE = 1;
DOMApplicationCache.prototype.CHECKING = 2;
DOMApplicationCache.prototype.DOWNLOADING = 3;
DOMApplicationCache.prototype.UPDATEREADY = 4;
DOMApplicationCache.prototype.OBSOLETE = 5;

WebInspector.openRequestInNetworkPanel = function(request) {}

var InjectedFakeWorker = function() {}

WebInspector.queryParamsObject = {}

WebInspector.Events = {
    InspectorClosing: "InspectorClosing"
}

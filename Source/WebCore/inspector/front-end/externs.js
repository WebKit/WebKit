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

// WebKit Web Facing API
var console = {}
/** @param {...*} vararg */
console.warn = function(vararg) {}
/** @param {...*} vararg */
console.assert = function(vararg) {}
/** @param {...*} vararg */
console.error = function(vararg) {}
console.trace = function() {}

var JSON = {}
/** @param {string} str */
JSON.parse = function(str) {}
/**
 * @param {*} obj
 * @param {Function=} replacer
 * @param {number=} space
 * @return {string}
 */
JSON.stringify = function(obj, replacer, space) {}

/** @param {boolean=} param */
Element.prototype.scrollIntoViewIfNeeded = function(param) {}
/** @type {boolean} */
Event.prototype.isMetaOrCtrlForTest = false;
/** @param {...*} vararg */
Event.prototype.initWebKitWheelEvent = function(vararg) {}
Event.prototype.stopImmediatePropagation = function() {}

/** @param {Element} element */
window.getComputedStyle = function(element) {}
/** @param {*} message */
function postMessage(message) {}

/**
 * @param {string} eventName
 * @param {Function} listener
 * @param {boolean=} capturing
 */
function addEventListener(eventName, listener, capturing) {}

/** @param {boolean=} onlyFirst */
Array.prototype.remove = function(obj, onlyFirst) {}
Array.prototype.keySet = function() {}
/** @return {number} */
Array.prototype.upperBound = function(anchor) {}
/** @return {number} */
Array.prototype.binaryIndexOf = function(anchor) {}
Array.prototype.sortRange = function(comparator, leftBound, rightBound, k) {}

/**
 * @this {Array.<number>}
 * @param {function(number,number):boolean} comparator
 * @param {number} left
 * @param {number} right
 * @param {number} pivotIndex
 * @return {number}
 */
Array.prototype.partition = function(comparator, left, right, pivotIndex) {}

/**
 * @this {Array.<number>}
 * @param {number} k
 * @param {function(number,number):boolean=} comparator
 * @return {number}
 */
Array.prototype.qselect = function(k, comparator) {}

/**
 * @this {Array.<*>}
 * @param {string} field
 * @return {Array.<*>}
 */
Array.prototype.select = function(field) {}

DOMApplicationCache.prototype.UNCACHED = 0;
DOMApplicationCache.prototype.IDLE = 1;
DOMApplicationCache.prototype.CHECKING = 2;
DOMApplicationCache.prototype.DOWNLOADING = 3;
DOMApplicationCache.prototype.UPDATEREADY = 4;
DOMApplicationCache.prototype.OBSOLETE = 5;



// Inspector Backend
var InspectorBackend = {}
InspectorBackend.runAfterPendingDispatches = function(message) {}


// FIXME: remove everything below.
var WebInspector = {}
  
WebInspector.panels = {};

/**
 * @type {WebInspector.InspectorView}
 */
WebInspector.inspectorView;

/**
 * @param {Element} element
 * @param {WebInspector.View} view
 * @param {function()=} onclose
 */
WebInspector.showViewInDrawer = function(element, view, onclose) {}

WebInspector.closeViewInDrawer = function() {}

/**
 * @param {string=} messageLevel
 * @param {boolean=} showConsole
 */
WebInspector.log = function(message, messageLevel, showConsole) {}

WebInspector.addMainEventListeners = function(doc) {}

WebInspector.openResource = function(url, external) {}

WebInspector.showConsole = function() {}

/**
 * @param {string} expression
 * @param {boolean=} showResultOnly
 */
WebInspector.evaluateInConsole = function(expression, showResultOnly) {}

WebInspector.queryParamsObject = {}

WebInspector.Events = {
    InspectorClosing: "InspectorClosing"
}

/** Extensions API */

/** @constructor */
function AuditCategory() {}
/** @constructor */
function AuditResult() {}
/** @constructor */
function EventSink() {}
/** @constructor */
function ExtensionSidebarPane() {}
/** @constructor */
function Panel() {}
/** @constructor */
function PanelWithSidebar() {}
/** @constructor */
function Request() {}
/** @constructor */
function Resource() {}
/** @constructor */
function Timeline() {}

/** @type {string} */
Location.prototype.origin = "";

/**
 * @constructor
 */
function ExtensionDescriptor() {
    this.startPage = "";
    this.name = "";
}

/**
 * @constructor
 */
function ExtensionReloadOptions() {
    this.ignoreCache = false;
    this.injectedScript = "";
    this.userAgent = "";
}

/**
 * @type {WebInspector.HandlerRegistry}
 */
WebInspector.openAnchorLocationRegistry = null;

/**
 * @param {WebInspector.Panel} panel
 */
WebInspector.showPanelForAnchorNavigation = function(panel)
{
}

WebInspector.showPanel = function(panel)
{
}

/**
 * @type {string} 
 */
WebInspector.inspectedPageDomain;

WebInspector.isCompactMode = function() { return false; }

WebInspector.SourceJavaScriptTokenizer = {}
WebInspector.SourceJavaScriptTokenizer.Keywords = {}

var InspectorTest = {}

/* jsdifflib API */
var difflib = {};
difflib.stringAsLines = function(text) { return []; }
/** @constructor */
difflib.SequenceMatcher = function(baseText, newText) { }
difflib.SequenceMatcher.prototype.get_opcodes = function() { return []; }

/** @constructor */
WebInspector.CodeMirrorTextEditor = function(url, delegate) { }

WebInspector.ProfileURLRegExp = "";

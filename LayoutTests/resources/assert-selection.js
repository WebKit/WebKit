// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';
// This file provides |assert_selection(sample, tester, expectedText)| assertion
// to W3C test harness to write editing test cases easier.
//
// |sample| is an HTML fragment text which is inserted as |innerHTML|. It should
// have at least one focus boundary point marker "|" and at most one anchor
// boundary point marker "^".
//
// |tester| is either name with parameter of execCommand or function taking
// one parameter |Selection|.
//
// |expectedText| is an HTML fragment text containing at most one focus marker
// and anchor marker. If resulting selection is none, you don't need to have
// anchor and focus markers.
//
// Example:
//  test(() => {
//    assert_selection(
//      '|foo',
//      (selection) => selection.modify('extent', 'forward, 'character'),
//      '<a href="http://bar">^f|oo</a>'
//  });
//
//  test(() => {
//    assert_selection(
//      'x^y|z',
//      'bold',  // execCommand name as a test
//      'x<b>y</b>z',
//      'Insert B tag');
//  });
//
//  test(() => {
//    assert_selection(
//      'x^y|z',
//      'createLink http://foo',  // execCommand name and parameter
//      'x<a href="http://foo/">y</a></b>z',
//      'Insert B tag');
//  });
//
//
// TODO(yosin): Please use "clang-format -style=Chromium -i" for formatting
// this file.
(function() {
/**
 * @param {!Node} node
 * @return {boolean}
 */
function isCharacterData(node) {
  return node.nodeType === Node.TEXT_NODE ||
      node.nodeType === Node.COMMENT_NODE;
}
/**
 * @param {!Node} node
 * @return {boolean}
 */
function isElement(node) {
  return node.nodeType === Node.ELEMENT_NODE;
}
/**
 * @param {!Node} node
 * @param {number} offset
 */
function checkValidNodeAndOffset(node, offset) {
  if (!node)
    throw new Error('Node parameter should not be a null.');
  if (offset < 0)
    throw new Error(`Assumes ${offset} >= 0`);
  if (isElement(node)) {
    if (offset > node.childNodes.length)
      throw new Error(`Bad offset ${offset} for ${node}`);
    return;
  }
  if (isCharacterData(node)) {
    if (offset > node.nodeValue.length)
      throw new Error(`Bad offset ${offset} for ${node}`);
    return;
  }
  throw new Error(`Invalid node: ${node}`);
}
class SampleSelection {
  /** @public */
  constructor() {
    /** @type {?Node} */
    this.anchorNode_ = null;
    /** @type {number} */
    this.anchorOffset_ = 0;
    /** @type {?Node} */
    this.focusNode_ = null;
    /** @type {number} */
    this.focusOffset_ = 0;
  }
  /**
   * @public
   * @param {!Node} node
   * @param {number} offset
   */
  collapse(node, offset) {
    checkValidNodeAndOffset(node, offset);
    this.anchorNode_ = this.focusNode_ = node;
    this.anchorOffset_ = this.focusOffset_ = offset;
  }
  /**
   * @public
   * @param {!Node} node
   * @param {number} offset
   */
  extend(node, offset) {
    checkValidNodeAndOffset(node, offset);
    this.focusNode_ = node;
    this.focusOffset_ = offset;
  }
  /** @public @return {?Node} */
  get anchorNode() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.anchorNode_;
  }
  /** @public @return {number} */
  get anchorOffset() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.anchorOffset_;
  }
  /** @public @return {?Node} */
  get focusNode() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.focusNode_;
  }
  /** @public @return {number} */
  get focusOffset() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.focusOffset_;
  }
  /**
   * @public
   * @return {boolean}
   */
  get isCollapsed() {
    return this.anchorNode === this.focusNode &&
        this.anchorOffset === this.focusOffset;
  }
  /**
   * @public
   * @return {boolean}
   */
  get isNone() { return this.anchorNode_ === null; }
  /**
   * @public
   * @param {!Selection} domSelection
   * @return {!SampleSelection}
   */
  static fromDOMSelection(domSelection) {
    /** type {!SampleSelection} */
    const selection = new SampleSelection();
    selection.anchorNode_ = domSelection.anchorNode;
    selection.anchorOffset_ = domSelection.anchorOffset;
    selection.focusNode_ = domSelection.focusNode;
    selection.focusOffset_ = domSelection.focusOffset;
    return selection;
  }
  /** @override */
  toString() {
    if (this.isNone)
      return 'SampleSelection()';
    if (this.isCollapsed)
      return `SampleSelection(${this.focusNode_}@${this.focusOffset_})`;
    return `SampleSelection(anchor: ${this.anchorNode_}@${this.anchorOffset_}` +
        `focus: ${this.focusNode_}@${this.focusOffset_}`;
  }
}
// Extracts selection from marker "^" as anchor and "|" as focus from
// DOM tree and removes them.
class Parser {
  /** @private */
  constructor() {
    /** @type {?Node} */
    this.anchorNode_ = null;
    /** @type {number} */
    this.anchorOffset_ = 0;
    /** @type {?Node} */
    this.focusNode_ = null;
    /** @type {number} */
    this.focusOffset_ = 0;
  }
  /**
   * @public
   * @return {!SampleSelection}
   */
  get selection() {
    const selection = new SampleSelection();
    if (!this.anchorNode_ && !this.focusNode_)
      return selection;
    if (this.anchorNode_ && this.focusNode_) {
      selection.collapse(this.anchorNode_, this.anchorOffset_);
      selection.extend(this.focusNode_, this.focusOffset_);
      return selection;
    }
    if (this.focusNode_) {
      selection.collapse(this.focusNode_, this.focusOffset_);
      return selection;
    }
    throw new Error('There is no focus marker');
  }
  /**
   * @private
   * @param {!CharacterData} node
   * @param {number} nodeIndex
   */
  handleCharacterData(node, nodeIndex) {
    /** @type {string} */
    const text = node.nodeValue;
    /** @type {number} */
    const anchorOffset = text.indexOf('^');
    /** @type {number} */
    const focusOffset = text.indexOf('|');
    /** @type {!Node} */
    const parentNode = node.parentNode;
    node.nodeValue = text.replace('^', '').replace('|', '');
    if (node.nodeValue.length == 0) {
      if (anchorOffset >= 0)
        this.rememberSelectionAnchor(parentNode, nodeIndex);
      if (focusOffset >= 0)
        this.rememberSelectionFocus(parentNode, nodeIndex);
      node.remove();
      return;
    }
    if (anchorOffset >= 0 && focusOffset >= 0) {
      if (anchorOffset > focusOffset) {
        this.rememberSelectionAnchor(node, anchorOffset - 1);
        this.rememberSelectionFocus(node, focusOffset);
        return;
      }
      this.rememberSelectionAnchor(node, anchorOffset);
      this.rememberSelectionFocus(node, focusOffset - 1);
      return;
    }
    if (anchorOffset >= 0) {
      this.rememberSelectionAnchor(node, anchorOffset);
      return;
    }
    if (focusOffset < 0)
      return;
    this.rememberSelectionFocus(node, focusOffset);
  }
  /**
   * @private
   * @param {!Element} element
   */
  handleElementNode(element) {
    /** @type {number} */
    let childIndex = 0;
    for (const child of Array.from(element.childNodes)) {
      this.parseInternal(child, childIndex);
      if (!child.parentNode)
        continue;
      ++childIndex;
    }
  }
  /**
   * @private
   * @param {!Node} node
   * @return {!SampleSelection}
   */
  parse(node) {
    this.parseInternal(node, 0);
    return this.selection;
  }
  /**
   * @private
   * @param {!Node} node
   * @param {number} nodeIndex
   */
  parseInternal(node, nodeIndex) {
    if (isElement(node))
      return this.handleElementNode(node);
    if (isCharacterData(node))
      return this.handleCharacterData(node, nodeIndex);
    throw new Error(`Unexpected node ${node}`);
  }
  /**
   * @private
   * @param {!Node} node
   * @param {number} offset
   */
  rememberSelectionAnchor(node, offset) {
    checkValidNodeAndOffset(node, offset);
    console.assert(
        this.anchorNode_ === null, 'Anchor marker should be one.',
        this.anchorNode_, this.anchorOffset_);
    this.anchorNode_ = node;
    this.anchorOffset_ = offset;
  }
  /**
   * @private
   * @param {!Node} node
   * @param {number} offset
   */
  rememberSelectionFocus(node, offset) {
    checkValidNodeAndOffset(node, offset);
    console.assert(
        this.focusNode_ === null, 'Focus marker should be one.',
        this.focusNode_, this.focusOffset_);
    this.focusNode_ = node;
    this.focusOffset_ = offset;
  }
  /**
   * @public
   * @param {!Node} node
   * @return {!SampleSelection}
   */
  static parse(node) { return (new Parser()).parse(node); }
}
// TODO(yosin): Once we can import JavaScript file from scripts, we should
// import "imported/wpt/html/resources/common.js", since |HTML5_VOID_ELEMENTS|
// is defined in there.
/**
 * @const @type {!Set<string>}
 * only void (without end tag) HTML5 elements
 */
const HTML5_VOID_ELEMENTS = new Set([
  'area', 'base', 'br', 'col', 'command', 'embed', 'hr', 'img', 'input',
  'keygen', 'link', 'meta', 'param', 'source','track', 'wbr' ]);
class Serializer {
  /**
   * @public
   * @param {!SampleSelection} selection
   */
  constructor(selection) {
    /** @type {!SampleSelection} */
    this.selection_ = selection;
    /** @type {!Array<strings>} */
    this.strings_ = [];
  }
  /**
   * @private
   * @param {string} string
   */
  emit(string) { this.strings_.push(string); }
  /**
   * @private
   * @param {!HTMLElement} parentNode
   * @param {number} childIndex
   */
  handleSelection(parentNode, childIndex) {
    if (this.selection_.isNone)
      return;
    if (parentNode === this.selection_.focusNode &&
        childIndex === this.selection_.focusOffset) {
      this.emit('|');
      return;
    }
    if (parentNode === this.selection_.anchorNode &&
        childIndex === this.selection_.anchorOffset) {
      this.emit('^');
    }
  }
  /**
   * @private
   * @param {!CharacterData} node
   */
  handleCharacterData(node) {
    /** @type {string} */
    const text = node.nodeValue;
    if (this.selection_.isNone)
      return this.emit(text);
    /** @type {number} */
    const anchorOffset = this.selection_.anchorOffset;
    /** @type {number} */
    const focusOffset = this.selection_.focusOffset;
    if (node === this.selection_.focusNode &&
        node === this.selection_.anchorNode) {
      if (anchorOffset === focusOffset) {
        this.emit(text.substr(0, focusOffset));
        this.emit('|');
        this.emit(text.substr(focusOffset));
        return;
      }
      if (anchorOffset < focusOffset) {
        this.emit(text.substr(0, anchorOffset));
        this.emit('^');
        this.emit(text.substr(anchorOffset, focusOffset - anchorOffset));
        this.emit('|');
        this.emit(text.substr(focusOffset));
        return;
      }
      this.emit(text.substr(0, focusOffset));
      this.emit('|');
      this.emit(text.substr(focusOffset, anchorOffset - focusOffset));
      this.emit('^');
      this.emit(text.substr(anchorOffset));
      return;
    }
    if (node === this.selection_.anchorNode) {
      this.emit(text.substr(0, anchorOffset));
      this.emit('^');
      this.emit(text.substr(anchorOffset));
      return;
    }
    if (node === this.selection_.focusNode) {
      this.emit(text.substr(0, focusOffset));
      this.emit('|');
      this.emit(text.substr(focusOffset));
      return;
    }
    this.emit(text);
  }
  /**
   * @private
   * @param {!HTMLElement} element
   */
  handleElementNode(element) {
    /** @type {string} */
    const tagName = element.tagName.toLowerCase();
    this.emit(`<${tagName}`);
    Array.from(element.attributes)
        .sort((attr1, attr2) => attr1.name.localeCompare(attr2.name))
        .forEach(attr => {
          if (attr.value === '')
            return this.emit(` ${attr.name}`);
          const value = attr.value.replace(/&/g, '&amp;')
                            .replace(/\u0022/g, '&quot;')
                            .replace(/\u0027/g, '&apos;');
          this.emit(` ${attr.name}="${value}"`);
        });
    this.emit('>');
    if (element.childNodes.length === 0 &&
        HTML5_VOID_ELEMENTS.has(tagName)) {
      return;
    }
    this.serializeChildren(element);
    this.emit(`</${tagName}>`);
  }
  /**
   * @public
   * @param {!HTMLDocument} document
   */
  serialize(document) {
    if (document.body)
        this.serializeChildren(document.body);
    else
        this.serializeInternal(document.documentElement);
    return this.strings_.join('');
  }
  /**
   * @private
   * @param {!HTMLElement} element
   */
  serializeChildren(element) {
    /** @type {!Array<!Node>} */
    const childNodes = Array.from(element.childNodes);
    if (childNodes.length === 0) {
      this.handleSelection(element, 0);
      return;
    }
    /** @type {number} */
    let childIndex = 0;
    for (const child of childNodes) {
      this.handleSelection(element, childIndex);
      this.serializeInternal(child, childIndex);
      ++childIndex;
    }
    this.handleSelection(element, childIndex);
  }
  /**
   * @private
   * @param {!Node} node
   */
  serializeInternal(node) {
    if (isElement(node))
      return this.handleElementNode(node);
    if (isCharacterData(node))
      return this.handleCharacterData(node);
    throw new Error(`Unexpected node ${node}`);
  }
}
/**
 * @this {!DOMSelection}
 * @param {string} html
 * @param {string=} opt_text
 */
function setClipboardData(html, opt_text) {
  assert_not_equals(window.internals, undefined,
    'This test requests clipboard access from JavaScript.');
  function computeTextData() {
    if (opt_text !== undefined)
      return opt_text;
    const element = document.createElement('div');
    element.innerHTML = html;
    return element.textContent;
  }
  function copyHandler(event) {
    const clipboardData = event.clipboardData;
    clipboardData.setData('text/plain', computeTextData());
    clipboardData.setData('text/html', html);
    event.preventDefault();
  }
  document.addEventListener('copy', copyHandler);
  document.execCommand('copy');
  document.removeEventListener('copy', copyHandler);
}
class Sample {
  /**
   * @public
   * @param {string} sampleText
   */
  constructor(sampleText) {
    /** @const @type {!HTMLIFame} */
    this.iframe_ = document.createElement('iframe');
    if (!document.body)
        document.body = document.createElement("body");
    document.body.appendChild(this.iframe_);
    /** @const @type {!HTMLDocument} */
    this.document_ = this.iframe_.contentDocument;
    /** @const @type {!Selection} */
    this.selection_ = this.iframe_.contentWindow.getSelection();
    this.selection_.document = this.document_;
    this.selection_.document.offsetLeft = this.iframe_.offsetLeft;
    this.selection_.document.offsetTop = this.iframe_.offsetTop;
    this.selection_.setClipboardData = setClipboardData;
    // Set focus to sample IFRAME to make |eventSender| and
    // |testRunner.execCommand()| to work on sample rather than main frame.
    this.iframe_.focus();
    this.load(sampleText);
  }
  /** @return {!HTMLDocument} */
  get document() { return this.document_; }
  /** @return {!Selection} */
  get selection() { return this.selection_; }
  /**
   * @private
   * @param {string} sampleText
   */
  load(sampleText) {
    const anchorMarker = sampleText.indexOf('^');
    const focusMarker = sampleText.indexOf('|');
    if (focusMarker < 0 && anchorMarker >= 0) {
      throw new Error(`You should specify caret position in "${sampleText}".`);
    }
    if (focusMarker != sampleText.lastIndexOf('|')) {
      throw new Error(
          `You should have at least one focus marker "|" in "${sampleText}".`);
    }
    if (anchorMarker != sampleText.lastIndexOf('^')) {
      throw new Error(
          `You should have at most one anchor marker "^" in "${sampleText}".`);
    }
    if (anchorMarker >= 0 && focusMarker >= 0 &&
        (anchorMarker + 1 === focusMarker || anchorMarker - 1 === focusMarker)) {
      throw new Error(
          `You should have focus marker and should not have anchor marker if and only if selection is a caret in "${sampleText}".`);
    }
    this.document_.body.innerHTML = sampleText;
    /** @type {!SampleSelection} */
    const selection = Parser.parse(this.document_.body);
    if (selection.isNone)
      return;
    this.selection_.collapse(selection.anchorNode, selection.anchorOffset);
    this.selection_.extend(selection.focusNode, selection.focusOffset);
  }
  /**
   * @public
   */
  remove() { this.iframe_.remove(); }
  /**
   * @public
   * @return {string}
   */
  serialize() {
    /** @type {!SampleSelection} */
    const selection = SampleSelection.fromDOMSelection(this.selection_);
    /** @type {!Serializer} */
    const serializer = new Serializer(selection);
    return serializer.serialize(this.document_);
  }
}
function assembleDescription() {
  function getStack() {
    let stack;
    try {
      throw new Error('get line number');
    } catch (error) {
      stack = error.stack.split('\n').slice(1);
    }
    return stack
  }
  const RE_IN_ASSERT_SELECTION = new RegExp('assert_selection\\.js');
  for (const line of getStack()) {
    const match = RE_IN_ASSERT_SELECTION.exec(line);
    if (!match) {
      const RE_LAYOUTTESTS = new RegExp('LayoutTests.*');
      return RE_LAYOUTTESTS.exec(line);
    }
  }
  return '';
}
/**
 * @param {string} expectedText
 */
function checkExpectedText(expectedText) {
  /** @type {number} */
  const anchorOffset = expectedText.indexOf('^');
  /** @type {number} */
  const focusOffset = expectedText.indexOf('|');
  if (anchorOffset != expectedText.lastIndexOf('^')) {
      throw new Error(
        `You should have at most one anchor marker "^" in "${expectedText}".`);
  }
  if (focusOffset != expectedText.lastIndexOf('|')) {
      throw new Error(
        `You should have at most one focus marker "|" in "${expectedText}".`);
  }
  if (anchorOffset >= 0 && focusOffset < 0) {
    throw new Error(
        `You should have a focus marker "|" in "${expectedText}".`);
  }
  if (anchorOffset >= 0 && focusOffset >= 0 &&
     (anchorOffset + 1 === focusOffset || anchorOffset - 1 === focusOffset)) {
    throw new Error(
        `You should have focus marker and should not have anchor marker if and only if selection is a caret in "${expectedText}".`);
  }
}
/**
 * @param {string} str1
 * @param {string} str2
 * @return {string}
 */
function commonPrefixOf(str1, str2) {
  for (let index = 0; index < str1.length; ++index) {
    if (str1[index] !== str2[index])
      return str1.substr(0, index);
  }
  return str1;
}
/**
 * @param {string} inputText
 * @param {function(!Selection)|string}
 * @param {string} expectedText
 * @param {Object=} opt_options
 * @return {!Sample}
 */
function assertSelection(
    inputText, tester, expectedText, opt_options = {}) {
  const kDescription = 'description';
  const kRemoveSampleIfSucceeded = 'removeSampleIfSucceeded';
  /** @type {!Object} */
  const options = typeof(opt_options) === 'string'
      ? {description: opt_options} : opt_options;
  /** @type {string} */
  const description = kDescription in options
      ? options[kDescription] : assembleDescription();
  /** @type {boolean} */
  const removeSampleIfSucceeded = kRemoveSampleIfSucceeded in options
      ? !!options[kRemoveSampleIfSucceeded] : true;
  checkExpectedText(expectedText);
  const sample = new Sample(inputText);
  if (typeof(tester) === 'function') {
    tester.call(window, sample.selection);
  } else if (typeof(tester) === 'string') {
    const strings = tester.split(/ (.+)/);
    sample.document.execCommand(strings[0], false, strings[1]);
  } else {
    throw new Error(`Invalid tester: ${tester}`);
  }
  /** @type {string} */
  const actualText = sample.serialize();
  // We keep sample HTML when assertion is false for ease of debugging test
  // case.
  if (actualText === expectedText) {
    if (removeSampleIfSucceeded)
        sample.remove();
    return sample;
  }
  throw new Error(`${description}\n` +
    `\t expected ${expectedText},\n` +
    `\t but got  ${actualText},\n` +
    `\t sameupto ${commonPrefixOf(expectedText, actualText)}`);
}
// Export symbols
window.Sample = Sample;
window.assert_selection = assertSelection;
})();

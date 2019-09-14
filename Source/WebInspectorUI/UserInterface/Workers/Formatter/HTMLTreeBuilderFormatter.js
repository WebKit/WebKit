/*
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

// This tree builder attempts to match input text to output DOM node.
// This therefore doesn't do HTML5 tree construction like auto-closing
// specific HTML parent nodes depending on being in a particular node,
// it only does basic auto-closing. In general this tries to be a
// whitespace reformatter for input text and not generate the ultimate
// html tree that a browser would generate.
//
// When run with the XML option, all HTML specific cases are disabled.

HTMLTreeBuilderFormatter = class HTMLTreeBuilderFormatter
{
    constructor({isXML} = {})
    {
        this._isXML = !!isXML;
    }

    // Public

    get dom() { return this._dom; }

    begin()
    {
        this._dom = [];
        this._stackOfOpenElements = [];
    }

    pushParserNode(parserNode)
    {
        let containerNode = this._stackOfOpenElements[this._stackOfOpenElements.length - 1];
        if (!containerNode)
            this._pushParserNodeTopLevel(parserNode);
        else
            this._pushParserNodeStack(parserNode, containerNode);
    }

    end()
    {
        for (let node of this._stackOfOpenElements)
            node.implicitClose = true;
    }

    // Private

    _pushParserNodeTopLevel(parserNode)
    {
        if (parserNode.type === HTMLParser.NodeType.OpenTag) {
            let node = this._buildDOMNodeFromOpenTag(parserNode);
            this._dom.push(node);
            if (!this._isEmptyNode(parserNode, node))
                this._stackOfOpenElements.push(node);
            return;
        }

        if (parserNode.type === HTMLParser.NodeType.CloseTag) {
            let errorNode = this._buildErrorNodeFromCloseTag(parserNode);
            this._dom.push(errorNode);
            return;
        }

        let node = this._buildSimpleNodeFromParserNode(parserNode);
        this._dom.push(node);
    }

    _pushParserNodeStack(parserNode, containerNode)
    {
        if (parserNode.type === HTMLParser.NodeType.OpenTag) {
            let node = this._buildDOMNodeFromOpenTag(parserNode);
            containerNode.children.push(node);
            if (!this._isEmptyNode(parserNode, node))
                this._stackOfOpenElements.push(node);
            return;
        }

        if (parserNode.type === HTMLParser.NodeType.CloseTag) {
            let found = false;
            let nodesToPop = 0;
            for (let i = this._stackOfOpenElements.length - 1; i >= 0; --i) {
                nodesToPop++;
                let stackNode = this._stackOfOpenElements[i];
                if (stackNode.name === parserNode.name) {
                    found = true;
                    break;
                }
            }

            // Found a matching tag, auto-close nodes.
            if (found) {
                console.assert(nodesToPop > 0);
                for (let i = 0; i < nodesToPop - 1; ++i) {
                    let autoClosingNode = this._stackOfOpenElements.pop();
                    autoClosingNode.implicitClose = true;
                }
                let autoClosingNode = this._stackOfOpenElements.pop();
                if (parserNode.pos)
                    autoClosingNode.closeTagPos = parserNode.pos;
                return;
            }

            // Did not find a matching tag to close.
            // Treat this as an error text node.
            let errorNode = this._buildErrorNodeFromCloseTag(parserNode);
            containerNode.children.push(errorNode);
            return;
        }

        let node = this._buildSimpleNodeFromParserNode(parserNode);
        containerNode.children.push(node);
    }

    _isEmptyNode(parserNode, node)
    {
        if (parserNode.closed)
            return true;

        if (!this._isXML && HTMLTreeBuilderFormatter.TagNamesWithoutChildren.has(node.lowercaseName))
            return true;

        return false;
    }

    _buildDOMNodeFromOpenTag(parserNode)
    {
        console.assert(parserNode.type === HTMLParser.NodeType.OpenTag);

        return {
            type: HTMLTreeBuilderFormatter.NodeType.Node,
            name: parserNode.name,
            lowercaseName: parserNode.name.toLowerCase(),
            children: [],
            attributes: parserNode.attributes,
            pos: parserNode.pos,
            selfClose: parserNode.closed,
            implicitClose: false,
        };
    }

    _buildErrorNodeFromCloseTag(parserNode)
    {
        console.assert(parserNode.type === HTMLParser.NodeType.CloseTag);

        return {
            type: HTMLTreeBuilderFormatter.NodeType.Error,
            raw: "</" + parserNode.name + ">",
            pos: parserNode.pos,
        };
    }

    _buildSimpleNodeFromParserNode(parserNode)
    {
        // Pass ErrorText through as Text.
        if (parserNode.type === HTMLParser.NodeType.ErrorText)
            parserNode.type = HTMLParser.NodeType.Text;

        // Pass these nodes right through: Text, Comment, Doctype, CData
        console.assert(parserNode.type === HTMLTreeBuilderFormatter.NodeType.Text || parserNode.type === HTMLTreeBuilderFormatter.NodeType.Comment || parserNode.type === HTMLTreeBuilderFormatter.NodeType.Doctype || parserNode.type === HTMLTreeBuilderFormatter.NodeType.CData);
        console.assert("data" in parserNode);

        return parserNode;
    }
};

HTMLTreeBuilderFormatter.TagNamesWithoutChildren = new Set([
    "area",
    "base",
    "basefont",
    "br",
    "canvas",
    "col",
    "command",
    "embed",
    "frame",
    "hr",
    "img",
    "input",
    "keygen",
    "link",
    "menuitem",
    "meta",
    "param",
    "source",
    "track",
    "wbr",
]);

HTMLTreeBuilderFormatter.NodeType = {
    Text: "text",
    Node: "node",
    Comment: "comment",
    Doctype: "doctype",
    CData: "cdata",
    Error: "error",
};

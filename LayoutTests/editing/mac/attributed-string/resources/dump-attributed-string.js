var shouldAutoDump = true;

(function () {
    if (window.testRunner)
        testRunner.dumpAsText();

    window.dumpAttributedString = function dumpAttributedString(root = document.body, startContainer, startOffset, endContainer, endOffset) {
        shouldAutoDump = false;

        const markup = serializeSubtreeWithShadow(root, startContainer, startOffset, endContainer, endOffset).trim();

        if (!startContainer)
            startContainer = root;
        if (startOffset == undefined)
            startOffset = 0;
        if (!endContainer)
            endContainer = root;
        if (endOffset == undefined)
            endOffset = endContainer.childNodes.length;

        const pre = document.createElement('pre');
        const result = serializeAttributedString(textInputController.legacyAttributedString(startContainer, startOffset, endContainer, endOffset));
        pre.textContent = 'Input:\n' + markup + '\n\nOutput:\n' + result;

        document.body.innerHTML = '';
        document.body.appendChild(pre);
    }

    function serializeSubtreeWithShadow(parent, startContainer, startOffset, endContainer, endOffset) {
        function serializeCharacterData(node) {
            let data = node.data;
            let result = data;
            if (node == startContainer && node == endContainer) {
                result = data.substring(0, startOffset);
                result += '<#start>';
                result = data.substring(startOffset, endOffset);
                result += '<#end>';
                result += data.substring(endOffset);
            } else if (node == startContainer) {
                result = data.substring(0, startOffset);
                result += '<#start>';
                result += data.substring(startOffset);
            } else if (node == endContainer) {
                result = data.substring(0, endOffset);
                result += '<#end>';
                result += data.substring(endOffset);
            }
            return result;
        }

        function serializeNode(node) {
            if (node.nodeType == Node.TEXT_NODE)
                return serializeCharacterData(node);
            if (node.nodeType == Node.COMMENT_NODE)
                return '<!--' + node.nodeValue + '-->';
            if (node.nodeType == Node.CDATA_SECTION_NODE)
                return '<!--[CDATA[' + node.nodeValue + '-->';

            const elementTags = node.cloneNode(false).outerHTML;
            const endTagIndex = elementTags.lastIndexOf('</');
            const startTag = endTagIndex >= 0 ? elementTags.substring(0, endTagIndex) : elementTags;
            const endTag = endTagIndex >= 0 ? elementTags.substring(endTagIndex) : '';
            return startTag + serializeShadowRootAndChildNodes(node) + endTag;
        }

        function serializeChildNodes(node) {
            let result = '';
            for (let i = 0; i < node.childNodes.length; i++) {
                if (node == startContainer && i == startOffset)
                    result += '<#start>';
                result += serializeNode(node.childNodes[i]);
                if (node == endContainer && i + 1 == endOffset)
                    result += '<#end>';
            }
            return result;
        }

        function serializeShadowRootAndChildNodes(node) {
            const shadowRoot = node.nodeType == Node.ELEMENT_NODE ? internals.shadowRoot(node) : null;
            if (!shadowRoot)
                return serializeChildNodes(node);
            return `<#shadow-start>${serializeChildNodes(shadowRoot)}<#shadow-end>${serializeChildNodes(node)}`;
        }

        return serializeShadowRootAndChildNodes(parent);
    }

    window.serializeAttributedString = function (attributedString) {
        var string = attributedString.string();
        var output = '';
        function log(text) {
            output += text + '\n';
        }

        var currentParagraphStyle;

        attributedString.ranges().forEach(function (range) {
            var location = range.location();
            var length = range.length();

            var attributeNames = attributedString.getAttributeNamesAtIndex(location, length).slice();
            if (attributeNames.indexOf('NSParagraphStyle') >= 0) {
                var value = '' + attributedString.getAttributeValueAtIndex('NSParagraphStyle', location);
                var newParagraphStyle = value.split(', ').reduce(function (result, component) {
                    var match = component.match(/(\w+)\s+([^]+)/);
                    return (result ? result + '\n' : '') + '    ' + match[1] + ': ' + formatParagraphStyle(match[1], match[2]);
                });
                if (newParagraphStyle != currentParagraphStyle) {
                    log('NSParagraphStyle:');
                    log(newParagraphStyle);
                    currentParagraphStyle = newParagraphStyle;
                }
            }

            log('[' + string.substring(location, location + length).replace('\n', '\\n') + ']');

            attributeNames.sort().forEach(function (attributeName) {
                var indentAndName = '    ' + attributeName + ':';
                var value = '' + attributedString.getAttributeValueAtIndex(attributeName, location);

                if (attributeName != 'NSParagraphStyle')
                    log(indentAndName + ' ' + formatNonParagraphAttributeValue(attributeName, value));                
            });
        });

        return output;
    }

    function formatNonParagraphAttributeValue(name, value) {
        value = value.replace(/^"|"$/g, '');
        switch (name) {
        case 'NSFont':
            return value.match(/(.+?)\s+P \[\]/)[1];
        case 'NSColor':
        case 'NSStrokeColor':
        case 'NSBackgroundColor':
            var parsed = parseNSColorDescription(value);
            return serializeColor(parsed.rgb, parsed.alpha) + ' (' + parsed.colorSpace + ')';
        case 'NSUnderline':    
        case 'NSStrikethrough':
            switch (value) {
            case '1':
                return 'true';
            case '0':
                return 'false';
            }
            return value;
        case 'NSKern':
            return value + 'pt';
        }
        return value;
    }

    function parseNSColorDescription(value) {
        var match = value.match(/^\s*(\w+).+\s([0-9\.]+)\s+([0-9\.]+)\s+([0-9\.]+)\s+([0-9\.]+)\s*$/);
        return {
            colorSpace: match[1],
            rgb: match.slice(2, 5).map(function (string) { return Math.round(string * 255); }),
            alpha: match[5],
        };
    }

    function serializeColor(rgb, alpha) {
        if (alpha == 1)
            return '#' + rgb.map(function (component) {
                var digits = component.toString(16);
                return digits.length < 2 ? '0' + digits : digits;
            }).join('');
        return 'rgba(' + rgb.concat(alpha).join(', ') + ')';
    }

    function formatParagraphStyle(name, value) {
        switch (name) {
        case 'Alignment':
            switch (parseInt(value)) {
            case 0:
                return 'left';
            case 1:
                return 'right';
            case 2:
                return 'center';
            case 3:
                return 'justified';
            case 4:
                return 'natural';
            }
            break;
        case 'Tabs':
            return value.replace('\n', '');
        }
        return value;
    }

    window.onload = function () {
        if (shouldAutoDump)
            dumpAttributedString();
    }
})();

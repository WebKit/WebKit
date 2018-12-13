var shouldAutoDump = true;

(function () {
    if (window.testRunner)
        testRunner.dumpAsText();

    function dumpAttributedString(container) {
        shouldAutoDump = false;

        var body = document.body;
        if (!container)
            container = body;

        var range = document.createRange();
        range.selectNodeContents(container);

        var pre = document.createElement('pre');
        var result = serializeAttributedString(textInputController.legacyAttributedString(container, 0, container, container.childNodes.length));
        pre.textContent = 'Input:\n' + container.innerHTML.trim() + '\n\nOutput:\n' + result;

        body.innerHTML = '';
        body.appendChild(pre);
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

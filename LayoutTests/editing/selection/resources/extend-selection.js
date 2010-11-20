function log(message)
{
    document.getElementById("console").appendChild(document.createTextNode(message));
}

function positionsExtendingInDirectionForEnclosingBlock(sel, direction, granularity)
{
    var positions = [];
    var ltrNum;
    var rtlNum;
    if (granularity == "character") {
        ltrNum = 5;
    } else {
        ltrNum = 1;
    }
    if (granularity == "character") {
        rtlNum = 15;
    } else {
        rtlNum = 3;
    }
    var index = 0;
    while (index <= ltrNum) {
        positions.push({ node: sel.extentNode, begin: sel.baseOffset, end: sel.extentOffset });
        sel.modify("extend", direction, granularity);
        ++index;
    }
    var antiDirection = direction;
    if (antiDirection == 'left') {
        antiDirection = "right";
    } else if (antiDirection = 'right') {
        antiDirection = "left";
    }

    index = 0;
    while (index <= rtlNum) {
        positions.push({ node: sel.extentNode, begin: sel.baseOffset, end: sel.extentOffset });
        sel.modify("extend", antiDirection, granularity);
        ++index;
    }
    var index = 0;
    while (index < ltrNum) {
        positions.push({ node: sel.extentNode, begin: sel.baseOffset, end: sel.extentOffset });
        sel.modify("extend", direction, granularity);
        ++index;
    }
    return positions;
}


function positionsExtendingInDirection(sel, direction, granularity)
{
    var positions = [];
    
    while (true) {
        positions.push({ node: sel.extentNode, begin: sel.baseOffset, end: sel.extentOffset });
        sel.modify("extend", direction, granularity);
        if (positions[positions.length - 1].node == sel.extentNode && positions[positions.length - 1].end == sel.extentOffset)
            break;
    };
    return positions;
}

function fold(string)
{
    var result = "";
    for (var i = 0; i < string.length; ++i) {
        var char = string.charCodeAt(i);
        if (char >= 0x05d0)
            char -= 0x058f;
        else if (char == 10) {
            result += "\\n";
            continue;
        }
        result += String.fromCharCode(char);
    }
    return result;
}

function logPositions(positions)
{
    for (var i = 0; i < positions.length; ++i) {
        if (i) {
            if (positions[i].node != positions[i - 1].node)
                log("]");
            log(", ");
        }
        if (!i || positions[i].node != positions[i - 1].node)
            log((positions[i].node instanceof Text ? '"' + fold(positions[i].node.data) + '"' : "<" + positions[i].node.tagName + ">") + "[");
        log("(" + positions[i].begin + "," + positions[i].end + ")");
    }
    log("]");
}

function checkReverseOrder(inputPositions, inputReversePositions)
{
    var positions = inputPositions.slice();
    var reversePositions = inputReversePositions.slice();
    var mismatch = (positions.length != reversePositions.length);
    if (mismatch)
        log("WARNING: positions should be the same, but the length are not the same " +  positions.length + " vs. " + reversePositions.length + "\n");
    while (!mismatch) {
        var pos = positions.pop();
        if (!pos)
            break;
        var reversePos = reversePositions.shift();
        if (pos.node != reversePos.node) {
            mismatch = true;
            log("WARNING: positions should be the reverse, but node are not the reverse\n");
        } 
        if (pos.begin != reversePos.begin) {
            mismatch = true;
            log("WARNING: positions should be the same, but begin are not " + pos.begin + " vs. " + reversePos.begin + "\n");
        }
        if (pos.end != reversePos.end) {
            mismatch = true;
            log("WARNING: positions should be the same, but end are not " + pos.end + " vs. " + reversePos.end + "\n");
        }
    }
}


function checkSameOrder(inputPositions, inputSamePositions)
{
    var positions = inputPositions.slice();
    var samePositions = inputSamePositions.slice();
    var mismatch = positions.length != samePositions.length;
    if (mismatch)
        log("WARNING: positions should be the same, but the length are not the same " +  positions.length + " vs. " + samePositions.length + "\n");
    while (!mismatch) {
        var pos = positions.pop();
        if (!pos)
            break;
        var samePos = samePositions.pop();
        if (pos.node != samePos.node) {
            mismatch = true;
            log("WARNING: positions should be the same, but node are not the same\n");
        } 
        if (pos.begin != samePos.begin) {
            mismatch = true;
            log("WARNING: positions should be the same, but begin are not the same " + pos.begin + " vs. " + samePos.begin + "\n");
        }
        if (pos.end != samePos.end) {
            mismatch = true;
            log("WARNING: positions should be the same, but end are not the same " + pos.end + " vs. " + samePos.end + "\n");
        }
   }
}


function extendingSelection(sel, direction, granularity, option)
{
    var positions;
    if (option == 0) {
        positions = positionsExtendingInDirection(sel, direction, granularity);
    } else {
        positions = positionsExtendingInDirectionForEnclosingBlock(sel, direction, granularity);
    }
    logPositions(positions);
    log("\n");
    return positions;
}

function testExtendingSelection(tests, sel, granularity)
{
    for (var i = 0; i < tests.length; ++i) {
        tests[i].style.direction = "ltr";
        log("Test " + (i + 1) + ", LTR:\n  Extending right: ");
        sel.setPosition(tests[i], 0);
        var ltrRightPos = extendingSelection(sel, "right", granularity, 0);

        log("  Extending left:  ");
        var ltrLeftPos = extendingSelection(sel, "left", granularity, 0);

        log("  Extending forward: ");
        sel.setPosition(tests[i], 0);
        var ltrForwardPos = extendingSelection(sel, "forward", granularity, 0);

        log("  Extending backward:  ");
        var ltrBackwardPos = extendingSelection(sel, "backward", granularity, 0);

        tests[i].style.direction = "rtl";

        log("Test " + (i + 1) + ", RTL:\n  Extending left: ");
        sel.setPosition(tests[i], 0);
        var rtlLeftPos = extendingSelection(sel, "left", granularity, 0);

        log("  Extending right:  ");
        var rtlRightPos = extendingSelection(sel, "right", granularity, 0);

        log("  Extending forward: ");
        sel.setPosition(tests[i], 0);
        var rtlForwardPos = extendingSelection(sel, "forward", granularity, 0);

        log("  Extending backward: ");
        var rtlBackwardPos = extendingSelection(sel, "backward", granularity, 0);

        // validations
        log("\n\n  validating ltrRight and ltrLeft\n");
        if (granularity == "character")
            checkReverseOrder(ltrRightPos, ltrLeftPos);
        // Order might not be reversed for extending by word because the 1-point shift by space.

        log("  validating ltrRight and ltrForward\n");
        checkSameOrder(ltrRightPos, ltrForwardPos);
        log("  validating ltrForward and rtlForward\n");
        checkSameOrder(ltrForwardPos, rtlForwardPos);
        log("  validating ltrLeft and ltrBackward\n");
        checkSameOrder(ltrLeftPos, ltrBackwardPos);
        log("  validating ltrBackward and rtlBackward\n");
        checkSameOrder(ltrBackwardPos, rtlBackwardPos);
        log("  validating ltrRight and rtlLeft\n");
        checkSameOrder(ltrRightPos, rtlLeftPos);
        log("  validating ltrLeft and rtlRight\n");
        checkSameOrder(ltrLeftPos, rtlRightPos);
        log("\n\n");
    }
}

var data = [
['char-and-word', null, '\nabc &#1488;&#1489;&#1490; xyz &#1491;&#1492;&#1493; def\n'],
['char-and-word', null, '\n&#1488;&#1489;&#1490; xyz &#1491;&#1492;&#1493; def &#1494;&#1495;&#1496;\n'],
['char-and-word', null, '\n&#1488;&#1489;&#1490; &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;\n'],
['char-and-word', null, '\nabc efd dabeb\n'],
['char-and-word', null, 'Lorem <span  style="direction: rtl">ipsum dolor sit</span> amet'],
['char-and-word', null, 'Lorem <span  dir="rtl">ipsum dolor sit</span> amet'],
['char-and-word', null, 'Lorem <span  style="direction: ltr">ipsum dolor sit</span> amet'],
['char-and-word', null, 'Lorem <span  dir="ltr">ipsum dolor sit</span> amet'],
['enclosing-block', null, 'Lorem <div  dir="rtl">ipsum dolor sit</div> amett'],
['home-end', null, 'Lorem <span  style="direction: ltr">ipsum dolor sit</span> amet'],
['home-end', null, 'Lorem <span  style="direction: ltr">ipsum dolor<div > just a test</div> sit</span> amet'],
['home-end', null, 'Lorem <span  dir="ltr">ipsum dolor sit</span> amet'],
['home-end', null, 'Lorem <div  dir="ltr">ipsum dolor sit</div> amet'],
['home-end', null, '\n Just\n <span>testing רק</span>\n בודק\n'],
['home-end', null, '\n Just\n <span>testing what</span>\n ever\n'],
['home-end', null, 'car means &#1488;&#1489;&#1490;.'],
['home-end', null, '&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;.&#x202c;'],
['home-end', null, 'he said "&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;&#x202c;."'],
['home-end', null, '&#1494;&#1495;&#1496; &#1497;&#1498;&#1499; &#1500;&#1501;&#1502; \'&#x202a;he said "&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;&#x202c;"&#x202c;\'?'],
['home-end', null, '&#1488;&#1489;&#1490; abc &#1491;&#1492;&#1493;<br />edf &#1494;&#1495;&#1496; abrebg'],
['home-end', 'line-break:before-white-space; width:100px', 'abcdefg abcdefg abcdefg a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg '],
['home-end', 'line-break:after-white-space; width:100px', 'abcdefg abcdefg abcdefg a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg ']
];

function createTestNodes(name) {
    var nodes = [];
    for (var i = 0; i < data.length; i++) {
        if (name && data[i][0] != name)
            continue;
        var style = data[i][1];
        var text = data[i][2];
        var div = document.createElement('div');
        div.contentEditable = true;
        if (style)
            div.setAttribute('style', style);
        div.innerHTML = text;
        document.body.insertBefore(div, document.getElementById('console'));
        nodes.push(div);
    }

    window.onload = function() {
        for (var i = 0; i < nodes.length; i++)
            nodes[i].parentNode.removeChild(nodes[i]);
    }

    return nodes;
}


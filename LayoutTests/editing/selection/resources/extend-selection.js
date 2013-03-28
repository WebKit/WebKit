function log(message)
{
    document.getElementById("console").appendChild(document.createTextNode(message));
}

function getSerializedSelection()
{
    return {
        node: getSelection().extentNode,
        begin: getSelection().baseOffset,
        end: getSelection().extentOffset
    };
}

function extendSelectionWithinBlock(direction, granularity)
{
    var positions = [];
    var leftIterations = (granularity === "character") ? 5 : 1;
    var rightIterations = (granularity === "character") ? 15 : 3;

    for (var index = 0; index <= leftIterations; ++index) {
        positions.push(getSerializedSelection());
        getSelection().modify("extend", direction, granularity);
    }

    for (var index = 0; index <= rightIterations; ++index) {
        positions.push(getSerializedSelection());
        getSelection().modify("extend", (direction === "left") ? "right" : "left", granularity);
    }

    for (var index = 0; index < leftIterations; ++index) {
        positions.push(getSerializedSelection());
        getSelection().modify("extend", direction, granularity);
    }

    return positions;
}

function extendSelectionToEnd(direction, granularity)
{
    var positions = [];
    do {
        var position = getSerializedSelection();
        positions.push(position);
        getSelection().modify("extend", direction, granularity);
    } while (position.node !== getSerializedSelection().node || position.end !== getSerializedSelection().end);
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
            result +="\\n";
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

function logMismatchingPositions(positions, comparisonPositions)
{
    if (positions.length !== comparisonPositions.length) {
        log("WARNING: positions should be the same, but the length are not the same " +  positions.length + " vs. " + comparisonPositions.length + "\n");
        return;
    }

    for (var i = 0; i < positions.length; ++i) {
        var pos = positions[i];
        var comparison = comparisonPositions[i];

        if (pos.node !== comparison.node)
            log("WARNING: positions should be the same, but node are not the same\n");
        if (pos.begin !== comparison.begin)
            log("WARNING: positions should be the same, but begin are not the same " + pos.begin + " vs. " + comparison.begin + "\n");
        if (pos.end !== comparison.end)
            log("WARNING: positions should be the same, but end are not the same " + pos.end + " vs. " + comparison.end + "\n");
    }
}

function extendAndLogSelection(functionToExtendSelection, direction, granularity)
{
    var positions = functionToExtendSelection(direction, granularity);
    logPositions(positions);
    log("\n");
    return positions;
}

function extendAndLogSelectionWithinBlock(direction, granularity, platform)
{
    return extendAndLogSelection({'mac': extendSelectionWithinBlock}[platform], direction, granularity);
}

function extendAndLogSelectionToEnd(direction, granularity)
{
    return extendAndLogSelection(extendSelectionToEnd, direction, granularity);
}

function runSelectionTestsWithGranularity(testNodes, granularity)
{
    for (var i = 0; i < testNodes.length; ++i) {
        var testNode = testNodes[i];

        log("Test " + (i + 1) + ", LTR:\n");
        testNode.style.direction = "ltr";

        log("  Extending right:    ");
        getSelection().setPosition(testNode);
        var ltrRightPos = extendAndLogSelectionToEnd("right", granularity);

        log("  Extending left:     ");
        var ltrLeftPos = extendAndLogSelectionToEnd("left", granularity);

        log("  Extending forward:  ");
        getSelection().setPosition(testNode);
        var ltrForwardPos = extendAndLogSelectionToEnd("forward", granularity);

        log("  Extending backward: ");
        var ltrBackwardPos = extendAndLogSelectionToEnd("backward", granularity);

        log("Test " + (i + 1) + ", RTL:\n");
        testNode.style.direction = "rtl";

        log("  Extending left:     ");
        getSelection().setPosition(testNode);
        var rtlLeftPos = extendAndLogSelectionToEnd("left", granularity);

        log("  Extending right:    ");
        var rtlRightPos = extendAndLogSelectionToEnd("right", granularity);

        log("  Extending forward:  ");
        getSelection().setPosition(testNode);
        var rtlForwardPos = extendAndLogSelectionToEnd("forward", granularity);

        log("  Extending backward: ");
        var rtlBackwardPos = extendAndLogSelectionToEnd("backward", granularity);

        // validations
        log("\n\n  validating ltrRight and ltrLeft\n");
        if (granularity === "character")
            logMismatchingPositions(ltrRightPos, ltrLeftPos.slice().reverse());
        // Order might not be reversed for extending by word because the 1-point shift by space.

        log("  validating ltrRight and ltrForward\n");
        logMismatchingPositions(ltrRightPos, ltrForwardPos);
        log("  validating ltrForward and rtlForward\n");
        logMismatchingPositions(ltrForwardPos, rtlForwardPos);
        log("  validating ltrLeft and ltrBackward\n");
        logMismatchingPositions(ltrLeftPos, ltrBackwardPos);
        log("  validating ltrBackward and rtlBackward\n");
        logMismatchingPositions(ltrBackwardPos, rtlBackwardPos);
        log("  validating ltrRight and rtlLeft\n");
        logMismatchingPositions(ltrRightPos, rtlLeftPos);
        log("  validating ltrLeft and rtlRight\n");
        logMismatchingPositions(ltrLeftPos, rtlRightPos);
        log("\n\n");
    }
}

function getTestNodeContainer()
{
    var tests = document.getElementById("tests");
    if (!tests) {
        tests = document.createElement("div");
        tests.id = "tests";
        document.body.insertBefore(tests, document.getElementById("console"));

        function createButton(name, onclick)
        {
            var button = document.createElement("button");
            button.innerText = name;
            button.onclick = onclick;
            button.onmousedown = function(event) {
                // Prevent the mouse event from cancelling the current selection.
                event.preventDefault();
            };
            return button;
        }

        function createButtonToSwitchDirection(direction)
        {
            return createButton(direction, function() {
                for (var i = 0; i < tests.childNodes.length; ++i) {
                    var node = tests.childNodes[i];
                    if (node.className === "testNode")
                        node.style.direction = direction;
                }
            });
        }

        tests.appendChild(document.createTextNode("Use these buttons to help run the tests manually: "));
        tests.appendChild(createButtonToSwitchDirection("ltr"));
        tests.appendChild(createButtonToSwitchDirection("rtl"));
    }
    return tests;
}

function createNode(content, style)
{
    var node = document.createElement("div");
    node.innerHTML = content;
    node.className = "testNode";
    node.contentEditable = true;
    if (style)
        node.setAttribute("style", style);
    getTestNodeContainer().appendChild(node);
    return node;
}

function createCharAndWordNodes()
{
    return [
        createNode('\nabc &#1488;&#1489;&#1490; xyz &#1491;&#1492;&#1493; def\n'),
        createNode('\n&#1488;&#1489;&#1490; xyz &#1491;&#1492;&#1493; def &#1494;&#1495;&#1496;\n'),
        createNode('\n&#1488;&#1489;&#1490; &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;\n'),
        createNode('\nabc efd dabeb\n'),
        createNode('Lorem <span style="direction: rtl">ipsum dolor sit</span> amet'),
        createNode('Lorem <span dir="rtl">ipsum dolor sit</span> amet'),
        createNode('Lorem <span style="direction: ltr">ipsum dolor sit</span> amet'),
        createNode('Lorem <span dir="ltr">ipsum dolor sit</span> amet')
    ];
}

function createEnclosingBlockNodes()
{
    return [
        createNode('Lorem <div  dir="rtl">ipsum dolor sit</div> amett')
    ];
}

function createHomeEndNodes()
{
    return [
        createNode('Lorem <span style="direction: ltr">ipsum dolor sit</span> amet'),
        createNode('Lorem <span style="direction: ltr">ipsum dolor<div > just a test</div> sit</span> amet'),
        createNode('Lorem <span dir="ltr">ipsum dolor sit</span> amet'),
        createNode('Lorem <div  dir="ltr">ipsum dolor sit</div> amet'),
        createNode('\n Just\n <span>testing רק</span>\n בודק\n'),
        createNode('\n Just\n <span>testing what</span>\n ever\n'),
        createNode('car means &#1488;&#1489;&#1490;.'),
        createNode('&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;.&#x202c;'),
        createNode('he said "&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;&#x202c;."'),
        createNode('&#1494;&#1495;&#1496; &#1497;&#1498;&#1499; &#1500;&#1501;&#1502; \'&#x202a;he said ' +
                   '"&#x202B;car &#1491;&#1492;&#1493; &#1488;&#1489;&#1490;&#x202c;"&#x202c;\'?'),
        createNode('&#1488;&#1489;&#1490; abc &#1491;&#1492;&#1493;<br />edf &#1494;&#1495;&#1496; abrebg'),
        createNode('abcdefg abcdefg abcdefg a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg ',
                   'line-break:before-white-space; width:5em'),
        createNode('abcdefg abcdefg abcdefg a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg ',
                   'line-break:after-white-space; width:5em')
    ];
}

function createAllNodes()
{
    return createCharAndWordNodes().concat(
           createEnclosingBlockNodes()).concat(
           createHomeEndNodes());
}

if (window.testRunner) {
    var originalOnload = window.onload;
    window.onload = function() {
        if (originalOnload)
            originalOnload();
        testRunner.dumpAsText();
        document.body.removeChild(getTestNodeContainer());
    };
}

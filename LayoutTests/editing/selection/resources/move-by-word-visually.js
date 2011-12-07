var messages = [];

function log(message)
{
    messages.push(message);
}

function flushLog()
{
    document.getElementById("console").appendChild(document.createTextNode(messages.join("")));
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
        log(positions[i].offset);
    }
    log("]");
}

function nodeOfWordBreak(nodeAndOffset)
{
    var node = document.getElementById(nodeAndOffset[0]).firstChild;
    if (nodeAndOffset.length == 3) {
        var childIndex = nodeAndOffset[2];
        for (var i = 0; i < childIndex - 1; ++i) {
            node = node.nextSibling;
        }
    }
    return node;
}

var wordBreaks;

function logWordBreak(index, first)
{
    var withNodeData = false;
    var wordBreak = wordBreaks[index];
    if (wordBreak.search(',') == -1)
        log(wordBreak);
    else {
        var nodeAndOffset = wordBreak.split(',');
        var node = nodeOfWordBreak(nodeAndOffset);

        var differentNode = false;
        if (first == false) {
            differentNode = nodeOfWordBreak(nodeAndOffset) != nodeOfWordBreak(wordBreaks[index - 1].split(','));
        
        }

        if (differentNode == true)
            log("]");

        if (first == true || differentNode == true) {
            withNodeData = (node instanceof Text);
            log((node instanceof Text ? '"' + fold(node.data) + '"' : "<" + node.tagName + ">") + "[");
        }
        log(nodeAndOffset[1]);
    }
    return withNodeData;
}

function positionEqualToWordBreak(position, wordBreak)
{
    if (wordBreak.search(',') == -1)
        return position.offset == wordBreak;
    else {
        var nodeAndOffset = wordBreak.split(',');
        return position.node == nodeOfWordBreak(nodeAndOffset) && position.offset == nodeAndOffset[1];
    }
}

function validateData(positions)
{
    var equal = true;
    if (positions.length != wordBreaks.length)
        equal = false;
    else {
        for (var i = 0; i < wordBreaks.length - 1; ++i) {
            if (!positionEqualToWordBreak(positions[i], wordBreaks[i])) {
                equal = false;
                break;
            }
        }
    }
    if (equal == false) {
        log("    FAIL expected: [");
        for (var i = 0; i < wordBreaks.length; ++i) {
            logWordBreak(i, i == 0);
            if (i != wordBreaks.length - 1)
                log(", ");
        }
        log("]");
    }
}

function collectWordBreaks(test, searchDirection)
{
    var title;
    if (searchDirection == "right") 
        title = test.title.split("|")[0];
    else
        title = test.title.split("|")[1];

    var pattern = /\[(.+?)\]/g;
    var result;
    wordBreaks = [];
    while ((result = pattern.exec(title)) != null) {
        wordBreaks.push(result[1]);
    }
    if (wordBreaks.length == 0) {
        wordBreaks = title.split(" ");
    }
}

function setPosition(sel, node, wordBreak)
{
    if (wordBreak.search(',') == -1)
        sel.setPosition(node, wordBreak);
    else {
        var nodeAndOffset = wordBreak.split(',');
        sel.setPosition(nodeOfWordBreak(nodeAndOffset), nodeAndOffset[1]);
    }
}

function moveByWord(sel, test, searchDirection, dir)
{
    log("Move " + searchDirection + " by one word\n");
    var prevOffset = sel.anchorOffset;
    var prevNode = sel.anchorNode;
    var positions = [];
    positions.push({ node: sel.anchorNode, offset: sel.anchorOffset });

    while (1) {
        sel.modify("move", searchDirection, "word");
        if (prevNode == sel.anchorNode && prevOffset == sel.anchorOffset)
            break;
        positions.push({ node: sel.anchorNode, offset: sel.anchorOffset });
        prevNode = sel.anchorNode;
        prevOffset = sel.anchorOffset;
    };
    logPositions(positions);
    collectWordBreaks(test, searchDirection);
    validateData(positions);
    log("\n");
}

function moveByWordOnEveryChar(sel, test, searchDirection, dir)
{
    collectWordBreaks(test, searchDirection);
    var wordBreakIndex = 1;
    var prevOffset = sel.anchorOffset;
    var prevNode = sel.anchorNode;

    while (1) {
        var positions = [];
        positions.push({ node: sel.anchorNode, offset: sel.anchorOffset });
        sel.modify("move", searchDirection, "word");

        var position = { node: sel.anchorNode, offset: sel.anchorOffset };

        if (wordBreakIndex >= wordBreaks.length) {
            if (sel.anchorNode != prevNode || sel.anchorOffset != prevOffset) {
                positions.push(position);
                logPositions(positions);
                log("   FAIL expected to stay in the same position\n");
            }
        } else if (!positionEqualToWordBreak(position, wordBreaks[wordBreakIndex])) {
            positions.push(position);
            logPositions(positions);
            log("   FAIL expected ");
            var withNodeData = logWordBreak(wordBreakIndex, true);
            if (withNodeData)
                log("]");
            log("\n");
        }

        // Restore position and move by 1 character.
        sel.setPosition(prevNode, prevOffset);
        sel.modify("move", searchDirection, "character");
        if (prevNode == sel.anchorNode && prevOffset == sel.anchorOffset)
            break;

        position = { node: sel.anchorNode, offset: sel.anchorOffset };
        if (wordBreakIndex < wordBreaks.length 
            && positionEqualToWordBreak(position, wordBreaks[wordBreakIndex]))
            ++wordBreakIndex;

        prevNode = sel.anchorNode;
        prevOffset = sel.anchorOffset;
    };
}

function moveByWordForEveryPosition(sel, test, dir)
{
    // Check ctrl-right-arrow works for every position.
    sel.setPosition(test, 0);
    var direction = "right";
    if (dir == "rtl")
        direction = "left";    
    moveByWord(sel, test, direction, dir);    
    sel.setPosition(test, 0);
    moveByWordOnEveryChar(sel, test, direction, dir);

    sel.modify("move", "forward", "lineBoundary");
    var position = { node: sel.anchorNode, offset: sel.anchorOffset };

    // Check ctrl-left-arrow works for every position.
    if (dir == "ltr")
        direction = "left";
    else
        direction = "right";    
    moveByWord(sel, test, direction, dir);    

    sel.setPosition(position.node, position.offset);
    moveByWordOnEveryChar(sel, test, direction, dir);
}

function setWidth(test)
{
    if (test.className.search("fix_width") != -1) {
        var span = document.getElementById("span_size");
        var length = span.offsetWidth;
        test.style.width = length + "px";
    }
}

function runMoveLeftRight(tests, unit)
{
    var sel = getSelection();
    for (var i = 0; i < tests.length; ++i) {
        var positionsMovingRight;
        var positionsMovingLeft;

        setWidth(tests[i]);

        if (tests[i].getAttribute("dir") == 'ltr')
        {
            log("Test " + (i + 1) + ", LTR:\n");
            moveByWordForEveryPosition(sel, tests[i], "ltr");
        } else {
            log("Test " + (i + 1) + ", RTL:\n");
            moveByWordForEveryPosition(sel, tests[i], "rtl");
        }
    }
    if (document.getElementById("testMoveByWord"))
        document.getElementById("testMoveByWord").style.display = "none";
}

function runTest() {
    log("\n======== Move By Word ====\n");
    var tests = document.getElementsByClassName("test_move_by_word");
    runMoveLeftRight(tests, "word");
}

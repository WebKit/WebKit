if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

var consoleOutput = null;
var stopTest = false;
var focusCount = 0;
var blurCount = 0;
var focusedElem = null;
var failedTestCount = 0;

var tagNamesAlwaysFocused = ["A",
                             "AREA",
                             "BUTTON",
                             "IFRAME",
                             "INPUT",
                             "ISINDEX",
                             "SELECT",
                             "TEXTAREA",
                             "AUDIO",
                             "VIDEO"];

var tagNamesTransferFocused = ["LABEL"]; // labels always transfer focus to the labeled element

var noDisplayTagNamesWithFocus = ["AREA"];  // AREA elements can get focus, but are not displayed.

function printToConsole(str)
{
    if (!consoleOutput) {
        consoleOutput = window.frames[2].document.getElementById("theConsole");
    }
    consoleOutput.appendChild(window.frames[2].document.createTextNode(str));
    consoleOutput.appendChild(window.frames[2].document.createElement("br"));
}

function doFocus(elem)
{
    focusCount++;
    focusedElem = elem;
}

function doBlur(elem)
{
    blurCount++;
}

function test()
{
    for (var i = 0; i < 2; ++i)
        focusEachChild(window.frames[i].document.body);

    // focus an untested element so that blur can be dispatched on the last iframe tested
    var homeBase = window.frames[1].document.getElementsByClassName('homebase');
    homeBase[0].focus();

    var resultSummary = focusCount+" focus / "+blurCount+" blur events dispatched, and should be 333 / 333 ";
    resultSummary += (focusCount==blurCount) ? "<span style='color:green'>PASSED</span><br>" : "<span style='color:red'>FAILED</span><br>";
    resultSummary += "Total of "+failedTestCount+" focus test(s) failed.";
    if (failedTestCount)
        resultSummary += "<br>Details:<br>"+consoleOutput.innerHTML;
    else
        resultSummary += " <span style='color:green'>PASSED</span>";
    
    document.write(resultSummary);
    document.close();

    if (window.testRunner)
        testRunner.notifyDone();
}

function focusEachChild(elem) {
    var childNodes = elem.childNodes;
    for (var i = 0; i < childNodes.length; i++) {
        if (childNodes[i].nodeType == Node.ELEMENT_NODE && childNodes[i].id) {
            childNodes[i].addEventListener('focus',function () { doFocus(this) }, false);
            childNodes[i].addEventListener('blur',function () { doBlur(this) }, false);
            testProgrammaticFocus(childNodes[i]);
        }
        if (childNodes[i].childNodes.length)
            focusEachChild(childNodes[i]);
        if (childNodes[i].tagName =="IFRAME") {
            if (childNodes[i].id == "iframe1b") {
                window.frames[0].document.body.focus();
            }
            focusEachChild(childNodes[i].contentDocument.body);
        }
    }       
}

Array.prototype.find = function(element) {
    for (var i = 0; i < this.length; i++) {
        if(this[i] == element) {
            return this[i];
        }
    }
    return null;
}

function testProgrammaticFocus(elem)
{
    var elemThatShouldFocus = null;
    var OKtoFocusOtherElement = false;
    focusedElem = null;

    if (elem.tabIndex == elem.getAttribute("tabindex")) // this means tabindex was explicitly set
        elemThatShouldFocus = elem;
    else if (tagNamesAlwaysFocused.find(elem.tagName)) // special case form elements and other controls that are always focusable
        elemThatShouldFocus = elem;

    // Hidden elements should not be focusable. https://bugs.webkit.org/show_bug.cgi?id=27099
    if (document.defaultView.getComputedStyle(elem).display == "none" && !noDisplayTagNamesWithFocus.find(elem.tagName))
        elemThatShouldFocus = null;

    // AREA elements with tabindex = -1 should not be focusable.
    if (elem.tabIndex == -1 && elem.tagName == "AREA")
        elemThatShouldFocus = null;

    if (tagNamesTransferFocused.find(elem.tagName)) {
        elemThatShouldFocus = null;
        OKtoFocusOtherElement = true;
    }

    elem.focus();
    if (elemThatShouldFocus == focusedElem || (!elemThatShouldFocus && OKtoFocusOtherElement))
        printToConsole("<"+elem.tagName+"> "+elem.id+" PASSED focus test");
    else {
        failedTestCount++;
        printToConsole(elem.id+" FAILED - was " + (focusedElem ? "" : " not ") + " focused but focus " + (elemThatShouldFocus ? " was " : " wasn\'t") + " expected");
        if (elemThatShouldFocus && focusedElem)
            printToConsole("elemThatShouldFocus is <"+elemThatShouldFocus.tagName+"> "+elemThatShouldFocus.id+", focusedElem is <"+focusedElem.tagName+"> "+focusedElem.id);
        if (!elemThatShouldFocus)
            printToConsole("elemThatShouldFocus is null, focusedElem is <"+focusedElem.tagName+"> "+focusedElem.id);
        if (!focusedElem)
            printToConsole("elemThatShouldFocus is <"+elemThatShouldFocus.tagName+"> "+elemThatShouldFocus.id+", focusedElem is null");
    }
}

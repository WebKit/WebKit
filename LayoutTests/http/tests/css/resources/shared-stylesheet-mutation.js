var testCount = 17;
var testDocuments = [];
var testSheets = [];
var expectedResults = [];

function buildTestFrame(content)
{
    var iframe = document.createElement("iframe");
    iframe.setAttribute("width", "100");
    iframe.setAttribute("height", "50");
    document.body.appendChild(iframe);
    var iframeDocument = iframe.contentDocument;
    iframeDocument.body.innerHTML = content;
    return iframeDocument;
}

function buildTestFrames(content)
{
    for (var i = 0; i < testCount; ++i)
        testDocuments.push(buildTestFrame(content));
}

function printRules(ruleList)
{
    for (var i = 0; i < ruleList.length; ++i)
        debug(ruleList[i].cssText);
}

function printTestResults(index)
{
    testElement = testDocuments[index].getElementById('testdiv');
    
    debug("Test " + index);
    debug("--------------------------------------");
    shouldBe("getComputedStyle(testElement, null).getPropertyValue('background-color')", expectedResults[index]);
    debug("");
    printRules(testSheets[index].cssRules);
    debug("");
}

function finishedTests()
{
    for (var i = 0; i < testSheets.length; ++i)
        printTestResults(i);
    
    finishJSTest();
}

function ensureCSSOM(ruleList)
{
    // Touch all CSSOM objects to force wrapper creation.
    for (var i = 0; i < ruleList.length; ++i) {
        ruleList[i].style;
        ruleList[i].media;
        ruleList[i].selectorText;
        ruleList[i].name;
        ruleList[i].keyText;
        ruleList[i].cssText;
        if (ruleList[i].cssRules)
            ensureCSSOM(ruleList[i].cssRules);
    }
}

function mutationTest(index, testString, expectColor)
{
    var sheet = testSheets[index];
    eval(testString);
    expectedResults[index] = expectColor == "red" ? "'rgb(255, 0, 0)'" : "'rgb(0, 128, 0)'";
}

function executeTests(createCSSOMObjectBeforeTest)
{
    if (createCSSOMObjectBeforeTest) {
        for (var i = 0; i < testSheets.length; ++i)
            ensureCSSOM(testSheets[i].cssRules);
    }

    mutationTest(0, '', 'red');

    mutationTest(1, 'sheet.insertRule("#testdiv { background-color: green; }", 3)', 'green');
    mutationTest(2, 'sheet.deleteRule(1)');
    mutationTest(3, 'sheet.cssRules[1].insertRule("#testdiv { background-color: green; }", 1)', 'green');
    mutationTest(4, 'sheet.cssRules[1].deleteRule(0)', 'green');
    mutationTest(5, 'sheet.cssRules[1].cssRules[0].style.setProperty("background-color", "green", "")', 'green');
    mutationTest(6, 'sheet.cssRules[1].cssRules[0].style.removeProperty("background-color")', 'green');
    mutationTest(7, 'sheet.cssRules[1].cssRules[0].style.cssText = "background-color: green"', 'green');
    mutationTest(8, 'sheet.cssRules[1].cssRules[0].selectorText = "#dontmatch"', 'green');
    mutationTest(9, 'sheet.cssRules[1].media.mediaText = "print"', 'green');
    
    var testString = '\
        sheet.cssRules[1].media.appendMedium("print");\
        sheet.cssRules[1].media.deleteMedium("all");\
    ';
    mutationTest(10, testString, 'green');
    
    var testString = '\
        sheet.deleteRule(2);\
        sheet.insertRule("#testdiv { background-color: green; }", 0);\
        sheet.deleteRule(1);\
        sheet.deleteRule(3);\
        sheet.deleteRule(1);\
        sheet.deleteRule(1);\
    ';
    mutationTest(11, testString, 'green');

    var importRule = '@import "data:text/css;charset=utf-8,%23testdiv%7Bbackground-color%3Agreen%20!important%7D";';
    mutationTest(12, "sheet.insertRule('"+importRule+"', 0)", 'green');

    mutationTest(13, 'sheet.cssRules[2].selectorText = "foo"', 'red');
    mutationTest(14, 'sheet.cssRules[3].appendRule("40% { left: 40px; }")', 'red');
    mutationTest(15, 'sheet.cssRules[3].deleteRule("100%")', 'red');
    mutationTest(16, 'sheet.cssRules[4].style.setProperty("font-family", "Bar", "")', 'red');

    setTimeout(finishedTests, 80);
}

function runTestsAfterLoadComplete(createCSSOMObjectBeforeTest)
{
    var complete = true;
    for (var i = 0; i < testDocuments.length; ++i) {
        var sheet = testDocuments[i].styleSheets.length == 1 ? testDocuments[i].styleSheets[0] : null;
        if (sheet)
            testSheets[i] = sheet;
        else
            complete = false;
    }
    if (!complete) {
        setTimeout(runTestsAfterLoadComplete, 10);
        return;
    }
    executeTests(createCSSOMObjectBeforeTest);
}

function runTests(createCSSOMObjectBeforeTest)
{
    buildTestFrames("<link rel=stylesheet href=resources/shared.css><div id=testdiv>Test</div>");

    runTestsAfterLoadComplete(createCSSOMObjectBeforeTest);
}

description("Test to make sure we can toggle text decorations correctly.  The first three tests give different result on mac only.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, expectedContents, dependsOnPlatform)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    if (!expectedContents)
        debug('<span>' + escapeHTML("one " + toggleCommand + " command converted " + initialContents + " to " + testContainer.innerHTML) + '</span>');
    else if (testContainer.innerHTML === expectedContents)
        testPassed("one " + toggleCommand + " command converted " + initialContents + " to " + expectedContents);
    else
        testFailed("one " + toggleCommand + " command converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
}

debug('PLATFORM-DEPENDNET TESTS');
testSingleToggle("bold", "<u><b>hello</b> world</u>");
testSingleToggle("bold", "<b>hello </b>world");
testSingleToggle("bold", "<u><b>hello </b></u>world");
testSingleToggle("italic", "<i>hello</i> <img>");
testSingleToggle("italic", "<s><b>hello<i> world</i></b></s>");
debug('PLATFORM-INDEPENDNET TESTS');
testSingleToggle("bold", "<u><span id='test'><b>hello</b></span><b>world</b></u>", '<u><span id="test">hello</span>world</u>');
testSingleToggle("bold", "<span id='test' style='font-weight:normal;'><b>hello</b></span>", '<span id="test">hello</span>');
testSingleToggle("bold", "<div><b>hello</b><br><br><b>world</b></div>", "<div>hello<br><br>world</div>");
testSingleToggle("italic", "<i>hello </i><img>", "hello <img>");
testSingleToggle("italic", "<i><b>hello</b>world</i>", "<b>hello</b>world");
testSingleToggle("italic", "<span style='font-style: normal;'> <i> hello </i> </span>", "  hello  ");
testSingleToggle("italic", "<p><i>hello</i><span style='font-style:italic;'>world</span></p>", "<p>helloworld</p>");

document.body.removeChild(testContainer);

var successfullyParsed = true;

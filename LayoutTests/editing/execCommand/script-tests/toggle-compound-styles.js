description("Test to make sure we can toggle text decorations correctly.  The first three tests give different result on mac only.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    var action = 'one ' + toggleCommand + ' command converted "' + initialContents + '" to "' + expectedContents;
    if (testContainer.innerHTML === expectedContents)
        testPassed(action);
    else
        testFailed(action + '", expected "' + expectedContents + '"');
}

platforms = ['mac', 'win', 'unix'];

for (var i = 0; i < platforms.length; i++) {
    platform = platforms[i];
    debug('Platform: ' + platform);

    if (window.internals)
        internals.settings.setEditingBehavior(platform);

    if (platform == 'win' || platform == 'unix')
        platform = 'nonmac';

    testSingleToggle("bold", "<u><b>hello</b> world</u>", {mac: '<u>hello world</u>', nonmac: '<u><b>hello world</b></u>'}[platform]);
    testSingleToggle("bold", "<b>hello </b>world", {mac: 'hello world', nonmac: '<b>hello world</b>'}[platform]);
    testSingleToggle("bold", "<u><b>hello </b></u>world", {mac: '<u>hello </u>world', nonmac: '<b><u>hello </u>world</b>'}[platform]);
    testSingleToggle("italic", "<i>hello</i> <img>", {mac: 'hello <img>', nonmac: '<i>hello <img></i>'}[platform]);

    // Following tests are cross-platform
    testSingleToggle("bold", "<u><span id='test'><b>hello</b></span><b>world</b></u>", '<u><span id="test">hello</span>world</u>');
    testSingleToggle("bold", "<span id='test' style='font-weight:normal;'><b>hello</b></span>", '<span id="test">hello</span>');
    testSingleToggle("bold", "<div><b>hello</b><br><br><b>world</b></div>", "<div>hello<br><br>world</div>");
    testSingleToggle("italic", "<i>hello </i><img>", "hello <img>");
    testSingleToggle("italic", "<i><b>hello</b>world</i>", "<b>hello</b>world");
    testSingleToggle("italic", "<span style='font-style: normal;'> <i> hello </i> </span>", "  hello  ");
    testSingleToggle("italic", "<p><i>hello</i><span style='font-style:italic;'>world</span></p>", "<p>helloworld</p>");
    testSingleToggle("italic", "<s><b>hello<i> world</i></b></s>", "<s><b><i>hello world</i></b></s>");

    debug('');
}

document.body.removeChild(testContainer);

var successfullyParsed = true;

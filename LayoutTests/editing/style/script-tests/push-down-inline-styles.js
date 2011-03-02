description('Test to make sure we push down inline styles properly.');

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    if (document.getElementById('test'))
        window.getSelection().selectAllChildren(document.getElementById('test'));
    else
        window.getSelection().selectAllChildren(testContainer);
    document.execCommand('styleWithCSS', false, 'false');
    document.execCommand(toggleCommand, false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed(toggleCommand + " converted " + initialContents + " to " + expectedContents);
    } else {
        testFailed(toggleCommand + " converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
    }
}


testSingleToggle("bold", '<span style="font-weight: 900;"> <div>text</div> </span>', ' <div>text</div> ');
testSingleToggle("bold", '<span style="font-weight: 900;"><div>text</div></span>', '<div>text</div>');
testSingleToggle("bold", '<span style="font-weight: 900;"><div id="test">hello</div><div>world</div></span>', '<div id="test">hello</div><div style="font-weight: 900; ">world</div>');
testSingleToggle("bold", '<div style="font-weight: bold;">hello<div id="test">world</div></div>', '<div><b>hello</b><div id="test">world</div></div>');
testSingleToggle("bold", '<span style="font-weight: bold;">hello<span id="test">world</div></div>', '<b>hello</b><span id="test">world</span>');
testSingleToggle("bold", '<span style="font-style: italic; font-weight: bold;">hello<span id="test">world</div></div>', '<span style="font-style: italic; "><b>hello</b><span id="test">world</span></span>');
testSingleToggle("bold", '<span style="font-weight: bold;"><div id="test">hello</div><div style="font-weight: normal;"><div>world</div>webkit</div>', '<div id="test">hello</div><div>world</div>webkit');
testSingleToggle("italic", '<span style="font-style: italic;"><div>hello</div></span>', '<div>hello</div>');
testSingleToggle("italic", '<span style="font-style: italic;"><div id="test">hello</div><span style="font-style: oblique;">world</span>', '<div id="test">hello</div><span style="font-style: oblique; ">world</span>');
testSingleToggle("italic", '<span style="font-style: italic; font-weight: bold;"><div>hello</div></span>', '<span style="font-weight: bold; "><div>hello</div></span>');
testSingleToggle("italic", '<span style="font-style: italic; text-decoration: line-through;"><div>hello</div></span>', '<span style="text-decoration: line-through; "><div>hello</div></span>');
testSingleToggle("italic", '<span style="font-style: italic;">hello<div id="test">world</div><blockquote>webkit</blockquote></span>', '<i>hello</i><div id="test">world</div><blockquote style="font-style: italic; ">webkit</blockquote>');
testSingleToggle("italic", '<span style="font-style: italic;">hello <span id="test">world</span> webkit</span>', '<i>hello </i><span id="test">world</span><i> webkit</i>');
testSingleToggle("underline", '<span style="text-decoration: underline;"><div id="test">hello</div>world</span>', '<div id="test">hello</div><u>world</u>');
testSingleToggle("underline", '<span style="text-decoration: underline;"><div id="test">hello</div><blockquote>world<br>webkit</blockquote></span>', '<div id="test">hello</div><blockquote style="text-decoration: underline; ">world<br>webkit</blockquote>');
testSingleToggle("underline", '<span style="text-decoration: underline;">hello<div id="test">world</div>webkit</u>', '<u>hello</u><div id="test">world</div><u>webkit</u>');
testSingleToggle("underline",
    '<div style="text-decoration: underline;"><div>hello</span></div><div id="test">webkit</div><span style="font-style: italic;">rocks</span>',
    '<div><div style="text-decoration: underline; ">hello</span></div><div id="test">webkit</div><u><span style="font-style: italic;">rocks</span></u></div>');
testSingleToggle("underline", '<span style="text-decoration: underline;"><div style="text-decoration: line-through;">hello</div><div id="test">world</div></span>', '<div style="text-decoration: underline line-through; ">hello</div><div id="test">world</div>');
testSingleToggle("strikeThrough", '<span style="text-decoration: line-through;"><div id="test">hello</div><div style="text-decoration: underline;">world</div></span>', '<div id="test">hello</div><div style="text-decoration: line-through underline; ">world</div>');

document.body.removeChild(testContainer);

var successfullyParsed = true;

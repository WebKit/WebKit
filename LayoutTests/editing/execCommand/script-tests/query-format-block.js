description("Tests queryCommandValue('formatBlock')")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function queryFormatBlock(selector, content, expected)
{
    testContainer.innerHTML = content;
    selector(testContainer);
    var actual = document.queryCommandValue('formatBlock');
    var action = "queryCommand('format') returned \"" + actual + '"';
    if (actual == expected)
        testPassed(action);
    else
        testFailed(action + ' but expected "' + expected + '"');
}

function selectFirstPosition(container) {
    while (container.firstChild)
        container = container.firstChild;
    window.getSelection().setPosition(container, 0);
}

function selectMiddleOfHelloWorld(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
}

debug('Basic cases');
queryFormatBlock(function () {}, 'hello', '');
queryFormatBlock(selectFirstPosition, 'hello', '');
queryFormatBlock(selectFirstPosition, '<address>hello</address>', 'address');
queryFormatBlock(selectFirstPosition, '<h1>hello</h1>', 'h1');
queryFormatBlock(selectFirstPosition, '<h2>hello</h2>', 'h2');
queryFormatBlock(selectFirstPosition, '<h3>hello</h3>', 'h3');
queryFormatBlock(selectFirstPosition, '<h4>hello</h4>', 'h4');
queryFormatBlock(selectFirstPosition, '<h5>hello</h5>', 'h5');
queryFormatBlock(selectFirstPosition, '<h6>hello</h6>', 'h6');
queryFormatBlock(selectFirstPosition, '<p>hello</p>', 'p');
queryFormatBlock(selectFirstPosition, '<pre>hello</pre>', 'pre');

debug('');
debug('Nested cases');
queryFormatBlock(selectFirstPosition, '<h1><h2>hello</h2></h1>', 'h2');
queryFormatBlock(selectFirstPosition, '<h1><address>hello</address></h1>', 'address');
queryFormatBlock(selectFirstPosition, '<pre>hello<p>world</p></pre>', 'pre');
queryFormatBlock(selectMiddleOfHelloWorld, '<pre>hello<p>world</p></pre>', 'pre');
queryFormatBlock(selectMiddleOfHelloWorld, '<address><h1>hello</h1>world</address>', 'address');
queryFormatBlock(selectMiddleOfHelloWorld, '<h1>hello</h1>world', '');

document.body.removeChild(testContainer);
var successfullyParsed = true;

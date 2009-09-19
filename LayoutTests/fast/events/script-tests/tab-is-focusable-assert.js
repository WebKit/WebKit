description('Making sure that the assert in Node::isFocusable does not fail. ' +
            'https://bugs.webkit.org/show_bug.cgi?id=29210');

var div = document.createElement('div');
div.style.display = 'none';

var a = document.createElement('a');
a.href = 'http://webkit.org';
div.appendChild(a);

document.body.appendChild(div);

if (window.eventSender)
    eventSender.keyDown('\t');

testPassed('No crash');

var successfullyParsed = true;

description('Test to make sure WebKit does not remove content when unbolding nested b\'s. See <a href="https://bugs.webkit.org/show_bug.cgi?id=30083">Bug 30083</a> for detail.');

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function removeStyleAndExpect(command, content, expected)
{
    testContainer.innerHTML = content;
    var e = document.getElementById('e');
    var s = window.getSelection();
    var r = document.createRange();
    r.setStart(e, 0);
    r.setEnd(e, 1);
    e.focus();
    s.removeAllRanges();
    s.addRange(r);

    document.execCommand(command, false, null);

    return shouldBe("'"+testContainer.innerHTML+"'","'"+expected+"'");
}

removeStyleAndExpect('bold', '<span id="e"><b>1<b>2</b></b></span>', '<span id="e">12</span>');
removeStyleAndExpect('bold', '<span id="e"><b id="foo">1<b>2</b></b></span>', '<span id="e"><span id="foo">12</span></span>');
removeStyleAndExpect('bold', '<span id="e"><b id="foo"><b>1</b>2</b></span>', '<span id="e"><span id="foo">12</span></span>');
removeStyleAndExpect('bold', '<span id="e"><b><b><b>12</b></b></b></span>', '<span id="e">12</span>');
removeStyleAndExpect('bold', '<span id="e"><b><b><b>1</b></b>2</b></span>', '<span id="e">12</span>');
removeStyleAndExpect('italic', '<span id="e"><i>1<i>2</i></i></span>', '<span id="e">12</span>');
removeStyleAndExpect('strikeThrough', '<span id="e"><s>1<s>2</s></s></span>', '<span id="e">12</span>');

document.body.removeChild(testContainer);

var successfullyParsed = true;

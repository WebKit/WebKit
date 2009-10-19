description("Ensure that double clicking does not expand selection across new line boundaries.")

function doubleClickOnElement(el)
{
    var rect = el.getBoundingClientRect();
    var x = rect.left + rect.width / 2;
    var y = rect.top + rect.height / 2;
    doubleClickAt(x, y);
}

function doubleClickAt(x, y)
{
    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function selectionShouldBe(s)
{
    shouldBeEqualToString('String(window.getSelection())', s);
}

var div = document.createElement('div');
document.body.appendChild(div);

// DIV with br
div.innerHTML = 'm<span>o</span>re &nbsp;<br>&nbsp;<br>text';
doubleClickOnElement(div.firstElementChild);
selectionShouldBe('more \xa0');

// DIV with white-space pre
div.style.whiteSpace = 'pre';
div.innerHTML = 'm<span>o</span>re  \n \ntext';
doubleClickOnElement(div.firstElementChild);
selectionShouldBe('more  ');

document.body.removeChild(div);

// Textarea -- we cannot really get the position of the word we want so we use
// a large font in a textarea and double click in the upper left corner

var textarea = document.createElement('textarea');
textarea.style.border = 0;
textarea.style.padding = 0;
textarea.style.fontSize = '30px';
document.body.appendChild(textarea);

var rect = textarea.getBoundingClientRect();
var x = rect.left + 10;
var y = rect.top + 10;

textarea.value = 'more  \n \ntext';
doubleClickAt(x, y);
selectionShouldBe('more  ');

document.body.removeChild(textarea);

var successfullyParsed = true;

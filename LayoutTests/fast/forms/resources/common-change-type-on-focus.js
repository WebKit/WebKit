var mouseDownCount = 0;

function testChangeTypeOnFocus()
{
    var input = document.getElementById('test');
    description('Assertion failure by changing type from type=' + input.type + ' in focus event.');
    input.onfocus = function () { this.type = 'text'; };

    var spinX = input.offsetLeft + input.offsetWidth - 6;
    var middleX = input.offsetLeft + input.offsetWidth / 2
    var middleY = input.offsetTop + input.offsetHeight / 4;

    if (window.eventSender) {
        // Click the spin button.
        eventSender.mouseMoveTo(spinX, middleY);
        eventSender.mouseDown(); // This made an assertion fail.
        eventSender.mouseUp();
    } else
        debug('Manual testing: Click the spin button, and see if the browser crashes or not.');

    testPassed('Not crashed.');

    // Click the input element. The event should not be captured by the spin button.
    if (window.eventSender) {
        input.onmousedown = function() { ++mouseDownCount; };
        eventSender.mouseMoveTo(middleX, middleY);
        eventSender.mouseDown();
        eventSender.mouseUp();
        shouldBe('mouseDownCount', '1');
    }
}

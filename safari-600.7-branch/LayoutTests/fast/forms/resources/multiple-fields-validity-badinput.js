var invalidStyleColor = 'rgb(255, 0, 0)';
var input;
var quiet = true;

function colorOf(el) {
    return document.defaultView.getComputedStyle(el, null).getPropertyValue('background-color');
}
function testBadInput(type) {
    if (!window.eventSender) {
        debug('Needs to run this on DRT/WTR.');
        return;
    }
    description('A ' + type + ' input fields with a bad user input should make validity.badInput true and have :invalid style.');
    input = document.createElement('input');
    input.type = type;
    document.body.appendChild(input);
    input.focus();

    debug('Initial state. The elment has no value.');
    shouldNotBe('colorOf(input)', 'invalidStyleColor');
    shouldBeFalse('input.validity.badInput');

    debug('Set a value to the first sub-field. The element becomes badInput.');
    eventSender.keyDown('upArrow');
    shouldBe('colorOf(input)', 'invalidStyleColor');
    shouldBeTrue('input.validity.badInput');

    if (type === 'date' || type === 'datetime' || type === 'datetime-local') {
        debug('Set an invalid date, 2012-02-31.');
        if (type == 'date')
            input.value = '2012-02-01';
        else if (type == 'datetime')
            input.value = '2012-02-01T03:04Z';
        else
            input.value = '2012-02-01T03:04';
        shouldNotBe('colorOf(input)', 'invalidStyleColor', quiet);
        shouldBeFalse('input.validity.badInput', quiet);
        eventSender.keyDown('rightArrow'); // -> 02/[01]/2012 ...
        eventSender.keyDown('downArrow'); //  -> 02/[31]/2012 ...
        shouldBeEqualToString('input.value', '');
        shouldBeTrue('input.validity.badInput');
        shouldBe('colorOf(input)', 'invalidStyleColor');
    }
}

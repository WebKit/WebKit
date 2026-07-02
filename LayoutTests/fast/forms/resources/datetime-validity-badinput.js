function testBadInput(type) {
    test(() => {
        if (!window.eventSender) {
            assert_false(true, 'eventSender is required for this test');
            return;
        }

        function backgroundColor(el) {
            return getComputedStyle(el, null).getPropertyValue('background-color');
        }

        const invalidStyleColor = 'rgb(255, 0, 0)';

        const input = document.createElement('input');
        input.type = type;
        document.body.appendChild(input);

        input.focus();

        // Initial state. The element has no value.
        assert_not_equals(backgroundColor(input), invalidStyleColor, 'Input has no value. It should not have invalid style.');
        assert_false(input.validity.badInput, 'Input has no value. badInput should be false.');

        // Set a value to the first sub-field. The element becomes badInput.
        eventSender.keyDown('upArrow');
        assert_equals(backgroundColor(input), invalidStyleColor, 'Input is partially filled, it is invalid.');
        assert_true(input.validity.badInput, 'Input is partially filled. badInput should be true.');

        if (type == 'date' || type== 'datetime' || type == 'datetime-local') {
            // Set an invalid date, 2012-02-31.
            if (type == 'date')
                input.value = '2012-02-01';
            else if (type == 'datetime')
                input.value = '2012-02-01T03:04Z';
            else
                input.value = '2012-02-01T03:04';
            assert_not_equals(backgroundColor(input), invalidStyleColor, 'Input has valid date, it is valid.');
            assert_false(input.validity.badInput, 'Input has valid date. badInput should be false.');
            eventSender.keyDown('rightArrow'); // -> 02/[01]/2012 ...
            eventSender.keyDown('downArrow'); //  -> 02/[31]/2012 ...
            assert_equals(input.value, '', 'Input was changed to invalid date, value is empty.');
            assert_true(input.validity.badInput, 'Input was changed to invalid date. badInput should be true');
            assert_equals(backgroundColor(input), invalidStyleColor, 'Input was changed to invalid date.');
        }
    }, 'A ' + type + ' input field with a bad user input should make validity.badInput true and have :invalid style.');
}

/**
 * Tests attribute parsing and handling of whitespace in attribute values.
 *
 * @param type Name of the type being tested (only for test output)
 * @param target The element that should be tested
 * @param attribute The name of the attribute that should be tested
 * @param expected The fallback/default value that is the expectation for invalid values
 * @param whitespace An array of strings that are valid whitespace characters
 * @param valid An array of strings containing valid attribute values
 * @param validUnits An array of strings containing valid unit specifiers
 * @param garbage An array of strings containing values that would make a valid value invalid when concatenated
 * @param assert_valid_custom A function for asserting validity of a valid value, arguments passed to this function: the element and the string from valid values array
 * @param assert_invalid_custom A function for asserting that an invalid value results in the expected default value, arguments passed to this function: the element and the expected value
 */
function testType(type, target, attribute, expected, whitespace, valid, validunits, garbage, assert_valid_custom, assert_invalid_custom) {
    whitespace.forEach(function(leading) {
        whitespace.forEach(function(trailing) {
            valid.forEach(function(value) {
                validunits.forEach(function(unit) {
                    var valueStr = leading + value + unit + trailing;
                    var escapedValueStr = valueStr.replace(/(\r)/g, '\\r').replace(/(\n)/g, '\\n').replace(/(\t)/g, '\\t').replace(/(\f)/g, '\\f');
                    test(function() {
                        try {
                            target.setAttribute(attribute, valueStr);
                            assert_equals(target.getAttribute(attribute), valueStr);
                            assert_valid_custom(target, value);
                        }
                        finally {
                            target.removeAttribute(attribute);
                        }
                    }, "Test " + type + " valid value: " + escapedValueStr );
                });
            });
        });

        // test whitespace between value and unit
        validunits.forEach(function(unit) {
            if (unit == "" || leading == "")
                return;
            valid.forEach(function(value) {
                var valueStr = value + leading + unit;
                var escapedValueStr = valueStr.replace(/(\r)/g, '\\r').replace(/(\n)/g, '\\n').replace(/(\t)/g, '\\t').replace(/(\f)/g, '\\f');
                test(function() {
                    try {
                        target.setAttribute(attribute, valueStr);
                        assert_equals(target.getAttribute(attribute), valueStr);
                        assert_invalid_custom(target, expected);
                    }
                    finally {
                        target.removeAttribute(attribute);
                    }
                }, "Test " + type + " WS invalid value: " + escapedValueStr);
            });
        });

        // test trailing garbage
        garbage.forEach(function(trailing) {
            valid.forEach(function(value) {
                var valueStr = leading + value + trailing;
                var escapedValueStr = valueStr.replace(/(\r)/g, '\\r').replace(/(\n)/g, '\\n').replace(/(\t)/g, '\\t').replace(/(\f)/g, '\\f');
                test(function() {
                    try {
                        target.setAttribute(attribute, valueStr);
                        assert_equals(target.getAttribute(attribute), valueStr);
                        assert_invalid_custom(target, expected);
                    }
                    finally {
                        target.removeAttribute(attribute);
                    }
                }, "Test " + type + " trailing garbage, value: " + escapedValueStr);
            });
        });
    });
}

/**
 * Tests attribute parsing and handling of invalid whitespace in attribute values.
 *
 * @param type Name of the type being tested (only for test output)
 * @param target The element that should be tested
 * @param attribute The name of the attribute that should be tested
 * @param expected The fallback/default value that is the expectation for invalid values
 * @param whitespace An array of strings that are valid whitespace characters
 * @param invalid An array of strings containing invalid attribute values * @param garbage An array of strings containing values that would make a valid value invalid when concatenated
 * @param validunits An array of strings containing valid unit specifiers
 * @param assert_valid_custom A function for asserting validity of a valid value, arguments passed to this function: the element and the string from valid values array
 * @param assert_invalid_custom A function for asserting that an invalid value results in the expected default value, arguments passed to this function: the element and the expected value
 */
function testInvalidType(type, target, attribute, expected, whitespace, invalid, validunits, assert_valid_custom, assert_invalid_custom) {
    whitespace.forEach(function(leading) {
        whitespace.forEach(function(trailing) {
            // test invalid values
            invalid.forEach(function(value) {
                validunits.forEach(function(unit) {
                    var valueStr = leading + value + unit + trailing;
                    var escapedValueStr = valueStr.replace(/(\r)/g, '\\r').replace(/(\n)/g, '\\n').replace(/(\t)/g, '\\t').replace(/(\f)/g, '\\f');
                    test(function() {
                        try {
                            target.setAttribute(attribute, valueStr);
                            assert_equals(target.getAttribute(attribute), valueStr);
                            assert_invalid_custom(target, expected);
                        }
                        finally {
                            target.removeAttribute(attribute);
                        }
                    }, "Test " + type + " invalid value: " + escapedValueStr);
                });
            });
        });
    });
}

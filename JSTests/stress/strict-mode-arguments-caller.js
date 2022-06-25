function strictArguments() {
    "use strict";
    return arguments;
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(strictArguments().caller, undefined);
shouldBe('caller' in strictArguments(), false);
shouldBe(Object.getOwnPropertyDescriptor(strictArguments(), 'caller'), undefined);

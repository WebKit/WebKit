(function() {
    "use strict";
    var TEST_LENGTH = 10;

    var slice = Array.prototype.slice;
    var clonedArguments = (function() { return arguments; }).apply(null, new Array(TEST_LENGTH).fill(1));

    var slicedArray;
    for (var i = 0; i < 1e6; i++)
        slicedArray = slice.call(clonedArguments);

    if (slicedArray.length !== TEST_LENGTH)
        throw new Error("Bad assertion!");
})();

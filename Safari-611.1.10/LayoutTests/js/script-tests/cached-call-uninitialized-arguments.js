description(
"This test checks that uninitialized parameters for cached call functions correctly defaults to undefined."

);

function doForEach(arr) {
    function callback(element, index, array, arg4, arg5, arg6) {

        function shouldBeUndefined(_a) {
            var exception;
            var _av;
            try {
                _av = eval(_a);
            } catch (e) {
                exception = e;
            }

            if (exception)
                testFailed(_a + " should be undefined. Threw exception " + exception);
            else if (typeof _av == "undefined")
                testPassed(_a + " is undefined.");
            else
                testFailed(_a + " should be undefined. Was " + _av);
        }

        shouldBeUndefined("arg4");
        shouldBeUndefined("arg5");
        shouldBeUndefined("arg6");
    }

    arr.forEach(callback);
}

function callAfterRecursingForDepth(depth, func, arr) {
    if (depth > 0) {
        callAfterRecursingForDepth(depth - 1, func, arr);
    } else {
        func(arr);
    }
}

var arr = [1];
callAfterRecursingForDepth(20, doForEach, arr);

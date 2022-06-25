description("Verify that objects with numeric named properties don't set length like an array.");

var numOfIterations = 10000;
var count = 0;
var obj = {
    1: 'foo',
    8: 'bar',
    50: 'baz'
};

var expectedCount = Object.keys(obj).length;

function isArrayLike(collection) {
    var length = collection && collection.length;

    return typeof length == 'number';
}

function filter(obj, callback, context) {
    var results = [];
    var i, length;

    if (isArrayLike(obj)) {
        for (i = 0, length = obj.length; i < length; i++) {
            var value = obj[i];
            if (callback(value))
                results.push(value);
        }
    } else {
        for (var key in obj) {
            var value = obj[key];
            if (callback(value))
                results.push(value);
        }
    }

    return results;
}

for (var i = 0; i < numOfIterations; i++) {
    filter([], function() { return true; });
}

filter(obj, function() { 
    count++;
    return true;
});

if (count !== expectedCount)
    testFailed("Incorrect number of iterated keys: " + count + ", expected: " + expectedCount);
else
    testPassed("Correct number of iterated keys: " + count);

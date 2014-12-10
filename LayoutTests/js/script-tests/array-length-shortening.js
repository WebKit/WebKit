description(
"Tests array length shortening."
);

var count;

function testLengthShortening(array) {
    array.length = 1;
    count = 0;
    for (var x of array) {
        count++;
    }

    shouldBe("count", "1");
}

var arr;

// Test Objects with densely packed indexed properties:
function denseInt32Elements(arr) {
    arr[0] = 1;
    arr[1] = 2;
    return arr;
}
testLengthShortening(denseInt32Elements(Object.create(Array.prototype)));
testLengthShortening(denseInt32Elements([]));

function denseDoubleElements(arr) {
    arr[0] = 1.5;
    arr[1] = 2.5;
    return arr;
}
testLengthShortening(denseDoubleElements(Object.create(Array.prototype)));
testLengthShortening(denseDoubleElements([]));

function denseObjectElements(arr) {
    arr[0] = {};
    arr[1] = {};
    return arr;
}
testLengthShortening(denseObjectElements(Object.create(Array.prototype)));
testLengthShortening(denseObjectElements([]));

// Test Objects with hole-y indexed properties:
function holeyInt32Elements(arr) {
    arr[0] = 1;
    arr[1] = 2;
    arr[4] = 4;
    return arr;
}
testLengthShortening(holeyInt32Elements(Object.create(Array.prototype)));
testLengthShortening(holeyInt32Elements([]));

function holeyDoubleElements(arr) {
    arr[0] = 1.5;
    arr[1] = 2.5;
    arr[4] = 4.5;
    return arr;
}
testLengthShortening(holeyDoubleElements(Object.create(Array.prototype)));
testLengthShortening(holeyDoubleElements([]));

function holeyObjectElements(arr) {
    arr[0] = {};
    arr[1] = {};
    arr[4] = {};
    return arr;
}
testLengthShortening(holeyObjectElements(Object.create(Array.prototype)));
testLengthShortening(holeyObjectElements([]));

// Test Objects with ArrayStorage indexed properties:
function arrayStorageInt32Elements(arr) {
    arr[0] = 1;
    arr[1] = 2;
    arr.unshift(100); // Force conversion to using ArrayStorage.
    return arr;
}
testLengthShortening(arrayStorageInt32Elements(Object.create(Array.prototype)));
testLengthShortening(arrayStorageInt32Elements([]));

function arrayStorageDoubleElements(arr) {
    arr[0] = 1.5;
    arr[1] = 2.5;
    arr.unshift(100.5); // Force conversion to using ArrayStorage.
    return arr;
}
testLengthShortening(arrayStorageDoubleElements(Object.create(Array.prototype)));
testLengthShortening(arrayStorageDoubleElements([]));

function arrayStorageObjectElements(arr) {
    arr[0] = {};
    arr[1] = {};
    arr.unshift({}); // Force conversion to using ArrayStorage.
    return arr;
}
testLengthShortening(arrayStorageObjectElements(Object.create(Array.prototype)));
testLengthShortening(arrayStorageObjectElements([]));

// Test Objects with sparse indexed properties:
function sparseInt32Elements(arr) {
    arr[0] = 1;
    arr[1] = 2;
    arr[100000] = 100;
    return arr;
}
testLengthShortening(sparseInt32Elements(Object.create(Array.prototype)));
testLengthShortening(sparseInt32Elements([]));

function sparseDoubleElements(arr) {
    arr[0] = 1.5;
    arr[1] = 2.5;
    arr[100000] = 100.5;
    return arr;
}
testLengthShortening(sparseDoubleElements(Object.create(Array.prototype)));
testLengthShortening(sparseDoubleElements([]));

function sparseObjectElements(arr) {
    arr[0] = {};
    arr[1] = {};
    arr[100000] = {};
    return arr;
}
testLengthShortening(sparseObjectElements(Object.create(Array.prototype)));
testLengthShortening(sparseObjectElements([]));

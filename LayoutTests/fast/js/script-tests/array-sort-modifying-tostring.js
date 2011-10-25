description(
"Test of array with toString() override that truncates array."
);

var maxSize = 2000;
var keepSize = 100;
var digits = 4;
var countToDelete = maxSize - keepSize;

var a = new Array(maxSize);

function do_gc() {
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 1000; i++)
        new String(i);
}

function Item(val) {
    this.value = val;
}

function toString_Mutate() {
    a.splice(keepSize, countToDelete);
    do_gc();
    for (var i = keepSize; i < countToDelete; i++) {
        delete a[i];
    }

    if ((this != undefined) && (this.value != undefined)) {
        var s = this.value.toString();
        if (s.length < digits)
            s = ('0000' + s).slice(-digits);

        return s;
    } else
        return "Undef";
}

function test() {
    for (var i = 0; i < a.length; i++) {
        a[i] = new Item(a.length - i - 1);
        a[i].toString = toString_Mutate;
    }
    try {
        a.sort();
    if (a.length == maxSize)
        testPassed("Array length is unchanged.");
    else
        testFailed("Array length should be " + maxSize + " but is " + a.length + ".");

    var firstFailedValue = -1;

    for (var i = 0; i < a.length; i++) {
        if (a[i].value != i) {
            firstFailedValue = i;
            break;
        }
    }

    if (firstFailedValue == -1)
        testPassed("Array values are correct.");
    else
        testFailed("Array values are wrong, first bad value at index " + firstFailedValue + ", should be " + firstFailedValue + " was " + a[firstFailedValue].value + ".");
    } catch(er) {
        testFailed("Got exception processing sort()");
    }

    for (var i = 0; i < a.length; i++) {
        a[i] = 0x42424242;
    }
}

test();

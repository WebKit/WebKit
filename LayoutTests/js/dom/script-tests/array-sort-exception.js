description(
"Test of array sort with toString() override that throws exception."
);

var size = 200;
var digits = 3;
var exceptionString = 'From toString()';
var catchArg = "";

var a = new Array(size);

function do_gc() {
    if (window.GCController)
        return GCController.collect();
    
    for (var i = 0; i < 1000; i++)
        new String(i);
}

function Item(val) {
    this.value = val;
}

function toString_throw() {
    var s = this.value.toString();
    
    if (this.value >= size/2)
        throw(exceptionString);
    
    s = ('0000' + s).slice(-digits);

    return s;
}

function test() {
    for (var i = 0; i < a.length; i++) {
        a[i] = new Item(a.length - i - 1);
        a[i].toString = toString_throw;
    }

    try {
        a.sort();
    } catch(err) {
        catchArg = err;
        shouldBe("catchArg", "exceptionString");

        do_gc();

        return;
    }
    
    debug('ERROR: Never got toString() exception');
}

test();

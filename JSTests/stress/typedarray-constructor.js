load("./resources/typedarray-constructor-helper-functions.js", "caller relative");

let TypedArray = Object.getPrototypeOf(Int32Array);

class A extends TypedArray {
    constructor() { super(); }

}

shouldThrow("new A()");

let foo = [1,2,3,4];

function iterator() {
    return { i: 0,
             next: function() {
                 if (this.i < foo.length/2) {
                     return { done: false,
                              value: foo[this.i++]
                            };
                 }
                 return { done: true };
             }
           };
}

foo[Symbol.iterator] = iterator;

shouldBeTrue("testConstructor('(foo)', [1,2])");
debug("");

debug("Test that we don't premptively convert to native values and use a gc-safe temporary storage.");


done = false;
obj = {
    valueOf: function() {
        if (!done)
            throw "bad";
        return 1;
    }
};

foo = [obj, 2, 3, 4];

function iterator2() {
    done = false;
    return { i: 0,
             next: function() {
                 gc();
                 if (this.i < foo.length/2) {
                     return { done: false,
                              value: foo[this.i++]
                            };
                 }
                 done = true;
                 return { done: true };
             }
           };
}

foo[Symbol.iterator] = iterator2;

shouldBeTrue("testConstructor('(foo)', [1,2])");

shouldBeTrue("testConstructor('(true)', [0])");
shouldBeTrue("testConstructor('(`hi`)', [])");

finishJSTest();

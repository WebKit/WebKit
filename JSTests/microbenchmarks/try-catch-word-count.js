//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
"use strict";

let assert = (b, m) => {
    if (!b)
        throw new Error("Bad assertion: " + m);
}

class EOF extends Error { };

let texts = [`function foo() {
    var o = {};
    o.a = 1;
    o.b = 2;
    o.c = 3;
    o.d = 4;
    o.e = 5;
    o.f = 6;
    o.g = 7;
    return o;
}

var result = 0;
for (var i = 0; i < 100000; ++i)
    result += foo().a;

if (result != 100000)
    throw "Error, bad result: " + result;

`,`"use strict";

(function() {
    let o = {
        apply(a, b) {
            return a + b;
        }
    };
    
    function foo() {
        let result = 0;
        for (let i = 0; i < 1000; ++i)
            result = o.apply(result, 1);
        return result;
    }
    
    noInline(foo);
    
    let result = 0;
    for (let i = 0; i < 10000; ++i)
        result += foo();
    
    if (result != 10000000)
        throw new "Bad result: " + result;
})();

`,`function foo(a, b) {
    return arguments[0] + b;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(i, 1);
    if (result != i + 1)
        throw "Error: bad result: " + result;
}
`,`function foo() { return arguments; }
noInline(foo);

function bar(o) {
    var tmp = o[0];
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        if (tmp)
            result += tmp * i;
    }
    return result;
}
noInline(bar);

var result = 0;
var o = foo();
for (var i = 0; i < 10000; ++i)
    result += bar(o);

if (result !== 0)
    throw "Error: bad result: " + result;
`,`function foo() {
    "use strict";
    return [...arguments];

}

noInline(foo);

for (var i = 0; i < 200000; ++i) {
    var result = foo(i);
    if (result[0] != i)
        throw "Error: bad result: " + result;
}
`,`function foo() {
    return arguments[0];
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(i);
    if (result != i)
        throw "Error: bad result: " + result;
}
`,`function foo(a, b) {
    return a + b;
}

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, 3);
    if (result != 3)
        throw "Bad result: " + result;
}

`,`function foo(a) {
    var result = a[0];
    if (result)
        result += a[1];
    if (result)
        result += a[2];
    if (result)
        result += a[3];
    if (result)
        result += a[4];
    return result;
}

var result = 0;

for (var i = 0; i < 100000; ++i) {
    var array = [1, 2, 3, 4, 5];
    if (i & 1)
        array.f = 42;
    result += foo(array);
}

if (result != 1500000)
    throw "Error: bad result: " + result;
`,`//@ runNoFTL

var f = function(a) {
    var sum = 0;
    for (var i = 0; i < a.length; i++) {
        sum += a[i];
    }   
    return sum;
};

var run = function() {
    var o1 = []; 
    for (var i = 0; i < 100; i++) {
        o1[i] = i;
    }
    
    var o2 = {}; 
    for (var i = 0; i < o1.length; i++) {
        o2[i] = o1[i];
    }
    o2.length = o1.length;

    var sum = 0;
    for (var i = 0; i < 100000; i++) {
        if (i % 2 === 0)
            sum += f(o1);
        else
            sum += f(o2);
    }
    return sum;
};

var result = run();
if (result !== 495000000)
    throw "Bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.every(test1);
    array.every(test2);
    array.every(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.forEach(test1);
    array.forEach(test2);
    array.forEach(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.map(test1);
    array.map(test2);
    array.map(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.reduce(test1, {});
    array.reduce(test2, {});
    array.reduce(test3, {});
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.reduceRight(test1, {});
    array.reduceRight(test2, {});
    array.reduceRight(test3, {});
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`var result = 0;
function test1(a) {
    result << 1;
    result++;
    return false;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return false;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return false;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.some(test1);
    array.some(test2);
    array.some(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
`,`function foo() {
    var a = new Array(1000);
    for (var i = 0; i < 1000; ++i) {
        if (i % 7 === 0)
            continue;
        a[i] = i;
    }

    var niters = 10000;
    var remove = true;
    var lastRemovedItem = null;
    var lastRemovedIndex = null;
    for (var i = 0; i < niters; ++i) {
        if (remove) {
            lastRemovedIndex = Math.floor(Math.random() * a.length);
            lastRemovedItem = a[lastRemovedIndex];
            a.splice(lastRemovedIndex, 1);
        } else {
            a.splice(lastRemovedIndex, 0, lastRemovedItem);
        }
        remove = !remove;
    }
    if (a.length !== 1000)
        throw new Error("Incorrect length");
};
foo();
`,`function foo() {
    var a = [];
    var b = [];
    
    for (var i = 0; i < 1000; ++i) {
        a.push(i + 0.5);
        b.push(i - 0.5);
    }
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < a.length; ++j)
            a[j] += b[j];
    }

    var result = 0;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    
    return result;
}

var result = foo();
if (result != 499500000)
    throw "Bad result: " + result;

`,`function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + 0.5);
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            array[j]++;
    }

    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    
    return result;
}

var result = foo();
if (result != 1500000)
    throw "Bad result: " + result;


`,`function foo() {
    var a = [];
    var b = [];
    var c = [];
    
    for (var i = 0; i < 1000; ++i) {
        a.push(i + 0.5);
        b.push(i - 0.5);
        c.push((i % 2) ? 0.5 : -0.25);
    }
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < a.length; ++j)
            a[j] += b[j] * c[j];
    }

    var result = 0;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    
    return result;
}

var result = foo();
if (result != 63062500)
    throw "Bad result: " + result;


`,`function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + 0.5);
    
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            result += array[j];
    }
    
    return result;
}

if (foo() != 500000000)
    throw "ERROR";

`,`function foo() {
    var a = [];
    var b = [];
    
    for (var i = 0; i < 1000; ++i) {
        a.push(i + 1);
        b.push(i - 1);
    }
    
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < a.length; ++j)
            a[j] += b[j];
        for (var j = 0; j < a.length; ++j)
            a[j] -= b[j];
    }

    var result = 0;
    for (var i = 0; i < a.length; ++i)
        result += a[i];
    
    return result;
}

if (foo() != 500500)
    throw "ERROR";


`,`function foo() {
    var array = [];
    
    for (var i = 0; i < 1000; ++i)
        array.push(i + ((i % 2) ? 0.5 : 0));
    
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        for (var j = 0; j < array.length; ++j)
            result += array[j];
    }
    
    return result;
}

var result = foo();
if (result != 499750000)
    throw "Bad result: " + result;

`,`//@ skip

var array = new Array(10000);

for (var i = 0; i < 100000; ++i)
    array[i % array.length] = new DataView(new ArrayBuffer(1000));

for (var i = 0; i < array.length; ++i) {
    if (array[i].byteLength != 1000)
        throw "Error: bad length: " + array[i].byteLength;
    if (array[i].buffer.byteLength != 1000)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
}
`,`//@ skip

var array = new Array(10000);

for (var i = 0; i < 70000; ++i)
    array[i % array.length] = new DataView(new ArrayBuffer(10));

for (var i = 0; i < array.length; ++i) {
    if (array[i].byteLength != 10)
        throw "Error: bad length: " + array[i].byteLength;
    if (array[i].buffer.byteLength != 10)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
}
`,`//@ skip

var result = 0;
var buffer = new ArrayBuffer(10);
var array1 = new Int32Array(buffer, 4, 1);
var array2 = new Uint32Array(10);

for (var i = 0; i < 1000000; ++i) {
    result += array1.byteOffset;
    result += array2.byteOffset;
}

if (result != 4000000)
    throw "Error: wrong result: " + result;
`,`//@ skip

var array = new Array(10000);

for (var i = 0; i < 100000; ++i)
    array[i % array.length] = new Int8Array(new ArrayBuffer(1000));

for (var i = 0; i < array.length; ++i) {
    if (array[i].length != 1000)
        throw "Error: bad length: " + array[i].length;
    if (array[i].buffer.byteLength != 1000)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
}
`,`//@ skip

var array = new Array(10000);

for (var i = 0; i < 100000; ++i)
    array[i % array.length] = new Int8Array(new ArrayBuffer(10)).buffer;

for (var i = 0; i < array.length; ++i) {
    if (array[i].byteLength != 10)
        throw "Error: bad byteLength: " + array[i].byteLength;
}
`,`//@ skip

var array = new Array(10000);

for (var i = 0; i < 70000; ++i)
    array[i % array.length] = new Int8Array(new ArrayBuffer(10));

for (var i = 0; i < array.length; ++i) {
    if (array[i].length != 10)
        throw "Error: bad length: " + array[i].length;
    if (array[i].buffer.byteLength != 10)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
}
`,`//@ skip

for (var i = 0; i < 70000; ++i)
    new Int8Array(new ArrayBuffer(10));

`,`var fn = function() {
    return () => arguments[0];
}(1);

for (var i = 0; i < 100000; i++) {
    if(fn(2) !== 1) throw 0; 
}
`,`var testValue  = 'test-value';

class A {
    constructor() {
        this.value = testValue;
    }
}

class B extends A {
    constructor() {
        super();
        var arrow  = () => testValue;
        arrow();
    }
}

noInline(B);

for (let i = 0; i < 1000000; ++i) {
    let b = new B();
    if (b.value != testValue)
        throw "Error: bad result: " + result;
}
`,`var testValue  = 'test-value';

class A {
    constructor() {
        this.value = testValue;
    }

    getValue () {
        return this.value;
    }
}

class B extends A {
    getParentValue() {
        var arrow  = () => testValue;
        return arrow();
    }
}

for (let i = 0; i < 100000; ++i) {
    let b = new B();
    if (b.getParentValue() != testValue)
        throw "Error: bad result: " + result;
}
`,`function bar(a, b) {
    return ((_a, _b) => _a + _b)(a, b);
}

noInline(bar);

for (let i = 0; i < 1000000; ++i) {
    let result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
`,`var af = (a, b) => a + b;

noInline(af);

function bar(a, b) {
    return af(a, b);
}

noInline(bar);

for (let i = 0; i < 1000000; ++i) {
    let result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
`,`// The strlen function is derived from here:
// http://kripken.github.io/mloc_emscripten_talk/#/20

var MEM8  = new Uint8Array(1024);

// Calculate length of C string:
function strlen(ptr) {
  ptr = ptr|0;
  var curr = 0;
  curr = ptr;
  while (MEM8[curr]|0 != 0) {
    curr = (curr + 1)|0;
  }
  return (curr - ptr)|0;
}

//----- Test driver ----

for (i = 0; i < 1024; i++) {
 MEM8[i] = i%198;
}

MEM8[7] = 0;

var sum = 0
for (i = 0; i < 1000000; i++) {
  sum += strlen(5);
}

if (sum != 2000000)
    throw "Bad result: " + result;
`,`
o = RegExp;
j = 0;
l = 2;
z = 0;
function test(o, z) {
    var k = arguments[(((j << 1 | l) >> 1) ^ 1) & (z *= 1)];
    k.input = 0;
    for (var i = 0; i < 25000; i++) {
        k.input = "foo";
    }

    return k.input;
}
var result = test({__proto__: {bar:"wibble", input:"foo"}});
var result = test({input:"foo"});
var result = test(o)
for (var k = 0; k < 6; k++) {
    var start = new Date;
    var newResult = test(o)
    var end = new Date;
    if (newResult != result)
        throw "Failed at " + k + "with " + newResult + " vs. " + result
    result = newResult;
    o = {__proto__ : o }
}
`,`// RegExp.input is a handy setter

var o = RegExp;
function test(o) {
    var k = 0;
    o.input = "bar";
    for (var i = 0; i < 30000; i++)
        o.input = "foo";

    return o.input;
}

var result = test(o);

for (var k = 0; k < 9; k++) {
    var start = new Date;
    var newResult = test(o)
    var end = new Date;
    if (newResult != result)
        throw "Failed at " + k + "with " +newResult + " vs. " + result
    result = newResult; 
    o = {__proto__ : o }
}

`,`//@ runNoFTL

var set = new Set;
for (var i = 0; i < 8000; ++i) {
    set.add(i);
    set.add("" + i)
    set.add({})
    set.add(i + .5)
}

var result = 0;

set.forEach(function(k){ result += set.size; })
for (var i = 8000; i >= 0; --i) {
    set.delete(i)
    set.has("" + i)
    set.add(i + .5)
}
set.forEach(function(k){ result += set.size; })

if (result != 1600048001)
    throw "Bad result: " + result;


`,`// Test the performance of integer multiplication by implementing a 16-bit
// string hash.

function stringHash(string)
{
    // source: http://en.wikipedia.org/wiki/Java_hashCode()#Sample_implementations_of_the_java.lang.String_algorithm
    var h = 0;
    var len = string.length;
    for (var index = 0; index < len; index++) {
        h = (((31 * h) | 0) + string.charCodeAt(index)) | 0;
    }
    return h;
}

for (var i = 0; i < 10000; ++i) {
    var result =
        (stringHash("The spirit is willing but the flesh is weak.") *
         stringHash("The vodka is strong but the meat is rotten.")) | 0;
    if (result != -1136304128)
        throw "Bad result: " + result;
}

`,`
Function.prototype.a = Function.prototype.apply;

function testFunction(a, b)
{
    "use strict"
    return this * 10000 + a * 1000 + b * 100 + arguments[2] * 10 + arguments.length;
}

var arrayArguments = [1, [2, 3, 4]]

for (var i = 0; i < 10000; i++) {
    var result1 = testFunction.apply(...arrayArguments);
    var result2 = testFunction.a(...arrayArguments);
    if (result1 != result2) 
        throw "Call with spread array failed at iteration " + i + ": " + result1 + " vs " + result2;
}

for (var i = 0; i < 10000; i++) {
    var result1 = testFunction.apply(...[1, [2, 3, 4]]);
    var result2 = testFunction.a(...[1, [2, 3, 4]]);
    if (result1 != result2) 
        throw "Call with spread array failed at iteration " + i + ": " + result1 + " vs " + result2;
}

function test2() {
    for (var i = 0; i < 10000; i++) {
        var result1 = testFunction.apply(...arguments);
        var result2 = testFunction.a(...arguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test2(1,[2,3,4])


function test3() {
    aliasedArguments = arguments;
    for (var i = 0; i < 10000; i++) {
        var result1 = testFunction.apply(...aliasedArguments);
        var result2 = testFunction.a(...aliasedArguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test3(1,[2,3,4])`]

let processTexts = (texts) => {
    let wc = 0;
    for (let text of texts) {
        try {
            let globalIndex = 0;
            while (1) {
                let index = globalIndex;
                let word;
                while (true) {
                    let c = text[index];
                    if (c.match(/\s/)) {
                        word = text.substring(globalIndex, index);
                        while (c.match(/\s/)) {
                            ++index;
                            if (index >= text.length) {
                                globalIndex = index;
                                break;
                            }
                            c = text[index];
                        }

                        globalIndex = index;
                        break;
                    }

                    ++index;
                    if (index >= text.length) {
                        globalIndex = index;
                        break;
                    }
                }

                if (globalIndex < text.length)
                    wc++;
                else 
                    throw new EOF;
            }
        } catch(e) {
            assert(e instanceof EOF);
        }
    }
    return wc;
};

for (let i = 0; i < 20; ++i)
    processTexts(texts);

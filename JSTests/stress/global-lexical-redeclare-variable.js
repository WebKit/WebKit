let globalLet = "let";
function globalFunction() { }
class globalClass { }
const globalConst = 20;
var globalVar = 21;
this.globalProperty = 22;

let sentinel = "__s__";

function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}

function assertExpectations() {
    assert(sentinel === "__s__");
}


let errorCount = 0;
function assertProperError(e) {
    if (e instanceof SyntaxError && e.message.indexOf("Can't create duplicate variable") !== -1) {
        errorCount++;
    } else {
        assert(false);
    }

}

assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/first.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/second.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/third.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/fourth.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/fifth.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

// Checking if the implementation is following
// ES6 spec 8.1.1.4.14 http://www.ecma-international.org/ecma-262/6.0/index.html#sec-hasrestrictedglobalproperty

try {
    sentinel = "bad";
    assert(Object.getOwnPropertyDescriptor(this, "globalProperty").configurable);
    load("./multiple-files-tests/global-lexical-redeclare-variable/sixth.js", "caller relative");
} catch(e) {
    assert(false);
}
assertExpectations();

try {
    sentinel = "bad";
    assert(Object.getOwnPropertyDescriptor(this, "Array").configurable);
    load("./multiple-files-tests/global-lexical-redeclare-variable/seventh.js", "caller relative");
} catch(e) {
    assert(false);
}
assertExpectations();

try {
    sentinel = "bad";
    Object.defineProperty(this, 'foo', {value: 5, configurable: true, writable: true});
    load("./multiple-files-tests/global-lexical-redeclare-variable/eighth.js", "caller relative");
} catch(e) {
    assert(false);
}
assertExpectations();

try {
    Object.defineProperty(this, 'bar', {value: 5, configurable: false, writable: true});
    load("./multiple-files-tests/global-lexical-redeclare-variable/ninth.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

assert(errorCount === 6);

try {
    Object.defineProperty(this, 'zoo', {value: undefined, configurable: false, writable: true});
    load("./multiple-files-tests/global-lexical-redeclare-variable/tenth.js", "caller relative");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

assert(errorCount === 7);

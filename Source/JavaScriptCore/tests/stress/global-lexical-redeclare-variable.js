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
    load("./multiple-files-tests/global-lexical-redeclare-variable/first.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/second.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/third.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/fourth.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/fifth.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

try {
    load("./multiple-files-tests/global-lexical-redeclare-variable/sixth.js");
} catch(e) {
    assertProperError(e);
}
assertExpectations();

assert(errorCount === 6);

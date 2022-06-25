"use strict";

function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

const NUM_LOOPS = 100;

;(function () {

function foo() {
    let x = 20;
    let y = "y";
    try {
        assert(x === 20);
        assert(y === "y");
        throw "error";
    } catch(e) {
        assert(x === 20);
    } finally {
        assert(x === 20);
        assert(y === "y");
    }

    for (let i = 0; i < 1; i++) {
        let numFinally = 0;
        try {
            let a = 40;
            let capA = function() { return a; }
            assert(capA() === 40);
            try {
                let b = 41;
                let capB = function() { return b; }
                assert(capB() === 41);
                assert(capA() === 40);
                try {
                    return 20;
                } catch(e){
                } finally {
                    let c = 42;
                    let capC = function() { return c; }
                    assert(capC() === 42);
                    assert(capB() === 41);
                    assert(capA() === 40);
                    if (i === 0) {
                        numFinally++;
                    }
                    return 22;
                }
            } catch(e) {
            } finally {
                if (i === 0) {
                    numFinally++;
                }
                return 23;
            }
        } catch(e) {
        } finally {
            if (i === 0) {
                numFinally++;
            }
            assert(numFinally === 3);
            return 24;
        }
    }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo() === 24);
}

})();


;(function () {
function foo() {
    for (let i = 0; i < 1; i++) {
        let numFinally = 0;
        let numErrors = 0;
        try {
            let a = 40;
            let capA = function() { return a; }
            assert(capA() === 40);
            try {
                let b = 41;
                let capB = function() { return b; }
                assert(capB() === 41);
                assert(capA() === 40);
                try {
                    throw "e";
                } catch(e) {
                    assert(i === 0);
                    assert(capB() === 41);
                    assert(capA() === 40);
                    numErrors++;
                    throw e;
                } finally {
                    let c = 42;
                    let capC = function() { return c; }
                    let local = "local";
                    assert(local === "local");
                    assert(capC() === 42);
                    assert(capB() === 41);
                    assert(capA() === 40);
                    if (i === 0) {
                        numFinally++;
                    }
                }
            } catch(e) {
                assert(i === 0);
                assert(capA() === 40);
                numErrors++;
                let local = "local";
                assert(local === "local");
            } finally {
                assert(capA() === 40);
                if (i === 0) {
                    numFinally++;
                }
                let local = "local";
                assert(local === "local");
                return 23;
            }    
        } catch(e) {
            //assert(i === 0);
        } finally {
            if (i === 0) {
                numFinally++;
            }

            assert(numFinally === 3);
            assert(numErrors === 2);
            return 24;
        }
    }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo() === 24);
}

})();


var d = 100;
;(function (){
function foo() {
    assert(d === 100);
    for (let i = 0; i < 1; i++) {
        let numFinally = 0;
        let numErrors = 0;
        let c = 44;
        assert(d === 100);
        try {
            let d = 45;
            if (truth()) {
                let a = 20;
                let capA = function() { return a; }
                assert(capA() === 20);
                if (truth()) {
                    let b = 21;
                    let e = 48;
                    let capB = function() { return b; }
                    assert(capB() === 21);
                    assert(d === 45);
                    try {
                        throw "e";
                    } catch(e) {
                        assert(capA() === 20);
                        assert(a === 20);
                        numErrors++;
                    } finally {
                        assert(capA() === 20);
                        assert(e === 48);
                        numFinally++;
                        return 30;
                    }
                } 
            }
        } finally {
            assert(c === 44);
            assert(d === 100);
            numFinally++;
            assert(numFinally === 2);
            assert(numErrors === 1);
            return 40;
        }
    }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo() === 40);
}

})();

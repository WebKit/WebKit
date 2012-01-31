description(
'This test checks stack trace corectness in special cases.'
);

function printStack(stackTrace) {
    debug("--> Stack Trace:")
    var length = Math.min(stackTrace.length, 100);
    for (var i = 0; i < length; i++)
        debug("    " + i + "   " + stackTrace[i].substring(0, stackTrace[i].indexOf('file:')) + stackTrace[i].substring(stackTrace[i].lastIndexOf('/')+1));

    debug('');
}

function hostThrower() { Element.prototype.appendChild.call({ }, [{ }]);  }
function callbacker(f) { [0].map(f); }
function outer(errorName) { inner(errorName); }
function inner(errorName) { throw new Error("Error in " + errorName); }
function evaler(code) { eval(code); }
function normalOuter() { normalInner(); }
function normalInner() { if(thisVarDoesntExist) failIfTrue("shouldFailBeforeThis") };
function scripterInner() { htmlInner(); }
function scripterOuter() { htmlOuter(); }
                                                                       // Expected functions in stack trace
// Normal Case
try { normalOuter() } catch (e) { printStack(e.stack) }                     // normalOuter -> normalInner

// Eval Case
try { evaler("inner('inner eval');"); } catch (e) { printStack(e.stack) }   // evaler -> eval -> inner
try { evaler("outer('outer eval');"); } catch (e) { printStack(e.stack) }   // evaler -> eval -> outer -> inner

// Function Callback Case
try { callbacker(inner('inner map')); } catch (e) { printStack(e.stack); }   // callbacker -> map -> inner
try { callbacker(outer('outer map')); } catch (e) { printStack(e.stack); }   // callbacker -> map -> outer -> inner

// Host Code Case
try { hostThrower(); } catch (e) { printStack(e.stack); }                    // hostThrower

try { scripterInner(); } catch (e) { printStack(e.stack) }                   // program -> scripter -> inner
try { scripterOuter(); } catch (e) { printStack(e.stack) }                   // program -> scripter -> outer -> inner

function selfRecursive1() {
    selfRecursive1();
}

try { selfRecursive1(); } catch (e) { printStack(e.stack) }                   // selfRecursive1 -> selfRecursive1 -> selfRecursive1 -> selfRecursive1 ...

function selfRecursive2() {
    // A little work to make the DFG kick in
    for (var i = 0; i < 10; i++) {
        if (i == 9)
            selfRecursive2(); 
    }
}

try { selfRecursive2(); } catch (e) { printStack(e.stack) }                   // selfRecursive2 -> selfRecursive2 -> selfRecursive2 -> selfRecursive2 ...

function selfRecursive3() {
    eval("selfRecursive3()");
}

try { selfRecursive3(); } catch (e) { printStack(e.stack) }                   // selfRecursive3 -> eval -> selfRecursive3 -> eval ...


successfullyParsed = true;

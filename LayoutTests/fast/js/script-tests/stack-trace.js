description(
'This test checks stack trace corectness in special cases.'
);

function printStack(stackTrace) {
	debug("--> Stack Trace:")
	var i = 0;
	for (var level in stackTrace) {
		debug("    " + i + "   " + stackTrace[level].substring(0, stackTrace[level].indexOf('file:')) + stackTrace[level].substring(stackTrace[level].lastIndexOf('/')+1));
		i++;
	}
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
try { normalOuter() } catch (e) { printStack(e.jscStack) }                     // normalOuter -> normalInner

// Eval Case
try { evaler("inner('inner eval');"); } catch (e) { printStack(e.jscStack) }   // evaler -> eval -> inner
try { evaler("outer('outer eval');"); } catch (e) { printStack(e.jscStack) }   // evaler -> eval -> outer -> inner

// Function Callback Case
try { callbacker(inner('inner map')); } catch (e) { printStack(e.jscStack); }   // callbacker -> map -> inner
try { callbacker(outer('outer map')); } catch (e) { printStack(e.jscStack); }   // callbacker -> map -> outer -> inner

// Host Code Case
try { hostThrower(); } catch (e) { printStack(e.jscStack); }                    // hostThrower

try { scripterInner(); } catch (e) { printStack(e.jscStack) }                   // program -> scripter -> inner
try { scripterOuter(); } catch (e) { printStack(e.jscStack) }                   // program -> scripter -> outer -> inner

successfullyParsed = true;

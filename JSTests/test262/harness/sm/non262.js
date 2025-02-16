/*---
defines: [printBugNumber, inSection, printStatus, writeHeaderToLog,
  assertThrownErrorContains, assertThrowsInstanceOfWithMessageCheck, newGlobal, print, assertEq, reportCompare, reportMatch, createIsHTMLDDA, createExternalArrayBuffer,
  enableGeckoProfilingWithSlowAssertions, enableGeckoProfiling, disableGeckoProfiling]
---*/

function printBugNumber() {}
function inSection() {}
function printStatus() {}
function writeHeaderToLog() {}

function assertThrownErrorContains(f, substr) {
  try {
    f();
  } catch (exc) {
    // Do not test error messages
    return;
  }
  throw new Test262Error("Expected error containing " + substr + ", no exception thrown");
}

function assertThrowsInstanceOfWithMessageCheck(f, ctor, _check, msg) {
  var fullmsg;
  try {
    f();
  } catch (exc) {
    if (exc instanceof ctor)
      return;
    fullmsg = `Assertion failed: expected exception ${ctor.name}, got ${exc}`;
  }

  if (fullmsg === undefined)
    fullmsg = `Assertion failed: expected exception ${ctor.name}, no exception thrown`;
  if (msg !== undefined)
    fullmsg += " - " + msg;

  throw new Error(fullmsg);
}

globalThis.createNewGlobal = function() {
  return $262.createRealm().global
}

function print(...args) {
}
function assertEq(...args) {
  assert.sameValue(...args)
}
function reportCompare(...args) {
  assert.sameValue(...args)
}

function reportMatch(expectedRegExp, actual, description = "") {
  assert.sameValue(typeof actual, "string",
    `Type mismatch, expected string, actual type ${typeof actual}`);
  assert.notSameValue(expectedRegExp.exec(actual), null,
    `Expected match to '${expectedRegExp}', Actual value '${actual}'`);
}

if (globalThis.createIsHTMLDDA === undefined) {
  globalThis.createIsHTMLDDA = function createIsHTMLDDA() {
    return $262.IsHTMLDDA;
  }
}

if (globalThis.createExternalArrayBuffer === undefined) {
  globalThis.createExternalArrayBuffer = function createExternalArrayBuffer(size) {
    return new ArrayBuffer(size);
  }
}
if (globalThis.enableGeckoProfilingWithSlowAssertions === undefined) {
  globalThis.enableGeckoProfilingWithSlowAssertions = globalThis.enableGeckoProfiling =
    globalThis.disableGeckoProfiling = () => {}
}
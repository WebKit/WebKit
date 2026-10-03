function z() { return 1; }
let originalObject = {a() { return 1; }};
let otherObject = {a() { return 1; }};
let unrelatedObject = {originalFunctionName() { return 1; }};
let duplicateObject1 = {d() { return 1; }};
let duplicateObject2 = {e() { return 1; }};
let memberObject = {};
memberObject.f = function() { return 1; };
let accessorObject = {get accessorOriginalFunctionName() { return 1; }};
globalThis.accessorActionCount = 0;
function callZeroOffsetFunction() { z(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-ZeroOffset"); }
function callOriginalFunction() { originalObject.a(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Original"); }
function callOtherFunction() { otherObject.a(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Other"); }
function callUnrelatedFunction() { unrelatedObject.originalFunctionName(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Unrelated"); }
function callDuplicateFunction1() { duplicateObject1.d(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Duplicate1"); }
function callDuplicateFunction2() { duplicateObject2.e(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Duplicate2"); }
function callMemberFunction() { memberObject.f(); TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Member"); }
function callAccessorFunction() { void accessorObject.accessorOriginalFunctionName; TestPage.dispatchEventToFrontend("TestPage-SymbolicBreakpoint-Accessor"); }
//# sourceMappingURL=source-mapped-symbolic-breakpoint.js.map

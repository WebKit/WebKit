description(
"This file tests whether the 'debugger' statement throws a syntax error. (IE respects 'debugger' as a statement that behaves like a breakpoint)."
);

debugger
debugger;
var successfullyParsed = true;

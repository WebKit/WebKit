//@ requireOptions("--exception=SyntaxError") && runDefault
"use strict";

function foo() { (b=b) = b; }

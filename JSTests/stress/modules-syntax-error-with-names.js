function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function checkModuleSyntaxError(source, errorMessage) {
    shouldThrow(() => checkModuleSyntax(source), errorMessage);
}

// --------------- import -------------------

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
const A = 20;
`, `SyntaxError: Cannot declare a const variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
const A = 20;
import A from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
let A = 20;
`, `SyntaxError: Cannot declare a let variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
let A = 20;
import A from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import * as A from "Cocoa"
const A = 20;
`, `SyntaxError: Cannot declare a const variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
const A = 20;
import * as A from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import * as A from "Cocoa"
let A = 20;
`, `SyntaxError: Cannot declare a let variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
let A = 20;
import * as A from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import { A } from "Cocoa"
const A = 20;
`, `SyntaxError: Cannot declare a const variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
const A = 20;
import { A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import { A } from "Cocoa"
let A = 20;
`, `SyntaxError: Cannot declare a let variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
let A = 20;
import { A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import { B as A } from "Cocoa"
const A = 20;
`, `SyntaxError: Cannot declare a const variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
const A = 20;
import { B as A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import { B as A } from "Cocoa"
let A = 20;
`, `SyntaxError: Cannot declare a let variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
let A = 20;
import { B as A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:3`);

checkModuleSyntax(String.raw`
import { A as B } from "Cocoa"
const A = 20;
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntax(String.raw`
const A = 20;
import { A as B } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntax(String.raw`
import { A as B } from "Cocoa"
let A = 20;
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntax(String.raw`
let A = 20;
import { A as B } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import { A, B as A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import { A, A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import { C as A, B as A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import A, { A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import A, { B as A } from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import A, * as A from "Cocoa"
`, `SyntaxError: Cannot declare an imported binding name twice: 'A'.:2`);

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
const { A } = obj;
`, `SyntaxError: Unexpected token '}'. Cannot declare a lexical variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
const [ A ] = array;
`, `SyntaxError: Unexpected identifier 'A'. Cannot declare a lexical variable twice: 'A'.:3`);

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
const { B:A } = obj;
`, `SyntaxError: Unexpected identifier 'A'. Cannot declare a lexical variable twice: 'A'.:3`);

checkModuleSyntax(String.raw`
import A from "Cocoa"
const { A:B } = obj;
`);

// --------------- export -------------------

checkModuleSyntaxError(String.raw`
export { A as B } from 'mod'
export B from 'mod2'
`, `SyntaxError: Unexpected identifier 'B'. Expected either a declaration or a variable statement.:3`);

checkModuleSyntax(String.raw`
export { A as B } from 'mod'
const A = 42;
const B = 'Cocoa';
`);

checkModuleSyntaxError(String.raw`
export { A as B }
const B = 'Cocoa';
`, `SyntaxError: Exported binding 'A' needs to refer to a top-level declared variable.:4`);


checkModuleSyntaxError(String.raw`
export default 20;
export default function hello () { }
`, `SyntaxError: Only one 'default' export is allowed.:4`);

checkModuleSyntaxError(String.raw`
export default function hello () { }
export default 20;
`, `SyntaxError: Only one 'default' export is allowed.:4`);

checkModuleSyntaxError(String.raw`
var a = 42;
var b = 55;
export { a as Cocoa, b as Cocoa };
`, `SyntaxError: Cannot export a duplicate name 'Cocoa'.:4`);

checkModuleSyntaxError(String.raw`
var a = 42;
var b = 55;
export { a as Cocoa, b as Cocoa };
`, `SyntaxError: Cannot export a duplicate name 'Cocoa'.:4`);

checkModuleSyntaxError(String.raw`
var Cocoa = 42;
var b = 55;
export { Cocoa, b as Cocoa };
`, `SyntaxError: Cannot export a duplicate name 'Cocoa'.:4`);

checkModuleSyntaxError(String.raw`
export var Cocoa = 42;
var b = 55;
export { b as Cocoa };
`, `SyntaxError: Cannot export a duplicate name 'Cocoa'.:4`);

checkModuleSyntaxError(String.raw`
var a = 42;
export { a as Cocoa };
export function Cocoa() { }
`, `SyntaxError: Cannot export a duplicate function name: 'Cocoa'.:5`);

checkModuleSyntaxError(String.raw`
var a = 42;
export { a as Cocoa };
export class Cocoa { }
`, `SyntaxError: Cannot export a duplicate class name: 'Cocoa'.:5`);

// FIXME: These tests also should be passed. But now, var and lexical declared names can be co-exist on Script / Module top level scope.
// This will be fixed when this issue is fixed for Script environment.
// http://www.ecma-international.org/ecma-262/6.0/#sec-scripts-static-semantics-early-errors
// http://www.ecma-international.org/ecma-262/6.0/#sec-module-semantics-static-semantics-early-errors
checkModuleSyntaxError(String.raw`
import A from "Cocoa"
var A = 20;
`, `SyntaxError: Cannot declare a var variable that shadows a let/const/class variable: 'A'.:3`);

// checkModuleSyntaxError(String.raw`
// var A = 20;
// import A from "Cocoa"
// `, ``);

checkModuleSyntaxError(String.raw`
import A from "Cocoa"
var A = 20;
`, `SyntaxError: Cannot declare a var variable that shadows a let/const/class variable: 'A'.:3`);

// checkModuleSyntaxError(String.raw`
// var A = 20;
// import A from "Cocoa"
// `, ``);



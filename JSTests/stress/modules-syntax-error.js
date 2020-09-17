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
import {,} from "Cocoa"
`, `SyntaxError: Unexpected token ','. Expected an imported name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import * from "Cocoa"
`, `SyntaxError: Unexpected identifier 'from'. Expected 'as' before imported binding name.:2`);

checkModuleSyntaxError(String.raw`
import * from "Cocoa"
`, `SyntaxError: Unexpected identifier 'from'. Expected 'as' before imported binding name.:2`);

checkModuleSyntaxError(String.raw`
import * of name from "Cocoa"
`, `SyntaxError: Unexpected identifier 'of'. Expected 'as' before imported binding name.:2`);

checkModuleSyntaxError(String.raw`
import * as name fro "Cocoa"
`, `SyntaxError: Unexpected identifier 'fro'. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import * as name fro "Cocoa"
`, `SyntaxError: Unexpected identifier 'fro'. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import d, { e, f, g as c }, c from "Cappuccino"
`, `SyntaxError: Unexpected token ','. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import d, c from "Cappuccino"
`, `SyntaxError: Unexpected identifier 'c'. Expected namespace import or import list.:2`);

checkModuleSyntaxError(String.raw`
import i, * as j, * as k from "Cappuccino"
`, `SyntaxError: Unexpected token ','. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import * as a, b from "Cappuccino"
`, `SyntaxError: Unexpected token ','. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import { a, b, c as d }, e from "Cappuccino"
`, `SyntaxError: Unexpected token ','. Expected 'from' before imported module name.:2`);

checkModuleSyntaxError(String.raw`
import a
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import a from
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import { a }
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import {} from
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import *
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import * as
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import * from
`, `SyntaxError: Unexpected identifier 'from'. Expected 'as' before imported binding name.:2`);

checkModuleSyntaxError(String.raw`
import * as from from
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import * as from from d
`, `SyntaxError: Unexpected identifier 'd'. Imported modules names must be string literals.:2`);

checkModuleSyntaxError(String.raw`
import * as from from 20
`, `SyntaxError: Unexpected number '20'. Imported modules names must be string literals.:2`);

checkModuleSyntaxError(String.raw`
function noTopLevel() {
    import * as from from "Cocoa"
}
`, `SyntaxError: Unexpected token '*'. import call expects exactly one argument.:3`);

checkModuleSyntaxError(String.raw`
if (noTopLevel) {
    import * as from from "Cocoa"
}
`, `SyntaxError: Unexpected token '*'. import call expects exactly one argument.:3`);

checkModuleSyntaxError(String.raw`
{
    import * as from from "Cocoa"
}
`, `SyntaxError: Unexpected token '*'. import call expects exactly one argument.:3`);

checkModuleSyntaxError(String.raw`
for (var i = 0; i < 1000; ++i) {
    import * as from from "Cocoa"
}
`, `SyntaxError: Unexpected token '*'. import call expects exactly one argument.:3`);

checkModuleSyntaxError(String.raw`
import for from "Cocoa";
`, `SyntaxError: Unexpected keyword 'for'. Expected namespace import or import list.:2`);

checkModuleSyntaxError(String.raw`
import enum from "Cocoa";
`, `SyntaxError: Unexpected use of reserved word 'enum'. Expected namespace import or import list.:2`);

checkModuleSyntaxError(String.raw`
import * as for from "Cocoa";
`, `SyntaxError: Unexpected keyword 'for'. Expected a variable name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import * as enum from "Cocoa";
`, `SyntaxError: Unexpected use of reserved word 'enum'. Expected a variable name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import { module as default } from "Cocoa"
`, `SyntaxError: Unexpected keyword 'default'. Expected a variable name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import { module as enum } from "Cocoa"
`, `SyntaxError: Unexpected use of reserved word 'enum'. Expected a variable name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import { for } from "Cocoa"
`, `SyntaxError: Cannot use keyword as imported binding name.:2`);


checkModuleSyntaxError(String.raw`
import a, { [assign] as c } from "Cocoa"
`, `SyntaxError: Unexpected token '['. Expected an imported name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import d, { g as {obj} } from "Cappuccino"
`, `SyntaxError: Unexpected token '{'. Expected a variable name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import d, { {obj} } from "Cappuccino"
`, `SyntaxError: Unexpected token '{'. Expected an imported name for the import declaration.:2`);

checkModuleSyntaxError(String.raw`
import { binding
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import { hello, binding as
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
import { hello, binding as
`, `SyntaxError: Unexpected end of script:3`);

// --------------- export -------------------

checkModuleSyntaxError(String.raw`
export { , } from "Cocoa"
`, `SyntaxError: Unexpected token ','. Expected a variable name for the export declaration.:2`);

checkModuleSyntaxError(String.raw`
export { a, , } from "Cocoa"
`, `SyntaxError: Unexpected token ','. Expected a variable name for the export declaration.:2`);

checkModuleSyntaxError(String.raw`
export a from "Cocoa"
`, `SyntaxError: Unexpected identifier 'a'. Expected either a declaration or a variable statement.:2`);

checkModuleSyntaxError(String.raw`
export a
`, `SyntaxError: Unexpected identifier 'a'. Expected either a declaration or a variable statement.:2`);

checkModuleSyntaxError(String.raw`
export { * as b } from "Cocoa"
`, `SyntaxError: Unexpected token '*'. Expected a variable name for the export declaration.:2`);

checkModuleSyntaxError(String.raw`
export * as b
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
export * as 42 from "Cocoa"
`, `SyntaxError: Unexpected number '42'. Expected an exported name for the export declaration.:2`);

checkModuleSyntaxError(String.raw`
export const b = 42;
export * as b from "mod"
`, `SyntaxError: Cannot export a duplicate name 'b'.:4`);

checkModuleSyntaxError(String.raw`
export * "Cocoa"
`, `SyntaxError: Unexpected string literal "Cocoa". Expected 'from' before exported module name.:2`);

checkModuleSyntaxError(String.raw`
export *
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
export const a;
`, `SyntaxError: Unexpected token ';'. const declared variable 'a' must have an initializer.:2`);

checkModuleSyntaxError(String.raw`
export const a = 20, b;
`, `SyntaxError: Unexpected token ';'. const declared variable 'b' must have an initializer.:2`);

checkModuleSyntaxError(String.raw`
export default 20, 30, 40;
`, `SyntaxError: Unexpected token ','. Expected a ';' following a targeted export declaration.:2`);

checkModuleSyntaxError(String.raw`
export function () { }
`, `SyntaxError: Function statements must have a name.:2`);

checkModuleSyntaxError(String.raw`
export class { }
`, `SyntaxError: Class statements must have a name.:2`);

checkModuleSyntaxError(String.raw`
export class extends Drink {
}
`, `SyntaxError: Cannot use the keyword 'extends' as a class name.:2`);

checkModuleSyntaxError(String.raw`
export default 20 30
`, `SyntaxError: Unexpected number '30'. Expected a ';' following a targeted export declaration.:2`);

checkModuleSyntaxError(String.raw`
export default 20 + 30, 40;
`, `SyntaxError: Unexpected token ','. Expected a ';' following a targeted export declaration.:2`);

checkModuleSyntaxError(String.raw`
export { default as default }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { default }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { default as binding }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { hello, default as binding }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { implements }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { static }
`, `SyntaxError: Cannot use keyword as exported variable name.:3`);

checkModuleSyntaxError(String.raw`
export { binding
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
export { hello, binding as
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
export { hello, binding as
`, `SyntaxError: Unexpected end of script:3`);

checkModuleSyntaxError(String.raw`
function noTopLevel() {
    export * from "Cocoa"
}
`, `SyntaxError: Unexpected keyword 'export':3`);

checkModuleSyntaxError(String.raw`
if (noTopLevel) {
    export * from "Cocoa"
}
`, `SyntaxError: Unexpected keyword 'export':3`);

checkModuleSyntaxError(String.raw`
{
    export * from "Cocoa"
}
`, `SyntaxError: Unexpected keyword 'export':3`);

checkModuleSyntaxError(String.raw`
for (var i = 0; i < 1000; ++i) {
    export * from "Cocoa"
}
`, `SyntaxError: Unexpected keyword 'export':3`);

// --------------- other ---------------------

checkModuleSyntaxError(String.raw`
new.target;
`, `SyntaxError: new.target is only valid inside functions.:2`);

checkModuleSyntaxError(String.raw`
super();
`, `SyntaxError: super is not valid in this context.:2`);

checkModuleSyntaxError(String.raw`
super.test();
`, `SyntaxError: super is not valid in this context.:2`);

checkModuleSyntaxError(String.raw`
super.test = 20;
`, `SyntaxError: super is not valid in this context.:2`);

checkModuleSyntaxError(String.raw`
new import()
`, `SyntaxError: Cannot use new with import.:2`);

checkModuleSyntaxError(String.raw`
new import
`, `SyntaxError: Cannot use new with import.:3`);

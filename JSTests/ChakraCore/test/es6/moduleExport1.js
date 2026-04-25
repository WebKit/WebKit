//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function foo() { }
class bar { }
function* baz() { }
function foobar() { }

// Export function expressions
export function fn1 () { };
export function fn2 () { }

// Export generator expressions
export function* gn1 () { };
export function* gn2 () { }

// Export class expressions
export class cl1 { };
export class cl2 { }

// Export let decls
export let let1;
export let let2 = 2;
export let let3, let4, let5;
export let let6 = { }
export let let7 = [ ]

// Export const decls
export const const2 = 'str';
export const const3 = 3, const4 = 4;
export const const5 = { }
export const const6 = [ ]

// Export with export clauses
export {};
export { foo };
export { bar, };
export { foo as foo2, baz }
export { foo as foo3, baz as baz2, }
export { foo as foo4, bar as bar2, foobar }

// Export var decls
export var var1 = 'string';
export var var2;
export var var3 = 5, var4
export var var5, var6, var7

export default 'default';
export function changeContext()
{
    foo = 20;
    var1 = 'new string';
    var2 = 'changed';
    var tmp = fn1;
    fn1 = fn2;
    fn2 = fn1;
}

export function verifyNamespace(ns)
{
    var unused = 1;
    for (var i in ns) helpers.writeln(i + " = " + ns[i]);
    assert.areEqual(ns.var7, var7, "var7 is the same");
    assert.areEqual(ns.var6, var6, "var6 is the same");
    assert.areEqual(ns.var5, var5, "var5 is the same");
    assert.areEqual(ns.var4, var4, "var4 is the same");
    assert.areEqual(ns.var3, var3, "var3 is the same");
    assert.areEqual(ns.var2, var2, "var2 is the same");
    assert.areEqual(ns.var1, var1, "var1 is the same");
    assert.areEqual(ns.foobar, foobar, "foobar is the same");
    assert.areEqual(ns.foo4, foo, "foo4 is the same");
    assert.areEqual(ns.baz2, baz, "baz2 is the same");
    assert.areEqual(ns.foo3, foo, "foo3 is the same");
    assert.areEqual(ns.bar2, bar, "bar2 is the same");
    assert.areEqual(ns.baz, baz, "baz is the same");
    assert.areEqual(ns.foo2, foo, "foo2 is the same");
    assert.areEqual(ns.foo, foo, "foo is the same");
    assert.areEqual(ns.bar, bar, "bar is the same");
    assert.areEqual(ns.const6, const6, "const6 is the same");
    assert.areEqual(ns.const5, const5, "const5 is the same");
    assert.areEqual(ns.const4, const4, "const4 is the same");
    assert.areEqual(ns.const3, const3, "const3 is the same");
    assert.areEqual(ns.const2, const2, "const2 is the same");
    assert.areEqual(ns.let7, let7, "let7 is the same");
    assert.areEqual(ns.let6, let6, "let6 is the same");
    assert.areEqual(ns.let5, let5, "let5 is the same");
    assert.areEqual(ns.let4, let4, "let4 is the same");
    assert.areEqual(ns.let3, let3, "let3 is the same");
    assert.areEqual(ns.let2, let2, "let2 is the same");
    assert.areEqual(ns.let1, let1, "let1 is the same");
    assert.areEqual(ns.cl2, cl2, "cl2 is the same");
    assert.areEqual(ns.cl1, cl1, "cl1 is the same");
    assert.areEqual(ns.gn2, gn2, "gn2 is the same");
    assert.areEqual(ns.gn1, gn1, "gn1 is the same");
    assert.areEqual(ns.fn2, fn2, "fn2 is the same");
    assert.areEqual(ns.fn1, fn1, "fn1 is the same");
}

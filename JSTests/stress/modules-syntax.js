checkModuleSyntax(String.raw`
import "Cocoa";
`);

// Examples in 15.2.1.16
// http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
checkModuleSyntax(String.raw`
import v from "mod";
`);

checkModuleSyntax(String.raw`
import * as ns from "mod";
`);

checkModuleSyntax(String.raw`
import {x} from "mod";
`);

checkModuleSyntax(String.raw`
import {x,} from "mod";
`);

checkModuleSyntax(String.raw`
import {} from "mod";
`);

checkModuleSyntax(String.raw`
import {x as v} from "mod";
`);

checkModuleSyntax(String.raw`
import "mod";
`);

checkModuleSyntax(String.raw`
export var v;
`);

checkModuleSyntax(String.raw`
export default function f(){};
`);

checkModuleSyntax(String.raw`
export default function(){};
`);

checkModuleSyntax(String.raw`
export default 42;
`);

checkModuleSyntax(String.raw`
const x = 20;
export {x};
`);

checkModuleSyntax(String.raw`
const v = 20;
export {v as x};
`);

checkModuleSyntax(String.raw`
export {x} from "mod";
`);

checkModuleSyntax(String.raw`
export {v as x} from "mod";
`);

checkModuleSyntax(String.raw`
export * from "mod";
`);

checkModuleSyntax(String.raw`
export * as b from "Cocoa"
`);

checkModuleSyntax(String.raw`
export * as delete from "Cocoa"
`);

// semicolon is not needed.
checkModuleSyntax(String.raw`
export default function () { } 40;
`);

checkModuleSyntax(String.raw`
export default class { } 40;
`);

checkModuleSyntax(String.raw`
export default function Cappuccino() { } 40
`);

checkModuleSyntax(String.raw`
export default class Cappuccino { } 40
`);

checkModuleSyntax(String.raw`
import a, { b as c } from "Cocoa"
import d, { e, f, g as h } from "Cappuccino"
import { } from "Cappuccino"
import i, * as j from "Cappuccino"
`);

checkModuleSyntax(String.raw`
import a, { } from "Cappuccino"
`);

checkModuleSyntax(String.raw`
import a, { b, } from "Cappuccino"
`);

checkModuleSyntax(String.raw`
import * as from from "Matcha"
import * as as from "Cocoa"
`);

checkModuleSyntax(String.raw`
import { default as module } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export * from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { a } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { a as b } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { a, b } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { a, c as d, b }
let a, c, b;
`);

checkModuleSyntax(String.raw`
export { }
`);

checkModuleSyntax(String.raw`
export { a }
let a;
`);

checkModuleSyntax(String.raw`
export { a, }
let a;
`);

checkModuleSyntax(String.raw`
var a = 20;
export { a as b }
`);

checkModuleSyntax(String.raw`
export { a, b }
var a, b = 40;
`);

checkModuleSyntax(String.raw`
export { a, c as d, b }
let a, c, b;
`);

checkModuleSyntax(String.raw`
export var a;
`);

checkModuleSyntax(String.raw`
export var a, b, c = 20;
`);

checkModuleSyntax(String.raw`
export var a, { b, c } = obj, d = 20;
`);

checkModuleSyntax(String.raw`
export const a = 20;
`);

checkModuleSyntax(String.raw`
export const [b, ...a] = obj;
`);

checkModuleSyntax(String.raw`
export const {b, c: d} = obj;
`);

checkModuleSyntax(String.raw`
export let a;
`);

checkModuleSyntax(String.raw`
export let a = 20;
`);

checkModuleSyntax(String.raw`
export let a = 20, b = 30;
`);

checkModuleSyntax(String.raw`
export let [b, ...a] = obj;
`);

checkModuleSyntax(String.raw`
export let {b, c: d} = obj;
`);

checkModuleSyntax(String.raw`
export function Cocoa() {
}
`);

checkModuleSyntax(String.raw`
export class Cocoa {
}
`);

checkModuleSyntax(String.raw`
export class Cocoa extends Drink {
}
`);

checkModuleSyntax(String.raw`
export default function Cocoa() {
}
`);

checkModuleSyntax(String.raw`
export default function () {
}
`);

checkModuleSyntax(String.raw`
export default class Cocoa {
}
`);

checkModuleSyntax(String.raw`
export default class {
}
`);

checkModuleSyntax(String.raw`
export default class Cocoa extends Drink {
}
`);

checkModuleSyntax(String.raw`
export default class extends Drink {
}
`);

checkModuleSyntax(String.raw`
export default 20;
`);

checkModuleSyntax(String.raw`
export default "Cocoa";
`);

checkModuleSyntax(String.raw`
export default 20 + 30;
`);

checkModuleSyntax(String.raw`
export default call();
`);

checkModuleSyntax(String.raw`
export { default } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { enum } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { default as default } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { enum as enum } from "Cocoa"
`);

checkModuleSyntax(String.raw`
export { binding as default }
let binding = 20;
`);

checkModuleSyntax(String.raw`
export { binding as enum }
var binding = 40;
`);

checkModuleSyntax(String.raw`
export { binding as for }
const binding = 40;
`);

// --------------- other ---------------------

checkModuleSyntax(String.raw`
let i = 20;
`);

checkModuleSyntax(String.raw`
const i = 20;
`);

checkModuleSyntax(String.raw`
new import.meta
`);

checkModuleSyntax(String.raw`
new import.meta();
`);

checkModuleSyntax(String.raw`
new import.meta.hey();
`);

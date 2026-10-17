function shouldThrowModuleSyntaxError(source) {
    let error;
    try {
        checkModuleSyntax(source);
    } catch (e) {
        error = e;
    }
    if (!String(error).startsWith("SyntaxError"))
        throw new Error(`\`${source}\` did not throw a SyntaxError: ${String(error)}`);
}

const declarations = [
    `import x from "./m.js";`,
    `import * as x from "./m.js";`,
    `import { x } from "./m.js";`,
    `import { y as x } from "./m.js";`,
    `export let x;`,
    `export let [x] = [];`,
    `export const x = 1;`,
    `export class x { }`,
    `export function x() { }`,
    `export async function x() { }`,
    `export default class x { }`,
    `export default function x() { }`,
    `export default async function x() { }`,
];

for (const declaration of declarations) {
    for (const statement of [`var x;`, `{ var x; }`]) {
        shouldThrowModuleSyntaxError(`${statement} ${declaration}`);
        shouldThrowModuleSyntaxError(`${declaration} ${statement}`);
    }
    for (const statement of [`function f() { var x; }`, `{ let x; }`]) {
        checkModuleSyntax(`${statement} ${declaration}`);
        checkModuleSyntax(`${declaration} ${statement}`);
    }
}

const notDeclaringX = [
    `import { x as y } from "./m.js";`,
    `export { x };`,
    `export * as x from "./m.js";`,
    `export { y as x } from "./m.js";`,
    `export default x;`,
    `export default (class x { });`,
    `export default function () { let x; }`,
    `export class C { static { let x; } }`,
    `export let y = x;`,
    `export let { x: y } = {};`,
    `export let y = function x() { };`,
];

for (const declaration of notDeclaringX) {
    checkModuleSyntax(`var x; ${declaration}`);
    checkModuleSyntax(`${declaration} var x;`);
}

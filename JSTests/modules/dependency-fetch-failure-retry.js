// A module whose static dependency failed to fetch must fetch it again on the next import.
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script step 13.1

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

async function shouldReject(promise, check) {
    try {
        await promise;
    } catch (error) {
        check?.(error);
        return error;
    }
    throw new Error("expected rejection");
}

const directory = import.meta.filename.replace(/\.js$/, "-generated-");
const run = `${Date.now()}-${Math.floor(Math.random() * 1e9)}`;
let counter = 0;

function write(url, source) {
    writeFile(decodeURIComponent(url.replace(/^file:\/\//, "")), source);
}

function file(name, source) {
    const url = `${directory}${run}-${counter++}-${name}.js`;
    if (source !== undefined)
        write(url, source);
    return url;
}

// Parent -> missing dependency.
{
    const dep = file("dep");
    const parent = file("parent", `export { value } from "${dep}";`);
    await shouldReject(import(parent));
    await shouldReject(import(parent));
    write(dep, `export const value = 42;`);
    shouldBe((await import(parent)).value, 42);
}

// Diamond: root -> middle -> shared, root -> shared.
{
    const shared = file("shared");
    const middle = file("middle", `export { value as middleValue } from "${shared}";`);
    const root = file("root", `export { middleValue } from "${middle}"; export { value } from "${shared}";`);
    await shouldReject(import(root));
    await shouldReject(import(middle));
    write(shared, `export const value = "shared";`);
    const namespace = await import(root);
    shouldBe(namespace.value, "shared");
    shouldBe(namespace.middleValue, "shared");
    shouldBe((await import(middle)).middleValue, "shared");
}

// Concurrent imports of the same parent, before and after the dependency appears.
{
    const dep = file("dep");
    const parent = file("parent", `export { value } from "${dep}";`);
    const results = await Promise.allSettled([import(parent), import(parent), import(parent)]);
    for (const result of results)
        shouldBe(result.status, "rejected");
    shouldBe(results[0].reason !== results[1].reason, true);
    shouldBe(results[1].reason !== results[2].reason, true);
    write(dep, `export const value = "concurrent";`);
    const namespaces = await Promise.all([import(parent), import(parent), import(parent)]);
    for (const namespace of namespaces)
        shouldBe(namespace.value, "concurrent");
    shouldBe(namespaces[0], namespaces[1]);
}

// A second import that starts a few microtasks after the first, then a third after both fail.
for (let delay = 0; delay < 8; ++delay) {
    const dep = file("dep");
    const parent = file("parent", `export { value } from "${dep}";`);
    const first = shouldReject(import(parent));
    for (let i = 0; i < delay; ++i)
        await null;
    await shouldReject(import(parent));
    await first;
    await shouldReject(import(parent));
    write(dep, `export const value = ${delay};`);
    shouldBe((await import(parent)).value, delay);
}

// Cycle: a <-> b, b -> missing dependency.
{
    const dep = file("dep");
    const a = file("a");
    const b = file("b", `import "${a}"; export { value } from "${dep}";`);
    write(a, `import "${b}"; export { value } from "${b}";`);
    await shouldReject(import(a));
    await shouldReject(import(b));
    write(dep, `export const value = "cycle";`);
    shouldBe((await import(a)).value, "cycle");
    shouldBe((await import(b)).value, "cycle");
}

// The dependency recovers through a direct import first.
{
    const dep = file("dep");
    const parent = file("parent", `export { value } from "${dep}";`);
    await shouldReject(import(parent));
    write(dep, `export const value = "direct";`);
    shouldBe((await import(dep)).value, "direct");
    shouldBe((await import(parent)).value, "direct");
}

// A dependency's parse error is cached, even after the file is fixed.
{
    const dep = file("dep", `export const = ;`);
    const parent = file("parent", `export { value } from "${dep}";`);
    const first = await shouldReject(import(parent), error => shouldBe(error instanceof SyntaxError, true));
    write(dep, `export const value = 1;`);
    const second = await shouldReject(import(parent), error => shouldBe(error instanceof SyntaxError, true));
    shouldBe(first, second);
}

// The parent's own evaluation error is cached once its dependency loads.
{
    const dep = file("dep");
    const parent = file("parent", `import "${dep}"; globalThis.evaluations = (globalThis.evaluations ?? 0) + 1; throw new Error("parent");`);
    await shouldReject(import(parent));
    write(dep, `export {};`);
    const first = await shouldReject(import(parent), error => shouldBe(error.message, "parent"));
    const second = await shouldReject(import(parent));
    shouldBe(first, second);
    shouldBe(globalThis.evaluations, 1);
}

// The parent's own link error is still reported after its dependency loads.
{
    const dep = file("dep");
    const parent = file("parent", `import { missing } from "${dep}";`);
    await shouldReject(import(parent));
    write(dep, `export const present = 1;`);
    await shouldReject(import(parent), error => shouldBe(error instanceof SyntaxError, true));
    await shouldReject(import(parent), error => shouldBe(error instanceof SyntaxError, true));
}

export async function a() {}

async function b() {}
async function c() {}

if (Object.getPrototypeOf(a) !== Object.getPrototypeOf(c)) {
    throw new Error("Expected export declaration & export function to have same prototype");
}

if (Object.getPrototypeOf(a) !== Object.getPrototypeOf(b)) {
    throw new Error("Expected export & non-exported functions to have same prototype");
}

if (Object.getPrototypeOf(a).constructor !== Object.getPrototypeOf(c).constructor) {
    throw new Error("Expected export declaration & export function to have same constructor");
}

if (Object.getPrototypeOf(a).constructor !== Object.getPrototypeOf(b).constructor) {
    throw new Error("Expected export & non-exported functions to have same constructor");
}

if (Object.getPrototypeOf(a).constructor.name !== "AsyncFunction") {
    throw new Error("Expected AsyncFunction");
}
globalThis.print ??= console.log;

const AsyncFunction = Object.getPrototypeOf(a).constructor;

// This should not throw
new AsyncFunction("await 42")();

export { c };

globalThis.moduleUsedByAAndBInvocations++;
await new Promise(resolve => { setTimeout(resolve, 1000); });

const someConst = "someConst";
let someObject = {};
export function someFunction() { return "a" in someObject; }
export { someConst };

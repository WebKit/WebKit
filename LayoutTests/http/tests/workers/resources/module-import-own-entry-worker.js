import { helperSeesId } from "./module-import-own-entry-helper.js";

export const id = Math.random();
globalThis.evals = (globalThis.evals ?? 0) + 1;
self.onmessage = () => self.postMessage(`evals=${globalThis.evals} same=${helperSeesId() === id}`);

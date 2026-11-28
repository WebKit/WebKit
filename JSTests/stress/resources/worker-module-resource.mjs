import { describe } from "./worker-module-dependency.mjs";

await Promise.resolve();

onmessage = (event) => {
    postMessage({ text: describe(event.data), thisIsUndefined: this === undefined });
    close();
};

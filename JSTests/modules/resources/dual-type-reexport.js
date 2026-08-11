export { default as reExportedJson } from "./dual-type.js" with { type: "json" };
export * as reExportedJsonNs from "./dual-type.js" with { type: "json" };
// Requests the same specifier as a JavaScript module in the defer phase, so this
// module's [[LoadedModules]] holds "./dual-type.js" under both types.
import defer * as deferredNs from "./dual-type.js";
export const deferredTag = Object.prototype.toString.call(deferredNs);

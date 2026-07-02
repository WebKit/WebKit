import "./tla-sync-dep.js";
import "./tla-child.js";
(globalThis.deferTLAEvaluations ||= []).push("tla-parent");
export const value = 1;

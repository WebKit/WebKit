// Root has its own local that shadows a star-reachable conflict.
export const conflict = "conflict-root";
// Diamond: both mid-left and mid-right reach leaf-a, so onlyA / shared have a single binding via two paths.
export * from "./mid-left.js";
export * from "./mid-right.js";

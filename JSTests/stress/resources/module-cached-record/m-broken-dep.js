// This module parses successfully (so its registry entry gets a record), but its
// dependency does not exist, so loadRequestedModules for it fails.
import "./does-not-exist.js";
export const m = "m";

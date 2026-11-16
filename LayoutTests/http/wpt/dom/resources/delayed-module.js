// This module simulates a delayed import that might take time to load
// to test that document readyState waits for all imports to complete

console.log('delayed-module.js loading...');

export const delayedExport = "delayed-value";

console.log(`delayed-module.js loaded with value: ${delayedExport}`);
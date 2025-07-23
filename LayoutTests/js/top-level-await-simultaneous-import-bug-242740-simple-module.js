// Simple module with top-level await that reproduces the bug
// This is the exact case mentioned in comment #8 of the bug report

console.log('Simple module loading started...');

// Simulate the 1-second delay mentioned in the bug report
await new Promise(resolve => setTimeout(resolve, 1000));

console.log('Simple module setup completed');

export default 'simple-module-loaded'; 

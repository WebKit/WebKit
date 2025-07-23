// Module with top-level await that causes issues when imported simultaneously
// This simulates the setup time mentioned in the bug report

console.log('Module loading started...');

// Simulate some async setup that takes time
await new Promise(resolve => {
    setTimeout(() => {
        console.log('Module setup completed');
        resolve();
    }, 100);
});

console.log('Module loaded successfully');

export default 'module-loaded'; 
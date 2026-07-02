//@ memoryHog!

try {
    eval('1'.repeat(2**20)+'n');
} catch {}

//@ runBytecodeCacheNoAssertion

try {
    loadString('function(){}');
    throw new Error('loadString should have thrown');
} catch (err) {
    if (err.message != 'Function statements must have a name.')
        throw new Error('Unexpected exception');
}

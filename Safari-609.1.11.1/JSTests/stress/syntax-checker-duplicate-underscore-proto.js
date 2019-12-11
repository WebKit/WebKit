try {
    eval("(function() { ({ x: 1, y: 1, z: 1, __proto__: 1, __proto__: 1 }) })");
    eval("(function() { ({ x: 1, y: 1, z: 1, '__proto__': 1, '__proto__': 1 }) })");
    throw new Error('Should have thrown a SyntaxError');
} catch (error) {
    if (!(error instanceof SyntaxError) && error.message !== 'Attempted to redefine __proto__ property.')
        throw new Error(`Unexpected error: ${error.message}`);
}

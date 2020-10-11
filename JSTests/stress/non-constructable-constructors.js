function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(() => new Symbol(), `TypeError: function is not a constructor (evaluating 'new Symbol()')`);
shouldThrow(() => new Symbol(Symbol("Hey")), `TypeError: function is not a constructor (evaluating 'new Symbol(Symbol("Hey"))')`);
shouldThrow(() => new BigInt(), `TypeError: function is not a constructor (evaluating 'new BigInt()')`);
shouldThrow(() => new BigInt(0), `TypeError: function is not a constructor (evaluating 'new BigInt(0)')`);

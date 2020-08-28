function shouldThrowSyntaxError(str, message) {
    try {
        eval(str);
        throw new Error("Expected `" + str + "` to throw a SyntaxError, but did not throw.")
    } catch (e) {
        if (e.constructor !== SyntaxError)
            throw new Error("Expected `" + str + "` to throw a SyntaxError, but threw '" + e + "'");
        if (message !== void 0 && e.message !== message)
            throw new Error("Expected `" + str + "` to throw SyntaxError: '" + message + "', but threw '" + e + "'");
    }
}

shouldThrowSyntaxError("(function() { ({ x: 1, y: 1, z: 1, __proto__: 1, __proto__: 1 }) })", 'Attempted to redefine __proto__ property.');
shouldThrowSyntaxError("(function() { ({ x: 1, y: 1, __proto__: 1, '__proto__': 1, z: 1 }) })", 'Attempted to redefine __proto__ property.');
shouldThrowSyntaxError('(function() { ({ x: 1, "__proto__": 1, "__proto__": 1, y: 1, z: 1 }) })', 'Attempted to redefine __proto__ property.');
shouldThrowSyntaxError('(function() { ({ __proto__: 1, "__proto__": 1, x: 1, y: 1, z: 1 }) })', 'Attempted to redefine __proto__ property.');

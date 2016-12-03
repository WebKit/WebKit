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

shouldThrowSyntaxError("0/-async J", "SyntaxError: Cannot parse member expression.")
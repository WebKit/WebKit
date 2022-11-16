try {
    for (let x in x = new 0) { }
} catch (e) {
    if (!(e instanceof TypeError))
        throw "Expected 'TypeError: 0 is not a constructor', got '" + e + "'";
}

try {
    class x {
        [x = new 0];
    }
} catch (e) {
    if (!(e instanceof TypeError))
        throw "Expected 'TypeError: 0 is not a constructor', got '" + e + "'";
}

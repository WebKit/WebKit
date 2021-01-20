let error = null;
try {
    eval('let r = "prop" i\\u{006E} 20');
} catch(e) {
    error = e;
}

if (!error || error.message !== "Unexpected escaped characters in keyword token: 'i\\u{006E}'")
    throw new Error("Bad");

error = null;
try {
    eval('let r = "prop" i\\u006E 20');
} catch(e) {
    error = e;
}

if (!error || error.message !== "Unexpected escaped characters in keyword token: 'i\\u006E'")
    throw new Error("Bad");

// This test should not crash.
error = null;
try {
    eval('let r = "prop" i\u006E 20');
} catch(e) {
    error = e;
}

if (!error || error.message !== "20 is not an Object. (evaluating \'\"prop\" in 20\')")
    throw new Error("Bad");
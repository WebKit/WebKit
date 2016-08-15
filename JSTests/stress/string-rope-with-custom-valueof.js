// This file tests the concatenating of known strings with different objects with overridden valueOf functions.
// Note: we intentionally do not test Symbols since they cannot be appended to strings...

function catNumber(obj) {
    return "test" + "things" + obj;
}
noInline(catNumber);

number = { valueOf: function() { return 1; } };

function catBool(obj) {
    return "test" + "things" + obj;
}
noInline(catBool);

bool = { valueOf: function() { return true; } };

function catUndefined(obj) {
    return "test" + "things" + obj;
}
noInline(catUndefined);

undef = { valueOf: function() { return undefined; } };

function catRandom(obj) {
    return "test" + "things" + obj;
}
noInline(catRandom);

i = 0;
random = { valueOf: function() {
    switch (i % 3) {
    case 0:
        return number.valueOf();
    case 1:
        return bool.valueOf();
    case 2:
        return undef.valueOf();
    }
} };

for (i = 0; i < 100000; i++) {
    if (catNumber(number) !== "testthings1")
        throw "bad number";
    if (catBool(bool) !== "testthingstrue")
        throw "bad bool";
    if (catUndefined(undef) !== "testthingsundefined")
        throw "bad undefined";
    if (catRandom(random) !== "testthings" + random.valueOf())
        throw "bad random";
}

// Try passing new types to each of the other functions.
for (i = 0; i < 100000; i++) {
    if (catUndefined(number) !== "testthings1")
        throw "bad number";
    if (catNumber(bool) !== "testthingstrue")
        throw "bad bool";
    if (catBool(undef) !== "testthingsundefined")
        throw "bad undefined";
}

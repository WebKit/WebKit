function foo() { }

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo();
    if (result !== void 0)
        throw "You broke JSC so hard that even the empty function doesn't work: " + result;
}


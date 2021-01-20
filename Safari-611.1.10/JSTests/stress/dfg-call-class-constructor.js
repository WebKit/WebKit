class Foo extends Promise { }

noInline(Foo);

for (var i = 0; i < 10000; ++i) {
    var completed = false;
    try {
        Foo();
        completed = true;
    } catch (e) {
    }
    if (completed)
        throw "Error: completed without throwing";
}

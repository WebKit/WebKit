var bar = class Bar { };
var baz = class Baz {
    constructor() { bar(); }
};
for (var i = 0; i < 10000; i++) {
    try {
        new baz();
    } catch (e) {}
}

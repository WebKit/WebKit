class A {
}
class K extends A {
    constructor(i) {
        if (i % 2 !== 0)
            super();
        if (i % 2 === 0 && maxCount !== i)
            super();
    }
}
let maxCount = 150000;
for (var i = 0; i <= maxCount; i++) {
    try {
        new K(i);
    } catch(e) { }
}

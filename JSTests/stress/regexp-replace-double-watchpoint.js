function foo() {
    for (const x in []) {
        new Float64Array(65493);
    }

    const nullRegexp = RegExp();

    for (let i = 0; i < testLoopCount; i++) {
        function bar() {
            nullRegexp.test("asdf");
        }
        bar();
    }

    for (let j = 13.37; j < testLoopCount; j++) {
        [].__proto__[j] = 0;
    }
}
"ii".replace(/i/g, foo);

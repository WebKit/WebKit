description("document.all is callable (builtins)");
const documentAll = document.all;

function testArray() {
    for (var i = 0; i < 1e4; ++i) {
        Array.from([], documentAll);
        [].reduce(documentAll, null);
        [].reduceRight(documentAll, null);
        [].every(documentAll);
        [].forEach(documentAll);
        [].filter(documentAll);
        [].map(documentAll);
        [].some(documentAll);
        [].find(documentAll);
        [].findIndex(documentAll);
        [].sort(documentAll);
        [].flatMap(documentAll);
    }
    return true;
}

function testTypedArray() {
    for (var i = 0; i < 1e4; ++i) {
        Float64Array.from([], documentAll);
        (new Float32Array).every(documentAll);
        (new Int32Array).find(documentAll);
        (new Int16Array).findIndex(documentAll);
        (new Int8Array).forEach(documentAll);
        (new Uint32Array).some(documentAll);
        (new Uint16Array).sort(documentAll);
        (new Uint8Array).reduce(documentAll, null);
        (new Uint8ClampedArray).reduceRight(documentAll, null);
        (new Float64Array).map(documentAll);
        (new Float32Array).filter(documentAll);
    }
    return true;
}

function testRegExp() {
    var test = RegExp.prototype.test;
    var replace = RegExp.prototype[Symbol.replace];

    for (var i = 0; i < 1e4; ++i) {
        if (test.call({exec: documentAll}, "x") !== false)
            return false;

        if (replace.call(/./, "x", documentAll) !== "null")
            return false;
    }
    return true;
}

function testOtherBuiltins() {
    Promise.resolve = documentAll;

    for (var i = 0; i < 1e4; ++i) {
        Promise.all([]);
        Promise.allSettled([]);
        Promise.any([]);
        Promise.race([]);
        new Promise(documentAll);

        Reflect.apply(documentAll, null, []);
        Function.prototype.bind.call(documentAll);
        (new Map).forEach(documentAll);
        (new Set).forEach(documentAll);
    }
    return true;
}

shouldBeTrue("testArray()");
shouldBeTrue("testTypedArray()");
shouldBeTrue("testRegExp()");
shouldBeTrue("testOtherBuiltins()");

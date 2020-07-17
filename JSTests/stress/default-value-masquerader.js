function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var masquerader = makeMasquerader();

var test1 = (arg = 1) => arg;
noInline(test1);
for (var i = 0; i < 1e5; ++i)
    shouldBe(test1(masquerader), masquerader);

var test2 = obj => { var {key = 2} = obj; return key; };
noInline(test2);
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test2({key: masquerader}), masquerader);

    var {key = 2} = {key: masquerader};
    shouldBe(key, masquerader);
}

var test3 = arr => { var [item = 3] = arr; return item; };
noInline(test3);
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test3([masquerader]), masquerader);

    var [item = 3] = [masquerader];
    shouldBe(item, masquerader);
}

function target1() {
    return Array.from({
        length: 5,
        0: 0,
        1: 0,
        2: 0,
        3: 0,
        4: 0
    });
}
noInline(target1);

function target2() {
    return Array.from({
        length: 5.4,
        0: 0,
        1: 0,
        2: 0,
        3: 0,
        4: 0
    });
}
noInline(target2);

function target3() {
    return Array.from({
        length: -5.4,
        0: 0,
        1: 0,
        2: 0,
        3: 0,
        4: 0
    });
}
noInline(target3);

for (var i = 0; i < 10000; ++i)
    target1();
for (var i = 0; i < 10000; ++i)
    target2();
for (var i = 0; i < 10000; ++i)
    target3();

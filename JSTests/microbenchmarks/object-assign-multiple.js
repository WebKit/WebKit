function extend1(source) {
    source.checked = 22;
    source.onChanged = 44;
}

function test() {
    var target = {
        type: 42,
        step: 31,
        min: 33,
        max: 42,
    };

    var source1 = {
        className: "string",
        type: 41,
    };
    extend1(source1);

    var source2 = {
        defaultChecked: 44,
        defaultValue: 22,
        value: 33,
        checked: 44,
        onChange: 55
    };

    return Object.assign(target, source1, source2);
}


for (var i = 0; i < 1e6; ++i)
    test();

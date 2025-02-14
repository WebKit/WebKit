function test(array) {
    var result;
    for (let i = 0; i < 1e6; i++) {
        result = array.at(1000);
    }
    return result;
}

noInline(test);

var array = [
    { value: 12 }, { value: 4 }, { value: 4 }, { value: 66 }, { value: 56 },
    { value: 44 }, { value: 44 }, { value: 38 }, { value: 65 }, { value: 89 },
    { value: 2 }, { value: 45 }, { value: 789 }, { value: 56 }, { value: 432 },
    { value: 677 }, { value: 2234 }, { value: 77 }, { value: 3457 }, { value: 133 },
    { value: 6544 }, { value: 76 }, { value: 17 }, { value: 322 }, { value: 34 }
];
var result = test(array);
if (result !== undefined)
    throw "Bad result: " + JSON.stringify(result);


description(

"This test checks for potential edge case bugs with certain math transforms involving multiplication by 1 and unary plus."

);

var values = {
    someInt: 42,
    someFloat: 42.42,
    one: 1,
    minusOne: -1,
    zero: 0,
    minusZero: -0,
    infinity: Infinity,
    minusInfinity: -Infinity,
    notANumber: NaN,
    nonNumberString: "x",
    someFloatString: "42.42"
};

var numberForString = {
    nonNumberString: "notANumber",
    someFloatString: "someFloat"
};

for (var name in values) {
    var numForStr = numberForString[name] ? numberForString[name] : name;

    shouldBe("values." + name + " * 1", "+values." + name);
    shouldBe("values." + name + " * 1", stringify(values[numForStr]));

    shouldBe("1 * values." + name, "+values." + name);
    shouldBe("1 * values." + name, stringify(values[numForStr]));
}

for (var name1 in values) {
    var numForStr1 = numberForString[name1] ? numberForString[name1] : name1;
    for (var name2 in values) {
        var numForStr2 = numberForString[name2] ? numberForString[name2] : name2;

        shouldBe("+values." + name1 + " * values." + name2, "values." + name1 + " * values." + name2);
        shouldBe("+values." + name1 + " * values." + name2, stringify(values[name1] * values[name2]));
        shouldBe("values." + name1 + " * +values." + name2, "values." + name1 + " * values." + name2);
        shouldBe("values." + name1 + " * +values." + name2, stringify(values[name1] * values[name2]));
        shouldBe("+values." + name1 + " * +values." + name2, "values." + name1 + " * values." + name2);
        shouldBe("+values." + name1 + " * +values." + name2, stringify(values[name1] * values[name2]));

        shouldBe("+values." + name1 + " / values." + name2, "values." + name1 + " / values." + name2);
        shouldBe("+values." + name1 + " / values." + name2, stringify(values[name1] / values[name2]));
        shouldBe("values." + name1 + " / +values." + name2, "values." + name1 + " / values." + name2);
        shouldBe("values." + name1 + " / +values." + name2, stringify(values[name1] / values[name2]));
        shouldBe("+values." + name1 + " / +values." + name2, "values." + name1 + " / values." + name2);
        shouldBe("+values." + name1 + " / +values." + name2, stringify(values[name1] / values[name2]));

        shouldBe("+values." + name1 + " - values." + name2, "values." + name1 + " - values." + name2);
        shouldBe("+values." + name1 + " - values." + name2, stringify(values[name1] - values[name2]));
        shouldBe("values." + name1 + " - +values." + name2, "values." + name1 + " - values." + name2);
        shouldBe("values." + name1 + " - +values." + name2, stringify(values[name1] - values[name2]));
        shouldBe("+values." + name1 + " - +values." + name2, "values." + name1 + " - values." + name2);
        shouldBe("+values." + name1 + " - +values." + name2, stringify(values[name1] - values[name2]));
    }
}


var successfullyParsed = true;

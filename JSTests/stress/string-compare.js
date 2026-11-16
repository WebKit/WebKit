let typeCases = [
    "",
    "0",
    "1",
    "a",
    "aa",
]

let operators = ["<", "<=", ">", ">=", "==", "!=", "===", "!=="];

function makeRope(a)
{
    return a + a;
}
noInline(makeRope);

function makeString(a)
{
    return makeRope(a).slice(a.length);
}
noInline(makeString);

for (let operator of operators) {
    eval(`
        function compareStringIdent(a, b)
        {
            return a ${operator} b;
        }
        noInline(compareStringIdent);

        function compareStringString(a, b)
        {
            return a ${operator} b;
        }
        noInline(compareStringString);

        function compareStringIdentString(a, b)
        {
            return a ${operator} b;
        }
        noInline(compareStringIdentString);

        function compareStringStringIdent(a, b)
        {
            return a ${operator} b;
        }
        noInline(compareStringStringIdent);
    `);

    for (let left of typeCases) {
        for (let right of typeCases) {
            let expected = eval("'" + left + "'" + operator + "'" + right + "'");
            eval(`
                 for (let i = 0; i < 1e3; ++i) {
                     let stringIdentResult = compareStringIdent('${left}', '${right}');
                     if (stringIdentResult !== ${expected})
                        throw "Failed compareStringIdent('${left}', '${right}'), got " + stringIdentResult + " expected ${expected}";
                     let resolvedLeftString = makeString('${left}');
                     let resovledRightString = makeString('${right}');
                     let stringStringResult = compareStringString(resolvedLeftString, resovledRightString);
                     if (stringStringResult !== ${expected})
                        throw "Failed compareStringString('${left}', '${right}'), got " + stringStringResult + " expected ${expected}";
                     stringStringResult = compareStringString(makeString('${left}'), makeString('${right}'));
                     if (stringStringResult !== ${expected})
                        throw "Failed compareStringString('${left}', '${right}'), got " + stringStringResult + " expected ${expected}";

                     if (compareStringIdentString(makeString('${left}'), '${right}') !== ${expected})
                        throw "Failed compareStringIdentString('${left}', '${right}'), expected was ${expected}";
                     if (compareStringStringIdent('${left}', makeString('${right}')) !== ${expected})
                        throw "Failed compareStringStringIdent('${left}', '${right}'), expected was ${expected}";

                     if (('${left}' ${operator} '${right}') !== ${expected})
                        throw "Failed constant folding of ('${left}' ${operator} '${right}'). How do you even fail constant folding?";
                 }
            `)
        }
    }
}
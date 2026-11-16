import "./2.js";

export let lexical = 0;
export var hoisted = 0;

export function addLexical(value)
{
    if (value)
        lexical += value;
    return lexical;
}

export function readLexical()
{
    return lexical;
}

export function addLexicalThreeTimes()
{
    let result;
    for (let i = 0; i < 3; ++i)
        result = addLexical(1);
    return result;
}

export function addHoisted(value)
{
    if (value)
        hoisted += value;
    return hoisted;
}

export function readHoisted()
{
    return hoisted;
}

description(
"This test checks whether various seal/freeze/preventExtentions work on a regular object."
);

function obj()
{
    return { a: 1, b: 2 };
}

function test(obj)
{
    obj.c =3;
    obj.b =4;
    delete obj.a;

    var result = "";
    for (key in obj)
        result += ("(" + key + ":" + obj[key] + ")");
    if (Object.isSealed(obj))
        result += "S";
    if (Object.isFrozen(obj))
        result += "F";
    if (Object.isExtensible(obj))
        result += "E";
    return result;
}

function seal(obj)
{
    Object.seal(obj);
    return obj;
}

function freeze(obj)
{
    Object.freeze(obj);
    return obj;
}

function preventExtensions(obj)
{
    Object.preventExtensions(obj);
    return obj;
}

shouldBe('test(obj())', '"(b:4)(c:3)E"'); // extensible, can delete a, can modify b, and can add c
shouldBe('test(preventExtensions(obj()))', '"(b:4)"'); // <nothing>, can delete a, can modify b, and CANNOT add c
shouldBe('test(seal(obj()))', '"(a:1)(b:4)S"'); // sealed, CANNOT delete a, can modify b, and CANNOT add c
shouldBe('test(freeze(obj()))', '"(a:1)(b:2)SF"'); // sealed and frozen, CANNOT delete a, CANNOT modify b, and CANNOT add c

successfullyParsed = true;
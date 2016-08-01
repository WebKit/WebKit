function opaqueNewObject(prototype)
{
    return Object.create(prototype);
}
noInline(opaqueNewObject);

function putValueOnNewObject(value, prototype)
{
    var object = opaqueNewObject(prototype);
    object.myProperty = value;
    return object;
}
noInline(putValueOnNewObject)

for (var i = 0; i < 1e4; ++i) {
    var initialPrototype = new Object;
    for (var j = 0; j < 5; ++j) {
        var object = putValueOnNewObject(j, initialPrototype);
        if (object["myProperty"] !== j) {
            throw "Ooops, we mess up before the prototype change at iteration i = " + i + " j = " + j;
        }
    }

    initialPrototype.foo = "bar";
    for (var j = 0; j < 5; ++j) {
        var object = putValueOnNewObject(j, initialPrototype);
        if (object["myProperty"] !== j) {
            throw "Ooops, we mess up at iteration i = " + i + " j = " + j;
        }
    }
}
function isError(e)
{
    return Error.isError(e);
}
noInline(isError);

var inputs = [new Error(), new TypeError(), {}, undefined, null, 42, "str", []];
for (var i = 0; i < 1e6; ++i)
    isError(inputs[i & 7]);

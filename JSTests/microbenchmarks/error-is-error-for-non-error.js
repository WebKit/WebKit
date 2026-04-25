function isError(e)
{
    return Error.isError(e);
}
noInline(isError);

var obj = {};
for (var i = 0; i < 1e6; ++i)
    isError(obj);

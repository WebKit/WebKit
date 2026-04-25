function isError(e)
{
    return Error.isError(e);
}
noInline(isError);

var err = new Error();
for (var i = 0; i < 1e6; ++i)
    isError(err);

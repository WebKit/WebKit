function exceptionBasic()
{
    ({}).a.b.c.d;
}

function exceptionDOM()
{
    document.body.removeChild(document.createElement("span"));
}

function throwString()
{
    throw "exception string";
}

function throwParam(o)
{
    throw o;
}

function exceptionInHostFunction()
{
    [1].map(function(x) {
        throw "exception in host function";
    });
}

function catchNested(func, depth /* optionalArgsToFunc... */)
{
    if (depth > 1) {
        var args = Array.prototype.slice.call(arguments, 0);
        --args[1];
        catchNested.apply(this, args);
    } else {
        try {
            func.apply(this, Array.prototype.slice.call(arguments, 2));
        } catch (e) {
            console.log(`catchNested caught exception: ${stringifyError(e)}`);
        }
    }
}

function stringifyError(err)
{
    if (typeof err === "string")
        return JSON.stringify(err);

    return JSON.stringify({
        line: err.line,
        column: err.column,
        sourceURL: err.sourceURL,
    });
}

function noException()
{
    return "no exception";
}

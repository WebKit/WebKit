// RegExp#test on RegExps that are not compile-time constants (routes stored in an array, like
// Express / Koa path-to-regexp routers), against an input shorter than any of the patterns.
var routes = [
    /^\/api\/users\/([^\/]+?)\/?$/i,
    /^\/api\/users\/([^\/]+?)\/posts\/?$/i,
    /^\/api\/posts\/([^\/]+?)\/?$/i,
    /^\/api\/posts\/([^\/]+?)\/comments\/([^\/]+?)\/?$/i,
    /^\/api\/search\/?$/i,
    /^\/static\/(.*)\/?$/i,
    /^\/admin\/([^\/]+?)\/settings\/?$/i,
    /^\/health\/?$/i,
];

function dispatch(path)
{
    var matched = 0;
    for (var i = 0; i < routes.length; ++i) {
        if (routes[i].test(path))
            ++matched;
    }
    return matched;
}
noInline(dispatch);

var total = 0;
for (var i = 0; i < 1e6; ++i)
    total += dispatch("/");
if (total !== 0)
    throw new Error("bad result " + total);

//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ defaultNoEagerRun

function getHeader(headers, name) {
    return headers[name.toLowerCase()];
}
noInline(getHeader);
noOSRExitFuzzing(getHeader);

function putHeader(headers, name, value) {
    headers[name.toLowerCase()] = value;
}
noInline(putHeader);
noOSRExitFuzzing(putHeader);

function hasHeader(headers, name) {
    return name.toLowerCase() in headers;
}
noInline(hasHeader);
noOSRExitFuzzing(hasHeader);

function deleteHeader(headers, name) {
    return delete headers[name.toLowerCase()];
}
noInline(deleteHeader);
noOSRExitFuzzing(deleteHeader);

var headers = { 'content-type': 1, 'x-request-id': 2 };
for (var i = 0; i < testLoopCount * 10; ++i) {
    if (getHeader(headers, 'content-type') === undefined)
        throw new Error("bad get");
    if (getHeader(headers, 'X-Request-Id') === undefined)
        throw new Error("bad get");
    putHeader(headers, 'content-type', i);
    putHeader(headers, 'X-Request-Id', i);
    if (!hasHeader(headers, 'content-type'))
        throw new Error("bad in");
    if (!hasHeader(headers, 'Content-Type'))
        throw new Error("bad in");
    var h = { 'content-type': 1, 'x-request-id': 2 };
    deleteHeader(h, i & 1 ? 'Content-Type' : 'content-type');
    if ('content-type' in h)
        throw new Error("bad delete");
}

if (headers['content-type'] !== testLoopCount * 10 - 1 || headers['x-request-id'] !== testLoopCount * 10 - 1)
    throw new Error("bad put");

for (var f of [getHeader, putHeader, hasHeader, deleteHeader]) {
    if (numberOfDFGCompiles(f) > 3)
        throw new Error(f.name + " was compiled " + numberOfDFGCompiles(f) + " times");
}

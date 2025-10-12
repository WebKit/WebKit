//@ skip if $buildType == "debug"
(function() {
    var result = 0;
    var n = 1000000;
    let src = `
function cross(vec<float, 3> a, vec<float, 3> b) {
    vec<float, 3> result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}
function dot(vec<float, 3> a, vec<float, 3> b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
function test() {
    vec<float, 3> a = { 1, 2, 3 };
    vec<float, 3> b = { 400, 500, 600 };
    vec<float, 3> c = cross(a, b);
    assert(dot(a, c) == 0);
    assert(dot(b, c) == 0);
}
`;
    for (let i = 0; i < n; ++i) {
        let re = /[\(\)\{\}\[\]\<\>\,\.\;]/g;
        while (re.test(src))
            ++result;
    }
    if (result != n * 89)
        throw "Error: bad result: " + result;
})();

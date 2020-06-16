let x = false;
function f(x) {
    return Array.from(x);
}
noInline(f);

function next() {
    if (x) {
        x = false;
        return {value: 2, done: false, x: 1};
    }
    return {value: 1, done: true };
}
noInline(next);

var iterator = function() {
    if (x)
        return {};
    return { next };
};
noInline(iterator);

var node_list = {
    [Symbol.iterator]: iterator,
}
for (var i = 0; i < 1e5; ++i) {
    f(node_list);
}
x = true;
try {
    f(node_list);
} catch (err) {
    if (err.message !== 'undefined is not a function')
        throw err;
}

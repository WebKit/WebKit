module("base");

test("joinPath", 1, function() {
    var value = base.joinPath("path/to", "test.html");
    equals(value, "path/to/test.html");
});

test("endsWith", 9, function() {
    ok(base.endsWith("xyz", ""));
    ok(base.endsWith("xyz", "z"));
    ok(base.endsWith("xyz", "yz"));
    ok(base.endsWith("xyz", "xyz"));
    ok(!base.endsWith("xyz", "wxyz"));
    ok(!base.endsWith("xyz", "gwxyz"));
    ok(base.endsWith("", ""));
    ok(!base.endsWith("", "z"));
    ok(!base.endsWith("xyxy", "yx"));
});

test("trimExtension", 6, function() {
    equals(base.trimExtension("xyz"), "xyz");
    equals(base.trimExtension("xy.z"), "xy");
    equals(base.trimExtension("x.yz"), "x");
    equals(base.trimExtension("x.y.z"), "x.y");
    equals(base.trimExtension(".xyz"), "");
    equals(base.trimExtension(""), "");
});

test("joinPath with empty parent", 1, function() {
    var value = base.joinPath("", "test.html");
    equals(value, "test.html");
});

test("uniquifyArray", 5, function() {
    deepEqual(base.uniquifyArray([]), []);
    deepEqual(base.uniquifyArray(["a"]), ["a"]);
    deepEqual(base.uniquifyArray(["a", "b"]), ["a", "b"]);
    deepEqual(base.uniquifyArray(["a", "b", "b"]), ["a", "b"]);
    deepEqual(base.uniquifyArray(["a", "b", "b", "a"]), ["a", "b"]);
});

test("filterTree", 2, function() {
    var tree = {
        'path': {
            'to': {
                'test.html': {
                    'actual': 'PASS',
                    'expected': 'FAIL'
                }
            },
            'another.html': {
                'actual': 'TEXT',
                'expected': 'PASS'
            }
        }
    }

    function isLeaf(node)
    {
        return !!node.actual;
    }

    function actualIsText(node)
    {
        return node.actual == 'TEXT';
    }

    var all = base.filterTree(tree, isLeaf, function() { return true });
    deepEqual(all, {
        'path/to/test.html': {
            'actual': 'PASS',
            'expected': 'FAIL'
        },
        'path/another.html': {
            'actual': 'TEXT',
            'expected': 'PASS'
        }
    });

    var text = base.filterTree(tree, isLeaf, actualIsText);
    deepEqual(text, {
        'path/another.html': {
            'actual': 'TEXT',
            'expected': 'PASS'
        }
    });
});

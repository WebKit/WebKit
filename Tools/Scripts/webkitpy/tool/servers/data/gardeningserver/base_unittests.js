module("base");

test("joinPath", 1, function() {
    var value = base.joinPath("path/to", "test.html");
    equals(value, "path/to/test.html");
});

test("joinPath with empty parent", 1, function() {
    var value = base.joinPath("", "test.html");
    equals(value, "test.html");
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

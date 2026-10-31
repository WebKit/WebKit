(function() {
    var paths = [];
    for (var i = 0; i < 1000; i++)
        paths.push("/home/user/projects/application/packages/core/src/components/very/deeply/nested/directory/structure/module-" + i + (i % 7 == 0 ? ".tsx" : ".module.css"));

    var re = /\.[cm]?[jt]sx?$/;
    var n = 400;
    var result = 0;
    for (var i = 0; i < n; i++) {
        for (var j = 0; j < paths.length; j++) {
            if (re.test(paths[j]))
                result++;
        }
    }
    if (result !== n * 143)
        throw "Error: bad result: " + result;
})();

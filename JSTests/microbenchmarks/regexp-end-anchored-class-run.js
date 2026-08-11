(function() {
    var lines = [];
    for (var i = 0; i < 400; i++) {
        var line = "2026-07-23T12:34:56.789Z [worker-" + (i % 8) + "] processed request /api/v2/items/" + i + " in " + (i % 250) + "ms status=200 trace=";
        line += i % 3 == 0 ? "0123456789abcdef0123456789abcdef01234567" : "deadbeef" + i;
        lines.push(line);
    }

    var re = /[0-9a-f]{40}$/;
    var n = 400;
    var result = 0;
    for (var i = 0; i < n; i++) {
        for (var j = 0; j < lines.length; j++) {
            if (re.test(lines[j]))
                result++;
        }
    }
    if (result !== n * 134)
        throw "Error: bad result: " + result;
})();

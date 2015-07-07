function test() {
    var values = ["foo", "bar", "baz"];

    Array.prototype.__defineSetter__("0", function() { throw "In custom setter" })

    for (var i = 0; i < 2; i++) {
        var result = values.filter(function(current) {
            if (current == "foo")
                return true
            return false
        })

        if (result.length !== 1)
             throw "filter result length wrong, should be 1, but was " + result.length

        if (i == 0) // Change result to update its array profile to ArrayStorageShape
            result.shift()
    }
}

test()

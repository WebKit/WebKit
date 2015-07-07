description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=43401">Calling unshift passing more than 1 argument causes array corruption.  It also tests some other unshift combinations.'
);


function unshift1(n) {
    var anArray = [];
    for (var i = 0; i < n; i++) {
        anArray.unshift('a');
    }
    
    return anArray;
}

function unshift2(n) {
    var anArray = [];
    for (var i = 0; i < n; i++) {
        anArray.unshift('a', 'b');
    }

    return anArray;
}

function unshift5(n) {
    var anArray = [];
    for (var i = 0; i < n; i++) {
        anArray.unshift('a', 'b', 'c', 'd', 'e');
    }

    return anArray;
}


shouldBe('unshift1(1)', '["a"]');
shouldBe('unshift1(2)', '["a", "a"]');
shouldBe('unshift1(4)', '["a", "a", "a", "a"]');
shouldBe('unshift2(1)', '["a", "b"]');
shouldBe('unshift2(2)', '["a", "b", "a", "b"]');
shouldBe('unshift2(4)', '["a", "b", "a", "b", "a", "b", "a", "b"]');
shouldBe('unshift2(10)', '["a", "b", "a", "b", "a", "b", "a", "b", "a", "b", "a", "b", "a", "b", "a", "b", "a", "b", "a", "b"]');
shouldBe('unshift5(1)', '["a", "b", "c", "d", "e"]');
shouldBe('unshift5(2)', '["a", "b", "c", "d", "e", "a", "b", "c", "d", "e"]');
shouldBe('unshift5(6)', '["a", "b", "c", "d", "e", "a", "b", "c", "d", "e", "a", "b", "c", "d", "e", "a", "b", "c", "d", "e", "a", "b", "c", "d", "e", "a", "b", "c", "d", "e"]');

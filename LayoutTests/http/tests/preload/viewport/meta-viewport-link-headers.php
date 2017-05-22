<?php
header("Link: <http://127.0.0.1:8000/resources/square100.png?control>;rel=preload;as=image;\"", false);
header("Link: <http://127.0.0.1:8000/resources/square100.png?large>;rel=preload;as=image;media=\"(min-width: 300px)\"", false);
header("Link: <http://127.0.0.1:8000/resources/square100.png?small>;rel=preload;as=image;media=\"(max-width: 299px)\"", false);
?>
<!DOCTYPE html> <!-- webkit-test-runner [ useFlexibleViewport=true ] -->
<meta name="viewport" content="width=160">
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
    var t = async_test('Makes sure that Link headers support the media attribute and respond to <meta content=viewport>');
    window.addEventListener("load", t.step_func(function() {
        var entries = performance.getEntriesByType("resource");
        var controlLoaded = false;
        var smallLoaded = false;
        var largeLoaded = false;
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].name.indexOf("control") != -1)
                controlLoaded = true;
            else if (entries[i].name.indexOf("large") != -1)
                largeLoaded = true;
            else if (entries[i].name.indexOf("small") != -1)
                smallLoaded = true;
        }
        assert_true(controlLoaded);
        assert_false(largeLoaded);
        assert_true(smallLoaded);
        t.done();
    }));
</script>
<script src="../resources/slow-script.pl?delay=200"></script>


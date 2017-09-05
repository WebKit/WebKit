<?php
header("Link: <http://127.0.0.1:8000/resources/square100.png?timer>;rel=preload;as=image;\"", false);
header("Link: <http://127.0.0.1:8000/resources/square100.png?control>;rel=preload;as=image;\"", false);
header("Link: <http://127.0.0.1:8000/resources/square100.png?large>;rel=preload;as=image;media=\"(min-width: 300px)\"", false);
header("Link: <http://127.0.0.1:8000/resources/square100.png?small>;rel=preload;as=image;media=\"(max-width: 299px)\"", false);
?>
<!DOCTYPE html>
<meta name="viewport" content="width=160">
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
    var t = async_test('Makes sure that Link headers support the media attribute and respond to <meta content=viewport>');
    var img = new Image();
    var counter = 20;
    var timeout = 50;
    img.addEventListener("load", t.step_func(function(){
        var test = function() {
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
            // It's possible that due to network variance, these resource downloads didn't yet finish.
            // If so, wait a bit longer (up to 1 second).
            if (!(controlLoaded || smallLoaded) && counter) {
                --counter;
                t.step_timeout(test, timeout);
            }

            assert_true(controlLoaded, "The control element should be loaded");
            assert_false(largeLoaded, "The large element should not be loaded");
            assert_true(smallLoaded, "The small element should be loaded");
            t.done();
        };
        t.step_timeout(test, timeout);
    }));
    img.src = "http://127.0.0.1:8000/resources/square100.png?timer";
</script>

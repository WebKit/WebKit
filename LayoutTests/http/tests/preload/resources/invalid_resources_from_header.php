<?php
header("Link: <   ../resources/dummy.js >; rel=preload; as=script", false);
header("Link: <../משאבים/dummy.css>; rel=preload; as=style", false);
header("Link: <../résôûrcès/dummy.css>; rel=preload; as=style", false);
header("Link: <../resources/Ahem{.ttf,.woff}>; rel=preload; as=font; crossorigin", false);
header("Link: <../resources/test\f.mp4>; rel=preload; as=media", false);
header("Link: <../security/resources/cap\ttions.vtt>; rel=preload; as=track", false);
header("Link: <../resources/dummy   .xml>; rel=preload;", false);
header("Link: <../resources/dumm>y.xml>; rel=preload", false);
header("Content-Security-Policy: img-src 'none'", false);
header("Link: <http://localhost:8000/preload/resources/square.png>; rel=preload; as=image", false);
header("Link: <http://localhost:53/preload/resources/dummy.js>; rel=preload; as=script", false);
header("Link: <#foobar>; rel=preload; as=style", false);
header("Link: <>; rel=preload; as=style", false);
header("Link: <   \t>; rel=preload; as=style", false);
header("Link: >; rel=preload; as=style", false);
header("Link: <; rel=preload; as=style", false);
header("Link: ; rel=preload; as=style", false);
header("Link <../resources/Ahem.ttf>; rel=preload; as=font; crossorigin", false);
header("Link: <   ../resources/dummy.js?foobar >; rel=preload; as='", false);
?>
<!DOCTYPE html>
<script src="/js-test-resources/js-test.js"></script>
<script>
    shouldBeTrue("internals.isPreloaded('../resources/dummy.js');");
    shouldBeFalse("internals.isPreloaded('../משאבים/dummy.css');");
    shouldBeFalse("internals.isPreloaded('../résôûrcès/square.png');");
    // Invalid URLs get preloaded (and get terminated further down the stack)
    shouldBeTrue("internals.isPreloaded('../resources/Ahem{.ttf,.woff}');");
    shouldBeFalse("internals.isPreloaded('../resources/test.mp4');");
    shouldBeTrue("internals.isPreloaded('../resources/test\f.mp4');");
    shouldBeTrue("internals.isPreloaded('../security/resources/cap\ttions.vtt');");
    shouldBeFalse("internals.isPreloaded('../resources/dummy.xml?badvalue');");
    shouldBeTrue("internals.isPreloaded('../resources/dummy   .xml');");
    shouldBeFalse("internals.isPreloaded('../resources/dummy.xml');");
    shouldBeFalse("internals.isPreloaded('../resources/dumm');");
    shouldBeFalse("internals.isPreloaded('http://localhost:8000/preload/resources/square.png');");
    // Invalid ports get preloaded (and get terminated further down the stack).
    shouldBeTrue("internals.isPreloaded('http://localhost:53/preload/resources/dummy.js');");
    shouldBeFalse("internals.isPreloaded('#foobar');");
    shouldBeFalse("internals.isPreloaded('../resources/Ahem.ttf');");
</script>
<script>
    if (window.internals)
        window.internals.settings.setLinkPreloadEnabled(false);
</script>

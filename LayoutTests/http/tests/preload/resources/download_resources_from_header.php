<?php
header("Link: <../resources/dummy.js>; rel=preload; as=script", false);
header("Link:<../resources/dummy.css>; rel=preload; as=style", false);
header("Link: <../resources/square.png>;rel=preload;as=image", false);
header("Link: <../resources/Ahem.ttf>; rel=preload; as=font; crossorigin", false);
header("Link: <../resources/test.mp4>; rel=preload; as=media", false);
header("link: <../security/resources/captions.vtt>; rel=preload; as=track", false);
header("Link: <../resources/dummy.xml?badvalue>; rel=preload; as=foobar", false);
header("Link: <../resources/dummy.xml>; rel=preload", false);
?>
<!DOCTYPE html>
<script src="/js-test-resources/js-test.js"></script>
<script>
    shouldBeTrue("internals.isPreloaded('../resources/dummy.js');");
    shouldBeTrue("internals.isPreloaded('../resources/dummy.css');");
    shouldBeTrue("internals.isPreloaded('../resources/square.png');");
    shouldBeTrue("internals.isPreloaded('../resources/Ahem.ttf');");
    shouldBeTrue("internals.isPreloaded('../resources/test.mp4');");
    shouldBeTrue("internals.isPreloaded('../security/resources/captions.vtt');");
    shouldBeFalse("internals.isPreloaded('../resources/dummy.xml?badvalue');");
    shouldBeTrue("internals.isPreloaded('../resources/dummy.xml');");
</script>
<script>
    if (window.internals)
        window.internals.setLinkPreloadSupport(false);
</script>

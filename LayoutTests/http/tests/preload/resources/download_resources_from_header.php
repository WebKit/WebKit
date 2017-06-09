<?php
header("Link: <../resources/dummy.js>; rel=preload; as=script", false);
header("LiNk:<../resources/dummy.css>; rel=preload; as=style", false);
header("Link: <../resources/square.png>;rel=preload;as=image", false);
header("Link: <../resources/Ahem.ttf>; rel=preload; as=font; crossorigin", false);
header("Link: <../resources/test.mp4>; rel=preload; as=video", false);
header("Link: <../resources/test.oga>; rel=preload; as=audio", false);
header("link: <../security/resources/captions.vtt>; rel=preload; as=track", false);
header("Link: <../resources/dummy.xml?badvalue>; rel=preload; as=foobar", false);
header("Link: <../resources/dummy.xml?empty>; rel=preload", false);
header("Link: <../resources/dummy.xml>; rel=preload;as=fetch", false);
?>
<!DOCTYPE html>
<script src="/js-test-resources/js-test.js"></script>
<script>
    shouldBeTrue("internals.isPreloaded('../resources/dummy.js');");
    shouldBeTrue("internals.isPreloaded('../resources/dummy.css');");
    shouldBeTrue("internals.isPreloaded('../resources/square.png');");
    shouldBeTrue("internals.isPreloaded('../resources/Ahem.ttf');");
    shouldBeTrue("internals.isPreloaded('../resources/test.mp4');");
    shouldBeTrue("internals.isPreloaded('../resources/test.oga');");
    shouldBeTrue("internals.isPreloaded('../security/resources/captions.vtt');");
    shouldBeFalse("internals.isPreloaded('../resources/dummy.xml?badvalue');");
    shouldBeFalse("internals.isPreloaded('../resources/dummy.xml?empty');");
    shouldBeTrue("internals.isPreloaded('../resources/dummy.xml');");
</script>

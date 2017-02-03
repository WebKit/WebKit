#!/usr/bin/perl -wT
use strict;

print <<"EOL";
HTTP/1.1 200 OK
Link: <   ../resources/dummy.js >; rel=preload; as=script
Link: <../משאבים/dummy.css>; rel=preload; as=style
Link: <../résôûrcès/dummy.css>; rel=preload; as=style
Link: <../resources/Ahem{.ttf,.woff}>; rel=preload; as=font; crossorigin
Link: <../resources/test\f.mp4>; rel=preload; as=media
Link: <../security/resources/cap\ttions.vtt>; rel=preload; as=track
Link: <../resources/dummy   .xml>; rel=preload;
Link: <../resources/dumm>y.xml>; rel=preload
Content-Security-Policy: img-src 'none'
Link: <http://localhost:8000/preload/resources/square.png>; rel=preload; as=image
Link: <http://localhost:53/preload/resources/dummy.js>; rel=preload; as=script
Link: <#foobar>; rel=preload; as=style
Link: <>; rel=preload; as=style
Link: <   \t>; rel=preload; as=style
Link: >; rel=preload; as=style
Link: <; rel=preload; as=style
Link: ; rel=preload; as=style
Link <../resources/Ahem.ttf>; rel=preload; as=font; crossorigin
Link: <   ../resources/dummy.js?foobar >; rel=preload; as='
Link: <<../resources/dummy.js?invalid>>; rel=preload; as=script
Link: <../resources/dummy.js?invalid>>; rel=preload; as=script
Link: <<../resources/dummy.js?invalid>; rel=preload; as=script
Content-Type: text/html

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
    shouldBeFalse("internals.isPreloaded('../resources/dummy.js?invalid');");
</script>
EOL
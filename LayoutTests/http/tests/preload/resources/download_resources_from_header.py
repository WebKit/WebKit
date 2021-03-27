#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Link: <../resources/dummy.js>; rel=preload; as=script\r\n'
    'LiNk:<../resources/dummy.css>; rel=preload; as=style\r\n'
    'Link: <../resources/square.png>;rel=preload;as=image\r\n'
    'Link: <../resources/Ahem.ttf>; rel=preload; as=font; crossorigin\r\n'
    'Link: <../resources/test.mp4>; rel=preload; as=video\r\n'
    'Link: <../resources/test.oga>; rel=preload; as=audio\r\n'
    'link: <../security/resources/captions.vtt>; rel=preload; as=track\r\n'
    'Link: <../resources/dummy.xml?badvalue>; rel=preload; as=foobar\r\n'
    'Link: <../resources/dummy.xml?empty>; rel=preload\r\n'
    'Link: <../resources/dummy.xml>; rel=preload;as=fetch\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
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
</script>''')
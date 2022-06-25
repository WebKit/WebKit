#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Link: <http://127.0.0.1:8000/resources/dummy.js>; rel=preload; as=script\r\n'
    'LiNk:<http://127.0.0.1:8000/resources/dummy.css>; rel=preload; as=style\r\n'
    'Link: <http://127.0.0.1:8000/resources/square100.png>;rel=preload;as=image\r\n'
    'Link: <http://127.0.0.1:8000/resources/square100.png?background>;rel=preload;as=image\r\n'
    'Link: <http://127.0.0.1:8000/resources/Ahem.woff>; rel=preload; as=font; crossorigin\r\n'
    'Link: <http://127.0.0.1:8000/resources/test.mp4>; rel=preload; as=video\r\n'
    'Link: <http://127.0.0.1:8000/resources/test.oga>; rel=preload; as=audio\r\n'
    'link: <http://127.0.0.1:8000/security/resources/captions.vtt>; rel=preload; as=track\r\n'
    'Link: <http://127.0.0.1:8000/resources/dummy.xml?foobar>; rel=preload; as=foobar\r\n'
    'Link: <http://127.0.0.1:8000/resources/dummy.xml>; as=fetch; crossorigin; rel=preload\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<script src="/js-test-resources/testharness.js"></script>
<script src="/js-test-resources/testharnessreport.js"></script>
<script>
    var t = async_test('Makes sure that preloaded resources are not downloaded again when used');
</script>
<script src="http://127.0.0.1:8000/resources/slow-script.pl?delay=200"></script>
<style>
    #background {
        width: 200px;
        height: 200px;
        background-image: url(http://127.0.0.1:8000/resources/square100.png?background);
    }
    @font-face {
      font-family:ahem;
      src: url(http://127.0.0.1:8000/resources/Ahem.woff);
    }
    span { font-family: ahem, Arial; }
</style>
<link rel="stylesheet" href="http://127.0.0.1:8000/resources/dummy.css">
<script src="http://127.0.0.1:8000/resources/dummy.js"></script>
<div id="background"></div>
<img src="http://127.0.0.1:8000/resources/square100.png">
<video src="http://127.0.0.1:8000/resources/test.mp4">
    <track kind=subtitles src="http://127.0.0.1:8000/security/resources/captions.vtt" srclang=en>
</video>
<audio src="http://127.0.0.1:8000/resources/test.oga"></audio>
<script>
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "http://127.0.0.1:8000/resources/dummy.xml");
    xhr.send();

    window.addEventListener("load", t.step_func(function() {
        function verifyDownloadNumber(url, number) {
            assert_equals(performance.getEntriesByName(url).length, number, url);
        }
        setTimeout(t.step_func(function() {
            verifyDownloadNumber("http://127.0.0.1:8000/resources/dummy.js", 1);
            verifyDownloadNumber("http://127.0.0.1:8000/resources/dummy.css", 1);
            verifyDownloadNumber("http://127.0.0.1:8000/resources/square100.png", 1);
            verifyDownloadNumber("http://127.0.0.1:8000/resources/square100.png?background", 1);
            verifyDownloadNumber("http://127.0.0.1:8000/resources/Ahem.woff", 1);
            verifyDownloadNumber("http://127.0.0.1:8000/resources/dummy.xml?foobar", 0);
            verifyDownloadNumber("http://127.0.0.1:8000/security/resources/captions.vtt", 1);
            // FIXME: XHR should trigger a single download, but it downloads 2 resources instead.
            verifyDownloadNumber("http://127.0.0.1:8000/resources/dummy.xml", 2);
            // FIXME: We should verify for video and audio as well, but they seem to (flakily?) trigger multiple partial requests.
            t.done();
            }), 100);
    }));
</script>''')
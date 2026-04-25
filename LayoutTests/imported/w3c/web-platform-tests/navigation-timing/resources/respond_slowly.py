import time


def main(request, response):
    head = b"""<script>
    let navBeforeLoadFinished = performance.getEntriesByType('navigation')[0];
    let originalResponseEnd = navBeforeLoadFinished.responseEnd;
    let originalDuration = navBeforeLoadFinished.duration;
    function checkResponseEnd() {
        let navDuringLoadEvent = performance.getEntriesByType('navigation')[0];
        let responseEndDuringLoadEvent = navDuringLoadEvent.responseEnd;
        let durationDuringLoadEvent = navDuringLoadEvent.duration;
        setTimeout(function() {
            let navAfterLoadEvent = performance.getEntriesByType('navigation')[0];
            parent.postMessage([
                originalResponseEnd,
                originalDuration,
                responseEndDuringLoadEvent,
                durationDuringLoadEvent,
                navAfterLoadEvent.responseEnd,
                navAfterLoadEvent.duration], '*');
        }, 0);
    }
    </script><body onload='checkResponseEnd()'>"""
    response.headers.set(b"Content-Length", str(len(head) + 10000))
    response.headers.set(b"Content-Type", b"text/html")
    response.write_status_headers()
    response.writer.write_content(head)
    for i in range(1000):
        response.writer.write_content(b"1234567890")
        time.sleep(0.001)

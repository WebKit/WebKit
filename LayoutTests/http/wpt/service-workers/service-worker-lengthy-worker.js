function fetchTest(event)
{
    event.respondWith(fetch("/WebKit/service-workers/resources/lengthy-pass.py?delay=0.5"));
}

self.addEventListener("fetch", fetchTest);

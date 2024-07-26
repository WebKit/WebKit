self.addEventListener('fetch', function(event) {
    event.respondWith(new Response(new Blob(["<script>window.opener.postMessage('" + event.request.url + "', '*')</script>"])));
});

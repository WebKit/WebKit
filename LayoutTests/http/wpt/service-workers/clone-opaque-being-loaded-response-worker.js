importScripts('/common/get-host-info.sub.js');

var remoteUrl = get_host_info()['HTTP_REMOTE_ORIGIN'] + '/WebKit/service-workers/resources/lengthy-pass.py';

self.addEventListener('fetch', (event) => {
    if (event.request.url.indexOf("html") === -1)
        event.respondWith(fetch(remoteUrl, {mode: 'no-cors'}).then(response => response.clone()));
});

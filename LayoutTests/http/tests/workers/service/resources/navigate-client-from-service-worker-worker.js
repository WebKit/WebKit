self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', ev => ev.waitUntil(clients.claim()));

self.addEventListener('message', async ev => {
    if (ev.data.action !== 'navigate')
        return;
    let list = await clients.matchAll({ type: 'window', includeUncontrolled: true });
    for (let c of list) {
        if (c.frameType === 'top-level')
            continue;
        try {
            await c.navigate('navigate-client-from-service-worker-dest.html');
        } catch (err) {
        }
    }
});

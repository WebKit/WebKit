const VALID_SERVER_KEY = "BA1Hxzyi1RUM1b5wjxsn7nGxAszw2u61m164i3MrAIxHF6YK5h4SDYic-dRuU_RCPCfA5aq9ojSwk5Y2EmClBPs";

self.addEventListener('message', async (event) => {
    let [op, ...args] = event.data;

    if (op == 'permissionState') {
        try {
            let state = await self.registration.pushManager.permissionState();
            event.source.postMessage(state);
        } catch (e) {
            event.source.postMessage("error: " + e);
        }
    } else if (op == 'subscribe') {
        let subscription = null;
        let result = null;

        try {
            subscription = await self.registration.pushManager.subscribe({
                userVisibleOnly: true,
                applicationServerKey: args[0]
            });
            if (subscription)
                result = "successful";
            else
                result = "error: null subscription";
        } catch (e) {
            if (!e)
                result = "error: null exception";
            else if (e.name == 'AbortError')
                // Layout tests currently aren't connected to webpushd. So if we fail with an AbortError
                // when trying to connect to webpushd, we count that as a successful subscription for
                // testing purposes.
                result = "successful";
            else
                result = "error: " + e.name;
        }

        if (subscription)
            await subscription.unsubscribe();

        event.source.postMessage(result);
    }
});

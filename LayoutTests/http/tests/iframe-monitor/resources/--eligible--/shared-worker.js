let clients = [];

onconnect = (e) => {
    const port = e.ports[0];
    clients.push(port);

    port.onmessage = async (e) => {
        if (typeof e.data === 'number') {
            const response = await fetch(`../generate-byte.py?size=${e.data}`);
            const blob = await response.blob();

            clients.forEach((client) => client.postMessage(blob));
        }
    };

    port.onclose = () => {
        clients = clients.filter(client => client !== port);
    };
};

let installEvent;
oninstall = e => {
   installEvent = e;
}

onmessage = e => {
    let result = self.InstallEvent ? "Has InstallEvent" : "No InstallEvent";
    result  += installEvent.addRoutes ? " - has addRoutes" : " - no addRoutes"
    e.source.postMessage(result);
}

let busy = new Worker(`postMessage("started"); for (;;) { }`, { source: "string" });
busy.onmessage = function () {
    busy.terminate();
    busy.terminate();
};

let idle = new Worker(`onmessage = function () { throw new Error("terminated worker should not receive messages"); };`, { source: "string" });
idle.terminate();
idle.postMessage("dropped");

let pendingTimer = new Worker(`setTimeout(function () { throw new Error("terminated worker should not run timers"); }, 100000);`, { source: "string" });
pendingTimer.terminate();

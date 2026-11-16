"use strict";

let globalSink = 0;

function attempt(round) {
  function rootACtor() { this.round = round; }
    const rootA = new rootACtor();
    const rootB = {};
    const rootC = {};
    const mid = {};
    const leaf = {};

    Object.setPrototypeOf(mid, rootA);
    Object.setPrototypeOf(leaf, mid);

    let tick = 0;
    let sink = 0;

    const proxyProto = new Proxy(rootB, {
        getPrototypeOf(target) {
            tick++;
            if ((tick & 3) === 0)
                Object.setPrototypeOf(mid, rootA);
            return Reflect.getPrototypeOf(target);
        }
    });

    const quietGate = { get trip() { return 1; } };
    const noisyGate = {
        get trip() {
            tick++;
            Object.setPrototypeOf(mid, (tick & 1) ? proxyProto : rootC);
            return 1;
        }
    };

    function hot(value, proto, gate) {
        const guard = gate.trip;
        const result = value instanceof proto;
        sink ^= result ? guard : (guard << 1);
        return result;
    }

    function drive(i) {
        const gate = i < 42000 ? quietGate : noisyGate;
        if (i === 42000)
            Object.setPrototypeOf(mid, proxyProto);
        return hot(leaf, rootACtor, gate);
    }

    for (let i = 0; i < 70000; i++)
        drive(i);

    globalSink ^= sink;
}

for (let round = 0; round < 8; round++)
    attempt(round);

if (globalSink === 0x12345678)
    throw new Error("unreachable");

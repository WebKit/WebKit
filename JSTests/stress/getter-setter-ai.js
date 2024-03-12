//@runDefault("--useConcurrentJIT=false", "--validateAbstractInterpreterState=true", "--validateAbstractInterpreterStateProbability=1.0")
function opt(inline, callTypeOf, object, transition) {
    let result;

    object.p1;

    for (let i = 0; i < 2; i++) {
        if (inline) {
            object();

            result = object.p2;
        }

        if (callTypeOf) {
            transition.x = 1;

            result = typeof(object.p3);
        }
    }


    return result;
}

function watchP3(object) {
    function cache() {
        return object.p3;
    }

    for (let i = 0; i < 100; i++) {
        cache();
    }
}

function main() {
    noDFG(main);

    let object1 = function () {};
    object1.p1 = 1;
    object1.p2 = 1;
    object1.__defineGetter__('p3', () => {});

    let object2 = function () {};
    object2.p1 = 2;
    object2.p2 = 1;
    object2.p3 = 1;

    watchP3(object1);

    for (let i = 0; i < 100; i++) {
        opt(/* inline */ false, /* callTypeOf */ true, object2, {});
    }

    for (let i = 0; i < 10000; i++) {
        opt(/* inline */ true, /* callTypeOf */ false, object1, {});
    }

    opt(/* inline */ false, /* callTypeOf */ false, object1, {});
}

main();

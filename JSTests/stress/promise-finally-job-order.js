function check(name, ticks, expected, install) {
    let log = [];
    install(log);
    let chain = Promise.resolve();
    for (let i = 1; i <= ticks; i++)
        chain = chain.then((j => () => log.push("t" + j))(i));
    return chain.then(() => {
        let actual = log.join(" ");
        if (actual !== expected)
            throw new Error(name + ": " + actual);
    });
}

let steps = [
    () => check("fulfill", 5, "t1 t2 t3 settled t4 t5", log => {
        Promise.resolve(1).finally(() => { }).then(() => log.push("settled"));
    }),
    () => check("reject", 5, "t1 t2 t3 settled t4 t5", log => {
        Promise.reject(1).finally(() => { }).catch(() => log.push("settled"));
    }),
    () => check("return value", 5, "t1 t2 t3 settled t4 t5", log => {
        Promise.resolve(1).finally(() => 7).then(value => {
            if (value !== 1)
                throw new Error("value " + value);
            log.push("settled");
        });
    }),
    () => check("return promise", 5, "t1 t2 t3 settled t4 t5", log => {
        Promise.resolve(1).finally(() => Promise.resolve(5)).then(value => {
            if (value !== 1)
                throw new Error("value " + value);
            log.push("settled");
        });
    }),
    () => check("throw", 5, "t1 settled t2 t3 t4 t5", log => {
        Promise.resolve(1).finally(() => { throw 2; }).catch(error => {
            if (error !== 2)
                throw new Error("error " + error);
            log.push("settled");
        });
    }),
    () => check("chained", 7, "t1 t2 t3 t4 t5 t6 settled t7", log => {
        Promise.resolve(1).finally(() => { }).finally(() => { }).then(() => log.push("settled"));
    }),
];

let sequence = Promise.resolve();
for (let step of steps)
    sequence = sequence.then(step);
sequence.catch(error => { throw error; });

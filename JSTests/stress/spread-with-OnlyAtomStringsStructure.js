//@ runDefault("--forceEagerCompilation=1", "--validateAbstractInterpreterState=1")

const array = [""];

for (let index = 0; index < testLoopCount; index++)
    (() => {})(...array);

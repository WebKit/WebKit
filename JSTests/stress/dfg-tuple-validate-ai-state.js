//@ runDefault("--validateAbstractInterpreterState=1")
for (let i = 0; i < testLoopCount; i++)
    for (let x in 'a');

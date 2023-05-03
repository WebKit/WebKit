//@ runDefault("--validateAbstractInterpreterState=1")
for (let i = 0; i < 100000; i++)
    for (let x in 'a');

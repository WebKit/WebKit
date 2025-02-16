//@ runDefault("--sweepSynchronously=1", "--collectContinuously=1")

for (var i = 0; i < testLoopCount; i++)
    ''.toLocaleLowerCase.call(-1, 0);

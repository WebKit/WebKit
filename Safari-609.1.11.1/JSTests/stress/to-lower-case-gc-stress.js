//@ runDefault("--sweepSynchronously=1", "--collectContinuously=1")

for (var i = 0; i < 10000; i++)
    ''.toLocaleLowerCase.call(-1, 0);

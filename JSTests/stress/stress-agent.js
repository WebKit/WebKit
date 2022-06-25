//@ skip if $memoryLimited
//@ skip if $buildType == "debug"
//@ runDefault("--collectContinuously=1")
for (let i=0; i<1000; i++) {
  $.agent.start('+1');
}

//@ runDefault("--watchdog=1", "--watchdog-exception-ok")
import('').then(()=>{}, e => e.foo);

//@ runDefault("--collectContinuously=1", "--usePolyvariantDevirtualization=0", "--forceDebuggerBytecodeGeneration=1", "--verifyGC=1")
// UsePolyvariantDevirtualization gives us a PutPrivateName (not byID) while still letting us generate an IC with only one AccessCase
// DebuggerBytecodeGeneration seems to give the GC more time to interrupt the put because it forces reads from the stack 

function PutPrivateNameIC() {
    let leak = []

    class A {
        constructor() {
            this.a = 0
        }
    }
    noInline(A)

    class B extends A {
        #b
        #c
    }
    noInline(B)

    for (let i = 0; i < 100000; ++i) {
        let b1 = new B
        let b2 = new B
        let b3 = new B
        leak.push(b1, b2, b3)
    }
}
noInline(PutPrivateNameIC)
PutPrivateNameIC()

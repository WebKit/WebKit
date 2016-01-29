The Bare Bones Backend, or B3 for short, is WebKit's optimizing JIT for procedures containing C-like code.  It's currently used as the default backend for the [FTL JIT](https://trac.webkit.org/wiki/FTLJIT) inside [JavaScriptCore](https://trac.webkit.org/wiki/JavaScriptCore).

B3 comprises a [C-like SSA IR](https://trac.webkit.org/wiki/B3IntermediateRepresentation)  known as "B3 IR", optimizations on B3 IR, an [assembly IR](https://trac.webkit.org/wiko/AssemblyIntermediateRepresentation) known as "Air", optimizations on Air, an instruction selector that turns B3 IR into Air, and a code generator that assembles Air into machine code.

## Hello, World!

Here's a simple example of C++ code that uses B3 to generate a function that adds two to its argument and returns it:

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), 2)));
    
    std::unique_ptr<Compilation> compilation = std::make_unique<Compilation>(vm, proc);
    int64_t (*function)(int64_t) = static_cast<int64_t (*)(int64_t)>(compilation->code().executableAddress());
    
    printf("%d\n", function(42)); // prints 43

When compiled, the resulting machine code looks like this:

    0x3aa6eb801000: pushq %rbp
    0x3aa6eb801001: movq %rsp, %rbp
    0x3aa6eb801004: leaq 0x2(%rdi), %rax
    0x3aa6eb801008: popq %rbp
    0x3aa6eb801009: ret 

B3 always emits a frame pointer prologue/epilogue to play nice with debugging tools.  Besides that, you can see that B3 optimized the procedure's body to a single instruction: in this case, a Load Effective Address to transfer %rdi + 2, where %rdi is the first argument register, into %rax, which is the result register.

## B3 IR

Clients of B3 usually interact with it using B3 IR.  It's C-like, in the sense that it models heap references as integers and does not attempt to verify memory accesses.  It enforces static single assignment, or SSA for short.  An SSA program will contain only one assignment to each variable, which makes it trivial to trace from a use of a variable to the operation that defined its value.  B3 IR is designed to be easy to generate and cheap to manipulate.

B3 is designed to be used as a backend for JITs, rather than as a tool that programmers use directly.  Therefore, B3 embraces platform-specific concepts like argument registers, stack frame layout, the frame pointer, and the call argument areas.  It's possible to emit B3 IR that defines completely novel calling conventions, both for callers of the procedure being generated and for callees of the procedure's callsites.  B3 also makes it easy to just emit a C call.  There's an opcode for that.

See [the IR documentation](/documentation/b3/intermediate-representation/) for more info.

Here's an example of the IR from the example above:

    BB#0: ; frequency = 1.000000
        Int64 @0 = ArgumentReg(%rdi)
        Int64 @1 = Const64(2)
        Int64 @2 = Add(@0, $2(@1))
        Void @3 = Return(@2, Terminal)

## B3 Optimizations

B3 is fairly new - we only started working on it in late Oct 2015.  But it already has some awesome optimizations:

- CFG simplification.
- Constant folding with some flow sensitivity.
- Global CSE, including sophisticated load elimination.
- Aggressive dead code elimination.
- Tail duplication.
- SSA fix-up.
- Optimal placement of constant materializations.
- Integer overflow check elimination.
- Reassociation.
- Lots of miscellaneous strength reduction rules.

## Air

Air, or Assembly IR, is the way that B3 represents the machine instruction sequence prior to code generation.  Air is like assembly, except that in addition to registers it has temporaries, and in addition to the native address forms it has abstract ones like "stack" (an abstract stack slot) and "callArg" (an abstract location in the outgoing call argument area of the stack).

Here's the initial Air generated from the example above:

    BB#0: ; frequency = 1.000000
        Move %rdi, %tmp1, @0
        Move $2, %tmp2, $2(@1)
        Add64 $2, %tmp1, %tmp0, @2
        Move %tmp0, %rax, @3
        Ret64 %rax, @3

## Air Optimizations

Air has sophisticated optimizations that transform programs that use temporaries and abstract stack locations into ones that use registers directly.  Air is also responsible for ABI-related issues like stack layout and handling the C calling convention.  Air has the following optimizations:

- Iterated Register Coalescing (https://www.cs.princeton.edu/research/techreps/TR-498-95).  This is our register allocator.
- Graph coloring stack allocation.
- Spill code fix-up.
- Dead code elimination.
- Partial register stall fix-up.
- CFG simplification.
- CFG layout optimization.

Here's what these optimizations do to the example program:

    BB#0: ; frequency = 1.000000
        Add64 $2, %rdi, %rax, @2
        Ret64 %rax, @3

Note that the "@" references indicate the origin of the instruction in the B3 IR.

## B3->Air lowering, also known as Instruction Selection

The B3::LowerToAir phase converts B3 into Air by doing pattern-matching.  It processes programs backwards.  At each B3 value, it greedily tries to match both the value and as many of its children (i.e. Values it uses) and their children as possible to create a single instruction.  Different hardware targets support different instructions.  Air allows B3 to speak of the superset of all instructions on all targets, but exposes a fast query to check if a given instruction, or specific instruction form (like 3-operand add, for example) is available.  The instruction selector simply cascades through the patterns it knows about until it finds one that gives a legal instruction in Air.

The instruction selector is powerful enough to do basic things like compare-branch and load-op-store fusion.  It's smart enough to do things like what we call the Mega Combo, where the following B3 IR:

    Int64 @0 = ArgumentReg(%rdi)
    Int64 @1 = ArgumentReg(%rsi)
    Int32 @2 = Trunc(@1)
    Int64 @3 = ZExt32(@2)
    Int32 @4 = Const32(1)
    Int64 @5 = Shl(@3, $1(@4))
    Int64 @6 = Add(@0, @5)
    Int32 @7 = Load8S(@6, ControlDependent|Reads:Top)
    Int32 @8 = Const32(42)
    Int32 @9 = LessThan(@7, $42(@8))
    Void @10 = Check(@9:WarmAny, generator = 0x103fe1010, earlyClobbered = [], lateClobbered = [], usedRegisters = [], ExitsSideways|Reads:Top)

Is turned into the following Air:

    Move %rsi, %tmp7, @1
    Move %rdi, %tmp1, @0
    Move32 %tmp7, %tmp2, @3
    Patch &Branch8(3,SameAsRep), LessThan, (%tmp1,%tmp2,2), $42, @10

And the resulting code ends up being:

    0x311001401004: movl %esi, %eax
    0x311001401006: cmpb $0x2a, (%rdi,%rax,2)
    0x31100140100a: jl 0x311001401015

Other than the mandatory zero-extending operation to deal with the 32-bit argument being used as an index, B3 is smart enough to convert the address computation, load, and compare into a single instruction and then fuse that with the branch.

## Code generation

The final form of Air contains no registers or abstract stack slots.  Therefore, it maps directly to machine code.  The final code generation step is a very fast transformation from Air's object-oriented way of representing those instructions to the target's machine code.  We use JavaScriptCore's macro assembler for this purpose.

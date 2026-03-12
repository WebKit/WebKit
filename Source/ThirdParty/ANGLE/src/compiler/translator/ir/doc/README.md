# ANGLE IR

## Background

The ANGLE translator was originally based on an old fork of glslang.
While it has diverged significantly from that project, the core AST (Abstract Syntax Tree) design of
glslang has remained in the ANGLE translator.
In time, ANGLE has accumulated numerous AST transformations, which have gotten more complex over
time as more and more corner case bugs were discovered by users, security researchers and fuzzers.
The ANGLE IR was created to step away from this situation by providing a more easily malleable
intermediate representation of the input GLSL.

Currently, the ANGLE IR is experimental, and conditional to a build option (`angle_ir=true`).
It is created during parse, but is soon turned back into AST for the existing transformations to
use, although a dozen transformations are immediately made obsolete.
Over time, we intend to port the rest of the compilation steps to use the IR directly, leading to
generating the output directly from the IR.
Simultaneously, once the IR is no longer optional, the parse step can also be simplified not to
create an AST (which it currently does for validation purposes).

## Overview

The ANGLE IR is written in Rust and can be found in [src/compiler/translator/ir/src/](../src):

* [ir.rs](../src/ir.rs): The data structure of the IR can be found in this file, such as the various
  ids, type information, variable information, opcodes, control flow blocks, the IR metadata and
  finally the IR itself.
* [debug.rs](../src/debug.rs): Provides debug utils, mainly to dump the IR in a readable format.
* [validator.rs](../src/validator.rs): Provides internal validation.
* [instruction.rs](../src/instruction.rs): Provides helpers to create an instruction, including
  deriving precision from operands, constant folding etc.
* [builder.rs](../src/builder.rs): Used during parse to create the IR.
* [traverser.rs](../src/traverser.rs): Provides utilities to visit or transform the IR.
* [transform/](../src/transform/): The set of transformations that can be applied to the IR can be
  found here.
* [output/](../src/output/): Output generators are placed here.
* [ast.rs](../src/ast.rs): Helper to generate an AST out of the IR.  This is currently used to
  actually generate the legacy AST, but can also be used to generate GLSL, WGSL, etc that are
  text-based outputs with similar restrictions as GLSL.
* [compile.rs](../src/compile.rs): Orchestrator of the compilation process, deciding which
  transformations to apply and what output to generate.

## IR Basics

The IR includes the following entities, all of which are referenced by ids:

* Types (`enum Type`): Referenced by `TypeId`, a type can be:
  * `Type::Scalar`: A single `BasicType`, such as `float`, `int`, `yuvCscStandardEXT`,
    `atomic_uint`
  * `Type::Vector`: A vector of scalars
  * `Type::Matrix`: A set of vectors (each, one column of the matrix)
  * `Type::Array`: An array of other types
  * `Type::UnsizedArray`: An unsized array of other types, such as allowed at the end of a storage
    block.
  * `Type::Struct`: A collection of other types.
    Both `struct` and interface blocks are considered structs.
  * `Type::Image`: An image.
    This type includes information such as the basic type of the image (float, int, uint), whether
    it's sampled (e.g. `texture2D` vs `image2D`), dimensionality (e.g. `texture2D` vs `texture3D`)
    etc.
  * `Type::Pointer`: A pointer to another type.
    Pointers are references to memory locations, and they are used to load and store values.

* Constants (`struct Constant`): Referenced by `ConstantId`, a constant pairs a `TypeId` with a
  value `ConstantValue`.
  The constant value is either a scalar if the type is a scalar, or a composite
  (`ConstantValue::Composite`) which is simply a collection of other constants that make up the
  value based on the type.
  Constants do not have an intrinsic precision, but may inherit one in the specific instruction they
  are used at based on neighboring operands according to GLSL rules.

* Variables (`struct Variable`): Referenced by `VariableId`, a variable includes the following
  information:
  * Name (`struct Name`): The variable name is a string paired with the name's source: A shader
    interface variable, an internal variable or a temporary.
    The name source is used to generate exact or mangled names based on whether the backend
    references it or not.
  * Type (`TypeId`)
  * Precision (`enum Precision`): Corresponding to GLSL's `highp`, `mediump`, etc
  * Decorations (a set of `enum Decoration`): A set of additional properties of the variable,
    originating from various qualifiers and layout qualifiers in GLSL
  * Whether the variable is a built-in and which (`enum BuiltIn`)
  * Whether the variable has an initializer, which must always be a constant (`ConstantId`)

* Functions (`struct Function`): Referenced by `FunctionId`, a function includes the following
  information:
  * Name (`struct Name`)
  * Parameters (a set of `struct FunctionParam`): Each `FunctionParam` consists of a variable
    (`VariableId`) and the direction of the variable (`in`, `out`, `inout`).
  * Return type, precision and decorations

* Registers: Every intermediate value in the IR is called a register, referenced by `RegisterId`.
  Registers are typically the result of operations.
  They are never "void", i.e. they contain some value.

For simplicity, some common ids are predefined:

* Types, e.g. `TYPE_ID_VOID`, `TYPE_ID_FLOAT`, `TYPE_ID_VEC4`, `TYPE_ID_MAT3x2`, etc
* Constants, e.g. `CONSTANT_ID_FALSE`, `CONSTANT_ID_FLOAT_ONE`, `CONSTANT_ID_YUV_CSC_ITU601`, etc

The IR itself is split in two structs, the IR metadata (`struct IRMeta`) and the actual
control-flow blocks and instructions.
This was necessarily to satisfy Rust's borrow checker during build and transformations of IR as the
instructions and blocks mutate at the same time as the metadata.

The IR metadata includes global properties such as whether `early_fragment_tests` was specified, the
geometry shader max vertices, which variables are global, which function is `main()`, etc as well as:

* The list of types, constants, variables and functions, each indexed by their corresponding id.
* The list of instructions that produce values, indexed by `RegisterId`

The `IRMeta` struct provides helper functionality to create types from other types, create basic
constants or composite constants from other constants, declare variables, create instructions and
declare functions.

The control-flow part of the IR consists of a set of entry point blocks (`struct Block`) per
function.
See below for details on control flow.

## IR Operations

The actual instructions in the IR are instances of `enum OpCode`.
This enum includes all sorts of instructions, each an opcode and its parameters if any.

The opcode parameters are typically of type `TypedId`.
Rarely, a parameter may be a `ConstantId` or directly `u32`.
A `TypedId` bundles the `TypeId` and `Id` of the operand and its associated precision (mainly
because of constants, which don't have a precision out of context of the instruction).
`enum Id` is simply an enum that can either reference a `RegisterId`, `ConstantId` or `VariableId`.

The opcodes in `OpCode` are:

* Control flow ops, such as `OpCode::Return` (GLSL's `return`), `OpCode::Loop` (GLSL's `for` and
  `while`), etc
* Indexing operations, such as `OpCode::ExtractVectorComponent` (GLSL's `value.x`),
  `OpCode::AccessStructField` (GLSL's `pointer.field`), etc
* Constructors, such as `OpCode::ConstructMatrixFromScalar` (GLSL's `mat2x3(f)`),
    `OpCode::ConstructVectorFromMultiple` (GLSL's `vec4(a, b, c)`).
* `OpCode::Load` and `OpCode::Store` to access memory
* `OpCode::Call` to call functions
* `OpCode::Unary`, `OpCode::Binary` and `OpCode::BuiltIn`, which themselves include an
  `enum UnaryOpCode`, `enum BinaryOpCode` and `enum BuiltInOpCode` that indicate the specific op.
  Unary and binary ops can be GLSL ops (like `+`) or built-ins (like `round()`) and produce values.
  Built-in ops contain miscellaneous GLSL built-ins that either do not produce values (like
  `barrier()` or have more than 2 parameters (like `faceforward`).
* `OpCode::Texture` encapsulates the _many_ GLSL texture built-ins.
  This op consists of the `sampler` and `coord` parameters which are always present, plus
  `enum TextureOpCode` which covers all the variations such as whether it's `texture*Proj`,
  `texture*Lod`, `texture*Grad`, `texture*Gather`, etc, each with their own additional parameters.

There is additionally `OpCode::Alias` which is used by transformations to make local changes
without having to propagate a register ID change over the whole IR.

## IR Control Flow

The instructions of a function are grouped in control-flow blocks (`struct Block`).
A block is simply:

* A set of variables the block declares
* Possibly a merge input (see below)
* A set of instructions in this block
* Sub-blocks (see below)

Within a block, there are no control-flow operations; either all or none of the block is executed.
A block is _always_ terminated with a control-flow operation, so for example there is always a
`return` instruction at the end of a function or a `continue` at the end of a loop body.

Non-diverging control-flow operations (`OpCode::Discard`, `OpCode::Return`, `OpCode::Break`,
`OpCode::Continue`, `OpCode::Passthrough`, `OpCode::LoopIf` and `OpCode::Merge`) exit the block and
jump to a predetermined location, defined by a previous block.
There are no sub-blocks when a block is terminated by these operations.

Some operations however diverge the control flow.
They always include a "merge" sub-block, which is where control flow reconverges after the
sub-blocks terminate.

* `OpCode::If`: Based on the condition, either the true or false sub-block is executed.
* `OpCode::Loop`: The condition sub-block is executed, and if its terminating `OpCode::LoopIf`
  evaluates to true, the body sub-block is executed.
  * If a sub-block terminates with `OpCode::Continue`, control flow jumps back to the continue
    block.
  * If a sub-block terminates with `OpCode::Break`, control flow jumps to the merge sub-block.
* `OpCode::DoLoop`: Same as `OpCode::Loop`, except the body sub-block is executed first.
* `OpCode::Switch`: Based on the switch expression, one of the case sub-blocks is executed.
  * If a case sub-block terminates with `OpCode::Passthrough`, control flow jumps to the next case
    in the switch operation's list of case sub-blocks.
  * If a case sub-block terminates with `OpCode::Break`, control flow jumps to the merge sub-block.
* `OpCode::NextBlock`: This is a special control flow instruction that doesn't actually diverge
  control flow but chains blocks.
  It's useful for transformations that might remove control-flow, such as constant folding an `if`,
  or that might stitch blocks, for example to prepend a block to the beginning of the shader.
  Control flow in this case unconditionally jumps to the merge sub-block.

The merge sub-block may optionally include a "merge input".
This is the new preferred replacement for SSA's "phi" operator.
A merge block's input is a register whose value is determined by the blocks that merge into it (via
`OpCode::Merge`).
Merge inputs are present to avoid excessive use of temporary variables when generating SPIR-V, which
uses SPIR-V's `OpPhi` instruction for ternary operators and short-circuits.
The ANGLE IR's use of merge inputs is currently limited to a single input for simplicity, enough to
cover the SPIR-V generator's use-case, and is used in the same scenarios.

The following is an example of control flow using `OpCode::If` with a merge input, corresponding to
`v3 = v1 ? v2 : c1` (where `vi` are variables and `c1` is a constant):

```
+-------------------+
| r1 = Load v1      |
| If r1             |
+-------------------+
 | | |
 | |  -- true block --> +-------------------+
 | |                    | r2 = Load v2      |
 | |                    | Merge r2          |..
 | |                    +-------------------+  .
 | |                                           .
 |  -- false block --> +-------------------+   .
 |                     | Merge c1          |.  .
 |                     +-------------------+ . .
 |                                           . .
 | merge block                               . .
 V                                           . .
+-------------------+ <... merge target .......
| Input: r3         |
| Store v3 r3       |
+-------------------+
```

The following is an example of control flow using `OpCode::Loop` with a continue block,
corresponding to `for (int v1 = c1; v1 < c2; ++v1) {...}`:

```
+-------------------+
| Declare v1 = c1   |
| Loop              |
+-------------------+
 | | | |                                  continue target
 | | |  -- condition --> +---------------------+ <...
 | | |                   | r1 = Load v1        |     .
 | | |                   | r2 = LessThan r1 c2 |     .
 | | |                   | LoopIf r2           |..   .
 | | |                   +---------------------+  .  .
 | | |                                            .  .
 | |  --- body ---> +-------------------+         .  .
 | |                | ...               |         .  .
 | |                | Continue          |.....    .  .
 | |                +-------------------+     .   .  .
 | |                                          .   .  .
 | |                     body continue target .   .  .
 |  -- continue --> +--------------------+ <..    .  .
 |                  | PrefixIncrement v1 |        .  .
 |                  | Continue           |        .  .
 |                  +--------------------+...........
 |                                                .
 | merge block                                    .
 V                                                .
+-------------------+ <... merge target ..........
| ...               |
+-------------------+
```

## Debug Output

The IR can be dumped to `stdout` by calling `debug::dump(&ir);`.
This section explains the output from this utility for the following simple GLSL shader:

```
#version 310 es
precision mediump float;

// Miscellaneous metadata:
layout(early_fragment_tests) in;

// A global interface variable:
out vec4 color;

void main()
{
    // A local variable with initializer:
    vec2 result = vec2(0, 0);

    // A loop with a continue block
    for (int i = 0; i < 10; ++i)
    {
        // Swizzle, constructor and built-in:
        result.yx += vec2(i, exp(float(i)));
    }

    // Ternary operator:
    color = result.x > result.y ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);
}
```

The following IR is generated from the above shader:

```
Fragment Shader
[Early fragment tests]

Types:
  t0: void
  t1: float
  t2: int
  t3: uint
  t4: bool
  t5: atomic_uint
  t6: yuvCscStandardEXT
  t7: Vector of t1[2]
  t8: Vector of t1[3]
  t9: Vector of t1[4]
  t10: Vector of t2[2]
  t11: Vector of t2[3]
  t12: Vector of t2[4]
  t13: Vector of t3[2]
  t14: Vector of t3[3]
  t15: Vector of t3[4]
  t16: Vector of t4[2]
  t17: Vector of t4[3]
  t18: Vector of t4[4]
  t19: Matrix of t7[2]
  t20: Matrix of t8[2]
  t21: Matrix of t9[2]
  t22: Matrix of t7[3]
  t23: Matrix of t8[3]
  t24: Matrix of t9[3]
  t25: Matrix of t7[4]
  t26: Matrix of t8[4]
  t27: Matrix of t9[4]
  t28: Pointer to t9
  t29: Pointer to t7
  t30: Pointer to t2
  t31: Pointer to t1

Constants:
  c0 (t4): false
  c1 (t4): true
  c2 (t1): 0.0
  c3 (t1): 1.0
  c4 (t2): 0
  c5 (t2): 1
  c6 (t3): 0
  c7 (t3): 1
  c8 (t6): itu_601
  c9 (t6): itu_601_full_range
  c10 (t6): itu_709
  c11 (t7): composite(c2, c2)
  c12 (t2): 10
  c13 (t9): composite(c2, c3, c2, c3)
  c14 (t9): composite(c3, c2, c2, c3)

Variables:
  v0 (t28): '_ucolor' [mediump, output]
  v1 (t29): 'tresult_1'=c11 [mediump]
  v2 (t30): 'ti_2'=c4 [mediump]

Globals: v0

Functions:

  f0: 'main'() -> t0

    __________________
    Entry To Function:
    Declare: v1, v2
                   Loop
    |
    +-> _______________
    |   Loop Condition:
    |   r0   (t2) =    Load v2                                       [mediump]
    |   r1   (t4) =    LessThan r0 c12[mediump]
    |                  LoopIf r1
    |
    +-> __________
    |   Loop Body:
    |   r3  (t29) =    AccessVectorComponentMulti v1 (1, 0)          [mediump]
    |   r4   (t2) =    Load v2                                       [mediump]
    |   r5   (t1) =    ConstructScalarFromScalar r4                  [mediump]
    |   r6   (t1) =    Exp r5                                        [mediump]
    |   r7   (t2) =    Load v2                                       [mediump]
    |   r8   (t7) =    ConstructVectorFromMultiple (r7, r6)          [mediump]
    |   r9   (t7) =    Load r3                                       [mediump]
    |   r10   (t7) =   Add r9 r8                                     [mediump]
    |                  Store r3 r10
    |                  Continue
    |
    +-> ____________________
    |   Loop Continue Block:
    |   r2   (t2) =    PrefixIncrement v2                            [mediump]
    |                  Continue
    |
    V___________
    Merge Block:
    r11  (t31) =   AccessVectorComponent v1 0                    [mediump]
    r12  (t31) =   AccessVectorComponent v1 1                    [mediump]
    r13   (t1) =   Load r12                                      [mediump]
    r14   (t1) =   Load r11                                      [mediump]
    r15   (t4) =   GreaterThan r14 r13
                   If r15
    |
    +-> ______________
    |   If True Block:
    |                  Merge c13[mediump]
    |
    +-> _______________
    |   If False Block:
    |                  Merge c14[mediump]
    |
    V___________
    Merge Block:
    Input: r16 (t9) [mediump]
                   Store v0 r16
                   Return
```

The above IR is explained piecemeal below:

First, the shader type and metadata are specified:

```
Fragment Shader
[Early fragment tests]
```

The list of types follows, with scalar, vector and matrix types predefined and always present.
The types are identified by `tN`, with `N` being the `TypeId` value.

```
Types:
  t0: void
  t1: float
  t2: int
  ...
  t4: bool
  ...
  t7: Vector of t1[2]     <--- corresponding to GLSL's vec2
  ...
  t9: Vector of t1[4]     <--- corresponding to GLSL's vec4
  ...
  ...
  t28: Pointer to t9
  t29: Pointer to t7
  t30: Pointer to t2
  t31: Pointer to t1
```

The constants are output next, identified by `cN`, with `N` being the `ConstantId` value.
The type of the constant is specified in parenthesis.

```
Constants:
  ...
  c2 (t1): 0.0
  c3 (t1): 1.0
  c4 (t2): 0
  c5 (t2): 1
  ...
  c11 (t7): composite(c2, c2)
  c12 (t2): 10
  c13 (t9): composite(c2, c3, c2, c3)
  c14 (t9): composite(c3, c2, c2, c3)
```

The list of all variables (regardless) of scope follow, identified by `vN`, with `N` being the
`VariableId` value.
Similar to constants, the type of the variable is specified in parentheses.
The name of the variable is specified in quotations; currently, temporary variable names are
prefixed with `t` and suffixed by `_N` to disambiguate them, interface variables are prefixed with
`_u` per the legacy AST's convention, and built-ins and internal names are output as-is.
If the variable has an initializer, it's output as `=cM`.
The precision and other decorations of the variable are output as a list in brackets.

```
Variables:
  v0 (t28): '_ucolor' [mediump, output]
  v1 (t29): 'tresult_1'=c11 [mediump]
  v2 (t30): 'ti_2'=c4 [mediump]
```

The list of variables in the global scope are provided before the functions are output.

```
Globals: v0
```

The functions are output next, identified by `fN`, with `N` being the `FunctionId` value, each
starting with the function prototype:

```
Functions:

  f0: 'main'() -> t0
```

In the above, the function `'main'` takes no argument and its return type is `t0` (i.e. `void`).

The entry block of the function is the first block to be written.
In this example, this block defines two variables `v1` and `v2` (`result` and `i` respectively).
The sole instruction in this block is the `Loop` instruction, which corresponds to the beginning of
the `for` loop in the shader.
The `for` loop's init expression would go in this block, which in this case is only the declaration
of variable `v2` with a constant initializer.

```
    __________________
    Entry To Function:
    Declare: v1, v2
                   Loop
```

This block has four sub-blocks:

```
    |
    +-> _______________
    |   Loop Condition:
    ...
    |                  LoopIf r1
    |
    +-> __________
    |   Loop Body:
    ...
    |                  Continue
    |
    +-> ____________________
    |   Loop Continue Block:
    ...
    |                  Continue
    |
    V___________
    Merge Block:
    ...
```

The "Loop Condition" block executes before entering the loop body; the `LoopIf` terminating this
block decides if the body should be entered, or control flow should move to the "Merge Block".

The "Loop Body" is the contents of the loop body, as expected.
The "Continue" instruction, when executed, moves control flow to the "Loop Continue Block", if any,
or the "Loop Condition" block otherwise.

The "Loop Continue Block" corresponds to the `for` loop's continue expression and always terminates
with `Continue`, moving control flow back to the "Loop Condition".

The loop condition makes a decision based on a boolean value:

```
    +-> _______________
    |   Loop Condition:
    |   r0   (t2) =    Load v2                                       [mediump]
    |   r1   (t4) =    LessThan r0 c12[mediump]
    |                  LoopIf r1
```

In the above, the value of `v2` is loaded in register 0.
Registers are identified by `rN`, with `N` being the `RegisterId` value.
The type of the result is specified in parentheses.
The precision of this operation is specified in brackets on the right.

Then, `r0` is compared with the constant `c12`.
Constants do not intrinsically have a precision, but are assigned a precision in the context of the
instruction based on GLSL rules, in this case `c12` is assigned a `mediump` precision specified in
brackets next to the constant.
The result of this operation is stored in `r1`.

Finally, the decision to execute the loop body or break out of the loop is made based on `r1`.

The loop body executes a number of instructions:

The pointer `v1` is swizzled with `.yx`; `r3` holds this pointer:

```
    +-> __________
    |   Loop Body:
    |   r3  (t29) =    AccessVectorComponentMulti v1 (1, 0)          [mediump]
```

The value of `v2` is loaded (which is of type `t2` (`int`)) and cast to `t1` (`float`) with a
constructor of scalar from scalar:

```
    |   r4   (t2) =    Load v2                                       [mediump]
    |   r5   (t1) =    ConstructScalarFromScalar r4                  [mediump]
```

The `exp()` built-in is applied to the result:

```
    |   r6   (t1) =    Exp r5                                        [mediump]
```

The value of `v2` is loaded again (unnecessarily, as no optimization passes have run) for the first
argument of the constructor `vec2(i, exp(float(i)))` with the second argument calculated above in
`r6` already:

```
    |   r7   (t2) =    Load v2                                       [mediump]
    |   r8   (t7) =    ConstructVectorFromMultiple (r7, r6)          [mediump]
```

Finally, the add-assign operator is implemented as a load from the swizzled pointer, add and store
back to the same pointer:

```
    |   r9   (t7) =    Load r3                                       [mediump]
    |   r10   (t7) =   Add r9 r8                                     [mediump]
    |                  Store r3 r10
```

As described previously, the body terminates with a `Continue` instruction:

```
    |                  Continue
```

The loop continue block increments `i` and continues the loop:

```
    +-> ____________________
    |   Loop Continue Block:
    |   r2   (t2) =    PrefixIncrement v2                            [mediump]
    |                  Continue
```

Once the loop terminates, control flow is converged to the merge block.
The merge block continues the shader to evaluate the ternary operator.
The IR does not include a ternary operator, and implements this operation with an `if`/`else`.

First, the components of `v1` are compared:

```
    |
    V___________
    Merge Block:
    r11  (t31) =   AccessVectorComponent v1 0                    [mediump]
    r12  (t31) =   AccessVectorComponent v1 1                    [mediump]
    r13   (t1) =   Load r12                                      [mediump]
    r14   (t1) =   Load r11                                      [mediump]
    r15   (t4) =   GreaterThan r14 r13
```

This block is terminated with an `if` based on the previously calculated condition:

```
                   If r15
```

This block has three sub-blocks:

```
    |
    +-> ______________
    |   If True Block:
    ...
    |
    +-> _______________
    |   If False Block:
    ...
    |
    V___________
    Merge Block:
    Input: r16 (t9) [mediump]
    ...
```

Importantly, the merge block includes a merge input, specified after `Input:` as a register (`r16`)
with its type and precision specified as usual in parentheses in brackets respectively.

The true and false blocks in this example are simple, they each assign a constant value to the merge
input:

```
    +-> ______________
    |   If True Block:
    |                  Merge c13[mediump]
    |
    +-> _______________
    |   If False Block:
    |                  Merge c14[mediump]
```

When the merge block of this `if` construct is executed, the merge input will contain either `c13`
or `c14` based on the path that executed before merging into this block.
This is equivalent to SSA's "phi" instruction with similar semantics, but is much easier to reason
about; think of the merge block's input register as a nameless temporary variable, with `Merge id`
storing into this variable and the merge block loading from it.

Finally, the merge block assigns the ternary operator's result (found in the merge input `r16`) to
the output variable `color`:

```
    |
    V___________
    Merge Block:
    Input: r16 (t9) [mediump]
                   Store v0 r16
```

This final block of the function ends with a `Return` statement:


```
                   Return
```

## Visitors

A read-only pass on the IR (a "visit") is useful to gather information, such as shader reflection
information, or to generate an output from it, such as the translated shader or the debug dump.

From a high level, visiting the IR is very simple:

```
* For each function
  * Visit the entry block (including its instructions)
  * Recursively visit the subblocks
```

There are helpers provided for this traversal, but it could just as easily be done free-form if the
helper is too restrictive.
These helpers can be found in the `traverser::visitor` module:

* `traverser::visitor::for_each_function` runs a callback for each function itself, then another
  callback for each of its blocks, and then a post-visit callback for the function.
  The callback is passed a `BlockKind` that describes what it refers to (like an `if`'s true block,
  a loop's continue block, etc).
  The block callback returns a value to control which sub-blocks to visit next:
  * `VISIT_SUB_BLOCKS`: continue to visit all sub-blocks
  * `SKIP_TO_MERGE_BLOCK`: in GLSL terms, skip nested blocks and continue in the same scope.
    For example, an `if`'s true and false blocks are skipped, and traversal continues to the
    instruction after the `if` blocks.
  * `STOP`: stop the traversal

* `traverser::visitor::visit` is used by `for_each_function`, taking the same block callback, and
  can be used to traverse a specific block instead of the whole IR.

* `traverser::visitor::visit_block_instructions` takes a few callbacks to visit the instructions
  within the block:
  * A callback to visit the block itself, returning a generic `BlockResult` type
  * A callback to visit each non-branch instruction, taking the `BlockResult`
  * A callback to visit the terminating branch instruction, taking the `BlockResult` of the current
    block, but also the `BlockResult` of the sub-blocks.
    This means that the branch instruction is not visited until all (non-merge) sub-blocks are
    visited first.
  * If there is a merge block, this function repeats the above until the chain of merge blocks are
    all visited.
    The list of `BlockResult`s from this merge chain is passed to one final callback to aggregate
    the results into a different `BlockResult` which is returned from this function.

For examples of visitor traversals, see `debug.rs` which dumps the IR and `duplicate_block()` in
`util.rs` which makes a copy of a block (while replacing variables and creating new register ids).

## Transformations

The instructions in the IR are found in a graph (DAG) of blocks, with every block containing a
vector of non-branch instructions, ending with a branch instruction.
As such, it is not onerous to write free-form transformations, as might be necessary for complex
transformations such as dead-code elimination, by creating blocks and stitching them together,
adding instructions anywhere in the vector, or in-place modify the instructions as necessary.

For common transformations, a set of utilities are provided.

### Generating Instructions

There are macros to create an instruction.
`instruction::make!()` creates an instruction, which is either void or has a new register as its
result.
For example:

```
let negate_operand = instruction::make!(negate, ir_meta, operand);
let add_operands   = instruction::make!(add, ir_meta, lhs, rhs);
```

Transformations will frequently need to make localized changes, which ideally wouldn't perturb the
rest of the IR.
To support this, it's possible to:

* Reassign an instruction to a new register with `IRMeta::assign_new_register_to_instruction()`
* Make an instruction whose result is the old register id with `instruction::make_with_result_id!()`

With the above, it's possible for example to transform:

```
id = SomeOpCode ...
```

to:

```
new_id = SomeOpCode ...
// transformations on new_id, ending with:
id = ...
```

And the rest of the IR would continue to use register `id` as usual, and does not need to be
modified to use some new register id.

### Transformation During Traversal

The following helpers are very high level, and simply iterate over the functions and blocks, issuing
a callback:

* `traverser::transformer::for_each_function` takes a few callbacks to transform the function:
  * A callback to visit the function itself in preparation for the transformation
  * A callback to transform the blocks, given the function's entry block

* `traverser::transformer::for_each_block` takes callbacks to apply to blocks as it traverses them.
  Each block is visited once before and once after the sub-blocks.
  Both callbacks may modify the block as desired and return a reference to the block from where
  traversal may continue.
  In practice, there are two common choices:
  * If the callback returns a reference to the original block, all the sub-blocks will get traversed
  * If the callback has already processed the sub-blocks (including e.g. because it generated new
    ones itself), it can return a reference to a merge block (in the chain of merge blocks) for
    traversal to continue.
    For example, if the callback splits a block to add an `if` in the middle, it may return the
    merge block so that the true and false blocks of the if are skipped and not traversed.

Another set of helpers take a callback that transforms an instruction, and returns a vector of
`Transform` operations to be applied by the helper in order:

* If an empty vector is returned, the instruction is taken as is.
  Otherwise, the instruction is _removed_ and replaced with the transformation.
* `Transform::Keep` indicates that the original instruction must be kept as-is.
* `Transform::Remove` indicates that the original instruction should be dropped.
  This is normally not needed, since the original instruction is always dropped when there is a
  transformation, but is useful if the only transformation is to drop the instruction.
* `Transform::Add` takes an instruction and adds it to the current block
* `Transform::AddBlock` takes a block and adds its instructions to the current block.
  This is useful if the transformation adds a block that has sub-blocks, in which case the current
  block is split, and the rest of the instructions are placed at the end of the merge chain of the
  given block.
* `Transform::DeclareVariable` takes a variable id (where the variable is already declared in the IR
  by the callback) and adds it to the list of variables to be declared by this block.

Since these `Transform`s do not provide facilities to directly manipulate the branch instruction,
the callback is not allowed to transform the terminating branch instruction.
However, it is possible to prepend instructions to it by using `Transform::Keep` at the end of the
list of transforms.

These helpers are:

* `traverser::transformer::for_each_instruction`, takes such a callback and runs it for every
  instruction of every function.
* `traverser::transformer::transform_block`, takes such a callback and runs it for every instruction
  the given block and its sub-blocks.

See the transformations under `transform/` for examples.

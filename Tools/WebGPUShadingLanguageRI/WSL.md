# WebGPU Shading Language

WebGPU Shading Language, or WSL for short, is a type-safe low-overhead programming language for GPUs (graphics processing units). This document explains how WSL works.

# Goals

WSL is designed to achieve the following goals:

- WSL should feel *familiar* to C++ programmers.
- WSL should be have a *sound* and *decidable* type system.
- WSL should not permit *out-of-bounds* memory accesses.
- WSL should be have *low overhead*.

The combination of a sound type system and bounds checking makes WSL a secure shader language: the language itself is responsible for isolating the shader from the rest of the GPU.

# Familiar Syntax

WSL is based on C syntax, but excludes features that are either unnecessary, insecure, or replaced by other WSL features:

- No strings.
- No `register`, `volatile`, `const`, `restrict`, or `extern` keywords.
- No unions. `union` is not a keyword.
- No goto or labels. `goto` is not a keyword.
- No `*` pointers.
- Effectless expressions are not statements (`a + b;` is a parse error).
- No undefined values (`int x;` initializes x to 0).
- No automatic type conversions (`int x; uint y = x;` is a type error).
- No recursion.
- No dynamic memory allocation.
- No modularity. The whole program is one file.

On top of this bare C-like foundation, WSL adds secure versions of familiar C++ features:

- Type-safe pointers (`^`) and array references (`[]`).
- Generics to replace templates.
- Operator overloading. Built-in operations like `int operator+(int, int)` are just native functions.
- Cast overloading. Built-in casts like `operator int(double)` are just native functions.
- Getters and setters. Property accesses like `vec.x` resolve to overloads like `float operator.field(float4)`.
- Array access overloading. Array accesses like `vec[0]` resolve to verloads like `float operator[](float4, uint)`.

In the following sections, WSL is shown by example starting with its C-like foundation and then building up to include more sophisticated features like generics.

## Common subset of C and WSL

The following is a valid WSL function definition:

    int foo(int x, int y, bool p)
    {
        if (p)
            return x - y;
        return x + y;
    }

WSL source files behave similarly to C source files:

- Top-level statements must be type or function definitions.
- WSL uses structured C control flow constructs, like `if`, `while`, `for`, `do`, `break`, and `continue`.
- WSL uses C-like `switch` statements, but does not allow them to overlap other control flow (i.e. no [Duff's device](https://en.wikipedia.org/wiki/Duff%27s_device)).
- WSL allows variable declarations anywhere C++ would.

WSL types differ from C types. For example, this is an array of 42 integers in WSL:

    int[42] array;

The type never surrounds the variable, like it would in C (`int array[42]`).

## Type-safe pointers

WSL includes a secure pointer type. To emphasize that it is not like the C pointer, WSL uses `^` for the pointer type and for dereference. Like in C, `&` is used to take the address of a value.  Because GPUs have different kinds of memories, pointers must be annotated with an address space (one of `thread`, `threadgroup`, `device`, or `constant`). For example:

    void bar(thread int^ p)
    {
        ^p += 42;
    }
    int foo()
    {
        int x = 24;
        bar(&x);
        return x; // Returns 66.
    }

Pointers can be `null`. Each pointer access is null-checked, though most pointer accesses (like this one) will not have a null check. WSL places enough constraints on the programmer that programs are easy for the compiler to analyze. The compiler will always know that `^p += 42` adds `42` to `x` in this case.

WSL pointers do not support casting or pointer arithmetic. All memory accessible to a shader outlives the shader. This is even true of local variables. This is possible because WSL does not support recursion. Therefore, local variables simply get global storage. Local variables are initialized at the point of their declaration. Hence, the following is a valid program, which will exhibit the same behavior on every WSL implementation:

    thread int^ foo()
    {
        int x = 42;
        return &x;
    }
    int bar()
    {
        thread int^ p = foo();
        thread int^ q = foo();
        ^p = 53;
        return ^q; // Returns 53.
    }
    int baz()
    {
        thread int^ p = foo();
        ^p = 53;
        foo();
        return ^p; // Returns 42.
    }

It's possible to point to any kind of data type. For example, `thread double[42]^` is a pointer to an array of 42 doubles.

## Type-safe array references

WSL supports array references that carry a pointer to the base of the array and the array's length. This allows accesses to the array to be bounds-checked.

An array reference can be created using the `@` operator:

    int[42] array;
    thread int[] arrayRef = @array;

Both arrays and array references can be loaded from and stored to using `operator[]`:

    int x = array[i];
    int y = arrayRef[i];

Both arrays and array references know their length:

    uint arrayLength = array.length;
    uint arrayRefLength = arrayRef.length;

Given an array or array reference, it's possible to get a pointer to one of its elements:

    thread int^ ptr1 = &array[i];
    thread int^ ptr2 = &arrayRef[i];

A pointer is like an array reference with one element. It's possible to perform this conversion:

    thread int[] ref = @ptr1;
    ref[0] // Equivalent to ^ptr1.
    ref.length // 0 if ptr1 was null, 1 if ptr1 was not null.

Similarly, using `@` on a non-pointer value results in a reference of length 1:

    int x;
    thread int[] ref = @x;
    ref[0] // Aliases x.
    ref.length // Returns 1.

It's not legal to use `@` on an array reference:

    thread int[] ref;
    thread int[][] referef = @ref; // Error!

## Generics

WSL supports generic types using a simple syntax. WSL's generic are designed to integrate cleanly into the compiler pipeline:

- Generics have unambiguous syntax.
- Generic functions can be type checked before they are instantiated.

Semantic errors inside generic functions show up once regardless of the number of times the generic function is instantiated.

This is a simple generic function:

    T identity<T>(T value)
    {
        T tmp = value;
        return tmp;
    }

WSL also supports structs, which are also allowed to be generic:

    // Not generic.
    struct Foo {
        int x;
        double y;
    }
    // Generic.
    struct Bar<T, U> {
        T x;
        U y;
    }

Type parameters can also be constant expressions. For example:

    void initializeArray<T, uint length>(thread T[length]^ array, T value)
    {
        for (uint i = length; i--;)
            (^array)[i] = value;
    }

Constant expressions passed as type arguments must obey a very narrow definition of constantness. Only literals and references to other constant parameters qualify.

WSL is guaranteed to compile generics by instantiation. This is observable, since functions can return pointers to their locals. Here is an example of this phenomenon:

    thread int^ allocate<uint>()
    {
        int x;
        return &x;
    }

The `allocate` function will return a different pointer for each unsigned integer constant passed as a type parameter. This allocation is completely static, since the `uint` parameter must be given a compile-time constant.

WSL's `typedef` uses a slightly different syntax than C. For example:

    struct Complex<T> {
        T real;
        T imag;
    }
    typedef FComplex = Complex<float>;

`typedef` can be used to create generic types:

    struct Foo<T, U> {
        T x;
        U y;
    }
    typedef Bar<T> = Foo<T, T>;

## Protocols

Protocols enable generic functions to work with data of generic type. Because a function must be type-checkable before instantiation, the following would not be legal:

    int bar(int) { ... }
    double bar(double) { ... }
    
    T foo<T>(T value)
    {
        return bar(value); // Error!
    }

The call to `bar` doesn't type check because the compiler cannot know that `foo<T>` will be instantiated with `T = int` or `T = double`. Protocols enable the programmer to tell the compiler what to expect of a type variable:

    int bar(int) { ... }
    double bar(double) { ... }
    
    protocol SupportsBar {
        SupportsBar bar(SupportsBar);
    }
    
    T foo<T:SupportsBar>(T value)
    {
        return bar(value);
    }
    
    int x = foo(42);
    double y = foo(4.2);

Protocols have automatic relationships to one another based on what functions they contain:

    protocol Foo {
        void foo(Foo);
    }
    protocol Bar {
        void foo(Bar);
        void bar(Bar);
    }
    void fuzz<T:Foo>(T x) { ... }
    void buzz<T:Bar>(T x)
    {
        fuzz(x); // OK, because Bar is more specific than Foo.    
    }

Protocols can also mix other protocols in explicitly. Like in the example above, the example below relies on the fact that `Bar` is a more specific protocol than `Foo`. However, this example declares this explicitly (`protocol Bar : Foo`) instead of relying on the language to infer it:

    protocol Foo {
        void foo(Foo);
    }
    protocol Bar : Foo {
        void bar(Bar);
    }
    void fuzz<T:Foo>(T x) { ... }
    void buzz<T:Bar>(T x)
    {
        fuzz(x); // OK, because Bar is more specific than Foo.    
    }

## Overloading

WSL supports overloading very similarly to how C++ does it. For example:

    void foo(int);    // 1
    void foo(double); // 2
    
    int x;
    foo(x); // calls 1
    
    double y;
    foo(y); // calls 2

WSL automatically selects the most specific overload if given multiple choices. For example:

    void foo(int);    // 1
    void foo(double); // 2
    void foo<T>(T);   // 3

    foo(1); // calls 1
    foo(1.); // calls 2
    foo(1u); // calls 3

Generic functions like `foo<T>` can be called with or without all of their type arguments supplied. If they are not supplied, the function participates in overload resolution like any other function would. Functions are ranked by how specific they are; function A is more specific than function B if A's parameter types can be used as argument types for a call to B but not vice-versa. If one functions more specific than all others then this function is selected, otherwise a type error is issued.

## Operator Overloading

Many WSL operations desugar to calls to functions. Those functions are called *operator overloads* and are declared using syntax that involves the keyword `operator`. The following operations result in calls to operator overloads:

- Numerical operators (`+`, `-`, `*`, `/`, etc.).
- Increment and decrement (`++`, `--`).
- Casting (`type(value)`).
- Accessing values in arrays (`[]`, `[]=`, `&[]`).
- Accessing fields (`.field`, `.field=`, `&.field`).

WSL's operator overloading is designed to synthesize many operators for you:

- Read-modify-write operators like `+=` are desugared to a load of the load value, the underlying operator (like `+`), and a store of the new value. It's not possible to override `+=`, `-=`, etc.
- `x++` and `++x` both call the same operator overload. `operator++` takes the old value and returns a new one; for example the built-in `++` for `int` could be written as: `int operator++(int value) { return value + 1; }`.
- `operator==` can be overloaded, but `!=` is automatically synthesized and cannot be overloaded.

Some operators and overloads are restricted:

- `!`, `&&`, and `||` are built-in operations on the `bool` type.
- Self-casts (`T(T)`) are always the identity function.
- Casts with no arguments (`T()`) always return the default value for that type. Every type has a default value (`0`, `null`, or the equivalent for each field).

Cast overloading allows for supporting conversions between types and for creating constructors for custom types. Here is an example of cast overloading being used to create a constructor:

    struct Complex<T> {
        T real;
        T imag;
    }
    operator<T> Complex<T>(T real, T imag)
    {
        Complex<T> result;
        result.real = real;
        result.imag = imag;
        return result;
    }
    
    Complex<float> i = Complex<float>(0, 1);

WSL supports accessor overloading as part of the operator overloading syntax. This gives the programmer broad powers. For example:

    struct Foo {
        int x;
        int y;
    }
    int operator.sum(Foo foo)
    {
        return foo.x + foo.y;
    }

It's possible to say `foo.sum` to call the `operator.sum` function. Both getters and setters can be provided:

    struct Foo {
        int value;
    }
    double operator.doubleValue(Foo value)
    {
        return double(value.value);
    }
    Foo operator.doubleValue=(Foo value, double doubleValue)
    {
        value.value = int(doubleValue);
        return value;
    }

Providing both getter and setter overloads makes `doubleValue` behave almost as if it was a field of `Foo`. For example, it's possible to say:

    Foo foo;
    foo.value = 42;
    foo.doubleValue *= 2; // Now foo.value is 84

It's also possible to provide an address-getting overload called an *ander*:

    struct Foo {
        int value;
    }
    thread int^ operator&.valueAlias(thread Foo^ foo)
    {
        return &foo->value;
    }

Providing just this overload for a pointer type in every address space gives the same power as overloading getters and setters. Additionally, it makes it possible to `&foo.valueAlias`.

The same overloading power is provided for array accesses. For example:

    struct Vector<T> {
        T x;
        T y;
    }
    thread T^ operator&[](thread T^ ptr, uint index)
    {
        return index ? &ptr->y : &ptr->x;
    }

Alternatively, it's possible to overload getters and setters (`operator[]` and `operator[]=`).

# Mapping of API concepts

WSL is designed to be useful as both a graphics shading language and as a computation language. However, these two environments have
slightly different semantics.

When using WSL as a graphics shading language, there is a distinction between *entry-points* and *non-entry-points*. Entry points are top-level functions which have either the `vertex` or `fragment` keyword in front of their declaration. Entry points may not be forward declared. An entry point annotated with the `vertex` keyword may not be used as a fragment shader, and an entry point annotated with the `fragment` keyword may not be used as a vertex shader. No argument nor return value of an entry point may be a pointer. Entry points must not accept type arguments (also known as "generics").

## Vertex entry points

WebGPU's API passes data to a WSL vertex shader in four ways:

- Attributes
- Buffered data
- Texture data
- Samplers

Each of these API objects is referred to by name from the API. Variables in WSL are not annotated with extra API-visible names (like they are in some other graphics APIs).

Variables are passed to vertex shaders as arguments to a vertex entry point. Each buffer is represented as an argument with an array reference type (using the `[]` syntax). Textures and samplers are represented by arguments with the `texture` and `sampler` types, respectively. All other non-builtin arguments to a vertex entry point are implied to be attributes.

Some arguments are recognized by the compiler from their name and type. These arguments provide built-in functionality inherent in the graphics pipeline. For example, an argument of the form `int wsl_vertexID` refers to the ID of the current vertex, and is not recognized as an attribute. All non-builtin arguments to a vertex entry point must be associated with an API object whenever any draw call using the vertex entry point is invoked. Otherwise, the draw call will fail.

The only way to pass data between successive shader stages within a single draw call is by return value. An entry point must indicate that it returns a collection of values contained within a structure. Every variable inside this structure, recursively, is passed to the next stage in the graphics pipeline. Members of this struct may also be output built-in variables. For example, a vertex entry point may return a struct which contains a member `float4 wsl_Position`, and this variable will represent the rasterized position of the vertex. Buffers (as described by WSL array references), textures, and samplers must not be present in this returned struct. Built-in variables must never appear twice inside the returned structure.

## Fragment entry points

Fragment entry points may accept one argument with the type that the previous shader stage returned. The argument name for this argument must be `stageIn`. In addition to this argument, fragment entry points may accept buffers, textures, and samplers as arguments in the same way that vertex entry points accept them. Fragment entry points also must return a struct, and all members of this struct must be built-in variables. The set of recognized built-in variables which may be accepted or returned from an entry point is different between all types of entry points.

For example, this would be a valid graphics program:

    struct VertexInput {
        float2 position;
        float3 color;
    }
    
    struct VertexOutput {
        float4 wsl_Position;
        float3 color;
    }
    
    struct FragmentOutput {
        float4 wsl_Color;
    }
    
    vertex VertexOutput vertexShader(VertexInput vertexInput) {
        VertexOutput result;
        result.wsl_Position = float4(vertexInput.position, 0., 1.);
        result.color = vertexInput.color;
        return result;
    }
    
    fragment FragmentOutput fragmentShader(VertexOutput stageIn) {
        FragmentOutput result;
        result.wsl_Color = float4(stageIn.color, 1.);
        return result;
    }

## Compute entry points

WebGPU's API passes data to a compute shader in three ways:

- Buffered data
- Texture data
- Samplers

Compute entry points start with the keyword `compute`. The return type for a compute entry point must be `void`. Each buffer is represented as an argument with an array reference type (using the `[]` syntax). Textures and samplers are represented by arguments with the `texture` and `sampler` types, respectively. Compute entry points may also accept built-in variables as arguments. Arguments of any other type are disallowed. Arguments may not use the `threadgroup` memory space.

# Error handling

Errors may occur during shader processing. For example, the shader may attempt to dereference a `null` array reference. If this occurs, the shader stage immediately completes successfully. The entry point immediately returns a struct with all fields set to 0. After this event, subsequent shader stages will proceed as if there was no problem.

Buffer and texture reads and writes before the error all complete, and have the same semantics as if no error had occurred. Buffer and texture reads and writes after the error do not occur.

# Summary

WSL is a type-safe language based on C syntax. It eliminates some C features, like unions and pointer casts, but adds other modern features in their place, like generics and overloading.

# Additional Limitations

The following additional limitations may be placed on a WSL program:

- `device`, `constant`, and `threadgroup` pointers cannot point to data that may have pointers in it. This safety check is not done as part of the normal type system checks. It's performed only after instantiation.
- Pointers and array references (collectively, *references*) may be restricted to support compiling to SPIR-V *logical mode*. In this mode, arrays must not transitively hold references. References must be initialized upon declaration and never reassigned. Functions that return references must have one return point. Ternary expressions may not return references.
- Graphics entry points must transitively never refer to the `threadgroup` memory space.



.. WSL documentation master file, created by
   sphinx-quickstart on Thu Jun  7 15:53:54 2018.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

WSL Specification
#################

Grammar
=======

Lexical analysis
----------------

Before parsing, the text of a WSL program is first turned into a list of tokens, removing comments and whitespace along the way.
Tokens are built greedily, in other words each token is as long as possible.
If the program cannot be transformed into a list of tokens by following these rules, the program is invalid and must be rejected.

A token can be either of:

- An integer literal
- A float literal
- Punctuation
- A keyword
- A normal identifier
- An operator name

Literals
""""""""

An integer literal can either be decimal or hexadecimal, and either signed or unsigned, giving 4 possibilities.

- A signed decimal integer literal starts with an optional ``-``, then a number without leading 0.
- An unsigned decimal integer literal starts with a number without leading 0, then ``u``.
- A signed hexadecimal integer literal starts with an optional ``-``, then the string ``0x``, then a non-empty sequence of elements of [0-9a-fA-F] (non-case sensitive, leading 0s are allowed).
- An unsigned hexadecimal inter literal starts with the string ``0x``, then a non-empty sequence of elements of [0-9a-fA-F] (non-case sensitive, leading 0s are allowed), and finally the character ``u``.

.. todo:: I chose rather arbitrarily to allow leading 0s in hexadecimal, but not in decimal integer literals. This can obviously be changed either way.

A float literal is made of the following elements in sequence:

- an optional ``-`` character
- a sequence of 0 or more digits (in [0-9])
- a ``.`` character
- a sequence of 0 or more digits (in [0-9]). This sequence must instead have 1 or more elements, if the last sequence was empty.
- optionally a ``f`` or ``d`` character

In regexp form: '-'? ([0-9]+ '.' [0-9]* | [0-9]* '.' [0-9]+) [fd]?

Keywords and punctuation
""""""""""""""""""""""""

The following strings are reserved keywords of the language:

+-------------------------------+---------------------------------------------------------------------------------+
| Top level                     | struct typedef enum operator vertex fragment native restricted                  |
+-------------------------------+---------------------------------------------------------------------------------+
| Control flow                  | if else switch case default while do for break continue fallthrough return trap |
+-------------------------------+---------------------------------------------------------------------------------+
| Literals                      | null true false                                                                 |
+-------------------------------+---------------------------------------------------------------------------------+
| Address space                 | constant device threadgroup thread                                              |
+-------------------------------+---------------------------------------------------------------------------------+
| Reserved for future extension | protocol auto                                                                   |
+-------------------------------+---------------------------------------------------------------------------------+

``null``, ``true`` and ``false`` are keywords, but they are considered literals in the grammar rules later.

Similarily, the following elements of punctuation are valid tokens:

+----------------------+-----------------------------------------------------------------------------------------------+
| Relational operators | ``==`` ``!=`` ``<=`` ``=>`` ``<`` ``>``                                                       |
+----------------------+-----------------------------------------------------------------------------------------------+
| Assignment operators | ``=`` ``++`` ``--`` ``+=`` ``-=`` ``*=`` ``/=`` ``%=`` ``^=`` ``&=``  ``|=`` ``>>=``  ``<<=`` |
+----------------------+-----------------------------------------------------------------------------------------------+
| Arithmetic operators | ``+``  ``-`` ``*`` ``/`` ``%``                                                                |
+----------------------+-----------------------------------------------------------------------------------------------+
| Logic operators      | ``&&`` ``||`` ``&``  ``|``  ``^`` ``>>`` ``<<`` ``!`` ``~``                                   |
+----------------------+-----------------------------------------------------------------------------------------------+
| Memory operators     | ``->`` ``.`` ``&`` ``@``                                                                      |
+----------------------+-----------------------------------------------------------------------------------------------+
| Other                | ``?`` ``:`` ``;`` ``,`` ``[`` ``]`` ``{`` ``}`` ``(`` ``)``                                   |
+----------------------+-----------------------------------------------------------------------------------------------+

Identifiers and operator names
""""""""""""""""""""""""""""""

An identifier is any sequence of alphanumeric characters or underscores, that does not start by a digit, that is not a single underscore (the single underscore is reserved for future extension), and that is not a reserved keyword.
TODO: decide if we only accept [_a-zA-Z][_a-zA-Z0-9], or whether we also accept unicode characters.

Operator names can be either of the 4 following possibilities:

- the string ``operator``, followed immediately with one of the following strings: ``>>``, ``<<``, ``+``, ``-``, ``*``, ``/``, ``%``, ``&&``, ``||``, ``&``, ``|``, ``^``, ``>=``, ``<=``, ``>``, ``<``, ``++``, ``--``, ``!``, ``~``, ``[]``, ``[]=``, ``&[]``.
- the string ``operator.`` followed immediately with what would be a valid identifier x. We call this token a 'getter for x'.
- the string ``operator.`` followed immediately with what would be a valid identifier x, followed immediately with the character ``=``. We call this token 'a setter for x'.
- the string ``operator&.`` followed immediately with what would be a valid identifier x. We call this token an 'address taker for x'.

.. note:: Thanks to the rule that token are read greedily, the string "operator.foo" is a single token (a getter for foo), and not the keyword "operator" followed by the punctuation "." followed by the identifier "foo".

Whitespace and comments
"""""""""""""""""""""""

Any of the following characters are considered whitespace, and ignored after this phase: space, tabulation (``\t``), carriage return (``\r``), new line(``\n``).

.. todo:: do we want to also allow other unicode whitespace characters?

We also allow two kinds of comments, that are treated like whitespace (i.e. ignored during parsing).
The first kind is a line comment, that starts with the string ``//`` and continues until the next end of line character.
The second kind is a multi-line comment, that starts with the string ``/*`` and ends as soon as the string ``*/`` is read.

.. note:: Multi-line comments cannot be nested, as the first ``*/`` closes the outermost ``/*``

Parsing
-------

.. todo:: add here a quick explanation of BNF syntax and our conventions.

Top-level declarations
""""""""""""""""""""""

A valid file is made of a sequence of 0 or more top-level declarations, followed by the special End-Of-File token.

.. productionlist::
    topLevelDecl: ";" | `typedef` | `structDef` | `enumDef` | `funcDef`

.. todo:: We may want to also allow variable declarations at the top-level if it can easily be supported by all of our targets.
.. todo:: Decide whether we put native/restricted in the spec or not.

.. productionlist::
    typedef: "typedef" `Identifier` "=" `type` ";"

.. productionlist::
    structDef: "struct" `Identifier` "{" `structElement`* "}"
    structElement: `type` `Identifier` ";"

.. productionlist::
    enumDef: "enum" `Identifier` (":" `type`)? "{" `enumElement` ("," `enumElement`)* "}"
    enumElement: `Identifier` ("=" `constexpr`)?

.. productionlist::
    funcDef: `funcDecl` "{" `stmt`* "}"
    funcDecl: `entryPointDecl` | `normalFuncDecl` | `castOperatorDecl`
    entryPointDecl: ("vertex" | "fragment") `type` `Identifier` `parameters`
    normalFuncDecl: `type` (`Identifier` | `OperatorName`) `parameters`
    castOperatorDecl: "operator" `type` `parameters`
    parameters: "(" ")" | "(" `parameter` ("," `parameter`)* ")"
    parameter: `type` `Identifier`

.. note:: the return type is put after the "operator" keyword when declaring a cast operator, mostly because it is also the name of the created function. 

Statements
""""""""""

.. productionlist::
    stmt: "{" `stmt`* "}"
        : | `compoundStmt` 
        : | `terminatorStmt` ";" 
        : | `variableDecls` ";" 
        : | `maybeEffectfulExpr` ";"
    compoundStmt: `ifStmt` | `ifElseStmt` | `whileStmt` | `doWhileStmt` | `forStmt` | `switchStmt`
    terminatorStmt: "break" | "continue" | "fallthrough" | "return" `expr`? | "trap"

.. productionlist::
    ifStmt: "if" "(" `expr` ")" `stmt`
    ifElseStmt: "if" "(" `expr` ")" `stmt` "else" `stmt`

.. todo:: should I forbid assignments (without parentheses) inside the conditions of if/while to avoid the common mistaking of "=" for "==" ? 

The first of these two productions is merely syntactic sugar for the second:

.. math:: \textbf{if}(e) \,s \leadsto \textbf{if}(e) \,s\, \textbf{else} \,\{\}

.. productionlist::
    whileStmt: "while" "(" `expr` ")" `stmt`
    forStmt: "for" "(" (`maybeEffectfulExpr` | `variableDecls`) ";" `expr`? ";" `expr`? ")" `stmt`
    doWhileStmt: "do" `stmt` "while" "(" `expr` ")" ";"

Similarily, we desugar first for loops into while loops, and then all while loops into do while loops.
First, if the second element of the for is empty we replace it by "true".
Then, we apply the following two rules:

.. math::
    \textbf{for} (X_{pre} ; e_{cond} ; e_{iter}) \, s \leadsto \{ X_{pre} ; \textbf{while} (e_{cond}) \{ s \, e_{iter} ; \} \}

.. math::
    \textbf{while} (e)\, s \leadsto \textbf{if} (e) \textbf{do}\, s\, \textbf{while}(e)

.. productionlist::
    switchStmt: "switch" "(" `expr` ")" "{" `switchCase`* "}"
    switchCase: ("case" `constexpr` | "default") ":" `stmt`*

.. productionlist::
    variableDecls: `type` `variableDecl` ("," `variableDecl`)*
    variableDecl: `Identifier` ("=" `expr`)?

Complex variable declarations are also mere syntactic sugar.
Several variable declarations separated by commas are the same as separating them with semicolons and repeating the type for each one.
This transformation can always be done because variable declarations are only allowed inside blocks (and for loops, but these get desugared into a block, see above).

.. todo:: should I make the requirement that variableDecls only appear in blocks be part of the syntax, or should it just be part of the validation rules?

Types
"""""

.. productionlist::
    type: `addressSpace` `Identifier` `typeArguments` `typeSuffixAbbreviated`+
        : | `Identifier` `typeArguments` `typeSuffixNonAbbreviated`*
    addressSpace: "constant" | "device" | "threadgroup" | "thread"
    typeSuffixAbbreviated: "*" | "[" "]" | "[" `IntLiteral` "]"
    typeSuffixNonAbbreviated: "*" `addressSpace` | "[" "]" `addressSpace` | "[" `IntLiteral` "]"


Putting the address space before the identifier is just syntactic sugar for having that same address space applied to all type suffixes.
``thread int *[]*[42]`` is for example the same as ``int *thread []thread *thread [42]``.

.. productionlist::
    typeArguments: "<" (`typeArgument` ",")* `addressSpace`? `Identifier` "<" 
                 : (`typeArgument` ("," `typeArgument`)*)? ">>"
                 : | "<" (`typeArgument` ("," `typeArgument`)* ">"
                 : | ("<" ">")?
    typeArgument: `constepxr` | `type`

The first production rule for typeArguments is a way to say that `>>` can be parsed as two `>` closing delimiters, in the case of nested typeArguments.

Expressions
"""""""""""

We accept three different kinds of expressions, in different places in the grammar.

- ``expr`` is the most generic, and includes all expressions.
- ``maybeEffectfulExpr`` is used in places where a variable declaration would also be allowed. It forbids some expressions that are clearly effect-free, such as ``x*y`` or ``x < y``.
  As the name indicates, it may be empty. In that case it is equivalent to "null" (any other effect-free expression would be fine, as the result of such an expression is always discarded).
- ``constexpr`` is limited to literals and the elements of an enum. It is used in switch cases, and in type arguments.

.. productionlist::
    expr: (`expr` ",")? `ternaryConditional`
    ternaryConditional: `exprLogicalOr` "?" `expr` ":" `ternaryConditional`
                      : | `exprPrefix` `assignOperator` `ternaryConditional`
                      : | `exprLogicalOr`
    assignOperator: "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | ">>=" | "<<="
    exprLogicalOr: (`exprLogicalOr` "||")? `exprLogicalAnd`
    exprLogicalAnd: (`exprLogicalAnd` "&&")? `exprBitwiseOr`
    exprBitwiseOr: (`exprBitwiseOr` "|")? `exprBitwiseXor`
    exprBitwiseXor: (`exprBitwiseXor` "^")? `exprBitwiseAnd`
    exprBitwiseAnd: (`exprBitwiseAnd` "&")? `exprRelational`
    exprRelational: `exprShift` (`relationalBinop` `exprShift`)?
    relationalBinop: "<" | ">" | "<=" | ">=" | "==" | "!="
    exprShift: (`exprShift` ("<<" | ">>"))? `exprAdd`
    exprAdd: (`exprMult` ("*" | "/" | "%"))? `exprPrefix`
    exprPrefix: `prefixOp` `exprPrefix` | `exprSuffix`
    prefixOp: "++" | "--" | "+" | "-" | "~" | "!" | "*" | "&" | "@"
    exprSuffix: `callExpression` `limitedSuffixOp`*
              : | `term` (`limitedSuffixOp` | "++" | "--")*
    limitedSuffixOp: "." `Identifier` | "->" `Identifier` | "[" `expr` "]"
    callExpression: `Identifier` "(" (`ternaryConditional` ("," `ternaryConditional`)*)? ")"
    term: `Literal` | `Identifier` | "(" `expr` ")"

We match the precedence and associativity of operators from C++, with one exception: we made relational operators non-associative,
so that they cannot be chained. Chaining them has sufficiently surprising results that it is not a clear reduction in usability, and it should make it a lot easier to extend the syntax in the future to accept generics.

There is exactly one piece of syntactic sugar in the above rules: the ``!=`` operator.
``e0 != e1`` is equivalent with ``! (e0 == e1)``.

.. productionlist::
    maybeEffectfulExpr: (`effAssignment` ("," `effAssignment`)*)?
    effAssignment: `exprPrefix` `assignOperator` `expr` | `effPrefix`
    effPrefix: ("++" | "--") `exprPrefix` | `effSuffix`
    effSuffix: `exprSuffix` ("++" | "--") | `callExpression` | "(" `expr` ")"

The structure of maybeEffectfulExpr roughly match the structure of normal expressions, just with normally effect-free operators left off.

If the programmer still wants to use them in such a position (for example due to having overloaded an operator with an effectful operation),
it can be done just by wrapping the expression in parentheses (see the last alternative for effSuffix).

.. productionlist::
    constexpr: `Literal` | `Identifier` "." `Identifier`

Validation
===========

Phase 1: Gathering declarations
-------------------------------

the goal is to build the global typing environment, as well as the global execution environment.
I am afraid it may require several steps.
The first step would do most of the work, gathering all function signatures, as well as typedefs, etc..
The second step would go through the resulting environment, resolving all types, in particular all typedefs (as well as connecting function parameters to the corresponding type variables, etc..)

Phase 2: Local typing and early validation
------------------------------------------

Notations and definitions
"""""""""""""""""""""""""

- Definition of the typing environment
- Definition of the type hierarchy
- Definition of the type evaluation judgement

Validating top-level declarations
"""""""""""""""""""""""""""""""""

Typedefs and structs:

- checking no recursive types (both structs and typedefs.. maybe as part of the computation of the size of each type)
  No, the computation of the size of each type will necessarily happen during monomorphisation
  And we cannot defer this check until then, because we will need to eliminate all typedefs during the local typing phase.

Structs:

- checking that structs have distinct names for different fields, and that the type of their fields are well-defined (and non-void)

Enums:

- check that enums do not have duplicate values, and have one zero-valued element.
- check that enums have an integer base type (not another enum in particular).
- check that every value given to an enum element fits in the base type.

Protocols:

- check that each signature inside a protocol refers to the protocol name (aka type variable).
- check that the extension relation on protocols is sane (at the very least acyclic, maybe also no incompatible signatures for the same function name).

Functions:

- check that operator.field, operator.field=, operator[] and operator[]= are not defined for pointer types, nor declared for pointer types in a protocol.
- check that operator.field= is only defined if operator.field is as well, and that they agree on their return type in that case.
- check that operator[]= is only defined if operator[] is as well, and that they agree on their return type in that case.
- check that all of the type parameters of each operator declaration/definition are inferrable from their arguments (and from its return type in the case of cast operators).
- check that no argument is of type void
- finally, check that the function body can only end with a return of the right type, or with Nothing if it is a void function

Typing statements
"""""""""""""""""

Typing expressions
""""""""""""""""""

- typing rules (this and everything that follows can be managed by just a pair of judgements that type stmts/exprs)
- checking returns
- check that every variable declaration is in a block or at the top-level
- check that no variable declaration shadows another one at the same scope
- check that no variable is declared of type void
- check that switch statements treat all cases
- check that every case in a switch statement ends in a terminator (fallthrough/break/return/continue/trap)
- check that literals fit into the type they are stored into (optional?)

Phase 3: Monomorphisation and late validation
---------------------------------------------

- monomorphisation itself
- resolving function calls (probably done as part of monomorphisation)
- checking no recursive functions (seems very hard to do before that step, as it requires resolving overloaded functions)
- allocating a unique store identifier to each function parameter and variable declaration
- annotating each array access with the stride used by that array type? If we do it here and not at runtime, then each assignment would also need a size annotation..
- checks of proper use of address spaces

Dynamic rules
=============

Definitions
-----------

We split the semantics in two parts: a per-thread execution semantics that does not know anything about concurrency or the memory, and a global set of rules for
loads, stores, barriers and the like.

The per-thread semantics is a fairly classic small-step operational semantics, meaning that it describes what a list of possible transitions that the program can
take in one step.
The per-thread state is made of a few element:

- The program being executed. Each transition transforms it.
- A control-flow stack. This is a stack of values, which tracks whether we are in a branch, and is used by the rules for barriers to check that control-flow is uniform.
- A (constant) environment. This is a mapping from variable names to values and is used to keep track of arguments and variables declared in the function.

Each transition is a statement of the form "With environment :math:`\rho`, if some conditions are respected, the program may be transformed into the following, modifing
the control-flow stack in the following way, and emitting the following memory events."

Execution of statements
-----------------------

Blocks and variable declarations
""""""""""""""""""""""""""""""""

The program fragments that we use to define our semantics are richer than just the syntactically correct programs. In particular, we allow annotating blocks
(sequences of statements between braces) with an environment.

Here is how to reduce a block by one step:

#. If the block is not annotated, annotate it with the environment
#. If the first statement of the block is an empty block, remove it
#. Else if the first statement of the block is a terminator (break, continue, fallthrough, return or trap), replace the entire block by it.
#. Else if the first statement of the block is a variable declaration:

   #. Make a new environment from the one that annotates the block, mapping the variable name to its store identifier.
   #. If the variable declaration has an initializing expression that can be reduced, reduce it using the new environment
   #. Else:

      #. Change the annotation of the block to the new environment.
      #. Emit a store to the store identifier of the declaration, of a value that is either the initializing value (if available) or the default value for the type (otherwise)
      #. Remove this variable declaration from the block

#. Else reduce the first statement of the block, using the environment that the block was annotated with (not the top-level environment)

.. todo:: Specify this "default value for the type". It should be very simple (null for ptrs/refs, false for booleans, 0/0. for ints/floats, and the natural extension for arrays/structs).

Branches
""""""""

We add another kind of statement: the ``Join(s)`` construct, that takes as argument another statement ``s``.

Here is how to reduce a branch (if-then-else construct, remember that if-then is just syntactic sugar that was eliminated during parsing) by one step:

#. If the expression in the if is ``true`` or ``false``.

   #. Push that value on the control flow stack
   #. Replace the branch by the statement in the then (for ``true``) or else (for ``false``) branch, wrapped in the ``Join`` construct

#. Else reduce that expression

Here is how to reduce a ``Join(s)`` statement:

#. If the argument of the ``Join`` is a terminator (``break;``, ``continue;``, ``fallthrough;``, ``return e?;`` or ``trap;``) or an empty block

   #. Pop the last value from the control flow stack
   #. Replace the ``Join`` statement by its argument

#. Else reduce its argument

.. note:: Popping the last value from the control flow stack never fails, as a Join only appears when eliminating a branch, which pushes a value on it.

Switches
""""""""

We add another kind of statement: the ``Cases(..)`` construct that takes as argument a sequence.

Loops
"""""

Barriers and uniform control flow
"""""""""""""""""""""""""""""""""

Other
"""""

TODO: return reduce, and effectful expr.

Execution of expressions
------------------------

Memory model
------------

Standard library
================

Interface with JavaScript
=========================

Indices and tables
##################

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

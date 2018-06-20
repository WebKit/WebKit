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

Parsing
-------

Notations
"""""""""

Top-level declarations
""""""""""""""""""""""

Statements
""""""""""

Types
"""""

Expressions
"""""""""""

Static rules
============

Phase 1: Desugaring
-------------------

desugaring top level:

- Adding <> in basically every possible place. (after a struct name, after a typedef name, after a function/operator name)

desugaring statements:

- for (X; e; e') s --> {X; while (e) {s e';}}
  With special cases for if X is empty/vdecl/effectful expr
  if e is empty/an expr
  if e' is empty/an expr
- while (e) s --> if (e) do s while (e);
- if (e) s --> if (e) s else {}
- t vdecl0, vdecl1 .. vdecln --> t vdecl0; t vdecl1 .. vdecln (premise: n > 0)
- t x = e; --> t x; x = e;
- ; --> {}

desugaring expressions:

- e != e' --> ! (e == e')
- foo(x0, .., xn) --> foo<>(x0, .., xn)

Phase 2: Gathering declarations
-------------------------------

the goal is to build the global typing environment, as well as the global execution environment.
I am afraid it may require several steps.
The first step would do most of the work, gathering all function signatures, as well as typedefs, etc..
The second step would go through the resulting environment, resolving all types, in particular all typedefs (as well as connecting function parameters to the corresponding type variables, etc..)

Phase 3: Local typing and early validation
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

- checking that structs have distinct names for different fields, and that the type of their fields are well-defined

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
- finally, check that the function body can only end with a return of the right type, or with Nothing if it is a void function

Typing statements
"""""""""""""""""

Typing expressions
""""""""""""""""""

 typing rules (this and everything that follows can be managed by just a pair of judgements that type stmts/exprs)
- checking returns
- check that every variable declaration is in a block or at the top-level
- check that no variable declaration shadows another one at the same scope
- check that switch statements treat all cases
- check that every case in a switch statement ends in a terminator (fallthrough/break/return/continue/trap)
- check that literals fit into the type they are stored into (optional?)

Phase 4: Monomorphisation and late validation
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

Execution of statements
-----------------------

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

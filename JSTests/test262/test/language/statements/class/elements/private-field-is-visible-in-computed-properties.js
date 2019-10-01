// Copyright (C) 2019 Caio Lima (Igalia SL). All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: PrivateName of a class is visible in its ComputetProperty scope
esid: prod-ClassTail
info: |
  ClassTail : ClassHeritage { ClassBody }
    1. Let lex be the LexicalEnvironment of the running execution context.
    2. Let classScope be NewDeclarativeEnvironment(lex).
    3. Let classScopeEnvRec be classScope's EnvironmentRecord.
    ...
    8. If ClassBodyopt is present, then
        a. For each element dn of the PrivateBoundIdentifiers of ClassBodyopt,
          i. Perform classPrivateEnvRec.CreateImmutableBinding(dn, true).
    ...
    15. Set the running execution context's LexicalEnvironment to classScope.
    16. Set the running execution context's PrivateEnvironment to classPrivateEnvironment.
    ...
    27. For each ClassElement e in order from elements
      a. If IsStatic of e is false, then
        i. Let field be the result of ClassElementEvaluation for e with arguments proto and false.
    ...

  MemberExpression : MemberExpression . PrivateIdentifier
    ...
    5. Return MakePrivateReference(bv, fieldNameString).

  MakePrivateReference ( baseValue, privateIdentifier )
    ...
    2. Let privateNameBinding be ? ResolveBinding(privateIdentifier, env).
    3. Let privateName be GetValue(privateNameBinding).
    ...

  GetValue (V)
    ...
    6. Else,
      a. Assert: base is an Environment Record.
      b. Return ? base.GetBindingValue(GetReferencedName(V), IsStrictReference(V)).

  GetBindingValue ( N, S )
    ...
    3. If the binding for N in envRec is an uninitialized binding, throw a ReferenceError exception.
    ...

features: [class-fields-private, class-fields-public, class]
---*/

const self = this;
assert.throws(ReferenceError, function() {
  class C {
    [self.#f] = 'Test262';
    #f = 'foo';
  }
}, 'access to a not defined private field in object should throw a ReferenceError');


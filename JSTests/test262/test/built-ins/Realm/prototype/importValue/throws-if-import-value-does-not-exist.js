// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realmimportvalue
description: >
  Realm.prototype.importValue rejects when export name does not exist
info: |
  RealmImportValue ( specifierString, exportNameString, callerRealm, evalRealm, evalContext )

    Assert: Type(specifierString) is String.
    Assert: Type(exportNameString) is String.
    Assert: callerRealm is a Realm Record.
    Assert: evalRealm is a Realm Record.
    Assert: evalContext is an execution context associated to a Realm instance's [[ExecutionContext]].
    Let innerCapability be ! NewPromiseCapability(%Promise%).
    Let runningContext be the running execution context.
    If runningContext is not already suspended, suspend runningContext.
    Push evalContext onto the execution context stack; evalContext is now the running execution context.
    Perform ! HostImportModuleDynamically(null, specifierString, innerCapability).
    Suspend evalContext and remove it from the execution context stack.
    Resume the context that is now on the top of the execution context stack as the running
    execution context.
    Let steps be the steps of an ExportGetter function as described below.

  An ExportGetter function is an anonymous built-in function with a [[ExportNameString]]
  internal slot. When an ExportGetter function is called with argument exports,
  it performs the following steps:

    Assert: exports is a module namespace exotic object.
    Let f be the active function object.
    Let string be f.[[ExportNameString]].
    Assert: Type(string) is String.
    Let hasOwn be ? HasOwnProperty(exports, string).
    If hasOwn is false, throw a TypeError exception.
    ...

flags: [async, module]
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.importValue,
  'function',
  'This test must fail if Realm.prototype.importValue is not a function'
);

const r = new Realm();

r.importValue('./import-value_FIXTURE.js', 'y')
  .then(
    () => $DONE('Expected rejection'),
    () => $DONE()
  );

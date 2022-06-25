// This file was procedurally generated from the following sources:
// - src/class-elements/prod-private-method.case
// - src/class-elements/private-methods/cls-decl.template
/*---
description: Private Method (private method definitions in a class declaration)
esid: prod-MethodDefinition
features: [class, class-methods-private]
flags: [generated]
info: |
    ClassElement :
      MethodDefinition
      ...
      ;

    ClassElementName :
      PropertyName
      PrivateName

    PrivateName ::
      # IdentifierName

    MethodDefinition :
      ClassElementName ( UniqueFormalParameters ) { FunctionBody }
      GeneratorMethod
      AsyncMethod
      AsyncGeneratorMethod 
      get ClassElementName () { FunctionBody }
      set ClassElementName ( PropertySetParameterList ) { FunctionBody }

    GeneratorMethod :
      * ClassElementName ( UniqueFormalParameters ){GeneratorBody}

    AsyncMethod :
      async [no LineTerminator here] ClassElementName ( UniqueFormalParameters ) { AsyncFunctionBody }

    AsyncGeneratorMethod :
      async [no LineTerminator here]* ClassElementName ( UniqueFormalParameters ) { AsyncGeneratorBody }

    ---

    InitializeClassElements ( F, proto )

    ...
    5. For each item element in order from elements,
      a. Assert: If element.[[Placement]] is "prototype" or "static", then element.[[Key]] is not a Private Name.
      b. If element.[[Kind]] is "method" and element.[[Placement]] is "static" or "prototype",
        i. Let receiver be F if element.[[Placement]] is "static", else let receiver be proto.
        ii. Perform ? DefineClassElement(receiver, element).

    InitializeInstanceElements ( O, constructor )

    ...
    3. Let elements be the value of F's [[Elements]] internal slot.
    4. For each item element in order from elements,
      a. If element.[[Placement]] is "own" and element.[[Kind]] is "method",
        i. Perform ? DefineClassElement(O, element).

    DefineClassElement (receiver, element)

    ...
    6. If key is a Private Name,
      a. Perform ? PrivateFieldDefine(receiver, key, descriptor).

    PrivateFieldDefine (P, O, desc)

    ...
    6. Append { [[PrivateName]]: P, [[PrivateFieldDescriptor]]: desc } to O.[[PrivateFieldDescriptors]].

---*/


/*** template notes
 * method should always be #m
 * the template provides c.ref() for external reference
 */

function hasProp(obj, name, expected, msg) {
  var hasOwnProperty = Object.prototype.hasOwnProperty.call(obj, name);
  assert.sameValue(hasOwnProperty, expected, msg);

  var hasProperty = Reflect.has(obj, name);
  assert.sameValue(hasProperty, expected, msg);
}

class C {
  #m() { return 42; }


  get ref() { return this.#m; }

  constructor() {
    hasProp(this, '#m', false, 'private methods are defined in an special internal slot and cannot be found as own properties');
    assert.sameValue(typeof this.#m, 'function');
    assert.sameValue(this.ref, this.#m, 'returns the same value');
    assert.sameValue(this.#m, (() => this)().#m, 'memberexpression and call expression forms');

    assert.sameValue(this.#m(), 42, 'already defined in the ctor');
    assert.sameValue(this.#m.name, '#m', 'function name inside constructor');

  }
}

var c = new C();
var other = new C();

hasProp(C.prototype, '#m', false, 'method is not defined in the prototype');
hasProp(C, '#m', false, 'method is not defined in the contructor');
hasProp(c, '#m', false, 'method cannot be seen outside of the class');

/***
 * MethodDefinition : ClassElementName ( UniqueFormalParameters ) { FunctionBody }
 * 
 * 1. Let methodDef be DefineMethod of MethodDefinition with argument homeObject.
 * ...
 */
assert.sameValue(c.ref, other.ref, 'The method is defined once, and reused on every new instance');

// gets the returned value from #m
assert.sameValue(c.ref(), 42, 'function return');
assert.sameValue(c.ref.name, '#m', 'function name is preserved external reference');

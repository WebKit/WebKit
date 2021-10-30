/*
  (module
    (type $t (func))
    (func (export "externref") (result externref) (ref.null extern))
    (func (export "funcref") (result funcref) (ref.null func))
    (func (export "ref") (result (ref null $t)) (ref.null $t))

    (global externref (ref.null extern))
    (global funcref (ref.null func))
    (global (ref null $t) (ref.null $t))
  )
*/

let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x11\x04\x60\x00\x00\x60\x00\x01\x6f\x60\x00\x01\x70\x60\x00\x01\x6c\x00\x03\x04\x03\x01\x02\x03\x06\x11\x03\x6f\x00\xd0\x6f\x0b\x70\x00\xd0\x70\x0b\x6c\x00\x00\xd0\x00\x0b\x07\x1d\x03\x09\x65\x78\x74\x65\x72\x6e\x72\x65\x66\x00\x00\x07\x66\x75\x6e\x63\x72\x65\x66\x00\x01\x03\x72\x65\x66\x00\x02\x0a\x10\x03\x04\x00\xd0\x6f\x0b\x04\x00\xd0\x70\x0b\x04\x00\xd0\x00\x0b");

// ref_null.wast:12
assert_return(() => call($1, "externref", []), null);

// ref_null.wast:13
assert_return(() => call($1, "funcref", []), null);

// ref_null.wast:14
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=231222
// assert_return(() => call($1, "ref", []), null);

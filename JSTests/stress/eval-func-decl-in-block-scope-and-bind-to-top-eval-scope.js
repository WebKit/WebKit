var init1;

(function() {
  eval(
    '\
    init1 = f;\
    {\
      function f() {}\
    }{ function f() {  } }'
  );
}());

if (init1 !== undefined)
  throw new Error('Wrong binding of the function.');
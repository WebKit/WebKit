// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |

---*/

function __consolePrintHandle__(msg){
  print(msg);
}

function $DONE(){
  if(!arguments[0])
    __consolePrintHandle__('Test262:AsyncTestComplete');
  else
    __consolePrintHandle__('Error: ' + arguments[0]);
}

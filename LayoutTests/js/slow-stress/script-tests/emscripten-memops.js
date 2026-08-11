// Ignore this test in run-jsc-stress-tests
//@ skip

// Note: For maximum-speed code, see "Optimizing Code" on the Emscripten wiki, https://github.com/kripken/emscripten/wiki/Optimizing-Code
// Note: Some Emscripten settings may limit the speed of the generated code.
// The Module object: Our interface to the outside world. We import
// and export values on it, and do the work to get that through
// closure compiler if necessary. There are various ways Module can be used:
// 1. Not defined. We create it here
// 2. A function parameter, function(Module) { ..generated code.. }
// 3. pre-run appended it, var Module = {}; ..generated code..
// 4. External script tag defines var Module.
// We need to do an eval in order to handle the closure compiler
// case, where this code here is minified but Module was defined
// elsewhere (e.g. case 4 above). We also need to check if Module
// already exists (e.g. case 3 above).
// Note that if you want to run closure, and also to use Module
// after the generated code, you will need to define   var Module = {};
// before the code. Then that object will be used in the code, and you
// can continue to use Module afterwards as well.
var Module;
if (!Module) Module = eval('(function() { try { return Module || {} } catch(e) { return {} } })()');
// Sometimes an existing Module object exists with properties
// meant to overwrite the default module functionality. Here
// we collect those properties and reapply _after_ we configure
// the current environment's defaults to avoid having to be so
// defensive during initialization.
var moduleOverrides = {};
for (var key in Module) {
  if (Module.hasOwnProperty(key)) {
    moduleOverrides[key] = Module[key];
  }
}
// The environment setup code below is customized to use Module.
// *** Environment setup code ***
var ENVIRONMENT_IS_NODE = typeof process === 'object' && typeof require === 'function';
var ENVIRONMENT_IS_WEB = typeof window === 'object';
var ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
var JSRegress_outputBuffer = "";
if (ENVIRONMENT_IS_NODE) {
  // Expose functionality in the same simple way that the shells work
  // Note that we pollute the global namespace here, otherwise we break in node
  Module['print'] = function(x) {
    process['stdout'].write(x + '\n');
  };
  Module['printErr'] = function(x) {
    process['stderr'].write(x + '\n');
  };
  var nodeFS = require('fs');
  var nodePath = require('path');
  Module['read'] = function(filename, binary) {
    filename = nodePath['normalize'](filename);
    var ret = nodeFS['readFileSync'](filename);
    // The path is absolute if the normalized version is the same as the resolved.
    if (!ret && filename != nodePath['resolve'](filename)) {
      filename = path.join(__dirname, '..', 'src', filename);
      ret = nodeFS['readFileSync'](filename);
    }
    if (ret && !binary) ret = ret.toString();
    return ret;
  };
  Module['readBinary'] = function(filename) { return Module['read'](filename, true) };
  Module['load'] = function(f) {
    globalEval(read(f));
  };
  Module['arguments'] = process['argv'].slice(2);
  module.exports = Module;
}
else if (ENVIRONMENT_IS_SHELL) {
  Module['print'] = function() {
      for (var i = 0; i < arguments.length; ++i) {
          if (i)
              JSRegress_outputBuffer += " ";
          JSRegress_outputBuffer += arguments[i];
      }
      JSRegress_outputBuffer += "\n";
  };
  if (typeof printErr != 'undefined') Module['printErr'] = printErr; // not present in v8 or older sm
  if (typeof read != 'undefined') {
    Module['read'] = read;
  } else {
    Module['read'] = function() { throw 'no read() available (jsc?)' };
  }
  Module['readBinary'] = function(f) {
    return read(f, 'binary');
  };
  if (typeof scriptArgs != 'undefined') {
    Module['arguments'] = scriptArgs;
  } else if (typeof arguments != 'undefined') {
    Module['arguments'] = arguments;
  }
  this['Module'] = Module;
}
else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
  Module['read'] = function(url) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);
    xhr.send(null);
    return xhr.responseText;
  };
  if (typeof arguments != 'undefined') {
    Module['arguments'] = arguments;
  }
  if (ENVIRONMENT_IS_WEB) {
    Module['print'] = function(x) {
      for (var i = 0; i < arguments.length; ++i) {
          if (i)
              JSRegress_outputBuffer += " ";
          JSRegress_outputBuffer += arguments[i];
      }
      JSRegress_outputBuffer += "\n";
    };
    Module['printErr'] = function(x) {
      console.log(x);
    };
    this['Module'] = Module;
  } else if (ENVIRONMENT_IS_WORKER) {
    // We can do very little here...
    var TRY_USE_DUMP = false;
    Module['print'] = (TRY_USE_DUMP && (typeof(dump) !== "undefined") ? (function(x) {
      dump(x);
    }) : (function(x) {
      // self.postMessage(x); // enable this if you want stdout to be sent as messages
    }));
    Module['load'] = importScripts;
  }
}
else {
  // Unreachable because SHELL is dependant on the others
  throw 'Unknown runtime environment. Where are we?';
}
function globalEval(x) {
  eval.call(null, x);
}
if (!Module['load'] == 'undefined' && Module['read']) {
  Module['load'] = function(f) {
    globalEval(Module['read'](f));
  };
}
if (!Module['print']) {
  Module['print'] = function(){};
}
if (!Module['printErr']) {
  Module['printErr'] = Module['print'];
}
if (!Module['arguments']) {
  Module['arguments'] = [];
}
// *** Environment setup code ***
// Closure helpers
Module.print = Module['print'];
Module.printErr = Module['printErr'];
// Callbacks
Module['preRun'] = [];
Module['postRun'] = [];
// Merge back in the overrides
for (var key in moduleOverrides) {
  if (moduleOverrides.hasOwnProperty(key)) {
    Module[key] = moduleOverrides[key];
  }
}
// === Auto-generated preamble library stuff ===
//========================================
// Runtime code shared with compiler
//========================================
var Runtime = {
  stackSave: function () {
    return STACKTOP;
  },
  stackRestore: function (stackTop) {
    STACKTOP = stackTop;
  },
  forceAlign: function (target, quantum) {
    quantum = quantum || 4;
    if (quantum == 1) return target;
    if (isNumber(target) && isNumber(quantum)) {
      return Math.ceil(target/quantum)*quantum;
    } else if (isNumber(quantum) && isPowerOfTwo(quantum)) {
      var logg = log2(quantum);
      return '((((' +target + ')+' + (quantum-1) + ')>>' + logg + ')<<' + logg + ')';
    }
    return 'Math.ceil((' + target + ')/' + quantum + ')*' + quantum;
  },
  isNumberType: function (type) {
    return type in Runtime.INT_TYPES || type in Runtime.FLOAT_TYPES;
  },
  isPointerType: function isPointerType(type) {
  return type[type.length-1] == '*';
},
  isStructType: function isStructType(type) {
  if (isPointerType(type)) return false;
  if (isArrayType(type)) return true;
  if (/<?{ ?[^}]* ?}>?/.test(type)) return true; // { i32, i8 } etc. - anonymous struct types
  // See comment in isStructPointerType()
  return type[0] == '%';
},
  INT_TYPES: {"i1":0,"i8":0,"i16":0,"i32":0,"i64":0},
  FLOAT_TYPES: {"float":0,"double":0},
  or64: function (x, y) {
    var l = (x | 0) | (y | 0);
    var h = (Math.round(x / 4294967296) | Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  and64: function (x, y) {
    var l = (x | 0) & (y | 0);
    var h = (Math.round(x / 4294967296) & Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  xor64: function (x, y) {
    var l = (x | 0) ^ (y | 0);
    var h = (Math.round(x / 4294967296) ^ Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  getNativeTypeSize: function (type, quantumSize) {
    if (Runtime.QUANTUM_SIZE == 1) return 1;
    var size = {
      '%i1': 1,
      '%i8': 1,
      '%i16': 2,
      '%i32': 4,
      '%i64': 8,
      "%float": 4,
      "%double": 8
    }['%'+type]; // add '%' since float and double confuse Closure compiler as keys, and also spidermonkey as a compiler will remove 's from '_i8' etc
    if (!size) {
      if (type.charAt(type.length-1) == '*') {
        size = Runtime.QUANTUM_SIZE; // A pointer
      } else if (type[0] == 'i') {
        var bits = parseInt(type.substr(1));
        assert(bits % 8 == 0);
        size = bits/8;
      }
    }
    return size;
  },
  getNativeFieldSize: function (type) {
    return Math.max(Runtime.getNativeTypeSize(type), Runtime.QUANTUM_SIZE);
  },
  dedup: function dedup(items, ident) {
  var seen = {};
  if (ident) {
    return items.filter(function(item) {
      if (seen[item[ident]]) return false;
      seen[item[ident]] = true;
      return true;
    });
  } else {
    return items.filter(function(item) {
      if (seen[item]) return false;
      seen[item] = true;
      return true;
    });
  }
},
  set: function set() {
  var args = typeof arguments[0] === 'object' ? arguments[0] : arguments;
  var ret = {};
  for (var i = 0; i < args.length; i++) {
    ret[args[i]] = 0;
  }
  return ret;
},
  STACK_ALIGN: 8,
  getAlignSize: function (type, size, vararg) {
    // we align i64s and doubles on 64-bit boundaries, unlike x86
    if (type == 'i64' || type == 'double' || vararg) return 8;
    if (!type) return Math.min(size, 8); // align structures internally to 64 bits
    return Math.min(size || (type ? Runtime.getNativeFieldSize(type) : 0), Runtime.QUANTUM_SIZE);
  },
  calculateStructAlignment: function calculateStructAlignment(type) {
    type.flatSize = 0;
    type.alignSize = 0;
    var diffs = [];
    var prev = -1;
    var index = 0;
    type.flatIndexes = type.fields.map(function(field) {
      index++;
      var size, alignSize;
      if (Runtime.isNumberType(field) || Runtime.isPointerType(field)) {
        size = Runtime.getNativeTypeSize(field); // pack char; char; in structs, also char[X]s.
        alignSize = Runtime.getAlignSize(field, size);
      } else if (Runtime.isStructType(field)) {
        if (field[1] === '0') {
          // this is [0 x something]. When inside another structure like here, it must be at the end,
          // and it adds no size
          // XXX this happens in java-nbody for example... assert(index === type.fields.length, 'zero-length in the middle!');
          size = 0;
          alignSize = type.alignSize || QUANTUM_SIZE;
        } else {
          size = Types.types[field].flatSize;
          alignSize = Runtime.getAlignSize(null, Types.types[field].alignSize);
        }
      } else if (field[0] == 'b') {
        // bN, large number field, like a [N x i8]
        size = field.substr(1)|0;
        alignSize = 1;
      } else {
        throw 'Unclear type in struct: ' + field + ', in ' + type.name_ + ' :: ' + dump(Types.types[type.name_]);
      }
      if (type.packed) alignSize = 1;
      type.alignSize = Math.max(type.alignSize, alignSize);
      var curr = Runtime.alignMemory(type.flatSize, alignSize); // if necessary, place this on aligned memory
      type.flatSize = curr + size;
      if (prev >= 0) {
        diffs.push(curr-prev);
      }
      prev = curr;
      return curr;
    });
    type.flatSize = Runtime.alignMemory(type.flatSize, type.alignSize);
    if (diffs.length == 0) {
      type.flatFactor = type.flatSize;
    } else if (Runtime.dedup(diffs).length == 1) {
      type.flatFactor = diffs[0];
    }
    type.needsFlattening = (type.flatFactor != 1);
    return type.flatIndexes;
  },
  generateStructInfo: function (struct, typeName, offset) {
    var type, alignment;
    if (typeName) {
      offset = offset || 0;
      type = (typeof Types === 'undefined' ? Runtime.typeInfo : Types.types)[typeName];
      if (!type) return null;
      if (type.fields.length != struct.length) {
        printErr('Number of named fields must match the type for ' + typeName + ': possibly duplicate struct names. Cannot return structInfo');
        return null;
      }
      alignment = type.flatIndexes;
    } else {
      var type = { fields: struct.map(function(item) { return item[0] }) };
      alignment = Runtime.calculateStructAlignment(type);
    }
    var ret = {
      __size__: type.flatSize
    };
    if (typeName) {
      struct.forEach(function(item, i) {
        if (typeof item === 'string') {
          ret[item] = alignment[i] + offset;
        } else {
          // embedded struct
          var key;
          for (var k in item) key = k;
          ret[key] = Runtime.generateStructInfo(item[key], type.fields[i], alignment[i]);
        }
      });
    } else {
      struct.forEach(function(item, i) {
        ret[item[1]] = alignment[i];
      });
    }
    return ret;
  },
  dynCall: function (sig, ptr, args) {
    if (args && args.length) {
      if (!args.splice) args = Array.prototype.slice.call(args);
      args.splice(0, 0, ptr);
      return Module['dynCall_' + sig].apply(null, args);
    } else {
      return Module['dynCall_' + sig].call(null, ptr);
    }
  },
  functionPointers: [],
  addFunction: function (func) {
    for (var i = 0; i < Runtime.functionPointers.length; i++) {
      if (!Runtime.functionPointers[i]) {
        Runtime.functionPointers[i] = func;
        return 2 + 2*i;
      }
    }
    throw 'Finished up all reserved function pointers. Use a higher value for RESERVED_FUNCTION_POINTERS.';
  },
  removeFunction: function (index) {
    Runtime.functionPointers[(index-2)/2] = null;
  },
  warnOnce: function (text) {
    if (!Runtime.warnOnce.shown) Runtime.warnOnce.shown = {};
    if (!Runtime.warnOnce.shown[text]) {
      Runtime.warnOnce.shown[text] = 1;
      Module.printErr(text);
    }
  },
  funcWrappers: {},
  getFuncWrapper: function (func, sig) {
    assert(sig);
    if (!Runtime.funcWrappers[func]) {
      Runtime.funcWrappers[func] = function() {
        return Runtime.dynCall(sig, func, arguments);
      };
    }
    return Runtime.funcWrappers[func];
  },
  UTF8Processor: function () {
    var buffer = [];
    var needed = 0;
    this.processCChar = function (code) {
      code = code & 0xff;
      if (needed) {
        buffer.push(code);
        needed--;
      }
      if (buffer.length == 0) {
        if (code < 128) return String.fromCharCode(code);
        buffer.push(code);
        if (code > 191 && code < 224) {
          needed = 1;
        } else {
          needed = 2;
        }
        return '';
      }
      if (needed > 0) return '';
      var c1 = buffer[0];
      var c2 = buffer[1];
      var c3 = buffer[2];
      var ret;
      if (c1 > 191 && c1 < 224) {
        ret = String.fromCharCode(((c1 & 31) << 6) | (c2 & 63));
      } else {
        ret = String.fromCharCode(((c1 & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
      }
      buffer.length = 0;
      return ret;
    }
    this.processJSString = function(string) {
      string = unescape(encodeURIComponent(string));
      var ret = [];
      for (var i = 0; i < string.length; i++) {
        ret.push(string.charCodeAt(i));
      }
      return ret;
    }
  },
  stackAlloc: function (size) { var ret = STACKTOP;STACKTOP = (STACKTOP + size)|0;STACKTOP = ((((STACKTOP)+7)>>3)<<3); return ret; },
  staticAlloc: function (size) { var ret = STATICTOP;STATICTOP = (STATICTOP + size)|0;STATICTOP = ((((STATICTOP)+7)>>3)<<3); return ret; },
  dynamicAlloc: function (size) { var ret = DYNAMICTOP;DYNAMICTOP = (DYNAMICTOP + size)|0;DYNAMICTOP = ((((DYNAMICTOP)+7)>>3)<<3); if (DYNAMICTOP >= TOTAL_MEMORY) enlargeMemory();; return ret; },
  alignMemory: function (size,quantum) { var ret = size = Math.ceil((size)/(quantum ? quantum : 8))*(quantum ? quantum : 8); return ret; },
  makeBigInt: function (low,high,unsigned) { var ret = (unsigned ? ((+(((low)>>>(0))))+((+(((high)>>>(0))))*(+(4294967296)))) : ((+(((low)>>>(0))))+((+(((high)|(0))))*(+(4294967296))))); return ret; },
  GLOBAL_BASE: 8,
  QUANTUM_SIZE: 4,
  __dummy__: 0
}
//========================================
// Runtime essentials
//========================================
var __THREW__ = 0; // Used in checking for thrown exceptions.
var ABORT = false; // whether we are quitting the application. no code should run after this. set in exit() and abort()
var EXITSTATUS = 0;
var undef = 0;
// tempInt is used for 32-bit signed values or smaller. tempBigInt is used
// for 32-bit unsigned values or more than 32 bits. TODO: audit all uses of tempInt
var tempValue, tempInt, tempBigInt, tempInt2, tempBigInt2, tempPair, tempBigIntI, tempBigIntR, tempBigIntS, tempBigIntP, tempBigIntD;
var tempI64, tempI64b;
var tempRet0, tempRet1, tempRet2, tempRet3, tempRet4, tempRet5, tempRet6, tempRet7, tempRet8, tempRet9;
function assert(condition, text) {
  if (!condition) {
    abort('Assertion failed: ' + text);
  }
}
var globalScope = this;
// C calling interface. A convenient way to call C functions (in C files, or
// defined with extern "C").
//
// Note: LLVM optimizations can inline and remove functions, after which you will not be
//       able to call them. Closure can also do so. To avoid that, add your function to
//       the exports using something like
//
//         -s EXPORTED_FUNCTIONS='["_main", "_myfunc"]'
//
// @param ident      The name of the C function (note that C++ functions will be name-mangled - use extern "C")
// @param returnType The return type of the function, one of the JS types 'number', 'string' or 'array' (use 'number' for any C pointer, and
//                   'array' for JavaScript arrays and typed arrays; note that arrays are 8-bit).
// @param argTypes   An array of the types of arguments for the function (if there are no arguments, this can be ommitted). Types are as in returnType,
//                   except that 'array' is not possible (there is no way for us to know the length of the array)
// @param args       An array of the arguments to the function, as native JS values (as in returnType)
//                   Note that string arguments will be stored on the stack (the JS string will become a C string on the stack).
// @return           The return value, as a native JS value (as in returnType)
function ccall(ident, returnType, argTypes, args) {
  return ccallFunc(getCFunc(ident), returnType, argTypes, args);
}
Module["ccall"] = ccall;
// Returns the C function with a specified identifier (for C++, you need to do manual name mangling)
function getCFunc(ident) {
  try {
    var func = Module['_' + ident]; // closure exported function
    if (!func) func = eval('_' + ident); // explicit lookup
  } catch(e) {
  }
  assert(func, 'Cannot call unknown function ' + ident + ' (perhaps LLVM optimizations or closure removed it?)');
  return func;
}
// Internal function that does a C call using a function, not an identifier
function ccallFunc(func, returnType, argTypes, args) {
  var stack = 0;
  function toC(value, type) {
    if (type == 'string') {
      if (value === null || value === undefined || value === 0) return 0; // null string
      if (!stack) stack = Runtime.stackSave();
      var ret = Runtime.stackAlloc(value.length+1);
      writeStringToMemory(value, ret);
      return ret;
    } else if (type == 'array') {
      if (!stack) stack = Runtime.stackSave();
      var ret = Runtime.stackAlloc(value.length);
      writeArrayToMemory(value, ret);
      return ret;
    }
    return value;
  }
  function fromC(value, type) {
    if (type == 'string') {
      return Pointer_stringify(value);
    }
    assert(type != 'array');
    return value;
  }
  var i = 0;
  var cArgs = args ? args.map(function(arg) {
    return toC(arg, argTypes[i++]);
  }) : [];
  var ret = fromC(func.apply(null, cArgs), returnType);
  if (stack) Runtime.stackRestore(stack);
  return ret;
}
// Returns a native JS wrapper for a C function. This is similar to ccall, but
// returns a function you can call repeatedly in a normal way. For example:
//
//   var my_function = cwrap('my_c_function', 'number', ['number', 'number']);
//   alert(my_function(5, 22));
//   alert(my_function(99, 12));
//
function cwrap(ident, returnType, argTypes) {
  var func = getCFunc(ident);
  return function() {
    return ccallFunc(func, returnType, argTypes, Array.prototype.slice.call(arguments));
  }
}
Module["cwrap"] = cwrap;
// Sets a value in memory in a dynamic way at run-time. Uses the
// type data. This is the same as makeSetValue, except that
// makeSetValue is done at compile-time and generates the needed
// code then, whereas this function picks the right code at
// run-time.
// Note that setValue and getValue only do *aligned* writes and reads!
// Note that ccall uses JS types as for defining types, while setValue and
// getValue need LLVM types ('i8', 'i32') - this is a lower-level operation
function setValue(ptr, value, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': HEAP8[(ptr)]=value; break;
      case 'i8': HEAP8[(ptr)]=value; break;
      case 'i16': HEAP16[((ptr)>>1)]=value; break;
      case 'i32': HEAP32[((ptr)>>2)]=value; break;
      case 'i64': (tempI64 = [value>>>0,((Math.min((+(Math.floor((value)/(+(4294967296))))), (+(4294967295))))|0)>>>0],HEAP32[((ptr)>>2)]=tempI64[0],HEAP32[(((ptr)+(4))>>2)]=tempI64[1]); break;
      case 'float': HEAPF32[((ptr)>>2)]=value; break;
      case 'double': HEAPF64[((ptr)>>3)]=value; break;
      default: abort('invalid type for setValue: ' + type);
    }
}
Module['setValue'] = setValue;
// Parallel to setValue.
function getValue(ptr, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': return HEAP8[(ptr)];
      case 'i8': return HEAP8[(ptr)];
      case 'i16': return HEAP16[((ptr)>>1)];
      case 'i32': return HEAP32[((ptr)>>2)];
      case 'i64': return HEAP32[((ptr)>>2)];
      case 'float': return HEAPF32[((ptr)>>2)];
      case 'double': return HEAPF64[((ptr)>>3)];
      default: abort('invalid type for setValue: ' + type);
    }
  return null;
}
Module['getValue'] = getValue;
var ALLOC_NORMAL = 0; // Tries to use _malloc()
var ALLOC_STACK = 1; // Lives for the duration of the current function call
var ALLOC_STATIC = 2; // Cannot be freed
var ALLOC_DYNAMIC = 3; // Cannot be freed except through sbrk
var ALLOC_NONE = 4; // Do not allocate
Module['ALLOC_NORMAL'] = ALLOC_NORMAL;
Module['ALLOC_STACK'] = ALLOC_STACK;
Module['ALLOC_STATIC'] = ALLOC_STATIC;
Module['ALLOC_DYNAMIC'] = ALLOC_DYNAMIC;
Module['ALLOC_NONE'] = ALLOC_NONE;
// allocate(): This is for internal use. You can use it yourself as well, but the interface
//             is a little tricky (see docs right below). The reason is that it is optimized
//             for multiple syntaxes to save space in generated code. So you should
//             normally not use allocate(), and instead allocate memory using _malloc(),
//             initialize it with setValue(), and so forth.
// @slab: An array of data, or a number. If a number, then the size of the block to allocate,
//        in *bytes* (note that this is sometimes confusing: the next parameter does not
//        affect this!)
// @types: Either an array of types, one for each byte (or 0 if no type at that position),
//         or a single type which is used for the entire block. This only matters if there
//         is initial data - if @slab is a number, then this does not matter at all and is
//         ignored.
// @allocator: How to allocate memory, see ALLOC_*
function allocate(slab, types, allocator, ptr) {
  var zeroinit, size;
  if (typeof slab === 'number') {
    zeroinit = true;
    size = slab;
  } else {
    zeroinit = false;
    size = slab.length;
  }
  var singleType = typeof types === 'string' ? types : null;
  var ret;
  if (allocator == ALLOC_NONE) {
    ret = ptr;
  } else {
    ret = [_malloc, Runtime.stackAlloc, Runtime.staticAlloc, Runtime.dynamicAlloc][allocator === undefined ? ALLOC_STATIC : allocator](Math.max(size, singleType ? 1 : types.length));
  }
  if (zeroinit) {
    var ptr = ret, stop;
    assert((ret & 3) == 0);
    stop = ret + (size & ~3);
    for (; ptr < stop; ptr += 4) {
      HEAP32[((ptr)>>2)]=0;
    }
    stop = ret + size;
    while (ptr < stop) {
      HEAP8[((ptr++)|0)]=0;
    }
    return ret;
  }
  if (singleType === 'i8') {
    if (slab.subarray || slab.slice) {
      HEAPU8.set(slab, ret);
    } else {
      HEAPU8.set(new Uint8Array(slab), ret);
    }
    return ret;
  }
  var i = 0, type, typeSize, previousType;
  while (i < size) {
    var curr = slab[i];
    if (typeof curr === 'function') {
      curr = Runtime.getFunctionIndex(curr);
    }
    type = singleType || types[i];
    if (type === 0) {
      i++;
      continue;
    }
    if (type == 'i64') type = 'i32'; // special case: we have one i32 here, and one i32 later
    setValue(ret+i, curr, type);
    // no need to look up size unless type changes, so cache it
    if (previousType !== type) {
      typeSize = Runtime.getNativeTypeSize(type);
      previousType = type;
    }
    i += typeSize;
  }
  return ret;
}
Module['allocate'] = allocate;
function Pointer_stringify(ptr, /* optional */ length) {
  // Find the length, and check for UTF while doing so
  var hasUtf = false;
  var t;
  var i = 0;
  while (1) {
    t = HEAPU8[(((ptr)+(i))|0)];
    if (t >= 128) hasUtf = true;
    else if (t == 0 && !length) break;
    i++;
    if (length && i == length) break;
  }
  if (!length) length = i;
  var ret = '';
  if (!hasUtf) {
    var MAX_CHUNK = 1024; // split up into chunks, because .apply on a huge string can overflow the stack
    var curr;
    while (length > 0) {
      curr = String.fromCharCode.apply(String, HEAPU8.subarray(ptr, ptr + Math.min(length, MAX_CHUNK)));
      ret = ret ? ret + curr : curr;
      ptr += MAX_CHUNK;
      length -= MAX_CHUNK;
    }
    return ret;
  }
  var utf8 = new Runtime.UTF8Processor();
  for (i = 0; i < length; i++) {
    t = HEAPU8[(((ptr)+(i))|0)];
    ret += utf8.processCChar(t);
  }
  return ret;
}
Module['Pointer_stringify'] = Pointer_stringify;
// Memory management
var PAGE_SIZE = 4096;
function alignMemoryPage(x) {
  return ((x+4095)>>12)<<12;
}
var HEAP;
var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;
var STATIC_BASE = 0, STATICTOP = 0, staticSealed = false; // static area
var STACK_BASE = 0, STACKTOP = 0, STACK_MAX = 0; // stack area
var DYNAMIC_BASE = 0, DYNAMICTOP = 0; // dynamic area handled by sbrk
function enlargeMemory() {
  abort('Cannot enlarge memory arrays in asm.js. Either (1) compile with -s TOTAL_MEMORY=X with X higher than the current value, or (2) set Module.TOTAL_MEMORY before the program runs.');
}
var TOTAL_STACK = Module['TOTAL_STACK'] || 5242880;
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 134217728;
var FAST_MEMORY = Module['FAST_MEMORY'] || 2097152;
// Initialize the runtime's memory
// check for full engine support (use string 'subarray' to avoid closure compiler confusion)
assert(!!Int32Array && !!Float64Array && !!(new Int32Array(1)['subarray']) && !!(new Int32Array(1)['set']),
       'Cannot fallback to non-typed array case: Code is too specialized');
var buffer = new ArrayBuffer(TOTAL_MEMORY);
HEAP8 = new Int8Array(buffer);
HEAP16 = new Int16Array(buffer);
HEAP32 = new Int32Array(buffer);
HEAPU8 = new Uint8Array(buffer);
HEAPU16 = new Uint16Array(buffer);
HEAPU32 = new Uint32Array(buffer);
HEAPF32 = new Float32Array(buffer);
HEAPF64 = new Float64Array(buffer);
// Endianness check (note: assumes compiler arch was little-endian)
HEAP32[0] = 255;
assert(HEAPU8[0] === 255 && HEAPU8[3] === 0, 'Typed arrays 2 must be run on a little-endian system');
Module['HEAP'] = HEAP;
Module['HEAP8'] = HEAP8;
Module['HEAP16'] = HEAP16;
Module['HEAP32'] = HEAP32;
Module['HEAPU8'] = HEAPU8;
Module['HEAPU16'] = HEAPU16;
Module['HEAPU32'] = HEAPU32;
Module['HEAPF32'] = HEAPF32;
Module['HEAPF64'] = HEAPF64;
function callRuntimeCallbacks(callbacks) {
  while(callbacks.length > 0) {
    var callback = callbacks.shift();
    if (typeof callback == 'function') {
      callback();
      continue;
    }
    var func = callback.func;
    if (typeof func === 'number') {
      if (callback.arg === undefined) {
        Runtime.dynCall('v', func);
      } else {
        Runtime.dynCall('vi', func, [callback.arg]);
      }
    } else {
      func(callback.arg === undefined ? null : callback.arg);
    }
  }
}
var __ATPRERUN__  = []; // functions called before the runtime is initialized
var __ATINIT__    = []; // functions called during startup
var __ATMAIN__    = []; // functions called when main() is to be run
var __ATEXIT__    = []; // functions called during shutdown
var __ATPOSTRUN__ = []; // functions called after the runtime has exited
var runtimeInitialized = false;
function preRun() {
  // compatibility - merge in anything from Module['preRun'] at this time
  if (Module['preRun']) {
    if (typeof Module['preRun'] == 'function') Module['preRun'] = [Module['preRun']];
    while (Module['preRun'].length) {
      addOnPreRun(Module['preRun'].shift());
    }
  }
  callRuntimeCallbacks(__ATPRERUN__);
}
function ensureInitRuntime() {
  if (runtimeInitialized) return;
  runtimeInitialized = true;
  callRuntimeCallbacks(__ATINIT__);
}
function preMain() {
  callRuntimeCallbacks(__ATMAIN__);
}
function exitRuntime() {
  callRuntimeCallbacks(__ATEXIT__);
}
function postRun() {
  // compatibility - merge in anything from Module['postRun'] at this time
  if (Module['postRun']) {
    if (typeof Module['postRun'] == 'function') Module['postRun'] = [Module['postRun']];
    while (Module['postRun'].length) {
      addOnPostRun(Module['postRun'].shift());
    }
  }
  callRuntimeCallbacks(__ATPOSTRUN__);
}
function addOnPreRun(cb) {
  __ATPRERUN__.unshift(cb);
}
Module['addOnPreRun'] = Module.addOnPreRun = addOnPreRun;
function addOnInit(cb) {
  __ATINIT__.unshift(cb);
}
Module['addOnInit'] = Module.addOnInit = addOnInit;
function addOnPreMain(cb) {
  __ATMAIN__.unshift(cb);
}
Module['addOnPreMain'] = Module.addOnPreMain = addOnPreMain;
function addOnExit(cb) {
  __ATEXIT__.unshift(cb);
}
Module['addOnExit'] = Module.addOnExit = addOnExit;
function addOnPostRun(cb) {
  __ATPOSTRUN__.unshift(cb);
}
Module['addOnPostRun'] = Module.addOnPostRun = addOnPostRun;
// Tools
// This processes a JS string into a C-line array of numbers, 0-terminated.
// For LLVM-originating strings, see parser.js:parseLLVMString function
function intArrayFromString(stringy, dontAddNull, length /* optional */) {
  var ret = (new Runtime.UTF8Processor()).processJSString(stringy);
  if (length) {
    ret.length = length;
  }
  if (!dontAddNull) {
    ret.push(0);
  }
  return ret;
}
Module['intArrayFromString'] = intArrayFromString;
function intArrayToString(array) {
  var ret = [];
  for (var i = 0; i < array.length; i++) {
    var chr = array[i];
    if (chr > 0xFF) {
      chr &= 0xFF;
    }
    ret.push(String.fromCharCode(chr));
  }
  return ret.join('');
}
Module['intArrayToString'] = intArrayToString;
// Write a Javascript array to somewhere in the heap
function writeStringToMemory(string, buffer, dontAddNull) {
  var array = intArrayFromString(string, dontAddNull);
  var i = 0;
  while (i < array.length) {
    var chr = array[i];
    HEAP8[(((buffer)+(i))|0)]=chr
    i = i + 1;
  }
}
Module['writeStringToMemory'] = writeStringToMemory;
function writeArrayToMemory(array, buffer) {
  for (var i = 0; i < array.length; i++) {
    HEAP8[(((buffer)+(i))|0)]=array[i];
  }
}
Module['writeArrayToMemory'] = writeArrayToMemory;
function unSign(value, bits, ignore, sig) {
  if (value >= 0) {
    return value;
  }
  return bits <= 32 ? 2*Math.abs(1 << (bits-1)) + value // Need some trickery, since if bits == 32, we are right at the limit of the bits JS uses in bitshifts
                    : Math.pow(2, bits)         + value;
}
function reSign(value, bits, ignore, sig) {
  if (value <= 0) {
    return value;
  }
  var half = bits <= 32 ? Math.abs(1 << (bits-1)) // abs is needed if bits == 32
                        : Math.pow(2, bits-1);
  if (value >= half && (bits <= 32 || value > half)) { // for huge values, we can hit the precision limit and always get true here. so don't do that
                                                       // but, in general there is no perfect solution here. With 64-bit ints, we get rounding and errors
                                                       // TODO: In i64 mode 1, resign the two parts separately and safely
    value = -2*half + value; // Cannot bitshift half, as it may be at the limit of the bits JS uses in bitshifts
  }
  return value;
}
if (!Math['imul']) Math['imul'] = function(a, b) {
  var ah  = a >>> 16;
  var al = a & 0xffff;
  var bh  = b >>> 16;
  var bl = b & 0xffff;
  return (al*bl + ((ah*bl + al*bh) << 16))|0;
};
Math.imul = Math['imul'];
// A counter of dependencies for calling run(). If we need to
// do asynchronous work before running, increment this and
// decrement it. Incrementing must happen in a place like
// PRE_RUN_ADDITIONS (used by emcc to add file preloading).
// Note that you can add dependencies in preRun, even though
// it happens right before run - run will be postponed until
// the dependencies are met.
var runDependencies = 0;
var runDependencyTracking = {};
var calledInit = false, calledRun = false;
var runDependencyWatcher = null;
function addRunDependency(id) {
  runDependencies++;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
  if (id) {
    assert(!runDependencyTracking[id]);
    runDependencyTracking[id] = 1;
  } else {
    Module.printErr('warning: run dependency added without ID');
  }
}
Module['addRunDependency'] = addRunDependency;
function removeRunDependency(id) {
  runDependencies--;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
  if (id) {
    assert(runDependencyTracking[id]);
    delete runDependencyTracking[id];
  } else {
    Module.printErr('warning: run dependency removed without ID');
  }
  if (runDependencies == 0) {
    if (runDependencyWatcher !== null) {
      clearInterval(runDependencyWatcher);
      runDependencyWatcher = null;
    } 
    // If run has never been called, and we should call run (INVOKE_RUN is true, and Module.noInitialRun is not false)
    if (!calledRun && shouldRunNow) run([].concat(Module["arguments"]));
  }
}
Module['removeRunDependency'] = removeRunDependency;
Module["preloadedImages"] = {}; // maps url to image data
Module["preloadedAudios"] = {}; // maps url to audio data
function loadMemoryInitializer(filename) {
  function applyData(data) {
    HEAPU8.set(data, STATIC_BASE);
  }
  // always do this asynchronously, to keep shell and web as similar as possible
  addOnPreRun(function() {
    if (ENVIRONMENT_IS_NODE || ENVIRONMENT_IS_SHELL) {
      applyData(Module['readBinary'](filename));
    } else {
      Browser.asyncLoad(filename, function(data) {
        applyData(data);
      }, function(data) {
        throw 'could not load memory initializer ' + filename;
      });
    }
  });
}
// === Body ===
STATIC_BASE = 8;
STATICTOP = STATIC_BASE + 528;
/* global initializers */ __ATINIT__.push({ func: function() { runPostSets() } });
/* memory initializer */ allocate([102,105,110,97,108,58,32,37,100,46,10,0,0,0,0,0,101,114,114,111,114,58,32,37,100,10,0,0,0,0,0,0], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE)
var tempDoublePtr = Runtime.alignMemory(allocate(12, "i8", ALLOC_STATIC), 8);
assert(tempDoublePtr % 8 == 0);
function copyTempFloat(ptr) { // functions, because inlining this code increases code size too much
  HEAP8[tempDoublePtr] = HEAP8[ptr];
  HEAP8[tempDoublePtr+1] = HEAP8[ptr+1];
  HEAP8[tempDoublePtr+2] = HEAP8[ptr+2];
  HEAP8[tempDoublePtr+3] = HEAP8[ptr+3];
}
function copyTempDouble(ptr) {
  HEAP8[tempDoublePtr] = HEAP8[ptr];
  HEAP8[tempDoublePtr+1] = HEAP8[ptr+1];
  HEAP8[tempDoublePtr+2] = HEAP8[ptr+2];
  HEAP8[tempDoublePtr+3] = HEAP8[ptr+3];
  HEAP8[tempDoublePtr+4] = HEAP8[ptr+4];
  HEAP8[tempDoublePtr+5] = HEAP8[ptr+5];
  HEAP8[tempDoublePtr+6] = HEAP8[ptr+6];
  HEAP8[tempDoublePtr+7] = HEAP8[ptr+7];
}
  var ERRNO_CODES={EPERM:1,ENOENT:2,ESRCH:3,EINTR:4,EIO:5,ENXIO:6,E2BIG:7,ENOEXEC:8,EBADF:9,ECHILD:10,EAGAIN:11,EWOULDBLOCK:11,ENOMEM:12,EACCES:13,EFAULT:14,ENOTBLK:15,EBUSY:16,EEXIST:17,EXDEV:18,ENODEV:19,ENOTDIR:20,EISDIR:21,EINVAL:22,ENFILE:23,EMFILE:24,ENOTTY:25,ETXTBSY:26,EFBIG:27,ENOSPC:28,ESPIPE:29,EROFS:30,EMLINK:31,EPIPE:32,EDOM:33,ERANGE:34,ENOMSG:35,EIDRM:36,ECHRNG:37,EL2NSYNC:38,EL3HLT:39,EL3RST:40,ELNRNG:41,EUNATCH:42,ENOCSI:43,EL2HLT:44,EDEADLK:45,ENOLCK:46,EBADE:50,EBADR:51,EXFULL:52,ENOANO:53,EBADRQC:54,EBADSLT:55,EDEADLOCK:56,EBFONT:57,ENOSTR:60,ENODATA:61,ETIME:62,ENOSR:63,ENONET:64,ENOPKG:65,EREMOTE:66,ENOLINK:67,EADV:68,ESRMNT:69,ECOMM:70,EPROTO:71,EMULTIHOP:74,EDOTDOT:76,EBADMSG:77,ENOTUNIQ:80,EBADFD:81,EREMCHG:82,ELIBACC:83,ELIBBAD:84,ELIBSCN:85,ELIBMAX:86,ELIBEXEC:87,ENOSYS:88,ENOTEMPTY:90,ENAMETOOLONG:91,ELOOP:92,EOPNOTSUPP:95,EPFNOSUPPORT:96,ECONNRESET:104,ENOBUFS:105,EAFNOSUPPORT:106,EPROTOTYPE:107,ENOTSOCK:108,ENOPROTOOPT:109,ESHUTDOWN:110,ECONNREFUSED:111,EADDRINUSE:112,ECONNABORTED:113,ENETUNREACH:114,ENETDOWN:115,ETIMEDOUT:116,EHOSTDOWN:117,EHOSTUNREACH:118,EINPROGRESS:119,EALREADY:120,EDESTADDRREQ:121,EMSGSIZE:122,EPROTONOSUPPORT:123,ESOCKTNOSUPPORT:124,EADDRNOTAVAIL:125,ENETRESET:126,EISCONN:127,ENOTCONN:128,ETOOMANYREFS:129,EUSERS:131,EDQUOT:132,ESTALE:133,ENOTSUP:134,ENOMEDIUM:135,EILSEQ:138,EOVERFLOW:139,ECANCELED:140,ENOTRECOVERABLE:141,EOWNERDEAD:142,ESTRPIPE:143};
  var ERRNO_MESSAGES={0:"Success",1:"Not super-user",2:"No such file or directory",3:"No such process",4:"Interrupted system call",5:"I/O error",6:"No such device or address",7:"Arg list too long",8:"Exec format error",9:"Bad file number",10:"No children",11:"No more processes",12:"Not enough core",13:"Permission denied",14:"Bad address",15:"Block device required",16:"Mount device busy",17:"File exists",18:"Cross-device link",19:"No such device",20:"Not a directory",21:"Is a directory",22:"Invalid argument",23:"Too many open files in system",24:"Too many open files",25:"Not a typewriter",26:"Text file busy",27:"File too large",28:"No space left on device",29:"Illegal seek",30:"Read only file system",31:"Too many links",32:"Broken pipe",33:"Math arg out of domain of func",34:"Math result not representable",35:"No message of desired type",36:"Identifier removed",37:"Channel number out of range",38:"Level 2 not synchronized",39:"Level 3 halted",40:"Level 3 reset",41:"Link number out of range",42:"Protocol driver not attached",43:"No CSI structure available",44:"Level 2 halted",45:"Deadlock condition",46:"No record locks available",50:"Invalid exchange",51:"Invalid request descriptor",52:"Exchange full",53:"No anode",54:"Invalid request code",55:"Invalid slot",56:"File locking deadlock error",57:"Bad font file fmt",60:"Device not a stream",61:"No data (for no delay io)",62:"Timer expired",63:"Out of streams resources",64:"Machine is not on the network",65:"Package not installed",66:"The object is remote",67:"The link has been severed",68:"Advertise error",69:"Srmount error",70:"Communication error on send",71:"Protocol error",74:"Multihop attempted",76:"Cross mount point (not really error)",77:"Trying to read unreadable message",80:"Given log. name not unique",81:"f.d. invalid for this operation",82:"Remote address changed",83:"Can   access a needed shared lib",84:"Accessing a corrupted shared lib",85:".lib section in a.out corrupted",86:"Attempting to link in too many libs",87:"Attempting to exec a shared library",88:"Function not implemented",90:"Directory not empty",91:"File or path name too long",92:"Too many symbolic links",95:"Operation not supported on transport endpoint",96:"Protocol family not supported",104:"Connection reset by peer",105:"No buffer space available",106:"Address family not supported by protocol family",107:"Protocol wrong type for socket",108:"Socket operation on non-socket",109:"Protocol not available",110:"Can't send after socket shutdown",111:"Connection refused",112:"Address already in use",113:"Connection aborted",114:"Network is unreachable",115:"Network interface is not configured",116:"Connection timed out",117:"Host is down",118:"Host is unreachable",119:"Connection already in progress",120:"Socket already connected",121:"Destination address required",122:"Message too long",123:"Unknown protocol",124:"Socket type not supported",125:"Address not available",126:"Connection reset by network",127:"Socket is already connected",128:"Socket is not connected",129:"Too many references",131:"Too many users",132:"Quota exceeded",133:"Stale file handle",134:"Not supported",135:"No medium (in tape drive)",138:"Illegal byte sequence",139:"Value too large for defined data type",140:"Operation canceled",141:"State not recoverable",142:"Previous owner died",143:"Streams pipe error"};
  var ___errno_state=0;function ___setErrNo(value) {
      // For convenient setting and returning of errno.
      HEAP32[((___errno_state)>>2)]=value
      return value;
    }
  var VFS=undefined;
  var PATH={splitPath:function (filename) {
        var splitPathRe = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
        return splitPathRe.exec(filename).slice(1);
      },normalizeArray:function (parts, allowAboveRoot) {
        // if the path tries to go above the root, `up` ends up > 0
        var up = 0;
        for (var i = parts.length - 1; i >= 0; i--) {
          var last = parts[i];
          if (last === '.') {
            parts.splice(i, 1);
          } else if (last === '..') {
            parts.splice(i, 1);
            up++;
          } else if (up) {
            parts.splice(i, 1);
            up--;
          }
        }
        // if the path is allowed to go above the root, restore leading ..s
        if (allowAboveRoot) {
          for (; up--; up) {
            parts.unshift('..');
          }
        }
        return parts;
      },normalize:function (path) {
        var isAbsolute = path.charAt(0) === '/',
            trailingSlash = path.substr(-1) === '/';
        // Normalize the path
        path = PATH.normalizeArray(path.split('/').filter(function(p) {
          return !!p;
        }), !isAbsolute).join('/');
        if (!path && !isAbsolute) {
          path = '.';
        }
        if (path && trailingSlash) {
          path += '/';
        }
        return (isAbsolute ? '/' : '') + path;
      },dirname:function (path) {
        var result = PATH.splitPath(path),
            root = result[0],
            dir = result[1];
        if (!root && !dir) {
          // No dirname whatsoever
          return '.';
        }
        if (dir) {
          // It has a dirname, strip trailing slash
          dir = dir.substr(0, dir.length - 1);
        }
        return root + dir;
      },basename:function (path, ext) {
        // EMSCRIPTEN return '/'' for '/', not an empty string
        if (path === '/') return '/';
        var f = PATH.splitPath(path)[2];
        if (ext && f.substr(-1 * ext.length) === ext) {
          f = f.substr(0, f.length - ext.length);
        }
        return f;
      },join:function () {
        var paths = Array.prototype.slice.call(arguments, 0);
        return PATH.normalize(paths.filter(function(p, index) {
          if (typeof p !== 'string') {
            throw new TypeError('Arguments to path.join must be strings');
          }
          return p;
        }).join('/'));
      },resolve:function () {
        var resolvedPath = '',
          resolvedAbsolute = false;
        for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
          var path = (i >= 0) ? arguments[i] : FS.cwd();
          // Skip empty and invalid entries
          if (typeof path !== 'string') {
            throw new TypeError('Arguments to path.resolve must be strings');
          } else if (!path) {
            continue;
          }
          resolvedPath = path + '/' + resolvedPath;
          resolvedAbsolute = path.charAt(0) === '/';
        }
        // At this point the path should be resolved to a full absolute path, but
        // handle relative paths to be safe (might happen when process.cwd() fails)
        resolvedPath = PATH.normalizeArray(resolvedPath.split('/').filter(function(p) {
          return !!p;
        }), !resolvedAbsolute).join('/');
        return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
      },relative:function (from, to) {
        from = PATH.resolve(from).substr(1);
        to = PATH.resolve(to).substr(1);
        function trim(arr) {
          var start = 0;
          for (; start < arr.length; start++) {
            if (arr[start] !== '') break;
          }
          var end = arr.length - 1;
          for (; end >= 0; end--) {
            if (arr[end] !== '') break;
          }
          if (start > end) return [];
          return arr.slice(start, end - start + 1);
        }
        var fromParts = trim(from.split('/'));
        var toParts = trim(to.split('/'));
        var length = Math.min(fromParts.length, toParts.length);
        var samePartsLength = length;
        for (var i = 0; i < length; i++) {
          if (fromParts[i] !== toParts[i]) {
            samePartsLength = i;
            break;
          }
        }
        var outputParts = [];
        for (var i = samePartsLength; i < fromParts.length; i++) {
          outputParts.push('..');
        }
        outputParts = outputParts.concat(toParts.slice(samePartsLength));
        return outputParts.join('/');
      }};
  var TTY={ttys:[],register:function (dev, ops) {
        TTY.ttys[dev] = { input: [], output: [], ops: ops };
        FS.registerDevice(dev, TTY.stream_ops);
      },stream_ops:{open:function (stream) {
          // this wouldn't be required if the library wasn't eval'd at first...
          if (!TTY.utf8) {
            TTY.utf8 = new Runtime.UTF8Processor();
          }
          var tty = TTY.ttys[stream.node.rdev];
          if (!tty) {
            throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
          }
          stream.tty = tty;
          stream.seekable = false;
        },close:function (stream) {
          // flush any pending line data
          if (stream.tty.output.length) {
            stream.tty.ops.put_char(stream.tty, 10);
          }
        },read:function (stream, buffer, offset, length, pos /* ignored */) {
          if (!stream.tty || !stream.tty.ops.get_char) {
            throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
          }
          var bytesRead = 0;
          for (var i = 0; i < length; i++) {
            var result;
            try {
              result = stream.tty.ops.get_char(stream.tty);
            } catch (e) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
            if (result === undefined && bytesRead === 0) {
              throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
            }
            if (result === null || result === undefined) break;
            bytesRead++;
            buffer[offset+i] = result;
          }
          if (bytesRead) {
            stream.node.timestamp = Date.now();
          }
          return bytesRead;
        },write:function (stream, buffer, offset, length, pos) {
          if (!stream.tty || !stream.tty.ops.put_char) {
            throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
          }
          for (var i = 0; i < length; i++) {
            try {
              stream.tty.ops.put_char(stream.tty, buffer[offset+i]);
            } catch (e) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
          }
          if (length) {
            stream.node.timestamp = Date.now();
          }
          return i;
        }},default_tty_ops:{get_char:function (tty) {
          if (!tty.input.length) {
            var result = null;
            if (ENVIRONMENT_IS_NODE) {
              if (process.stdin.destroyed) {
                return undefined;
              }
              result = process.stdin.read();
            } else if (typeof window != 'undefined' &&
              typeof window.prompt == 'function') {
              // Browser.
              result = window.prompt('Input: ');  // returns null on cancel
              if (result !== null) {
                result += '\n';
              }
            } else if (typeof readline == 'function') {
              // Command line.
              result = readline();
              if (result !== null) {
                result += '\n';
              }
            }
            if (!result) {
              return null;
            }
            tty.input = intArrayFromString(result, true);
          }
          return tty.input.shift();
        },put_char:function (tty, val) {
          if (val === null || val === 10) {
            Module['print'](tty.output.join(''));
            tty.output = [];
          } else {
            tty.output.push(TTY.utf8.processCChar(val));
          }
        }},default_tty1_ops:{put_char:function (tty, val) {
          if (val === null || val === 10) {
            Module['printErr'](tty.output.join(''));
            tty.output = [];
          } else {
            tty.output.push(TTY.utf8.processCChar(val));
          }
        }}};
  var MEMFS={mount:function (mount) {
        return MEMFS.create_node(null, '/', 0040000 | 0777, 0);
      },create_node:function (parent, name, mode, dev) {
        if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
          // no supported
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        var node = FS.createNode(parent, name, mode, dev);
        if (FS.isDir(node.mode)) {
          node.node_ops = {
            getattr: MEMFS.node_ops.getattr,
            setattr: MEMFS.node_ops.setattr,
            lookup: MEMFS.node_ops.lookup,
            mknod: MEMFS.node_ops.mknod,
            mknod: MEMFS.node_ops.mknod,
            rename: MEMFS.node_ops.rename,
            unlink: MEMFS.node_ops.unlink,
            rmdir: MEMFS.node_ops.rmdir,
            readdir: MEMFS.node_ops.readdir,
            symlink: MEMFS.node_ops.symlink
          };
          node.stream_ops = {
            llseek: MEMFS.stream_ops.llseek
          };
          node.contents = {};
        } else if (FS.isFile(node.mode)) {
          node.node_ops = {
            getattr: MEMFS.node_ops.getattr,
            setattr: MEMFS.node_ops.setattr
          };
          node.stream_ops = {
            llseek: MEMFS.stream_ops.llseek,
            read: MEMFS.stream_ops.read,
            write: MEMFS.stream_ops.write,
            allocate: MEMFS.stream_ops.allocate,
            mmap: MEMFS.stream_ops.mmap
          };
          node.contents = [];
        } else if (FS.isLink(node.mode)) {
          node.node_ops = {
            getattr: MEMFS.node_ops.getattr,
            setattr: MEMFS.node_ops.setattr,
            readlink: MEMFS.node_ops.readlink
          };
          node.stream_ops = {};
        } else if (FS.isChrdev(node.mode)) {
          node.node_ops = {
            getattr: MEMFS.node_ops.getattr,
            setattr: MEMFS.node_ops.setattr
          };
          node.stream_ops = FS.chrdev_stream_ops;
        }
        node.timestamp = Date.now();
        // add the new node to the parent
        if (parent) {
          parent.contents[name] = node;
        }
        return node;
      },node_ops:{getattr:function (node) {
          var attr = {};
          // device numbers reuse inode numbers.
          attr.dev = FS.isChrdev(node.mode) ? node.id : 1;
          attr.ino = node.id;
          attr.mode = node.mode;
          attr.nlink = 1;
          attr.uid = 0;
          attr.gid = 0;
          attr.rdev = node.rdev;
          if (FS.isDir(node.mode)) {
            attr.size = 4096;
          } else if (FS.isFile(node.mode)) {
            attr.size = node.contents.length;
          } else if (FS.isLink(node.mode)) {
            attr.size = node.link.length;
          } else {
            attr.size = 0;
          }
          attr.atime = new Date(node.timestamp);
          attr.mtime = new Date(node.timestamp);
          attr.ctime = new Date(node.timestamp);
          // NOTE: In our implementation, st_blocks = Math.ceil(st_size/st_blksize),
          //       but this is not required by the standard.
          attr.blksize = 4096;
          attr.blocks = Math.ceil(attr.size / attr.blksize);
          return attr;
        },setattr:function (node, attr) {
          if (attr.mode !== undefined) {
            node.mode = attr.mode;
          }
          if (attr.timestamp !== undefined) {
            node.timestamp = attr.timestamp;
          }
          if (attr.size !== undefined) {
            var contents = node.contents;
            if (attr.size < contents.length) contents.length = attr.size;
            else while (attr.size > contents.length) contents.push(0);
          }
        },lookup:function (parent, name) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
        },mknod:function (parent, name, mode, dev) {
          return MEMFS.create_node(parent, name, mode, dev);
        },rename:function (old_node, new_dir, new_name) {
          // if we're overwriting a directory at new_name, make sure it's empty.
          if (FS.isDir(old_node.mode)) {
            var new_node;
            try {
              new_node = FS.lookupNode(new_dir, new_name);
            } catch (e) {
            }
            if (new_node) {
              for (var i in new_node.contents) {
                throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
              }
            }
          }
          // do the internal rewiring
          delete old_node.parent.contents[old_node.name];
          old_node.name = new_name;
          new_dir.contents[new_name] = old_node;
        },unlink:function (parent, name) {
          delete parent.contents[name];
        },rmdir:function (parent, name) {
          var node = FS.lookupNode(parent, name);
          for (var i in node.contents) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
          }
          delete parent.contents[name];
        },readdir:function (node) {
          var entries = ['.', '..']
          for (var key in node.contents) {
            if (!node.contents.hasOwnProperty(key)) {
              continue;
            }
            entries.push(key);
          }
          return entries;
        },symlink:function (parent, newname, oldpath) {
          var node = MEMFS.create_node(parent, newname, 0777 | 0120000, 0);
          node.link = oldpath;
          return node;
        },readlink:function (node) {
          if (!FS.isLink(node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
          return node.link;
        }},stream_ops:{read:function (stream, buffer, offset, length, position) {
          var contents = stream.node.contents;
          var size = Math.min(contents.length - position, length);
          if (contents.subarray) { // typed array
            buffer.set(contents.subarray(position, position + size), offset);
          } else
          {
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents[position + i];
            }
          }
          return size;
        },write:function (stream, buffer, offset, length, position) {
          var contents = stream.node.contents;
          while (contents.length < position) contents.push(0);
          for (var i = 0; i < length; i++) {
            contents[position + i] = buffer[offset + i];
          }
          stream.node.timestamp = Date.now();
          return length;
        },llseek:function (stream, offset, whence) {
          var position = offset;
          if (whence === 1) {  // SEEK_CUR.
            position += stream.position;
          } else if (whence === 2) {  // SEEK_END.
            if (FS.isFile(stream.node.mode)) {
              position += stream.node.contents.length;
            }
          }
          if (position < 0) {
            throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
          }
          stream.ungotten = [];
          stream.position = position;
          return position;
        },allocate:function (stream, offset, length) {
          var contents = stream.node.contents;
          var limit = offset + length;
          while (limit > contents.length) contents.push(0);
        },mmap:function (stream, buffer, offset, length, position, prot, flags) {
          if (!FS.isFile(stream.node.mode)) {
            throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
          }
          var ptr;
          var allocated;
          var contents = stream.node.contents;
          // Only make a new copy when MAP_PRIVATE is specified.
          if (!(flags & 0x02)) {
            // We can't emulate MAP_SHARED when the file is not backed by the buffer
            // we're mapping to (e.g. the HEAP buffer).
            assert(contents.buffer === buffer || contents.buffer === buffer.buffer);
            allocated = false;
            ptr = contents.byteOffset;
          } else {
            // Try to avoid unnecessary slices.
            if (position > 0 || position + length < contents.length) {
              if (contents.subarray) {
                contents = contents.subarray(position, position + length);
              } else {
                contents = Array.prototype.slice.call(contents, position, position + length);
              }
            }
            allocated = true;
            ptr = _malloc(length);
            if (!ptr) {
              throw new FS.ErrnoError(ERRNO_CODES.ENOMEM);
            }
            buffer.set(contents, ptr);
          }
          return { ptr: ptr, allocated: allocated };
        }}};
  var _stdin=allocate(1, "i32*", ALLOC_STATIC);
  var _stdout=allocate(1, "i32*", ALLOC_STATIC);
  var _stderr=allocate(1, "i32*", ALLOC_STATIC);
  function _fflush(stream) {
      // int fflush(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fflush.html
      // we don't currently perform any user-space buffering of data
    }var FS={root:null,nodes:[null],devices:[null],streams:[null],nextInode:1,name_table:null,currentPath:"/",initialized:false,ignorePermissions:true,ErrnoError:function ErrnoError(errno) {
          this.errno = errno;
          for (var key in ERRNO_CODES) {
            if (ERRNO_CODES[key] === errno) {
              this.code = key;
              break;
            }
          }
          this.message = ERRNO_MESSAGES[errno];
        },handleFSError:function (e) {
        if (!(e instanceof FS.ErrnoError)) throw e + ' : ' + new Error().stack;
        return ___setErrNo(e.errno);
      },hashName:function (parentid, name) {
        var hash = 0;
        for (var i = 0; i < name.length; i++) {
          hash = ((hash << 5) - hash + name.charCodeAt(i)) | 0;
        }
        return ((parentid + hash) >>> 0) % FS.name_table.length;
      },hashAddNode:function (node) {
        var hash = FS.hashName(node.parent.id, node.name);
        node.name_next = FS.name_table[hash];
        FS.name_table[hash] = node;
      },hashRemoveNode:function (node) {
        var hash = FS.hashName(node.parent.id, node.name);
        if (FS.name_table[hash] === node) {
          FS.name_table[hash] = node.name_next;
        } else {
          var current = FS.name_table[hash];
          while (current) {
            if (current.name_next === node) {
              current.name_next = node.name_next;
              break;
            }
            current = current.name_next;
          }
        }
      },lookupNode:function (parent, name) {
        var err = FS.mayLookup(parent);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        var hash = FS.hashName(parent.id, name);
        for (var node = FS.name_table[hash]; node; node = node.name_next) {
          if (node.parent.id === parent.id && node.name === name) {
            return node;
          }
        }
        // if we failed to find it in the cache, call into the VFS
        return FS.lookup(parent, name);
      },createNode:function (parent, name, mode, rdev) {
        var node = {
          id: FS.nextInode++,
          name: name,
          mode: mode,
          node_ops: {},
          stream_ops: {},
          rdev: rdev,
          parent: null,
          mount: null
        };
        if (!parent) {
          parent = node;  // root node sets parent to itself
        }
        node.parent = parent;
        node.mount = parent.mount;
        // compatibility
        var readMode = 292 | 73;
        var writeMode = 146;
        // NOTE we must use Object.defineProperties instead of individual calls to
        // Object.defineProperty in order to make closure compiler happy
        Object.defineProperties(node, {
          read: {
            get: function() { return (node.mode & readMode) === readMode; },
            set: function(val) { val ? node.mode |= readMode : node.mode &= ~readMode; }
          },
          write: {
            get: function() { return (node.mode & writeMode) === writeMode; },
            set: function(val) { val ? node.mode |= writeMode : node.mode &= ~writeMode; }
          },
          isFolder: {
            get: function() { return FS.isDir(node.mode); },
          },
          isDevice: {
            get: function() { return FS.isChrdev(node.mode); },
          },
        });
        FS.hashAddNode(node);
        return node;
      },destroyNode:function (node) {
        FS.hashRemoveNode(node);
      },isRoot:function (node) {
        return node === node.parent;
      },isMountpoint:function (node) {
        return node.mounted;
      },isFile:function (mode) {
        return (mode & 0170000) === 0100000;
      },isDir:function (mode) {
        return (mode & 0170000) === 0040000;
      },isLink:function (mode) {
        return (mode & 0170000) === 0120000;
      },isChrdev:function (mode) {
        return (mode & 0170000) === 0020000;
      },isBlkdev:function (mode) {
        return (mode & 0170000) === 0060000;
      },isFIFO:function (mode) {
        return (mode & 0170000) === 0010000;
      },cwd:function () {
        return FS.currentPath;
      },lookupPath:function (path, opts) {
        path = PATH.resolve(FS.currentPath, path);
        opts = opts || { recurse_count: 0 };
        if (opts.recurse_count > 8) {  // max recursive lookup of 8
          throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
        }
        // split the path
        var parts = PATH.normalizeArray(path.split('/').filter(function(p) {
          return !!p;
        }), false);
        // start at the root
        var current = FS.root;
        var current_path = '/';
        for (var i = 0; i < parts.length; i++) {
          var islast = (i === parts.length-1);
          if (islast && opts.parent) {
            // stop resolving
            break;
          }
          current = FS.lookupNode(current, parts[i]);
          current_path = PATH.join(current_path, parts[i]);
          // jump to the mount's root node if this is a mountpoint
          if (FS.isMountpoint(current)) {
            current = current.mount.root;
          }
          // follow symlinks
          // by default, lookupPath will not follow a symlink if it is the final path component.
          // setting opts.follow = true will override this behavior.
          if (!islast || opts.follow) {
            var count = 0;
            while (FS.isLink(current.mode)) {
              var link = FS.readlink(current_path);
              current_path = PATH.resolve(PATH.dirname(current_path), link);
              var lookup = FS.lookupPath(current_path, { recurse_count: opts.recurse_count });
              current = lookup.node;
              if (count++ > 40) {  // limit max consecutive symlinks to 40 (SYMLOOP_MAX).
                throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
              }
            }
          }
        }
        return { path: current_path, node: current };
      },getPath:function (node) {
        var path;
        while (true) {
          if (FS.isRoot(node)) {
            return path ? PATH.join(node.mount.mountpoint, path) : node.mount.mountpoint;
          }
          path = path ? PATH.join(node.name, path) : node.name;
          node = node.parent;
        }
      },flagModes:{"r":0,"rs":8192,"r+":2,"w":1537,"wx":3585,"xw":3585,"w+":1538,"wx+":3586,"xw+":3586,"a":521,"ax":2569,"xa":2569,"a+":522,"ax+":2570,"xa+":2570},modeStringToFlags:function (str) {
        var flags = FS.flagModes[str];
        if (typeof flags === 'undefined') {
          throw new Error('Unknown file open mode: ' + str);
        }
        return flags;
      },flagsToPermissionString:function (flag) {
        var accmode = flag & 3;
        var perms = ['r', 'w', 'rw'][accmode];
        if ((flag & 1024)) {
          perms += 'w';
        }
        return perms;
      },nodePermissions:function (node, perms) {
        if (FS.ignorePermissions) {
          return 0;
        }
        // return 0 if any user, group or owner bits are set.
        if (perms.indexOf('r') !== -1 && !(node.mode & 292)) {
          return ERRNO_CODES.EACCES;
        } else if (perms.indexOf('w') !== -1 && !(node.mode & 146)) {
          return ERRNO_CODES.EACCES;
        } else if (perms.indexOf('x') !== -1 && !(node.mode & 73)) {
          return ERRNO_CODES.EACCES;
        }
        return 0;
      },mayLookup:function (dir) {
        return FS.nodePermissions(dir, 'x');
      },mayMknod:function (mode) {
        switch (mode & 0170000) {
          case 0100000:
          case 0020000:
          case 0060000:
          case 0010000:
          case 0140000:
            return 0;
          default:
            return ERRNO_CODES.EINVAL;
        }
      },mayCreate:function (dir, name) {
        try {
          var node = FS.lookupNode(dir, name);
          return ERRNO_CODES.EEXIST;
        } catch (e) {
        }
        return FS.nodePermissions(dir, 'wx');
      },mayDelete:function (dir, name, isdir) {
        var node;
        try {
          node = FS.lookupNode(dir, name);
        } catch (e) {
          return e.errno;
        }
        var err = FS.nodePermissions(dir, 'wx');
        if (err) {
          return err;
        }
        if (isdir) {
          if (!FS.isDir(node.mode)) {
            return ERRNO_CODES.ENOTDIR;
          }
          if (FS.isRoot(node) || FS.getPath(node) === FS.currentPath) {
            return ERRNO_CODES.EBUSY;
          }
        } else {
          if (FS.isDir(node.mode)) {
            return ERRNO_CODES.EISDIR;
          }
        }
        return 0;
      },mayOpen:function (node, flags) {
        if (!node) {
          return ERRNO_CODES.ENOENT;
        }
        if (FS.isLink(node.mode)) {
          return ERRNO_CODES.ELOOP;
        } else if (FS.isDir(node.mode)) {
          if ((flags & 3) !== 0 ||  // opening for write
              (flags & 1024)) {
            return ERRNO_CODES.EISDIR;
          }
        }
        return FS.nodePermissions(node, FS.flagsToPermissionString(flags));
      },chrdev_stream_ops:{open:function (stream) {
          var device = FS.getDevice(stream.node.rdev);
          // override node's stream ops with the device's
          stream.stream_ops = device.stream_ops;
          // forward the open call
          if (stream.stream_ops.open) {
            stream.stream_ops.open(stream);
          }
        },llseek:function () {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }},major:function (dev) {
        return ((dev) >> 8);
      },minor:function (dev) {
        return ((dev) & 0xff);
      },makedev:function (ma, mi) {
        return ((ma) << 8 | (mi));
      },registerDevice:function (dev, ops) {
        FS.devices[dev] = { stream_ops: ops };
      },getDevice:function (dev) {
        return FS.devices[dev];
      },MAX_OPEN_FDS:4096,nextfd:function (fd_start, fd_end) {
        fd_start = fd_start || 1;
        fd_end = fd_end || FS.MAX_OPEN_FDS;
        for (var fd = fd_start; fd <= fd_end; fd++) {
          if (!FS.streams[fd]) {
            return fd;
          }
        }
        throw new FS.ErrnoError(ERRNO_CODES.EMFILE);
      },getStream:function (fd) {
        return FS.streams[fd];
      },createStream:function (stream, fd_start, fd_end) {
        var fd = FS.nextfd(fd_start, fd_end);
        stream.fd = fd;
        // compatibility
        Object.defineProperties(stream, {
          object: {
            get: function() { return stream.node; },
            set: function(val) { stream.node = val; }
          },
          isRead: {
            get: function() { return (stream.flags & 3) !== 1; }
          },
          isWrite: {
            get: function() { return (stream.flags & 3) !== 0; }
          },
          isAppend: {
            get: function() { return (stream.flags & 8); }
          }
        });
        FS.streams[fd] = stream;
        return stream;
      },closeStream:function (fd) {
        FS.streams[fd] = null;
      },getMode:function (canRead, canWrite) {
        var mode = 0;
        if (canRead) mode |= 292 | 73;
        if (canWrite) mode |= 146;
        return mode;
      },joinPath:function (parts, forceRelative) {
        var path = PATH.join.apply(null, parts);
        if (forceRelative && path[0] == '/') path = path.substr(1);
        return path;
      },absolutePath:function (relative, base) {
        return PATH.resolve(base, relative);
      },standardizePath:function (path) {
        return PATH.normalize(path);
      },findObject:function (path, dontResolveLastLink) {
        var ret = FS.analyzePath(path, dontResolveLastLink);
        if (ret.exists) {
          return ret.object;
        } else {
          ___setErrNo(ret.error);
          return null;
        }
      },analyzePath:function (path, dontResolveLastLink) {
        // operate from within the context of the symlink's target
        try {
          var lookup = FS.lookupPath(path, { follow: !dontResolveLastLink });
          path = lookup.path;
        } catch (e) {
        }
        var ret = {
          isRoot: false, exists: false, error: 0, name: null, path: null, object: null,
          parentExists: false, parentPath: null, parentObject: null
        };
        try {
          var lookup = FS.lookupPath(path, { parent: true });
          ret.parentExists = true;
          ret.parentPath = lookup.path;
          ret.parentObject = lookup.node;
          ret.name = PATH.basename(path);
          lookup = FS.lookupPath(path, { follow: !dontResolveLastLink });
          ret.exists = true;
          ret.path = lookup.path;
          ret.object = lookup.node;
          ret.name = lookup.node.name;
          ret.isRoot = lookup.path === '/';
        } catch (e) {
          ret.error = e.errno;
        };
        return ret;
      },createFolder:function (parent, name, canRead, canWrite) {
        var path = PATH.join(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.mkdir(path, mode);
      },createPath:function (parent, path, canRead, canWrite) {
        parent = typeof parent === 'string' ? parent : FS.getPath(parent);
        var parts = path.split('/').reverse();
        while (parts.length) {
          var part = parts.pop();
          if (!part) continue;
          var current = PATH.join(parent, part);
          try {
            FS.mkdir(current, 0777);
          } catch (e) {
            // ignore EEXIST
          }
          parent = current;
        }
        return current;
      },createFile:function (parent, name, properties, canRead, canWrite) {
        var path = PATH.join(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        return FS.create(path, mode);
      },createDataFile:function (parent, name, data, canRead, canWrite) {
        var path = PATH.join(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = FS.getMode(canRead, canWrite);
        var node = FS.create(path, mode);
        if (data) {
          if (typeof data === 'string') {
            var arr = new Array(data.length);
            for (var i = 0, len = data.length; i < len; ++i) arr[i] = data.charCodeAt(i);
            data = arr;
          }
          // make sure we can write to the file
          FS.chmod(path, mode | 146);
          var stream = FS.open(path, 'w');
          FS.write(stream, data, 0, data.length, 0);
          FS.close(stream);
          FS.chmod(path, mode);
        }
        return node;
      },createDevice:function (parent, name, input, output) {
        var path = PATH.join(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        var mode = input && output ? 0777 : (input ? 0333 : 0555);
        if (!FS.createDevice.major) FS.createDevice.major = 64;
        var dev = FS.makedev(FS.createDevice.major++, 0);
        // Create a fake device that a set of stream ops to emulate
        // the old behavior.
        FS.registerDevice(dev, {
          open: function(stream) {
            stream.seekable = false;
          },
          close: function(stream) {
            // flush any pending line data
            if (output && output.buffer && output.buffer.length) {
              output(10);
            }
          },
          read: function(stream, buffer, offset, length, pos /* ignored */) {
            var bytesRead = 0;
            for (var i = 0; i < length; i++) {
              var result;
              try {
                result = input();
              } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES.EIO);
              }
              if (result === undefined && bytesRead === 0) {
                throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
              }
              if (result === null || result === undefined) break;
              bytesRead++;
              buffer[offset+i] = result;
            }
            if (bytesRead) {
              stream.node.timestamp = Date.now();
            }
            return bytesRead;
          },
          write: function(stream, buffer, offset, length, pos) {
            for (var i = 0; i < length; i++) {
              try {
                output(buffer[offset+i]);
              } catch (e) {
                throw new FS.ErrnoError(ERRNO_CODES.EIO);
              }
            }
            if (length) {
              stream.node.timestamp = Date.now();
            }
            return i;
          }
        });
        return FS.mkdev(path, mode, dev);
      },createLink:function (parent, name, target, canRead, canWrite) {
        var path = PATH.join(typeof parent === 'string' ? parent : FS.getPath(parent), name);
        return FS.symlink(target, path);
      },forceLoadFile:function (obj) {
        if (obj.isDevice || obj.isFolder || obj.link || obj.contents) return true;
        var success = true;
        if (typeof XMLHttpRequest !== 'undefined') {
          throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
        } else if (Module['read']) {
          // Command-line.
          try {
            // WARNING: Can't read binary files in V8's d8 or tracemonkey's js, as
            //          read() will try to parse UTF8.
            obj.contents = intArrayFromString(Module['read'](obj.url), true);
          } catch (e) {
            success = false;
          }
        } else {
          throw new Error('Cannot load without read() or XMLHttpRequest.');
        }
        if (!success) ___setErrNo(ERRNO_CODES.EIO);
        return success;
      },createLazyFile:function (parent, name, url, canRead, canWrite) {
        if (typeof XMLHttpRequest !== 'undefined') {
          if (!ENVIRONMENT_IS_WORKER) throw 'Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc';
          // Lazy chunked Uint8Array (implements get and length from Uint8Array). Actual getting is abstracted away for eventual reuse.
          var LazyUint8Array = function() {
            this.lengthKnown = false;
            this.chunks = []; // Loaded chunks. Index is the chunk number
          }
          LazyUint8Array.prototype.get = function(idx) {
            if (idx > this.length-1 || idx < 0) {
              return undefined;
            }
            var chunkOffset = idx % this.chunkSize;
            var chunkNum = Math.floor(idx / this.chunkSize);
            return this.getter(chunkNum)[chunkOffset];
          }
          LazyUint8Array.prototype.setDataGetter = function(getter) {
            this.getter = getter;
          }
          LazyUint8Array.prototype.cacheLength = function() {
              // Find length
              var xhr = new XMLHttpRequest();
              xhr.open('HEAD', url, false);
              xhr.send(null);
              if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
              var datalength = Number(xhr.getResponseHeader("Content-length"));
              var header;
              var hasByteServing = (header = xhr.getResponseHeader("Accept-Ranges")) && header === "bytes";
              var chunkSize = 1024*1024; // Chunk size in bytes
              if (!hasByteServing) chunkSize = datalength;
              // Function to get a range from the remote URL.
              var doXHR = (function(from, to) {
                if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
                if (to > datalength-1) throw new Error("only " + datalength + " bytes available! programmer error!");
                // TODO: Use mozResponseArrayBuffer, responseStream, etc. if available.
                var xhr = new XMLHttpRequest();
                xhr.open('GET', url, false);
                if (datalength !== chunkSize) xhr.setRequestHeader("Range", "bytes=" + from + "-" + to);
                // Some hints to the browser that we want binary data.
                if (typeof Uint8Array != 'undefined') xhr.responseType = 'arraybuffer';
                if (xhr.overrideMimeType) {
                  xhr.overrideMimeType('text/plain; charset=x-user-defined');
                }
                xhr.send(null);
                if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
                if (xhr.response !== undefined) {
                  return new Uint8Array(xhr.response || []);
                } else {
                  return intArrayFromString(xhr.responseText || '', true);
                }
              });
              var lazyArray = this;
              lazyArray.setDataGetter(function(chunkNum) {
                var start = chunkNum * chunkSize;
                var end = (chunkNum+1) * chunkSize - 1; // including this byte
                end = Math.min(end, datalength-1); // if datalength-1 is selected, this is the last block
                if (typeof(lazyArray.chunks[chunkNum]) === "undefined") {
                  lazyArray.chunks[chunkNum] = doXHR(start, end);
                }
                if (typeof(lazyArray.chunks[chunkNum]) === "undefined") throw new Error("doXHR failed!");
                return lazyArray.chunks[chunkNum];
              });
              this._length = datalength;
              this._chunkSize = chunkSize;
              this.lengthKnown = true;
          }
          var lazyArray = new LazyUint8Array();
          Object.defineProperty(lazyArray, "length", {
              get: function() {
                  if(!this.lengthKnown) {
                      this.cacheLength();
                  }
                  return this._length;
              }
          });
          Object.defineProperty(lazyArray, "chunkSize", {
              get: function() {
                  if(!this.lengthKnown) {
                      this.cacheLength();
                  }
                  return this._chunkSize;
              }
          });
          var properties = { isDevice: false, contents: lazyArray };
        } else {
          var properties = { isDevice: false, url: url };
        }
        var node = FS.createFile(parent, name, properties, canRead, canWrite);
        // This is a total hack, but I want to get this lazy file code out of the
        // core of MEMFS. If we want to keep this lazy file concept I feel it should
        // be its own thin LAZYFS proxying calls to MEMFS.
        if (properties.contents) {
          node.contents = properties.contents;
        } else if (properties.url) {
          node.contents = null;
          node.url = properties.url;
        }
        // override each stream op with one that tries to force load the lazy file first
        var stream_ops = {};
        var keys = Object.keys(node.stream_ops);
        keys.forEach(function(key) {
          var fn = node.stream_ops[key];
          stream_ops[key] = function() {
            if (!FS.forceLoadFile(node)) {
              throw new FS.ErrnoError(ERRNO_CODES.EIO);
            }
            return fn.apply(null, arguments);
          };
        });
        // use a custom read function
        stream_ops.read = function(stream, buffer, offset, length, position) {
          if (!FS.forceLoadFile(node)) {
            throw new FS.ErrnoError(ERRNO_CODES.EIO);
          }
          var contents = stream.node.contents;
          var size = Math.min(contents.length - position, length);
          if (contents.slice) { // normal array
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents[position + i];
            }
          } else {
            for (var i = 0; i < size; i++) { // LazyUint8Array from sync binary XHR
              buffer[offset + i] = contents.get(position + i);
            }
          }
          return size;
        };
        node.stream_ops = stream_ops;
        return node;
      },createPreloadedFile:function (parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile) {
        Browser.init();
        // TODO we should allow people to just pass in a complete filename instead
        // of parent and name being that we just join them anyways
        var fullname = PATH.resolve(PATH.join(parent, name));
        function processData(byteArray) {
          function finish(byteArray) {
            if (!dontCreateFile) {
              FS.createDataFile(parent, name, byteArray, canRead, canWrite);
            }
            if (onload) onload();
            removeRunDependency('cp ' + fullname);
          }
          var handled = false;
          Module['preloadPlugins'].forEach(function(plugin) {
            if (handled) return;
            if (plugin['canHandle'](fullname)) {
              plugin['handle'](byteArray, fullname, finish, function() {
                if (onerror) onerror();
                removeRunDependency('cp ' + fullname);
              });
              handled = true;
            }
          });
          if (!handled) finish(byteArray);
        }
        addRunDependency('cp ' + fullname);
        if (typeof url == 'string') {
          Browser.asyncLoad(url, function(byteArray) {
            processData(byteArray);
          }, onerror);
        } else {
          processData(url);
        }
      },createDefaultDirectories:function () {
        FS.mkdir('/tmp', 0777);
      },createDefaultDevices:function () {
        // create /dev
        FS.mkdir('/dev', 0777);
        // setup /dev/null
        FS.registerDevice(FS.makedev(1, 3), {
          read: function() { return 0; },
          write: function() { return 0; }
        });
        FS.mkdev('/dev/null', 0666, FS.makedev(1, 3));
        // setup /dev/tty and /dev/tty1
        // stderr needs to print output using Module['printErr']
        // so we register a second tty just for it.
        TTY.register(FS.makedev(5, 0), TTY.default_tty_ops);
        TTY.register(FS.makedev(6, 0), TTY.default_tty1_ops);
        FS.mkdev('/dev/tty', 0666, FS.makedev(5, 0));
        FS.mkdev('/dev/tty1', 0666, FS.makedev(6, 0));
        // we're not going to emulate the actual shm device,
        // just create the tmp dirs that reside in it commonly
        FS.mkdir('/dev/shm', 0777);
        FS.mkdir('/dev/shm/tmp', 0777);
      },createStandardStreams:function () {
        // TODO deprecate the old functionality of a single
        // input / output callback and that utilizes FS.createDevice
        // and instead require a unique set of stream ops
        // by default, we symlink the standard streams to the
        // default tty devices. however, if the standard streams
        // have been overwritten we create a unique device for
        // them instead.
        if (Module['stdin']) {
          FS.createDevice('/dev', 'stdin', Module['stdin']);
        } else {
          FS.symlink('/dev/tty', '/dev/stdin');
        }
        if (Module['stdout']) {
          FS.createDevice('/dev', 'stdout', null, Module['stdout']);
        } else {
          FS.symlink('/dev/tty', '/dev/stdout');
        }
        if (Module['stderr']) {
          FS.createDevice('/dev', 'stderr', null, Module['stderr']);
        } else {
          FS.symlink('/dev/tty1', '/dev/stderr');
        }
        // open default streams for the stdin, stdout and stderr devices
        var stdin = FS.open('/dev/stdin', 'r');
        HEAP32[((_stdin)>>2)]=stdin.fd;
        assert(stdin.fd === 1, 'invalid handle for stdin (' + stdin.fd + ')');
        var stdout = FS.open('/dev/stdout', 'w');
        HEAP32[((_stdout)>>2)]=stdout.fd;
        assert(stdout.fd === 2, 'invalid handle for stdout (' + stdout.fd + ')');
        var stderr = FS.open('/dev/stderr', 'w');
        HEAP32[((_stderr)>>2)]=stderr.fd;
        assert(stderr.fd === 3, 'invalid handle for stderr (' + stderr.fd + ')');
      },staticInit:function () {
        FS.name_table = new Array(4096);
        FS.root = FS.createNode(null, '/', 0040000 | 0777, 0);
        FS.mount(MEMFS, {}, '/');
        FS.createDefaultDirectories();
        FS.createDefaultDevices();
      },init:function (input, output, error) {
        assert(!FS.init.initialized, 'FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)');
        FS.init.initialized = true;
        // Allow Module.stdin etc. to provide defaults, if none explicitly passed to us here
        Module['stdin'] = input || Module['stdin'];
        Module['stdout'] = output || Module['stdout'];
        Module['stderr'] = error || Module['stderr'];
        FS.createStandardStreams();
      },quit:function () {
        FS.init.initialized = false;
        for (var i = 0; i < FS.streams.length; i++) {
          var stream = FS.streams[i];
          if (!stream) {
            continue;
          }
          FS.close(stream);
        }
      },mount:function (type, opts, mountpoint) {
        var mount = {
          type: type,
          opts: opts,
          mountpoint: mountpoint,
          root: null
        };
        var lookup;
        if (mountpoint) {
          lookup = FS.lookupPath(mountpoint, { follow: false });
        }
        // create a root node for the fs
        var root = type.mount(mount);
        root.mount = mount;
        mount.root = root;
        // assign the mount info to the mountpoint's node
        if (lookup) {
          lookup.node.mount = mount;
          lookup.node.mounted = true;
          // compatibility update FS.root if we mount to /
          if (mountpoint === '/') {
            FS.root = mount.root;
          }
        }
        return root;
      },lookup:function (parent, name) {
        return parent.node_ops.lookup(parent, name);
      },mknod:function (path, mode, dev) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var err = FS.mayCreate(parent, name);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.mknod) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return parent.node_ops.mknod(parent, name, mode, dev);
      },create:function (path, mode) {
        mode &= 4095;
        mode |= 0100000;
        return FS.mknod(path, mode, 0);
      },mkdir:function (path, mode) {
        mode &= 511 | 0001000;
        mode |= 0040000;
        return FS.mknod(path, mode, 0);
      },mkdev:function (path, mode, dev) {
        mode |= 0020000;
        return FS.mknod(path, mode, dev);
      },symlink:function (oldpath, newpath) {
        var lookup = FS.lookupPath(newpath, { parent: true });
        var parent = lookup.node;
        var newname = PATH.basename(newpath);
        var err = FS.mayCreate(parent, newname);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.symlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return parent.node_ops.symlink(parent, newname, oldpath);
      },rename:function (old_path, new_path) {
        var old_dirname = PATH.dirname(old_path);
        var new_dirname = PATH.dirname(new_path);
        var old_name = PATH.basename(old_path);
        var new_name = PATH.basename(new_path);
        // parents must exist
        var lookup, old_dir, new_dir;
        try {
          lookup = FS.lookupPath(old_path, { parent: true });
          old_dir = lookup.node;
          lookup = FS.lookupPath(new_path, { parent: true });
          new_dir = lookup.node;
        } catch (e) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        // need to be part of the same mount
        if (old_dir.mount !== new_dir.mount) {
          throw new FS.ErrnoError(ERRNO_CODES.EXDEV);
        }
        // source must exist
        var old_node = FS.lookupNode(old_dir, old_name);
        // old path should not be an ancestor of the new path
        var relative = PATH.relative(old_path, new_dirname);
        if (relative.charAt(0) !== '.') {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        // new path should not be an ancestor of the old path
        relative = PATH.relative(new_path, old_dirname);
        if (relative.charAt(0) !== '.') {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
        }
        // see if the new path already exists
        var new_node;
        try {
          new_node = FS.lookupNode(new_dir, new_name);
        } catch (e) {
          // not fatal
        }
        // early out if nothing needs to change
        if (old_node === new_node) {
          return;
        }
        // we'll need to delete the old entry
        var isdir = FS.isDir(old_node.mode);
        var err = FS.mayDelete(old_dir, old_name, isdir);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        // need delete permissions if we'll be overwriting.
        // need create permissions if new doesn't already exist.
        err = new_node ?
          FS.mayDelete(new_dir, new_name, isdir) :
          FS.mayCreate(new_dir, new_name);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!old_dir.node_ops.rename) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(old_node) || (new_node && FS.isMountpoint(new_node))) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        // if we are going to change the parent, check write permissions
        if (new_dir !== old_dir) {
          err = FS.nodePermissions(old_dir, 'w');
          if (err) {
            throw new FS.ErrnoError(err);
          }
        }
        // remove the node from the lookup hash
        FS.hashRemoveNode(old_node);
        // do the underlying fs rename
        try {
          old_dir.node_ops.rename(old_node, new_dir, new_name);
        } catch (e) {
          throw e;
        } finally {
          // add the node back to the hash (in case node_ops.rename
          // changed its name)
          FS.hashAddNode(old_node);
        }
      },rmdir:function (path) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var err = FS.mayDelete(parent, name, true);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.rmdir) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        parent.node_ops.rmdir(parent, name);
        FS.destroyNode(node);
      },readdir:function (path) {
        var lookup = FS.lookupPath(path, { follow: true });
        var node = lookup.node;
        if (!node.node_ops.readdir) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
        }
        return node.node_ops.readdir(node);
      },unlink:function (path) {
        var lookup = FS.lookupPath(path, { parent: true });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var err = FS.mayDelete(parent, name, false);
        if (err) {
          // POSIX says unlink should set EPERM, not EISDIR
          if (err === ERRNO_CODES.EISDIR) err = ERRNO_CODES.EPERM;
          throw new FS.ErrnoError(err);
        }
        if (!parent.node_ops.unlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
        }
        parent.node_ops.unlink(parent, name);
        FS.destroyNode(node);
      },readlink:function (path) {
        var lookup = FS.lookupPath(path, { follow: false });
        var link = lookup.node;
        if (!link.node_ops.readlink) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        return link.node_ops.readlink(link);
      },stat:function (path, dontFollow) {
        var lookup = FS.lookupPath(path, { follow: !dontFollow });
        var node = lookup.node;
        if (!node.node_ops.getattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        return node.node_ops.getattr(node);
      },lstat:function (path) {
        return FS.stat(path, true);
      },chmod:function (path, mode, dontFollow) {
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: !dontFollow });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        node.node_ops.setattr(node, {
          mode: (mode & 4095) | (node.mode & ~4095),
          timestamp: Date.now()
        });
      },lchmod:function (path, mode) {
        FS.chmod(path, mode, true);
      },fchmod:function (fd, mode) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chmod(stream.node, mode);
      },chown:function (path, uid, gid, dontFollow) {
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: !dontFollow });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        node.node_ops.setattr(node, {
          timestamp: Date.now()
          // we ignore the uid / gid for now
        });
      },lchown:function (path, uid, gid) {
        FS.chown(path, uid, gid, true);
      },fchown:function (fd, uid, gid) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        FS.chown(stream.node, uid, gid);
      },truncate:function (path, len) {
        if (len < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var node;
        if (typeof path === 'string') {
          var lookup = FS.lookupPath(path, { follow: true });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(ERRNO_CODES.EPERM);
        }
        if (FS.isDir(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!FS.isFile(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var err = FS.nodePermissions(node, 'w');
        if (err) {
          throw new FS.ErrnoError(err);
        }
        node.node_ops.setattr(node, {
          size: len,
          timestamp: Date.now()
        });
      },ftruncate:function (fd, len) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if ((stream.flags & 3) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        FS.truncate(stream.node, len);
      },utime:function (path, atime, mtime) {
        var lookup = FS.lookupPath(path, { follow: true });
        var node = lookup.node;
        node.node_ops.setattr(node, {
          timestamp: Math.max(atime, mtime)
        });
      },open:function (path, flags, mode, fd_start, fd_end) {
        path = PATH.normalize(path);
        flags = typeof flags === 'string' ? FS.modeStringToFlags(flags) : flags;
        mode = typeof mode === 'undefined' ? 0666 : mode;
        if ((flags & 512)) {
          mode = (mode & 4095) | 0100000;
        } else {
          mode = 0;
        }
        var node;
        try {
          var lookup = FS.lookupPath(path, {
            follow: !(flags & 0200000)
          });
          node = lookup.node;
          path = lookup.path;
        } catch (e) {
          // ignore
        }
        // perhaps we need to create the node
        if ((flags & 512)) {
          if (node) {
            // if O_CREAT and O_EXCL are set, error out if the node already exists
            if ((flags & 2048)) {
              throw new FS.ErrnoError(ERRNO_CODES.EEXIST);
            }
          } else {
            // node doesn't exist, try to create it
            node = FS.mknod(path, mode, 0);
          }
        }
        if (!node) {
          throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
        }
        // can't truncate a device
        if (FS.isChrdev(node.mode)) {
          flags &= ~1024;
        }
        // check permissions
        var err = FS.mayOpen(node, flags);
        if (err) {
          throw new FS.ErrnoError(err);
        }
        // do truncation if necessary
        if ((flags & 1024)) {
          FS.truncate(node, 0);
        }
        // register the stream with the filesystem
        var stream = FS.createStream({
          path: path,
          node: node,
          flags: flags,
          seekable: true,
          position: 0,
          stream_ops: node.stream_ops,
          // used by the file family libc calls (fopen, fwrite, ferror, etc.)
          ungotten: [],
          error: false
        }, fd_start, fd_end);
        // call the new stream's open function
        if (stream.stream_ops.open) {
          stream.stream_ops.open(stream);
        }
        return stream;
      },close:function (stream) {
        try {
          if (stream.stream_ops.close) {
            stream.stream_ops.close(stream);
          }
        } catch (e) {
          throw e;
        } finally {
          FS.closeStream(stream.fd);
        }
      },llseek:function (stream, offset, whence) {
        if (!stream.seekable || !stream.stream_ops.llseek) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        return stream.stream_ops.llseek(stream, offset, whence);
      },read:function (stream, buffer, offset, length, position) {
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 3) === 1) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!stream.stream_ops.read) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var seeking = true;
        if (typeof position === 'undefined') {
          position = stream.position;
          seeking = false;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        var bytesRead = stream.stream_ops.read(stream, buffer, offset, length, position);
        if (!seeking) stream.position += bytesRead;
        return bytesRead;
      },write:function (stream, buffer, offset, length, position) {
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 3) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
        }
        if (!stream.stream_ops.write) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        var seeking = true;
        if (typeof position === 'undefined') {
          position = stream.position;
          seeking = false;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
        }
        if (stream.flags & 8) {
          // seek to the end before writing in append mode
          FS.llseek(stream, 0, 2);
        }
        var bytesWritten = stream.stream_ops.write(stream, buffer, offset, length, position);
        if (!seeking) stream.position += bytesWritten;
        return bytesWritten;
      },allocate:function (stream, offset, length) {
        if (offset < 0 || length <= 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
        if ((stream.flags & 3) === 0) {
          throw new FS.ErrnoError(ERRNO_CODES.EBADF);
        }
        if (!FS.isFile(stream.node.mode) && !FS.isDir(node.mode)) {
          throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
        }
        if (!stream.stream_ops.allocate) {
          throw new FS.ErrnoError(ERRNO_CODES.EOPNOTSUPP);
        }
        stream.stream_ops.allocate(stream, offset, length);
      },mmap:function (stream, buffer, offset, length, position, prot, flags) {
        // TODO if PROT is PROT_WRITE, make sure we have write access
        if ((stream.flags & 3) === 1) {
          throw new FS.ErrnoError(ERRNO_CODES.EACCES);
        }
        if (!stream.stream_ops.mmap) {
          throw new FS.errnoError(ERRNO_CODES.ENODEV);
        }
        return stream.stream_ops.mmap(stream, buffer, offset, length, position, prot, flags);
      }};
  function _send(fd, buf, len, flags) {
      var info = FS.getStream(fd);
      if (!info) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      if (info.socket.readyState === WebSocket.CLOSING || info.socket.readyState === WebSocket.CLOSED) {
        ___setErrNo(ERRNO_CODES.ENOTCONN);
        return -1;
      } else if (info.socket.readyState === WebSocket.CONNECTING) {
        ___setErrNo(ERRNO_CODES.EAGAIN);
        return -1;
      }
      info.sender(HEAPU8.subarray(buf, buf+len));
      return len;
    }
  function _pwrite(fildes, buf, nbyte, offset) {
      // ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      try {
        var slab = HEAP8;
        return FS.write(stream, slab, buf, nbyte, offset);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }function _write(fildes, buf, nbyte) {
      // ssize_t write(int fildes, const void *buf, size_t nbyte);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      if (stream && ('socket' in stream)) {
        return _send(fildes, buf, nbyte, 0);
      }
      try {
        var slab = HEAP8;
        return FS.write(stream, slab, buf, nbyte);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }function _fwrite(ptr, size, nitems, stream) {
      // size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fwrite.html
      var bytesToWrite = nitems * size;
      if (bytesToWrite == 0) return 0;
      var bytesWritten = _write(stream, ptr, bytesToWrite);
      if (bytesWritten == -1) {
        var streamObj = FS.getStream(stream);
        if (streamObj) streamObj.error = true;
        return 0;
      } else {
        return Math.floor(bytesWritten / size);
      }
    }
  Module["_strlen"] = _strlen;
  function __reallyNegative(x) {
      return x < 0 || (x === 0 && (1/x) === -Infinity);
    }function __formatString(format, varargs) {
      var textIndex = format;
      var argIndex = 0;
      function getNextArg(type) {
        // NOTE: Explicitly ignoring type safety. Otherwise this fails:
        //       int x = 4; printf("%c\n", (char)x);
        var ret;
        if (type === 'double') {
          ret = HEAPF64[(((varargs)+(argIndex))>>3)];
        } else if (type == 'i64') {
          ret = [HEAP32[(((varargs)+(argIndex))>>2)],
                 HEAP32[(((varargs)+(argIndex+8))>>2)]];
          argIndex += 8; // each 32-bit chunk is in a 64-bit block
        } else {
          type = 'i32'; // varargs are always i32, i64, or double
          ret = HEAP32[(((varargs)+(argIndex))>>2)];
        }
        argIndex += Math.max(Runtime.getNativeFieldSize(type), Runtime.getAlignSize(type, null, true));
        return ret;
      }
      var ret = [];
      var curr, next, currArg;
      while(1) {
        var startTextIndex = textIndex;
        curr = HEAP8[(textIndex)];
        if (curr === 0) break;
        next = HEAP8[((textIndex+1)|0)];
        if (curr == 37) {
          // Handle flags.
          var flagAlwaysSigned = false;
          var flagLeftAlign = false;
          var flagAlternative = false;
          var flagZeroPad = false;
          flagsLoop: while (1) {
            switch (next) {
              case 43:
                flagAlwaysSigned = true;
                break;
              case 45:
                flagLeftAlign = true;
                break;
              case 35:
                flagAlternative = true;
                break;
              case 48:
                if (flagZeroPad) {
                  break flagsLoop;
                } else {
                  flagZeroPad = true;
                  break;
                }
              default:
                break flagsLoop;
            }
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
          }
          // Handle width.
          var width = 0;
          if (next == 42) {
            width = getNextArg('i32');
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
          } else {
            while (next >= 48 && next <= 57) {
              width = width * 10 + (next - 48);
              textIndex++;
              next = HEAP8[((textIndex+1)|0)];
            }
          }
          // Handle precision.
          var precisionSet = false;
          if (next == 46) {
            var precision = 0;
            precisionSet = true;
            textIndex++;
            next = HEAP8[((textIndex+1)|0)];
            if (next == 42) {
              precision = getNextArg('i32');
              textIndex++;
            } else {
              while(1) {
                var precisionChr = HEAP8[((textIndex+1)|0)];
                if (precisionChr < 48 ||
                    precisionChr > 57) break;
                precision = precision * 10 + (precisionChr - 48);
                textIndex++;
              }
            }
            next = HEAP8[((textIndex+1)|0)];
          } else {
            var precision = 6; // Standard default.
          }
          // Handle integer sizes. WARNING: These assume a 32-bit architecture!
          var argSize;
          switch (String.fromCharCode(next)) {
            case 'h':
              var nextNext = HEAP8[((textIndex+2)|0)];
              if (nextNext == 104) {
                textIndex++;
                argSize = 1; // char (actually i32 in varargs)
              } else {
                argSize = 2; // short (actually i32 in varargs)
              }
              break;
            case 'l':
              var nextNext = HEAP8[((textIndex+2)|0)];
              if (nextNext == 108) {
                textIndex++;
                argSize = 8; // long long
              } else {
                argSize = 4; // long
              }
              break;
            case 'L': // long long
            case 'q': // int64_t
            case 'j': // intmax_t
              argSize = 8;
              break;
            case 'z': // size_t
            case 't': // ptrdiff_t
            case 'I': // signed ptrdiff_t or unsigned size_t
              argSize = 4;
              break;
            default:
              argSize = null;
          }
          if (argSize) textIndex++;
          next = HEAP8[((textIndex+1)|0)];
          // Handle type specifier.
          switch (String.fromCharCode(next)) {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
              // Integer.
              var signed = next == 100 || next == 105;
              argSize = argSize || 4;
              var currArg = getNextArg('i' + (argSize * 8));
              var argText;
              // Flatten i64-1 [low, high] into a (slightly rounded) double
              if (argSize == 8) {
                currArg = Runtime.makeBigInt(currArg[0], currArg[1], next == 117);
              }
              // Truncate to requested size.
              if (argSize <= 4) {
                var limit = Math.pow(256, argSize) - 1;
                currArg = (signed ? reSign : unSign)(currArg & limit, argSize * 8);
              }
              // Format the number.
              var currAbsArg = Math.abs(currArg);
              var prefix = '';
              if (next == 100 || next == 105) {
                argText = reSign(currArg, 8 * argSize, 1).toString(10);
              } else if (next == 117) {
                argText = unSign(currArg, 8 * argSize, 1).toString(10);
                currArg = Math.abs(currArg);
              } else if (next == 111) {
                argText = (flagAlternative ? '0' : '') + currAbsArg.toString(8);
              } else if (next == 120 || next == 88) {
                prefix = (flagAlternative && currArg != 0) ? '0x' : '';
                if (currArg < 0) {
                  // Represent negative numbers in hex as 2's complement.
                  currArg = -currArg;
                  argText = (currAbsArg - 1).toString(16);
                  var buffer = [];
                  for (var i = 0; i < argText.length; i++) {
                    buffer.push((0xF - parseInt(argText[i], 16)).toString(16));
                  }
                  argText = buffer.join('');
                  while (argText.length < argSize * 2) argText = 'f' + argText;
                } else {
                  argText = currAbsArg.toString(16);
                }
                if (next == 88) {
                  prefix = prefix.toUpperCase();
                  argText = argText.toUpperCase();
                }
              } else if (next == 112) {
                if (currAbsArg === 0) {
                  argText = '(nil)';
                } else {
                  prefix = '0x';
                  argText = currAbsArg.toString(16);
                }
              }
              if (precisionSet) {
                while (argText.length < precision) {
                  argText = '0' + argText;
                }
              }
              // Add sign if needed
              if (flagAlwaysSigned) {
                if (currArg < 0) {
                  prefix = '-' + prefix;
                } else {
                  prefix = '+' + prefix;
                }
              }
              // Add padding.
              while (prefix.length + argText.length < width) {
                if (flagLeftAlign) {
                  argText += ' ';
                } else {
                  if (flagZeroPad) {
                    argText = '0' + argText;
                  } else {
                    prefix = ' ' + prefix;
                  }
                }
              }
              // Insert the result into the buffer.
              argText = prefix + argText;
              argText.split('').forEach(function(chr) {
                ret.push(chr.charCodeAt(0));
              });
              break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
              // Float.
              var currArg = getNextArg('double');
              var argText;
              if (isNaN(currArg)) {
                argText = 'nan';
                flagZeroPad = false;
              } else if (!isFinite(currArg)) {
                argText = (currArg < 0 ? '-' : '') + 'inf';
                flagZeroPad = false;
              } else {
                var isGeneral = false;
                var effectivePrecision = Math.min(precision, 20);
                // Convert g/G to f/F or e/E, as per:
                // http://pubs.opengroup.org/onlinepubs/9699919799/functions/printf.html
                if (next == 103 || next == 71) {
                  isGeneral = true;
                  precision = precision || 1;
                  var exponent = parseInt(currArg.toExponential(effectivePrecision).split('e')[1], 10);
                  if (precision > exponent && exponent >= -4) {
                    next = ((next == 103) ? 'f' : 'F').charCodeAt(0);
                    precision -= exponent + 1;
                  } else {
                    next = ((next == 103) ? 'e' : 'E').charCodeAt(0);
                    precision--;
                  }
                  effectivePrecision = Math.min(precision, 20);
                }
                if (next == 101 || next == 69) {
                  argText = currArg.toExponential(effectivePrecision);
                  // Make sure the exponent has at least 2 digits.
                  if (/[eE][-+]\d$/.test(argText)) {
                    argText = argText.slice(0, -1) + '0' + argText.slice(-1);
                  }
                } else if (next == 102 || next == 70) {
                  argText = currArg.toFixed(effectivePrecision);
                  if (currArg === 0 && __reallyNegative(currArg)) {
                    argText = '-' + argText;
                  }
                }
                var parts = argText.split('e');
                if (isGeneral && !flagAlternative) {
                  // Discard trailing zeros and periods.
                  while (parts[0].length > 1 && parts[0].indexOf('.') != -1 &&
                         (parts[0].slice(-1) == '0' || parts[0].slice(-1) == '.')) {
                    parts[0] = parts[0].slice(0, -1);
                  }
                } else {
                  // Make sure we have a period in alternative mode.
                  if (flagAlternative && argText.indexOf('.') == -1) parts[0] += '.';
                  // Zero pad until required precision.
                  while (precision > effectivePrecision++) parts[0] += '0';
                }
                argText = parts[0] + (parts.length > 1 ? 'e' + parts[1] : '');
                // Capitalize 'E' if needed.
                if (next == 69) argText = argText.toUpperCase();
                // Add sign.
                if (flagAlwaysSigned && currArg >= 0) {
                  argText = '+' + argText;
                }
              }
              // Add padding.
              while (argText.length < width) {
                if (flagLeftAlign) {
                  argText += ' ';
                } else {
                  if (flagZeroPad && (argText[0] == '-' || argText[0] == '+')) {
                    argText = argText[0] + '0' + argText.slice(1);
                  } else {
                    argText = (flagZeroPad ? '0' : ' ') + argText;
                  }
                }
              }
              // Adjust case.
              if (next < 97) argText = argText.toUpperCase();
              // Insert the result into the buffer.
              argText.split('').forEach(function(chr) {
                ret.push(chr.charCodeAt(0));
              });
              break;
            }
            case 's': {
              // String.
              var arg = getNextArg('i8*');
              var argLength = arg ? _strlen(arg) : '(null)'.length;
              if (precisionSet) argLength = Math.min(argLength, precision);
              if (!flagLeftAlign) {
                while (argLength < width--) {
                  ret.push(32);
                }
              }
              if (arg) {
                for (var i = 0; i < argLength; i++) {
                  ret.push(HEAPU8[((arg++)|0)]);
                }
              } else {
                ret = ret.concat(intArrayFromString('(null)'.substr(0, argLength), true));
              }
              if (flagLeftAlign) {
                while (argLength < width--) {
                  ret.push(32);
                }
              }
              break;
            }
            case 'c': {
              // Character.
              if (flagLeftAlign) ret.push(getNextArg('i8'));
              while (--width > 0) {
                ret.push(32);
              }
              if (!flagLeftAlign) ret.push(getNextArg('i8'));
              break;
            }
            case 'n': {
              // Write the length written so far to the next parameter.
              var ptr = getNextArg('i32*');
              HEAP32[((ptr)>>2)]=ret.length
              break;
            }
            case '%': {
              // Literal percent sign.
              ret.push(curr);
              break;
            }
            default: {
              // Unknown specifiers remain untouched.
              for (var i = startTextIndex; i < textIndex + 2; i++) {
                ret.push(HEAP8[(i)]);
              }
            }
          }
          textIndex += 2;
          // TODO: Support a/A (hex float) and m (last error) specifiers.
          // TODO: Support %1${specifier} for arg selection.
        } else {
          ret.push(curr);
          textIndex += 1;
        }
      }
      return ret;
    }function _fprintf(stream, format, varargs) {
      // int fprintf(FILE *restrict stream, const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var result = __formatString(format, varargs);
      var stack = Runtime.stackSave();
      var ret = _fwrite(allocate(result, 'i8', ALLOC_STACK), 1, result.length, stream);
      Runtime.stackRestore(stack);
      return ret;
    }function _printf(format, varargs) {
      // int printf(const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var stdout = HEAP32[((_stdout)>>2)];
      return _fprintf(stdout, format, varargs);
    }
  function _abort() {
      Module['abort']();
    }
  function ___errno_location() {
      return ___errno_state;
    }var ___errno=___errno_location;
  Module["_memcpy"] = _memcpy;var _llvm_memcpy_p0i8_p0i8_i32=_memcpy;
  function _sbrk(bytes) {
      // Implement a Linux-like 'memory area' for our 'process'.
      // Changes the size of the memory area by |bytes|; returns the
      // address of the previous top ('break') of the memory area
      // We control the "dynamic" memory - DYNAMIC_BASE to DYNAMICTOP
      var self = _sbrk;
      if (!self.called) {
        DYNAMICTOP = alignMemoryPage(DYNAMICTOP); // make sure we start out aligned
        self.called = true;
        assert(Runtime.dynamicAlloc);
        self.alloc = Runtime.dynamicAlloc;
        Runtime.dynamicAlloc = function() { abort('cannot dynamically allocate, sbrk now has control') };
      }
      var ret = DYNAMICTOP;
      if (bytes != 0) self.alloc(bytes);
      return ret;  // Previous break location.
    }
  function _sysconf(name) {
      // long sysconf(int name);
      // http://pubs.opengroup.org/onlinepubs/009695399/functions/sysconf.html
      switch(name) {
        case 8: return PAGE_SIZE;
        case 54:
        case 56:
        case 21:
        case 61:
        case 63:
        case 22:
        case 67:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 69:
        case 28:
        case 101:
        case 70:
        case 71:
        case 29:
        case 30:
        case 199:
        case 75:
        case 76:
        case 32:
        case 43:
        case 44:
        case 80:
        case 46:
        case 47:
        case 45:
        case 48:
        case 49:
        case 42:
        case 82:
        case 33:
        case 7:
        case 108:
        case 109:
        case 107:
        case 112:
        case 119:
        case 121:
          return 200809;
        case 13:
        case 104:
        case 94:
        case 95:
        case 34:
        case 35:
        case 77:
        case 81:
        case 83:
        case 84:
        case 85:
        case 86:
        case 87:
        case 88:
        case 89:
        case 90:
        case 91:
        case 94:
        case 95:
        case 110:
        case 111:
        case 113:
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
        case 120:
        case 40:
        case 16:
        case 79:
        case 19:
          return -1;
        case 92:
        case 93:
        case 5:
        case 72:
        case 6:
        case 74:
        case 92:
        case 93:
        case 96:
        case 97:
        case 98:
        case 99:
        case 102:
        case 103:
        case 105:
          return 1;
        case 38:
        case 66:
        case 50:
        case 51:
        case 4:
          return 1024;
        case 15:
        case 64:
        case 41:
          return 32;
        case 55:
        case 37:
        case 17:
          return 2147483647;
        case 18:
        case 1:
          return 47839;
        case 59:
        case 57:
          return 99;
        case 68:
        case 58:
          return 2048;
        case 0: return 2097152;
        case 3: return 65536;
        case 14: return 32768;
        case 73: return 32767;
        case 39: return 16384;
        case 60: return 1000;
        case 106: return 700;
        case 52: return 256;
        case 62: return 255;
        case 2: return 100;
        case 65: return 64;
        case 36: return 20;
        case 100: return 16;
        case 20: return 6;
        case 53: return 4;
        case 10: return 1;
      }
      ___setErrNo(ERRNO_CODES.EINVAL);
      return -1;
    }
  function _time(ptr) {
      var ret = Math.floor(Date.now()/1000);
      if (ptr) {
        HEAP32[((ptr)>>2)]=ret
      }
      return ret;
    }
  Module["_memset"] = _memset;
  var Browser={mainLoop:{scheduler:null,shouldPause:false,paused:false,queue:[],pause:function () {
          Browser.mainLoop.shouldPause = true;
        },resume:function () {
          if (Browser.mainLoop.paused) {
            Browser.mainLoop.paused = false;
            Browser.mainLoop.scheduler();
          }
          Browser.mainLoop.shouldPause = false;
        },updateStatus:function () {
          if (Module['setStatus']) {
            var message = Module['statusMessage'] || 'Please wait...';
            var remaining = Browser.mainLoop.remainingBlockers;
            var expected = Browser.mainLoop.expectedBlockers;
            if (remaining) {
              if (remaining < expected) {
                Module['setStatus'](message + ' (' + (expected - remaining) + '/' + expected + ')');
              } else {
                Module['setStatus'](message);
              }
            } else {
              Module['setStatus']('');
            }
          }
        }},isFullScreen:false,pointerLock:false,moduleContextCreatedCallbacks:[],workers:[],init:function () {
        if (!Module["preloadPlugins"]) Module["preloadPlugins"] = []; // needs to exist even in workers
        if (Browser.initted || ENVIRONMENT_IS_WORKER) return;
        Browser.initted = true;
        try {
          new Blob();
          Browser.hasBlobConstructor = true;
        } catch(e) {
          Browser.hasBlobConstructor = false;
          console.log("warning: no blob constructor, cannot create blobs with mimetypes");
        }
        Browser.BlobBuilder = typeof MozBlobBuilder != "undefined" ? MozBlobBuilder : (typeof WebKitBlobBuilder != "undefined" ? WebKitBlobBuilder : (!Browser.hasBlobConstructor ? console.log("warning: no BlobBuilder") : null));
        Browser.URLObject = typeof window != "undefined" ? (window.URL ? window.URL : window.webkitURL) : undefined;
        if (!Module.noImageDecoding && typeof Browser.URLObject === 'undefined') {
          console.log("warning: Browser does not support creating object URLs. Built-in browser image decoding will not be available.");
          Module.noImageDecoding = true;
        }
        // Support for plugins that can process preloaded files. You can add more of these to
        // your app by creating and appending to Module.preloadPlugins.
        //
        // Each plugin is asked if it can handle a file based on the file's name. If it can,
        // it is given the file's raw data. When it is done, it calls a callback with the file's
        // (possibly modified) data. For example, a plugin might decompress a file, or it
        // might create some side data structure for use later (like an Image element, etc.).
        var imagePlugin = {};
        imagePlugin['canHandle'] = function(name) {
          return !Module.noImageDecoding && /\.(jpg|jpeg|png|bmp)$/i.test(name);
        };
        imagePlugin['handle'] = function(byteArray, name, onload, onerror) {
          var b = null;
          if (Browser.hasBlobConstructor) {
            try {
              b = new Blob([byteArray], { type: Browser.getMimetype(name) });
              if (b.size !== byteArray.length) { // Safari bug #118630
                // Safari's Blob can only take an ArrayBuffer
                b = new Blob([(new Uint8Array(byteArray)).buffer], { type: Browser.getMimetype(name) });
              }
            } catch(e) {
              Runtime.warnOnce('Blob constructor present but fails: ' + e + '; falling back to blob builder');
            }
          }
          if (!b) {
            var bb = new Browser.BlobBuilder();
            bb.append((new Uint8Array(byteArray)).buffer); // we need to pass a buffer, and must copy the array to get the right data range
            b = bb.getBlob();
          }
          var url = Browser.URLObject.createObjectURL(b);
          var img = new Image();
          img.onload = function() {
            assert(img.complete, 'Image ' + name + ' could not be decoded');
            var canvas = document.createElement('canvas');
            canvas.width = img.width;
            canvas.height = img.height;
            var ctx = canvas.getContext('2d');
            ctx.drawImage(img, 0, 0);
            Module["preloadedImages"][name] = canvas;
            Browser.URLObject.revokeObjectURL(url);
            if (onload) onload(byteArray);
          };
          img.onerror = function(event) {
            console.log('Image ' + url + ' could not be decoded');
            if (onerror) onerror();
          };
          img.src = url;
        };
        Module['preloadPlugins'].push(imagePlugin);
        var audioPlugin = {};
        audioPlugin['canHandle'] = function(name) {
          return !Module.noAudioDecoding && name.substr(-4) in { '.ogg': 1, '.wav': 1, '.mp3': 1 };
        };
        audioPlugin['handle'] = function(byteArray, name, onload, onerror) {
          var done = false;
          function finish(audio) {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = audio;
            if (onload) onload(byteArray);
          }
          function fail() {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = new Audio(); // empty shim
            if (onerror) onerror();
          }
          if (Browser.hasBlobConstructor) {
            try {
              var b = new Blob([byteArray], { type: Browser.getMimetype(name) });
            } catch(e) {
              return fail();
            }
            var url = Browser.URLObject.createObjectURL(b); // XXX we never revoke this!
            var audio = new Audio();
            audio.addEventListener('canplaythrough', function() { finish(audio) }, false); // use addEventListener due to chromium bug 124926
            audio.onerror = function(event) {
              if (done) return;
              console.log('warning: browser could not fully decode audio ' + name + ', trying slower base64 approach');
              function encode64(data) {
                var BASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
                var PAD = '=';
                var ret = '';
                var leftchar = 0;
                var leftbits = 0;
                for (var i = 0; i < data.length; i++) {
                  leftchar = (leftchar << 8) | data[i];
                  leftbits += 8;
                  while (leftbits >= 6) {
                    var curr = (leftchar >> (leftbits-6)) & 0x3f;
                    leftbits -= 6;
                    ret += BASE[curr];
                  }
                }
                if (leftbits == 2) {
                  ret += BASE[(leftchar&3) << 4];
                  ret += PAD + PAD;
                } else if (leftbits == 4) {
                  ret += BASE[(leftchar&0xf) << 2];
                  ret += PAD;
                }
                return ret;
              }
              audio.src = 'data:audio/x-' + name.substr(-3) + ';base64,' + encode64(byteArray);
              finish(audio); // we don't wait for confirmation this worked - but it's worth trying
            };
            audio.src = url;
            // workaround for chrome bug 124926 - we do not always get oncanplaythrough or onerror
            Browser.safeSetTimeout(function() {
              finish(audio); // try to use it even though it is not necessarily ready to play
            }, 10000);
          } else {
            return fail();
          }
        };
        Module['preloadPlugins'].push(audioPlugin);
        // Canvas event setup
        var canvas = Module['canvas'];
        canvas.requestPointerLock = canvas['requestPointerLock'] ||
                                    canvas['mozRequestPointerLock'] ||
                                    canvas['webkitRequestPointerLock'];
        canvas.exitPointerLock = document['exitPointerLock'] ||
                                 document['mozExitPointerLock'] ||
                                 document['webkitExitPointerLock'] ||
                                 function(){}; // no-op if function does not exist
        canvas.exitPointerLock = canvas.exitPointerLock.bind(document);
        function pointerLockChange() {
          Browser.pointerLock = document['pointerLockElement'] === canvas ||
                                document['mozPointerLockElement'] === canvas ||
                                document['webkitPointerLockElement'] === canvas;
        }
        document.addEventListener('pointerlockchange', pointerLockChange, false);
        document.addEventListener('mozpointerlockchange', pointerLockChange, false);
        document.addEventListener('webkitpointerlockchange', pointerLockChange, false);
        if (Module['elementPointerLock']) {
          canvas.addEventListener("click", function(ev) {
            if (!Browser.pointerLock && canvas.requestPointerLock) {
              canvas.requestPointerLock();
              ev.preventDefault();
            }
          }, false);
        }
      },createContext:function (canvas, useWebGL, setInModule) {
        var ctx;
        try {
          if (useWebGL) {
            ctx = canvas.getContext('experimental-webgl', {
              alpha: false
            });
          } else {
            ctx = canvas.getContext('2d');
          }
          if (!ctx) throw ':(';
        } catch (e) {
          Module.print('Could not create canvas - ' + e);
          return null;
        }
        if (useWebGL) {
          // Set the background of the WebGL canvas to black
          canvas.style.backgroundColor = "black";
          // Warn on context loss
          canvas.addEventListener('webglcontextlost', function(event) {
            alert('WebGL context lost. You will need to reload the page.');
          }, false);
        }
        if (setInModule) {
          Module.ctx = ctx;
          Module.useWebGL = useWebGL;
          Browser.moduleContextCreatedCallbacks.forEach(function(callback) { callback() });
          Browser.init();
        }
        return ctx;
      },destroyContext:function (canvas, useWebGL, setInModule) {},fullScreenHandlersInstalled:false,lockPointer:undefined,resizeCanvas:undefined,requestFullScreen:function (lockPointer, resizeCanvas) {
        Browser.lockPointer = lockPointer;
        Browser.resizeCanvas = resizeCanvas;
        if (typeof Browser.lockPointer === 'undefined') Browser.lockPointer = true;
        if (typeof Browser.resizeCanvas === 'undefined') Browser.resizeCanvas = false;
        var canvas = Module['canvas'];
        function fullScreenChange() {
          Browser.isFullScreen = false;
          if ((document['webkitFullScreenElement'] || document['webkitFullscreenElement'] ||
               document['mozFullScreenElement'] || document['mozFullscreenElement'] ||
               document['fullScreenElement'] || document['fullscreenElement']) === canvas) {
            canvas.cancelFullScreen = document['cancelFullScreen'] ||
                                      document['mozCancelFullScreen'] ||
                                      document['webkitCancelFullScreen'];
            canvas.cancelFullScreen = canvas.cancelFullScreen.bind(document);
            if (Browser.lockPointer) canvas.requestPointerLock();
            Browser.isFullScreen = true;
            if (Browser.resizeCanvas) Browser.setFullScreenCanvasSize();
          } else if (Browser.resizeCanvas){
            Browser.setWindowedCanvasSize();
          }
          if (Module['onFullScreen']) Module['onFullScreen'](Browser.isFullScreen);
        }
        if (!Browser.fullScreenHandlersInstalled) {
          Browser.fullScreenHandlersInstalled = true;
          document.addEventListener('fullscreenchange', fullScreenChange, false);
          document.addEventListener('mozfullscreenchange', fullScreenChange, false);
          document.addEventListener('webkitfullscreenchange', fullScreenChange, false);
        }
        canvas.requestFullScreen = canvas['requestFullScreen'] ||
                                   canvas['mozRequestFullScreen'] ||
                                   (canvas['webkitRequestFullScreen'] ? function() { canvas['webkitRequestFullScreen'](Element['ALLOW_KEYBOARD_INPUT']) } : null);
        canvas.requestFullScreen();
      },requestAnimationFrame:function (func) {
        if (!window.requestAnimationFrame) {
          window.requestAnimationFrame = window['requestAnimationFrame'] ||
                                         window['mozRequestAnimationFrame'] ||
                                         window['webkitRequestAnimationFrame'] ||
                                         window['msRequestAnimationFrame'] ||
                                         window['oRequestAnimationFrame'] ||
                                         window['setTimeout'];
        }
        window.requestAnimationFrame(func);
      },safeCallback:function (func) {
        return function() {
          if (!ABORT) return func.apply(null, arguments);
        };
      },safeRequestAnimationFrame:function (func) {
        return Browser.requestAnimationFrame(function() {
          if (!ABORT) func();
        });
      },safeSetTimeout:function (func, timeout) {
        return setTimeout(function() {
          if (!ABORT) func();
        }, timeout);
      },safeSetInterval:function (func, timeout) {
        return setInterval(function() {
          if (!ABORT) func();
        }, timeout);
      },getMimetype:function (name) {
        return {
          'jpg': 'image/jpeg',
          'jpeg': 'image/jpeg',
          'png': 'image/png',
          'bmp': 'image/bmp',
          'ogg': 'audio/ogg',
          'wav': 'audio/wav',
          'mp3': 'audio/mpeg'
        }[name.substr(name.lastIndexOf('.')+1)];
      },getUserMedia:function (func) {
        if(!window.getUserMedia) {
          window.getUserMedia = navigator['getUserMedia'] ||
                                navigator['mozGetUserMedia'];
        }
        window.getUserMedia(func);
      },getMovementX:function (event) {
        return event['movementX'] ||
               event['mozMovementX'] ||
               event['webkitMovementX'] ||
               0;
      },getMovementY:function (event) {
        return event['movementY'] ||
               event['mozMovementY'] ||
               event['webkitMovementY'] ||
               0;
      },mouseX:0,mouseY:0,mouseMovementX:0,mouseMovementY:0,calculateMouseEvent:function (event) { // event should be mousemove, mousedown or mouseup
        if (Browser.pointerLock) {
          // When the pointer is locked, calculate the coordinates
          // based on the movement of the mouse.
          // Workaround for Firefox bug 764498
          if (event.type != 'mousemove' &&
              ('mozMovementX' in event)) {
            Browser.mouseMovementX = Browser.mouseMovementY = 0;
          } else {
            Browser.mouseMovementX = Browser.getMovementX(event);
            Browser.mouseMovementY = Browser.getMovementY(event);
          }
          // check if SDL is available
          if (typeof SDL != "undefined") {
          	Browser.mouseX = SDL.mouseX + Browser.mouseMovementX;
          	Browser.mouseY = SDL.mouseY + Browser.mouseMovementY;
          } else {
          	// just add the mouse delta to the current absolut mouse position
          	// FIXME: ideally this should be clamped against the canvas size and zero
          	Browser.mouseX += Browser.mouseMovementX;
          	Browser.mouseY += Browser.mouseMovementY;
          }        
        } else {
          // Otherwise, calculate the movement based on the changes
          // in the coordinates.
          var rect = Module["canvas"].getBoundingClientRect();
          var x = event.pageX - (window.scrollX + rect.left);
          var y = event.pageY - (window.scrollY + rect.top);
          // the canvas might be CSS-scaled compared to its backbuffer;
          // SDL-using content will want mouse coordinates in terms
          // of backbuffer units.
          var cw = Module["canvas"].width;
          var ch = Module["canvas"].height;
          x = x * (cw / rect.width);
          y = y * (ch / rect.height);
          Browser.mouseMovementX = x - Browser.mouseX;
          Browser.mouseMovementY = y - Browser.mouseY;
          Browser.mouseX = x;
          Browser.mouseY = y;
        }
      },xhrLoad:function (url, onload, onerror) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, true);
        xhr.responseType = 'arraybuffer';
        xhr.onload = function() {
          if (xhr.status == 200 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
            onload(xhr.response);
          } else {
            onerror();
          }
        };
        xhr.onerror = onerror;
        xhr.send(null);
      },asyncLoad:function (url, onload, onerror, noRunDep) {
        Browser.xhrLoad(url, function(arrayBuffer) {
          assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
          onload(new Uint8Array(arrayBuffer));
          if (!noRunDep) removeRunDependency('al ' + url);
        }, function(event) {
          if (onerror) {
            onerror();
          } else {
            throw 'Loading data file "' + url + '" failed.';
          }
        });
        if (!noRunDep) addRunDependency('al ' + url);
      },resizeListeners:[],updateResizeListeners:function () {
        var canvas = Module['canvas'];
        Browser.resizeListeners.forEach(function(listener) {
          listener(canvas.width, canvas.height);
        });
      },setCanvasSize:function (width, height, noUpdates) {
        var canvas = Module['canvas'];
        canvas.width = width;
        canvas.height = height;
        if (!noUpdates) Browser.updateResizeListeners();
      },windowedWidth:0,windowedHeight:0,setFullScreenCanvasSize:function () {
        var canvas = Module['canvas'];
        this.windowedWidth = canvas.width;
        this.windowedHeight = canvas.height;
        canvas.width = screen.width;
        canvas.height = screen.height;
        // check if SDL is available   
        if (typeof SDL != "undefined") {
        	var flags = HEAPU32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)];
        	flags = flags | 0x00800000; // set SDL_FULLSCREEN flag
        	HEAP32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)]=flags
        }
        Browser.updateResizeListeners();
      },setWindowedCanvasSize:function () {
        var canvas = Module['canvas'];
        canvas.width = this.windowedWidth;
        canvas.height = this.windowedHeight;
        // check if SDL is available       
        if (typeof SDL != "undefined") {
        	var flags = HEAPU32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)];
        	flags = flags & ~0x00800000; // clear SDL_FULLSCREEN flag
        	HEAP32[((SDL.screen+Runtime.QUANTUM_SIZE*0)>>2)]=flags
        }
        Browser.updateResizeListeners();
      }};
FS.staticInit();__ATINIT__.unshift({ func: function() { if (!Module["noFSInit"] && !FS.init.initialized) FS.init() } });__ATMAIN__.push({ func: function() { FS.ignorePermissions = false } });__ATEXIT__.push({ func: function() { FS.quit() } });Module["FS_createFolder"] = FS.createFolder;Module["FS_createPath"] = FS.createPath;Module["FS_createDataFile"] = FS.createDataFile;Module["FS_createPreloadedFile"] = FS.createPreloadedFile;Module["FS_createLazyFile"] = FS.createLazyFile;Module["FS_createLink"] = FS.createLink;Module["FS_createDevice"] = FS.createDevice;
___errno_state = Runtime.staticAlloc(4); HEAP32[((___errno_state)>>2)]=0;
Module["requestFullScreen"] = function(lockPointer, resizeCanvas) { Browser.requestFullScreen(lockPointer, resizeCanvas) };
  Module["requestAnimationFrame"] = function(func) { Browser.requestAnimationFrame(func) };
  Module["setCanvasSize"] = function(width, height, noUpdates) { Browser.setCanvasSize(width, height, noUpdates) };
  Module["pauseMainLoop"] = function() { Browser.mainLoop.pause() };
  Module["resumeMainLoop"] = function() { Browser.mainLoop.resume() };
  Module["getUserMedia"] = function() { Browser.getUserMedia() }
STACK_BASE = STACKTOP = Runtime.alignMemory(STATICTOP);
staticSealed = true; // seal the static portion of memory
STACK_MAX = STACK_BASE + 5242880;
DYNAMIC_BASE = DYNAMICTOP = Runtime.alignMemory(STACK_MAX);
assert(DYNAMIC_BASE < TOTAL_MEMORY); // Stack must fit in TOTAL_MEMORY; allocations from here on may enlarge TOTAL_MEMORY
var Math_min = Math.min;
function invoke_ii(index,a1) {
  try {
    return Module["dynCall_ii"](index,a1);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_v(index) {
  try {
    Module["dynCall_v"](index);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_iii(index,a1,a2) {
  try {
    return Module["dynCall_iii"](index,a1,a2);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_vi(index,a1) {
  try {
    Module["dynCall_vi"](index,a1);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function asmPrintInt(x, y) {
  Module.print('int ' + x + ',' + y);// + ' ' + new Error().stack);
}
function asmPrintFloat(x, y) {
  Module.print('float ' + x + ',' + y);// + ' ' + new Error().stack);
}
// EMSCRIPTEN_START_ASM
var asm = (function(global, env, buffer) {
  'use asm';
  var HEAP8 = new global.Int8Array(buffer);
  var HEAP16 = new global.Int16Array(buffer);
  var HEAP32 = new global.Int32Array(buffer);
  var HEAPU8 = new global.Uint8Array(buffer);
  var HEAPU16 = new global.Uint16Array(buffer);
  var HEAPU32 = new global.Uint32Array(buffer);
  var HEAPF32 = new global.Float32Array(buffer);
  var HEAPF64 = new global.Float64Array(buffer);
  var STACKTOP=env.STACKTOP|0;
  var STACK_MAX=env.STACK_MAX|0;
  var tempDoublePtr=env.tempDoublePtr|0;
  var ABORT=env.ABORT|0;
  var NaN=+env.NaN;
  var Infinity=+env.Infinity;
  var __THREW__ = 0;
  var threwValue = 0;
  var setjmpId = 0;
  var undef = 0;
  var tempInt = 0, tempBigInt = 0, tempBigIntP = 0, tempBigIntS = 0, tempBigIntR = 0.0, tempBigIntI = 0, tempBigIntD = 0, tempValue = 0, tempDouble = 0.0;
  var tempRet0 = 0;
  var tempRet1 = 0;
  var tempRet2 = 0;
  var tempRet3 = 0;
  var tempRet4 = 0;
  var tempRet5 = 0;
  var tempRet6 = 0;
  var tempRet7 = 0;
  var tempRet8 = 0;
  var tempRet9 = 0;
  var Math_floor=global.Math.floor;
  var Math_abs=global.Math.abs;
  var Math_sqrt=global.Math.sqrt;
  var Math_pow=global.Math.pow;
  var Math_cos=global.Math.cos;
  var Math_sin=global.Math.sin;
  var Math_tan=global.Math.tan;
  var Math_acos=global.Math.acos;
  var Math_asin=global.Math.asin;
  var Math_atan=global.Math.atan;
  var Math_atan2=global.Math.atan2;
  var Math_exp=global.Math.exp;
  var Math_log=global.Math.log;
  var Math_ceil=global.Math.ceil;
  var Math_imul=global.Math.imul;
  var abort=env.abort;
  var assert=env.assert;
  var asmPrintInt=env.asmPrintInt;
  var asmPrintFloat=env.asmPrintFloat;
  var Math_min=env.min;
  var invoke_ii=env.invoke_ii;
  var invoke_v=env.invoke_v;
  var invoke_iii=env.invoke_iii;
  var invoke_vi=env.invoke_vi;
  var _pwrite=env._pwrite;
  var _sysconf=env._sysconf;
  var _sbrk=env._sbrk;
  var ___setErrNo=env.___setErrNo;
  var _fwrite=env._fwrite;
  var __reallyNegative=env.__reallyNegative;
  var _time=env._time;
  var __formatString=env.__formatString;
  var _send=env._send;
  var _write=env._write;
  var _abort=env._abort;
  var _fprintf=env._fprintf;
  var _printf=env._printf;
  var ___errno_location=env.___errno_location;
  var _fflush=env._fflush;
// EMSCRIPTEN_START_FUNCS
function stackAlloc(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + i1 | 0;
 STACKTOP = STACKTOP + 7 >> 3 << 3;
 return i2 | 0;
}
function stackSave() {
 return STACKTOP | 0;
}
function stackRestore(i1) {
 i1 = i1 | 0;
 STACKTOP = i1;
}
function setThrew(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 if ((__THREW__ | 0) == 0) {
  __THREW__ = i1;
  threwValue = i2;
 }
}
function copyTempFloat(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr] = HEAP8[i1];
 HEAP8[tempDoublePtr + 1 | 0] = HEAP8[i1 + 1 | 0];
 HEAP8[tempDoublePtr + 2 | 0] = HEAP8[i1 + 2 | 0];
 HEAP8[tempDoublePtr + 3 | 0] = HEAP8[i1 + 3 | 0];
}
function copyTempDouble(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr] = HEAP8[i1];
 HEAP8[tempDoublePtr + 1 | 0] = HEAP8[i1 + 1 | 0];
 HEAP8[tempDoublePtr + 2 | 0] = HEAP8[i1 + 2 | 0];
 HEAP8[tempDoublePtr + 3 | 0] = HEAP8[i1 + 3 | 0];
 HEAP8[tempDoublePtr + 4 | 0] = HEAP8[i1 + 4 | 0];
 HEAP8[tempDoublePtr + 5 | 0] = HEAP8[i1 + 5 | 0];
 HEAP8[tempDoublePtr + 6 | 0] = HEAP8[i1 + 6 | 0];
 HEAP8[tempDoublePtr + 7 | 0] = HEAP8[i1 + 7 | 0];
}
function setTempRet0(i1) {
 i1 = i1 | 0;
 tempRet0 = i1;
}
function setTempRet1(i1) {
 i1 = i1 | 0;
 tempRet1 = i1;
}
function setTempRet2(i1) {
 i1 = i1 | 0;
 tempRet2 = i1;
}
function setTempRet3(i1) {
 i1 = i1 | 0;
 tempRet3 = i1;
}
function setTempRet4(i1) {
 i1 = i1 | 0;
 tempRet4 = i1;
}
function setTempRet5(i1) {
 i1 = i1 | 0;
 tempRet5 = i1;
}
function setTempRet6(i1) {
 i1 = i1 | 0;
 tempRet6 = i1;
}
function setTempRet7(i1) {
 i1 = i1 | 0;
 tempRet7 = i1;
}
function setTempRet8(i1) {
 i1 = i1 | 0;
 tempRet8 = i1;
}
function setTempRet9(i1) {
 i1 = i1 | 0;
 tempRet9 = i1;
}
function runPostSets() {
}
function _main(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 i3 = STACKTOP;
 do {
  if ((i1 | 0) > 1) {
   i4 = HEAP8[HEAP32[i2 + 4 >> 2] | 0] | 0;
   if ((i4 | 0) == 50) {
    i5 = 400;
    break;
   } else if ((i4 | 0) == 51) {
    i6 = 4;
    break;
   } else if ((i4 | 0) == 52) {
    i5 = 4e3;
    break;
   } else if ((i4 | 0) == 53) {
    i5 = 8e3;
    break;
   } else if ((i4 | 0) == 49) {
    i5 = 55;
    break;
   } else if ((i4 | 0) == 48) {
    i7 = 0;
    STACKTOP = i3;
    return i7 | 0;
   } else {
    _printf(24, (i8 = STACKTOP, STACKTOP = STACKTOP + 8 | 0, HEAP32[i8 >> 2] = i4 - 48, i8) | 0) | 0;
    STACKTOP = i8;
    i7 = -1;
    STACKTOP = i3;
    return i7 | 0;
   }
  } else {
   i6 = 4;
  }
 } while (0);
 if ((i6 | 0) == 4) {
  i5 = 800;
 }
 i6 = _malloc(1048576) | 0;
 i2 = 0;
 i1 = 0;
 do {
  i4 = 0;
  while (1) {
   HEAP8[i6 + i4 | 0] = i4 + i2 & 255;
   i9 = i4 + 1 | 0;
   if ((i9 | 0) < 1048576) {
    i4 = i9;
   } else {
    i10 = i2;
    i11 = 0;
    break;
   }
  }
  do {
   i10 = (HEAP8[i6 + i11 | 0] & 1) + i10 | 0;
   i11 = i11 + 1 | 0;
  } while ((i11 | 0) < 1048576);
  i2 = (i10 | 0) % 1e3 | 0;
  i1 = i1 + 1 | 0;
 } while ((i1 | 0) < (i5 | 0));
 _printf(8, (i8 = STACKTOP, STACKTOP = STACKTOP + 8 | 0, HEAP32[i8 >> 2] = i2, i8) | 0) | 0;
 STACKTOP = i8;
 i7 = 0;
 STACKTOP = i3;
 return i7 | 0;
}
function _malloc(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0, i73 = 0, i74 = 0, i75 = 0, i76 = 0, i77 = 0, i78 = 0, i79 = 0, i80 = 0, i81 = 0, i82 = 0, i83 = 0, i84 = 0, i85 = 0, i86 = 0;
 do {
  if (i1 >>> 0 < 245) {
   if (i1 >>> 0 < 11) {
    i2 = 16;
   } else {
    i2 = i1 + 11 & -8;
   }
   i3 = i2 >>> 3;
   i4 = HEAP32[16] | 0;
   i5 = i4 >>> (i3 >>> 0);
   if ((i5 & 3 | 0) != 0) {
    i6 = (i5 & 1 ^ 1) + i3 | 0;
    i7 = i6 << 1;
    i8 = 104 + (i7 << 2) | 0;
    i9 = 104 + (i7 + 2 << 2) | 0;
    i7 = HEAP32[i9 >> 2] | 0;
    i10 = i7 + 8 | 0;
    i11 = HEAP32[i10 >> 2] | 0;
    do {
     if ((i8 | 0) == (i11 | 0)) {
      HEAP32[16] = i4 & ~(1 << i6);
     } else {
      if (i11 >>> 0 < (HEAP32[20] | 0) >>> 0) {
       _abort();
       return 0;
      }
      i12 = i11 + 12 | 0;
      if ((HEAP32[i12 >> 2] | 0) == (i7 | 0)) {
       HEAP32[i12 >> 2] = i8;
       HEAP32[i9 >> 2] = i11;
       break;
      } else {
       _abort();
       return 0;
      }
     }
    } while (0);
    i11 = i6 << 3;
    HEAP32[i7 + 4 >> 2] = i11 | 3;
    i9 = i7 + (i11 | 4) | 0;
    HEAP32[i9 >> 2] = HEAP32[i9 >> 2] | 1;
    i13 = i10;
    return i13 | 0;
   }
   if (i2 >>> 0 <= (HEAP32[18] | 0) >>> 0) {
    i14 = i2;
    break;
   }
   if ((i5 | 0) != 0) {
    i9 = 2 << i3;
    i11 = i5 << i3 & (i9 | -i9);
    i9 = (i11 & -i11) - 1 | 0;
    i11 = i9 >>> 12 & 16;
    i8 = i9 >>> (i11 >>> 0);
    i9 = i8 >>> 5 & 8;
    i12 = i8 >>> (i9 >>> 0);
    i8 = i12 >>> 2 & 4;
    i15 = i12 >>> (i8 >>> 0);
    i12 = i15 >>> 1 & 2;
    i16 = i15 >>> (i12 >>> 0);
    i15 = i16 >>> 1 & 1;
    i17 = (i9 | i11 | i8 | i12 | i15) + (i16 >>> (i15 >>> 0)) | 0;
    i15 = i17 << 1;
    i16 = 104 + (i15 << 2) | 0;
    i12 = 104 + (i15 + 2 << 2) | 0;
    i15 = HEAP32[i12 >> 2] | 0;
    i8 = i15 + 8 | 0;
    i11 = HEAP32[i8 >> 2] | 0;
    do {
     if ((i16 | 0) == (i11 | 0)) {
      HEAP32[16] = i4 & ~(1 << i17);
     } else {
      if (i11 >>> 0 < (HEAP32[20] | 0) >>> 0) {
       _abort();
       return 0;
      }
      i9 = i11 + 12 | 0;
      if ((HEAP32[i9 >> 2] | 0) == (i15 | 0)) {
       HEAP32[i9 >> 2] = i16;
       HEAP32[i12 >> 2] = i11;
       break;
      } else {
       _abort();
       return 0;
      }
     }
    } while (0);
    i11 = i17 << 3;
    i12 = i11 - i2 | 0;
    HEAP32[i15 + 4 >> 2] = i2 | 3;
    i16 = i15;
    i4 = i16 + i2 | 0;
    HEAP32[i16 + (i2 | 4) >> 2] = i12 | 1;
    HEAP32[i16 + i11 >> 2] = i12;
    i11 = HEAP32[18] | 0;
    if ((i11 | 0) != 0) {
     i16 = HEAP32[21] | 0;
     i3 = i11 >>> 3;
     i11 = i3 << 1;
     i5 = 104 + (i11 << 2) | 0;
     i10 = HEAP32[16] | 0;
     i7 = 1 << i3;
     do {
      if ((i10 & i7 | 0) == 0) {
       HEAP32[16] = i10 | i7;
       i18 = i5;
       i19 = 104 + (i11 + 2 << 2) | 0;
      } else {
       i3 = 104 + (i11 + 2 << 2) | 0;
       i6 = HEAP32[i3 >> 2] | 0;
       if (i6 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
        i18 = i6;
        i19 = i3;
        break;
       }
       _abort();
       return 0;
      }
     } while (0);
     HEAP32[i19 >> 2] = i16;
     HEAP32[i18 + 12 >> 2] = i16;
     HEAP32[i16 + 8 >> 2] = i18;
     HEAP32[i16 + 12 >> 2] = i5;
    }
    HEAP32[18] = i12;
    HEAP32[21] = i4;
    i13 = i8;
    return i13 | 0;
   }
   i11 = HEAP32[17] | 0;
   if ((i11 | 0) == 0) {
    i14 = i2;
    break;
   }
   i7 = (i11 & -i11) - 1 | 0;
   i11 = i7 >>> 12 & 16;
   i10 = i7 >>> (i11 >>> 0);
   i7 = i10 >>> 5 & 8;
   i15 = i10 >>> (i7 >>> 0);
   i10 = i15 >>> 2 & 4;
   i17 = i15 >>> (i10 >>> 0);
   i15 = i17 >>> 1 & 2;
   i3 = i17 >>> (i15 >>> 0);
   i17 = i3 >>> 1 & 1;
   i6 = HEAP32[368 + ((i7 | i11 | i10 | i15 | i17) + (i3 >>> (i17 >>> 0)) << 2) >> 2] | 0;
   i17 = i6;
   i3 = i6;
   i15 = (HEAP32[i6 + 4 >> 2] & -8) - i2 | 0;
   while (1) {
    i6 = HEAP32[i17 + 16 >> 2] | 0;
    if ((i6 | 0) == 0) {
     i10 = HEAP32[i17 + 20 >> 2] | 0;
     if ((i10 | 0) == 0) {
      break;
     } else {
      i20 = i10;
     }
    } else {
     i20 = i6;
    }
    i6 = (HEAP32[i20 + 4 >> 2] & -8) - i2 | 0;
    i10 = i6 >>> 0 < i15 >>> 0;
    i17 = i20;
    i3 = i10 ? i20 : i3;
    i15 = i10 ? i6 : i15;
   }
   i17 = i3;
   i8 = HEAP32[20] | 0;
   if (i17 >>> 0 < i8 >>> 0) {
    _abort();
    return 0;
   }
   i4 = i17 + i2 | 0;
   i12 = i4;
   if (i17 >>> 0 >= i4 >>> 0) {
    _abort();
    return 0;
   }
   i4 = HEAP32[i3 + 24 >> 2] | 0;
   i5 = HEAP32[i3 + 12 >> 2] | 0;
   do {
    if ((i5 | 0) == (i3 | 0)) {
     i16 = i3 + 20 | 0;
     i6 = HEAP32[i16 >> 2] | 0;
     if ((i6 | 0) == 0) {
      i10 = i3 + 16 | 0;
      i11 = HEAP32[i10 >> 2] | 0;
      if ((i11 | 0) == 0) {
       i21 = 0;
       break;
      } else {
       i22 = i11;
       i23 = i10;
      }
     } else {
      i22 = i6;
      i23 = i16;
     }
     while (1) {
      i16 = i22 + 20 | 0;
      i6 = HEAP32[i16 >> 2] | 0;
      if ((i6 | 0) != 0) {
       i22 = i6;
       i23 = i16;
       continue;
      }
      i16 = i22 + 16 | 0;
      i6 = HEAP32[i16 >> 2] | 0;
      if ((i6 | 0) == 0) {
       break;
      } else {
       i22 = i6;
       i23 = i16;
      }
     }
     if (i23 >>> 0 < i8 >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i23 >> 2] = 0;
      i21 = i22;
      break;
     }
    } else {
     i16 = HEAP32[i3 + 8 >> 2] | 0;
     if (i16 >>> 0 < i8 >>> 0) {
      _abort();
      return 0;
     }
     i6 = i16 + 12 | 0;
     if ((HEAP32[i6 >> 2] | 0) != (i3 | 0)) {
      _abort();
      return 0;
     }
     i10 = i5 + 8 | 0;
     if ((HEAP32[i10 >> 2] | 0) == (i3 | 0)) {
      HEAP32[i6 >> 2] = i5;
      HEAP32[i10 >> 2] = i16;
      i21 = i5;
      break;
     } else {
      _abort();
      return 0;
     }
    }
   } while (0);
   L100 : do {
    if ((i4 | 0) != 0) {
     i5 = i3 + 28 | 0;
     i8 = 368 + (HEAP32[i5 >> 2] << 2) | 0;
     do {
      if ((i3 | 0) == (HEAP32[i8 >> 2] | 0)) {
       HEAP32[i8 >> 2] = i21;
       if ((i21 | 0) != 0) {
        break;
       }
       HEAP32[17] = HEAP32[17] & ~(1 << HEAP32[i5 >> 2]);
       break L100;
      } else {
       if (i4 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
        return 0;
       }
       i16 = i4 + 16 | 0;
       if ((HEAP32[i16 >> 2] | 0) == (i3 | 0)) {
        HEAP32[i16 >> 2] = i21;
       } else {
        HEAP32[i4 + 20 >> 2] = i21;
       }
       if ((i21 | 0) == 0) {
        break L100;
       }
      }
     } while (0);
     if (i21 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
      return 0;
     }
     HEAP32[i21 + 24 >> 2] = i4;
     i5 = HEAP32[i3 + 16 >> 2] | 0;
     do {
      if ((i5 | 0) != 0) {
       if (i5 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
        return 0;
       } else {
        HEAP32[i21 + 16 >> 2] = i5;
        HEAP32[i5 + 24 >> 2] = i21;
        break;
       }
      }
     } while (0);
     i5 = HEAP32[i3 + 20 >> 2] | 0;
     if ((i5 | 0) == 0) {
      break;
     }
     if (i5 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i21 + 20 >> 2] = i5;
      HEAP32[i5 + 24 >> 2] = i21;
      break;
     }
    }
   } while (0);
   if (i15 >>> 0 < 16) {
    i4 = i15 + i2 | 0;
    HEAP32[i3 + 4 >> 2] = i4 | 3;
    i5 = i17 + (i4 + 4) | 0;
    HEAP32[i5 >> 2] = HEAP32[i5 >> 2] | 1;
   } else {
    HEAP32[i3 + 4 >> 2] = i2 | 3;
    HEAP32[i17 + (i2 | 4) >> 2] = i15 | 1;
    HEAP32[i17 + (i15 + i2) >> 2] = i15;
    i5 = HEAP32[18] | 0;
    if ((i5 | 0) != 0) {
     i4 = HEAP32[21] | 0;
     i8 = i5 >>> 3;
     i5 = i8 << 1;
     i16 = 104 + (i5 << 2) | 0;
     i10 = HEAP32[16] | 0;
     i6 = 1 << i8;
     do {
      if ((i10 & i6 | 0) == 0) {
       HEAP32[16] = i10 | i6;
       i24 = i16;
       i25 = 104 + (i5 + 2 << 2) | 0;
      } else {
       i8 = 104 + (i5 + 2 << 2) | 0;
       i11 = HEAP32[i8 >> 2] | 0;
       if (i11 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
        i24 = i11;
        i25 = i8;
        break;
       }
       _abort();
       return 0;
      }
     } while (0);
     HEAP32[i25 >> 2] = i4;
     HEAP32[i24 + 12 >> 2] = i4;
     HEAP32[i4 + 8 >> 2] = i24;
     HEAP32[i4 + 12 >> 2] = i16;
    }
    HEAP32[18] = i15;
    HEAP32[21] = i12;
   }
   i5 = i3 + 8 | 0;
   if ((i5 | 0) == 0) {
    i14 = i2;
    break;
   } else {
    i13 = i5;
   }
   return i13 | 0;
  } else {
   if (i1 >>> 0 > 4294967231) {
    i14 = -1;
    break;
   }
   i5 = i1 + 11 | 0;
   i6 = i5 & -8;
   i10 = HEAP32[17] | 0;
   if ((i10 | 0) == 0) {
    i14 = i6;
    break;
   }
   i17 = -i6 | 0;
   i8 = i5 >>> 8;
   do {
    if ((i8 | 0) == 0) {
     i26 = 0;
    } else {
     if (i6 >>> 0 > 16777215) {
      i26 = 31;
      break;
     }
     i5 = (i8 + 1048320 | 0) >>> 16 & 8;
     i11 = i8 << i5;
     i7 = (i11 + 520192 | 0) >>> 16 & 4;
     i9 = i11 << i7;
     i11 = (i9 + 245760 | 0) >>> 16 & 2;
     i27 = 14 - (i7 | i5 | i11) + (i9 << i11 >>> 15) | 0;
     i26 = i6 >>> ((i27 + 7 | 0) >>> 0) & 1 | i27 << 1;
    }
   } while (0);
   i8 = HEAP32[368 + (i26 << 2) >> 2] | 0;
   L148 : do {
    if ((i8 | 0) == 0) {
     i28 = 0;
     i29 = i17;
     i30 = 0;
    } else {
     if ((i26 | 0) == 31) {
      i31 = 0;
     } else {
      i31 = 25 - (i26 >>> 1) | 0;
     }
     i3 = 0;
     i12 = i17;
     i15 = i8;
     i16 = i6 << i31;
     i4 = 0;
     while (1) {
      i27 = HEAP32[i15 + 4 >> 2] & -8;
      i11 = i27 - i6 | 0;
      if (i11 >>> 0 < i12 >>> 0) {
       if ((i27 | 0) == (i6 | 0)) {
        i28 = i15;
        i29 = i11;
        i30 = i15;
        break L148;
       } else {
        i32 = i15;
        i33 = i11;
       }
      } else {
       i32 = i3;
       i33 = i12;
      }
      i11 = HEAP32[i15 + 20 >> 2] | 0;
      i27 = HEAP32[i15 + 16 + (i16 >>> 31 << 2) >> 2] | 0;
      i9 = (i11 | 0) == 0 | (i11 | 0) == (i27 | 0) ? i4 : i11;
      if ((i27 | 0) == 0) {
       i28 = i32;
       i29 = i33;
       i30 = i9;
       break;
      } else {
       i3 = i32;
       i12 = i33;
       i15 = i27;
       i16 = i16 << 1;
       i4 = i9;
      }
     }
    }
   } while (0);
   if ((i30 | 0) == 0 & (i28 | 0) == 0) {
    i8 = 2 << i26;
    i17 = (i8 | -i8) & i10;
    if ((i17 | 0) == 0) {
     i14 = i6;
     break;
    }
    i8 = (i17 & -i17) - 1 | 0;
    i17 = i8 >>> 12 & 16;
    i4 = i8 >>> (i17 >>> 0);
    i8 = i4 >>> 5 & 8;
    i16 = i4 >>> (i8 >>> 0);
    i4 = i16 >>> 2 & 4;
    i15 = i16 >>> (i4 >>> 0);
    i16 = i15 >>> 1 & 2;
    i12 = i15 >>> (i16 >>> 0);
    i15 = i12 >>> 1 & 1;
    i34 = HEAP32[368 + ((i8 | i17 | i4 | i16 | i15) + (i12 >>> (i15 >>> 0)) << 2) >> 2] | 0;
   } else {
    i34 = i30;
   }
   if ((i34 | 0) == 0) {
    i35 = i29;
    i36 = i28;
   } else {
    i15 = i34;
    i12 = i29;
    i16 = i28;
    while (1) {
     i4 = (HEAP32[i15 + 4 >> 2] & -8) - i6 | 0;
     i17 = i4 >>> 0 < i12 >>> 0;
     i8 = i17 ? i4 : i12;
     i4 = i17 ? i15 : i16;
     i17 = HEAP32[i15 + 16 >> 2] | 0;
     if ((i17 | 0) != 0) {
      i15 = i17;
      i12 = i8;
      i16 = i4;
      continue;
     }
     i17 = HEAP32[i15 + 20 >> 2] | 0;
     if ((i17 | 0) == 0) {
      i35 = i8;
      i36 = i4;
      break;
     } else {
      i15 = i17;
      i12 = i8;
      i16 = i4;
     }
    }
   }
   if ((i36 | 0) == 0) {
    i14 = i6;
    break;
   }
   if (i35 >>> 0 >= ((HEAP32[18] | 0) - i6 | 0) >>> 0) {
    i14 = i6;
    break;
   }
   i16 = i36;
   i12 = HEAP32[20] | 0;
   if (i16 >>> 0 < i12 >>> 0) {
    _abort();
    return 0;
   }
   i15 = i16 + i6 | 0;
   i10 = i15;
   if (i16 >>> 0 >= i15 >>> 0) {
    _abort();
    return 0;
   }
   i4 = HEAP32[i36 + 24 >> 2] | 0;
   i8 = HEAP32[i36 + 12 >> 2] | 0;
   do {
    if ((i8 | 0) == (i36 | 0)) {
     i17 = i36 + 20 | 0;
     i3 = HEAP32[i17 >> 2] | 0;
     if ((i3 | 0) == 0) {
      i9 = i36 + 16 | 0;
      i27 = HEAP32[i9 >> 2] | 0;
      if ((i27 | 0) == 0) {
       i37 = 0;
       break;
      } else {
       i38 = i27;
       i39 = i9;
      }
     } else {
      i38 = i3;
      i39 = i17;
     }
     while (1) {
      i17 = i38 + 20 | 0;
      i3 = HEAP32[i17 >> 2] | 0;
      if ((i3 | 0) != 0) {
       i38 = i3;
       i39 = i17;
       continue;
      }
      i17 = i38 + 16 | 0;
      i3 = HEAP32[i17 >> 2] | 0;
      if ((i3 | 0) == 0) {
       break;
      } else {
       i38 = i3;
       i39 = i17;
      }
     }
     if (i39 >>> 0 < i12 >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i39 >> 2] = 0;
      i37 = i38;
      break;
     }
    } else {
     i17 = HEAP32[i36 + 8 >> 2] | 0;
     if (i17 >>> 0 < i12 >>> 0) {
      _abort();
      return 0;
     }
     i3 = i17 + 12 | 0;
     if ((HEAP32[i3 >> 2] | 0) != (i36 | 0)) {
      _abort();
      return 0;
     }
     i9 = i8 + 8 | 0;
     if ((HEAP32[i9 >> 2] | 0) == (i36 | 0)) {
      HEAP32[i3 >> 2] = i8;
      HEAP32[i9 >> 2] = i17;
      i37 = i8;
      break;
     } else {
      _abort();
      return 0;
     }
    }
   } while (0);
   L198 : do {
    if ((i4 | 0) != 0) {
     i8 = i36 + 28 | 0;
     i12 = 368 + (HEAP32[i8 >> 2] << 2) | 0;
     do {
      if ((i36 | 0) == (HEAP32[i12 >> 2] | 0)) {
       HEAP32[i12 >> 2] = i37;
       if ((i37 | 0) != 0) {
        break;
       }
       HEAP32[17] = HEAP32[17] & ~(1 << HEAP32[i8 >> 2]);
       break L198;
      } else {
       if (i4 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
        return 0;
       }
       i17 = i4 + 16 | 0;
       if ((HEAP32[i17 >> 2] | 0) == (i36 | 0)) {
        HEAP32[i17 >> 2] = i37;
       } else {
        HEAP32[i4 + 20 >> 2] = i37;
       }
       if ((i37 | 0) == 0) {
        break L198;
       }
      }
     } while (0);
     if (i37 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
      return 0;
     }
     HEAP32[i37 + 24 >> 2] = i4;
     i8 = HEAP32[i36 + 16 >> 2] | 0;
     do {
      if ((i8 | 0) != 0) {
       if (i8 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
        return 0;
       } else {
        HEAP32[i37 + 16 >> 2] = i8;
        HEAP32[i8 + 24 >> 2] = i37;
        break;
       }
      }
     } while (0);
     i8 = HEAP32[i36 + 20 >> 2] | 0;
     if ((i8 | 0) == 0) {
      break;
     }
     if (i8 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i37 + 20 >> 2] = i8;
      HEAP32[i8 + 24 >> 2] = i37;
      break;
     }
    }
   } while (0);
   do {
    if (i35 >>> 0 < 16) {
     i4 = i35 + i6 | 0;
     HEAP32[i36 + 4 >> 2] = i4 | 3;
     i8 = i16 + (i4 + 4) | 0;
     HEAP32[i8 >> 2] = HEAP32[i8 >> 2] | 1;
    } else {
     HEAP32[i36 + 4 >> 2] = i6 | 3;
     HEAP32[i16 + (i6 | 4) >> 2] = i35 | 1;
     HEAP32[i16 + (i35 + i6) >> 2] = i35;
     i8 = i35 >>> 3;
     if (i35 >>> 0 < 256) {
      i4 = i8 << 1;
      i12 = 104 + (i4 << 2) | 0;
      i17 = HEAP32[16] | 0;
      i9 = 1 << i8;
      do {
       if ((i17 & i9 | 0) == 0) {
        HEAP32[16] = i17 | i9;
        i40 = i12;
        i41 = 104 + (i4 + 2 << 2) | 0;
       } else {
        i8 = 104 + (i4 + 2 << 2) | 0;
        i3 = HEAP32[i8 >> 2] | 0;
        if (i3 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
         i40 = i3;
         i41 = i8;
         break;
        }
        _abort();
        return 0;
       }
      } while (0);
      HEAP32[i41 >> 2] = i10;
      HEAP32[i40 + 12 >> 2] = i10;
      HEAP32[i16 + (i6 + 8) >> 2] = i40;
      HEAP32[i16 + (i6 + 12) >> 2] = i12;
      break;
     }
     i4 = i15;
     i9 = i35 >>> 8;
     do {
      if ((i9 | 0) == 0) {
       i42 = 0;
      } else {
       if (i35 >>> 0 > 16777215) {
        i42 = 31;
        break;
       }
       i17 = (i9 + 1048320 | 0) >>> 16 & 8;
       i8 = i9 << i17;
       i3 = (i8 + 520192 | 0) >>> 16 & 4;
       i27 = i8 << i3;
       i8 = (i27 + 245760 | 0) >>> 16 & 2;
       i11 = 14 - (i3 | i17 | i8) + (i27 << i8 >>> 15) | 0;
       i42 = i35 >>> ((i11 + 7 | 0) >>> 0) & 1 | i11 << 1;
      }
     } while (0);
     i9 = 368 + (i42 << 2) | 0;
     HEAP32[i16 + (i6 + 28) >> 2] = i42;
     HEAP32[i16 + (i6 + 20) >> 2] = 0;
     HEAP32[i16 + (i6 + 16) >> 2] = 0;
     i12 = HEAP32[17] | 0;
     i11 = 1 << i42;
     if ((i12 & i11 | 0) == 0) {
      HEAP32[17] = i12 | i11;
      HEAP32[i9 >> 2] = i4;
      HEAP32[i16 + (i6 + 24) >> 2] = i9;
      HEAP32[i16 + (i6 + 12) >> 2] = i4;
      HEAP32[i16 + (i6 + 8) >> 2] = i4;
      break;
     }
     if ((i42 | 0) == 31) {
      i43 = 0;
     } else {
      i43 = 25 - (i42 >>> 1) | 0;
     }
     i11 = i35 << i43;
     i12 = HEAP32[i9 >> 2] | 0;
     while (1) {
      if ((HEAP32[i12 + 4 >> 2] & -8 | 0) == (i35 | 0)) {
       break;
      }
      i44 = i12 + 16 + (i11 >>> 31 << 2) | 0;
      i9 = HEAP32[i44 >> 2] | 0;
      if ((i9 | 0) == 0) {
       i45 = 168;
       break;
      } else {
       i11 = i11 << 1;
       i12 = i9;
      }
     }
     if ((i45 | 0) == 168) {
      if (i44 >>> 0 < (HEAP32[20] | 0) >>> 0) {
       _abort();
       return 0;
      } else {
       HEAP32[i44 >> 2] = i4;
       HEAP32[i16 + (i6 + 24) >> 2] = i12;
       HEAP32[i16 + (i6 + 12) >> 2] = i4;
       HEAP32[i16 + (i6 + 8) >> 2] = i4;
       break;
      }
     }
     i11 = i12 + 8 | 0;
     i9 = HEAP32[i11 >> 2] | 0;
     i8 = HEAP32[20] | 0;
     if (i12 >>> 0 < i8 >>> 0) {
      _abort();
      return 0;
     }
     if (i9 >>> 0 < i8 >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i9 + 12 >> 2] = i4;
      HEAP32[i11 >> 2] = i4;
      HEAP32[i16 + (i6 + 8) >> 2] = i9;
      HEAP32[i16 + (i6 + 12) >> 2] = i12;
      HEAP32[i16 + (i6 + 24) >> 2] = 0;
      break;
     }
    }
   } while (0);
   i16 = i36 + 8 | 0;
   if ((i16 | 0) == 0) {
    i14 = i6;
    break;
   } else {
    i13 = i16;
   }
   return i13 | 0;
  }
 } while (0);
 i36 = HEAP32[18] | 0;
 if (i14 >>> 0 <= i36 >>> 0) {
  i44 = i36 - i14 | 0;
  i35 = HEAP32[21] | 0;
  if (i44 >>> 0 > 15) {
   i43 = i35;
   HEAP32[21] = i43 + i14;
   HEAP32[18] = i44;
   HEAP32[i43 + (i14 + 4) >> 2] = i44 | 1;
   HEAP32[i43 + i36 >> 2] = i44;
   HEAP32[i35 + 4 >> 2] = i14 | 3;
  } else {
   HEAP32[18] = 0;
   HEAP32[21] = 0;
   HEAP32[i35 + 4 >> 2] = i36 | 3;
   i44 = i35 + (i36 + 4) | 0;
   HEAP32[i44 >> 2] = HEAP32[i44 >> 2] | 1;
  }
  i13 = i35 + 8 | 0;
  return i13 | 0;
 }
 i35 = HEAP32[19] | 0;
 if (i14 >>> 0 < i35 >>> 0) {
  i44 = i35 - i14 | 0;
  HEAP32[19] = i44;
  i35 = HEAP32[22] | 0;
  i36 = i35;
  HEAP32[22] = i36 + i14;
  HEAP32[i36 + (i14 + 4) >> 2] = i44 | 1;
  HEAP32[i35 + 4 >> 2] = i14 | 3;
  i13 = i35 + 8 | 0;
  return i13 | 0;
 }
 do {
  if ((HEAP32[10] | 0) == 0) {
   i35 = _sysconf(8) | 0;
   if ((i35 - 1 & i35 | 0) == 0) {
    HEAP32[12] = i35;
    HEAP32[11] = i35;
    HEAP32[13] = -1;
    HEAP32[14] = -1;
    HEAP32[15] = 0;
    HEAP32[127] = 0;
    HEAP32[10] = (_time(0) | 0) & -16 ^ 1431655768;
    break;
   } else {
    _abort();
    return 0;
   }
  }
 } while (0);
 i35 = i14 + 48 | 0;
 i44 = HEAP32[12] | 0;
 i36 = i14 + 47 | 0;
 i43 = i44 + i36 | 0;
 i42 = -i44 | 0;
 i44 = i43 & i42;
 if (i44 >>> 0 <= i14 >>> 0) {
  i13 = 0;
  return i13 | 0;
 }
 i40 = HEAP32[126] | 0;
 do {
  if ((i40 | 0) != 0) {
   i41 = HEAP32[124] | 0;
   i37 = i41 + i44 | 0;
   if (i37 >>> 0 <= i41 >>> 0 | i37 >>> 0 > i40 >>> 0) {
    i13 = 0;
   } else {
    break;
   }
   return i13 | 0;
  }
 } while (0);
 L290 : do {
  if ((HEAP32[127] & 4 | 0) == 0) {
   i40 = HEAP32[22] | 0;
   L292 : do {
    if ((i40 | 0) == 0) {
     i45 = 198;
    } else {
     i37 = i40;
     i41 = 512;
     while (1) {
      i46 = i41 | 0;
      i38 = HEAP32[i46 >> 2] | 0;
      if (i38 >>> 0 <= i37 >>> 0) {
       i47 = i41 + 4 | 0;
       if ((i38 + (HEAP32[i47 >> 2] | 0) | 0) >>> 0 > i37 >>> 0) {
        break;
       }
      }
      i38 = HEAP32[i41 + 8 >> 2] | 0;
      if ((i38 | 0) == 0) {
       i45 = 198;
       break L292;
      } else {
       i41 = i38;
      }
     }
     if ((i41 | 0) == 0) {
      i45 = 198;
      break;
     }
     i37 = i43 - (HEAP32[19] | 0) & i42;
     if (i37 >>> 0 >= 2147483647) {
      i48 = 0;
      break;
     }
     i12 = _sbrk(i37 | 0) | 0;
     i4 = (i12 | 0) == ((HEAP32[i46 >> 2] | 0) + (HEAP32[i47 >> 2] | 0) | 0);
     i49 = i4 ? i12 : -1;
     i50 = i4 ? i37 : 0;
     i51 = i12;
     i52 = i37;
     i45 = 207;
    }
   } while (0);
   do {
    if ((i45 | 0) == 198) {
     i40 = _sbrk(0) | 0;
     if ((i40 | 0) == -1) {
      i48 = 0;
      break;
     }
     i6 = i40;
     i37 = HEAP32[11] | 0;
     i12 = i37 - 1 | 0;
     if ((i12 & i6 | 0) == 0) {
      i53 = i44;
     } else {
      i53 = i44 - i6 + (i12 + i6 & -i37) | 0;
     }
     i37 = HEAP32[124] | 0;
     i6 = i37 + i53 | 0;
     if (!(i53 >>> 0 > i14 >>> 0 & i53 >>> 0 < 2147483647)) {
      i48 = 0;
      break;
     }
     i12 = HEAP32[126] | 0;
     if ((i12 | 0) != 0) {
      if (i6 >>> 0 <= i37 >>> 0 | i6 >>> 0 > i12 >>> 0) {
       i48 = 0;
       break;
      }
     }
     i12 = _sbrk(i53 | 0) | 0;
     i6 = (i12 | 0) == (i40 | 0);
     i49 = i6 ? i40 : -1;
     i50 = i6 ? i53 : 0;
     i51 = i12;
     i52 = i53;
     i45 = 207;
    }
   } while (0);
   L312 : do {
    if ((i45 | 0) == 207) {
     i12 = -i52 | 0;
     if ((i49 | 0) != -1) {
      i54 = i50;
      i55 = i49;
      i45 = 218;
      break L290;
     }
     do {
      if ((i51 | 0) != -1 & i52 >>> 0 < 2147483647 & i52 >>> 0 < i35 >>> 0) {
       i6 = HEAP32[12] | 0;
       i40 = i36 - i52 + i6 & -i6;
       if (i40 >>> 0 >= 2147483647) {
        i56 = i52;
        break;
       }
       if ((_sbrk(i40 | 0) | 0) == -1) {
        _sbrk(i12 | 0) | 0;
        i48 = i50;
        break L312;
       } else {
        i56 = i40 + i52 | 0;
        break;
       }
      } else {
       i56 = i52;
      }
     } while (0);
     if ((i51 | 0) == -1) {
      i48 = i50;
     } else {
      i54 = i56;
      i55 = i51;
      i45 = 218;
      break L290;
     }
    }
   } while (0);
   HEAP32[127] = HEAP32[127] | 4;
   i57 = i48;
   i45 = 215;
  } else {
   i57 = 0;
   i45 = 215;
  }
 } while (0);
 do {
  if ((i45 | 0) == 215) {
   if (i44 >>> 0 >= 2147483647) {
    break;
   }
   i48 = _sbrk(i44 | 0) | 0;
   i51 = _sbrk(0) | 0;
   if (!((i51 | 0) != -1 & (i48 | 0) != -1 & i48 >>> 0 < i51 >>> 0)) {
    break;
   }
   i56 = i51 - i48 | 0;
   i51 = i56 >>> 0 > (i14 + 40 | 0) >>> 0;
   i50 = i51 ? i48 : -1;
   if ((i50 | 0) != -1) {
    i54 = i51 ? i56 : i57;
    i55 = i50;
    i45 = 218;
   }
  }
 } while (0);
 do {
  if ((i45 | 0) == 218) {
   i57 = (HEAP32[124] | 0) + i54 | 0;
   HEAP32[124] = i57;
   if (i57 >>> 0 > (HEAP32[125] | 0) >>> 0) {
    HEAP32[125] = i57;
   }
   i57 = HEAP32[22] | 0;
   L332 : do {
    if ((i57 | 0) == 0) {
     i44 = HEAP32[20] | 0;
     if ((i44 | 0) == 0 | i55 >>> 0 < i44 >>> 0) {
      HEAP32[20] = i55;
     }
     HEAP32[128] = i55;
     HEAP32[129] = i54;
     HEAP32[131] = 0;
     HEAP32[25] = HEAP32[10];
     HEAP32[24] = -1;
     i44 = 0;
     do {
      i50 = i44 << 1;
      i56 = 104 + (i50 << 2) | 0;
      HEAP32[104 + (i50 + 3 << 2) >> 2] = i56;
      HEAP32[104 + (i50 + 2 << 2) >> 2] = i56;
      i44 = i44 + 1 | 0;
     } while (i44 >>> 0 < 32);
     i44 = i55 + 8 | 0;
     if ((i44 & 7 | 0) == 0) {
      i58 = 0;
     } else {
      i58 = -i44 & 7;
     }
     i44 = i54 - 40 - i58 | 0;
     HEAP32[22] = i55 + i58;
     HEAP32[19] = i44;
     HEAP32[i55 + (i58 + 4) >> 2] = i44 | 1;
     HEAP32[i55 + (i54 - 36) >> 2] = 40;
     HEAP32[23] = HEAP32[14];
    } else {
     i44 = 512;
     while (1) {
      i59 = HEAP32[i44 >> 2] | 0;
      i60 = i44 + 4 | 0;
      i61 = HEAP32[i60 >> 2] | 0;
      if ((i55 | 0) == (i59 + i61 | 0)) {
       i45 = 230;
       break;
      }
      i56 = HEAP32[i44 + 8 >> 2] | 0;
      if ((i56 | 0) == 0) {
       break;
      } else {
       i44 = i56;
      }
     }
     do {
      if ((i45 | 0) == 230) {
       if ((HEAP32[i44 + 12 >> 2] & 8 | 0) != 0) {
        break;
       }
       i56 = i57;
       if (!(i56 >>> 0 >= i59 >>> 0 & i56 >>> 0 < i55 >>> 0)) {
        break;
       }
       HEAP32[i60 >> 2] = i61 + i54;
       i56 = HEAP32[22] | 0;
       i50 = (HEAP32[19] | 0) + i54 | 0;
       i51 = i56;
       i48 = i56 + 8 | 0;
       if ((i48 & 7 | 0) == 0) {
        i62 = 0;
       } else {
        i62 = -i48 & 7;
       }
       i48 = i50 - i62 | 0;
       HEAP32[22] = i51 + i62;
       HEAP32[19] = i48;
       HEAP32[i51 + (i62 + 4) >> 2] = i48 | 1;
       HEAP32[i51 + (i50 + 4) >> 2] = 40;
       HEAP32[23] = HEAP32[14];
       break L332;
      }
     } while (0);
     if (i55 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      HEAP32[20] = i55;
     }
     i44 = i55 + i54 | 0;
     i50 = 512;
     while (1) {
      i63 = i50 | 0;
      if ((HEAP32[i63 >> 2] | 0) == (i44 | 0)) {
       i45 = 240;
       break;
      }
      i51 = HEAP32[i50 + 8 >> 2] | 0;
      if ((i51 | 0) == 0) {
       break;
      } else {
       i50 = i51;
      }
     }
     do {
      if ((i45 | 0) == 240) {
       if ((HEAP32[i50 + 12 >> 2] & 8 | 0) != 0) {
        break;
       }
       HEAP32[i63 >> 2] = i55;
       i44 = i50 + 4 | 0;
       HEAP32[i44 >> 2] = (HEAP32[i44 >> 2] | 0) + i54;
       i44 = i55 + 8 | 0;
       if ((i44 & 7 | 0) == 0) {
        i64 = 0;
       } else {
        i64 = -i44 & 7;
       }
       i44 = i55 + (i54 + 8) | 0;
       if ((i44 & 7 | 0) == 0) {
        i65 = 0;
       } else {
        i65 = -i44 & 7;
       }
       i44 = i55 + (i65 + i54) | 0;
       i51 = i44;
       i48 = i64 + i14 | 0;
       i56 = i55 + i48 | 0;
       i52 = i56;
       i36 = i44 - (i55 + i64) - i14 | 0;
       HEAP32[i55 + (i64 + 4) >> 2] = i14 | 3;
       do {
        if ((i51 | 0) == (HEAP32[22] | 0)) {
         i35 = (HEAP32[19] | 0) + i36 | 0;
         HEAP32[19] = i35;
         HEAP32[22] = i52;
         HEAP32[i55 + (i48 + 4) >> 2] = i35 | 1;
        } else {
         if ((i51 | 0) == (HEAP32[21] | 0)) {
          i35 = (HEAP32[18] | 0) + i36 | 0;
          HEAP32[18] = i35;
          HEAP32[21] = i52;
          HEAP32[i55 + (i48 + 4) >> 2] = i35 | 1;
          HEAP32[i55 + (i35 + i48) >> 2] = i35;
          break;
         }
         i35 = i54 + 4 | 0;
         i49 = HEAP32[i55 + (i65 + i35) >> 2] | 0;
         if ((i49 & 3 | 0) == 1) {
          i53 = i49 & -8;
          i47 = i49 >>> 3;
          L377 : do {
           if (i49 >>> 0 < 256) {
            i46 = HEAP32[i55 + ((i65 | 8) + i54) >> 2] | 0;
            i42 = HEAP32[i55 + (i54 + 12 + i65) >> 2] | 0;
            i43 = 104 + (i47 << 1 << 2) | 0;
            do {
             if ((i46 | 0) != (i43 | 0)) {
              if (i46 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              }
              if ((HEAP32[i46 + 12 >> 2] | 0) == (i51 | 0)) {
               break;
              }
              _abort();
              return 0;
             }
            } while (0);
            if ((i42 | 0) == (i46 | 0)) {
             HEAP32[16] = HEAP32[16] & ~(1 << i47);
             break;
            }
            do {
             if ((i42 | 0) == (i43 | 0)) {
              i66 = i42 + 8 | 0;
             } else {
              if (i42 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              }
              i12 = i42 + 8 | 0;
              if ((HEAP32[i12 >> 2] | 0) == (i51 | 0)) {
               i66 = i12;
               break;
              }
              _abort();
              return 0;
             }
            } while (0);
            HEAP32[i46 + 12 >> 2] = i42;
            HEAP32[i66 >> 2] = i46;
           } else {
            i43 = i44;
            i12 = HEAP32[i55 + ((i65 | 24) + i54) >> 2] | 0;
            i41 = HEAP32[i55 + (i54 + 12 + i65) >> 2] | 0;
            do {
             if ((i41 | 0) == (i43 | 0)) {
              i40 = i65 | 16;
              i6 = i55 + (i40 + i35) | 0;
              i37 = HEAP32[i6 >> 2] | 0;
              if ((i37 | 0) == 0) {
               i4 = i55 + (i40 + i54) | 0;
               i40 = HEAP32[i4 >> 2] | 0;
               if ((i40 | 0) == 0) {
                i67 = 0;
                break;
               } else {
                i68 = i40;
                i69 = i4;
               }
              } else {
               i68 = i37;
               i69 = i6;
              }
              while (1) {
               i6 = i68 + 20 | 0;
               i37 = HEAP32[i6 >> 2] | 0;
               if ((i37 | 0) != 0) {
                i68 = i37;
                i69 = i6;
                continue;
               }
               i6 = i68 + 16 | 0;
               i37 = HEAP32[i6 >> 2] | 0;
               if ((i37 | 0) == 0) {
                break;
               } else {
                i68 = i37;
                i69 = i6;
               }
              }
              if (i69 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              } else {
               HEAP32[i69 >> 2] = 0;
               i67 = i68;
               break;
              }
             } else {
              i6 = HEAP32[i55 + ((i65 | 8) + i54) >> 2] | 0;
              if (i6 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              }
              i37 = i6 + 12 | 0;
              if ((HEAP32[i37 >> 2] | 0) != (i43 | 0)) {
               _abort();
               return 0;
              }
              i4 = i41 + 8 | 0;
              if ((HEAP32[i4 >> 2] | 0) == (i43 | 0)) {
               HEAP32[i37 >> 2] = i41;
               HEAP32[i4 >> 2] = i6;
               i67 = i41;
               break;
              } else {
               _abort();
               return 0;
              }
             }
            } while (0);
            if ((i12 | 0) == 0) {
             break;
            }
            i41 = i55 + (i54 + 28 + i65) | 0;
            i46 = 368 + (HEAP32[i41 >> 2] << 2) | 0;
            do {
             if ((i43 | 0) == (HEAP32[i46 >> 2] | 0)) {
              HEAP32[i46 >> 2] = i67;
              if ((i67 | 0) != 0) {
               break;
              }
              HEAP32[17] = HEAP32[17] & ~(1 << HEAP32[i41 >> 2]);
              break L377;
             } else {
              if (i12 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              }
              i42 = i12 + 16 | 0;
              if ((HEAP32[i42 >> 2] | 0) == (i43 | 0)) {
               HEAP32[i42 >> 2] = i67;
              } else {
               HEAP32[i12 + 20 >> 2] = i67;
              }
              if ((i67 | 0) == 0) {
               break L377;
              }
             }
            } while (0);
            if (i67 >>> 0 < (HEAP32[20] | 0) >>> 0) {
             _abort();
             return 0;
            }
            HEAP32[i67 + 24 >> 2] = i12;
            i43 = i65 | 16;
            i41 = HEAP32[i55 + (i43 + i54) >> 2] | 0;
            do {
             if ((i41 | 0) != 0) {
              if (i41 >>> 0 < (HEAP32[20] | 0) >>> 0) {
               _abort();
               return 0;
              } else {
               HEAP32[i67 + 16 >> 2] = i41;
               HEAP32[i41 + 24 >> 2] = i67;
               break;
              }
             }
            } while (0);
            i41 = HEAP32[i55 + (i43 + i35) >> 2] | 0;
            if ((i41 | 0) == 0) {
             break;
            }
            if (i41 >>> 0 < (HEAP32[20] | 0) >>> 0) {
             _abort();
             return 0;
            } else {
             HEAP32[i67 + 20 >> 2] = i41;
             HEAP32[i41 + 24 >> 2] = i67;
             break;
            }
           }
          } while (0);
          i70 = i55 + ((i53 | i65) + i54) | 0;
          i71 = i53 + i36 | 0;
         } else {
          i70 = i51;
          i71 = i36;
         }
         i35 = i70 + 4 | 0;
         HEAP32[i35 >> 2] = HEAP32[i35 >> 2] & -2;
         HEAP32[i55 + (i48 + 4) >> 2] = i71 | 1;
         HEAP32[i55 + (i71 + i48) >> 2] = i71;
         i35 = i71 >>> 3;
         if (i71 >>> 0 < 256) {
          i47 = i35 << 1;
          i49 = 104 + (i47 << 2) | 0;
          i41 = HEAP32[16] | 0;
          i12 = 1 << i35;
          do {
           if ((i41 & i12 | 0) == 0) {
            HEAP32[16] = i41 | i12;
            i72 = i49;
            i73 = 104 + (i47 + 2 << 2) | 0;
           } else {
            i35 = 104 + (i47 + 2 << 2) | 0;
            i46 = HEAP32[i35 >> 2] | 0;
            if (i46 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
             i72 = i46;
             i73 = i35;
             break;
            }
            _abort();
            return 0;
           }
          } while (0);
          HEAP32[i73 >> 2] = i52;
          HEAP32[i72 + 12 >> 2] = i52;
          HEAP32[i55 + (i48 + 8) >> 2] = i72;
          HEAP32[i55 + (i48 + 12) >> 2] = i49;
          break;
         }
         i47 = i56;
         i12 = i71 >>> 8;
         do {
          if ((i12 | 0) == 0) {
           i74 = 0;
          } else {
           if (i71 >>> 0 > 16777215) {
            i74 = 31;
            break;
           }
           i41 = (i12 + 1048320 | 0) >>> 16 & 8;
           i53 = i12 << i41;
           i35 = (i53 + 520192 | 0) >>> 16 & 4;
           i46 = i53 << i35;
           i53 = (i46 + 245760 | 0) >>> 16 & 2;
           i42 = 14 - (i35 | i41 | i53) + (i46 << i53 >>> 15) | 0;
           i74 = i71 >>> ((i42 + 7 | 0) >>> 0) & 1 | i42 << 1;
          }
         } while (0);
         i12 = 368 + (i74 << 2) | 0;
         HEAP32[i55 + (i48 + 28) >> 2] = i74;
         HEAP32[i55 + (i48 + 20) >> 2] = 0;
         HEAP32[i55 + (i48 + 16) >> 2] = 0;
         i49 = HEAP32[17] | 0;
         i42 = 1 << i74;
         if ((i49 & i42 | 0) == 0) {
          HEAP32[17] = i49 | i42;
          HEAP32[i12 >> 2] = i47;
          HEAP32[i55 + (i48 + 24) >> 2] = i12;
          HEAP32[i55 + (i48 + 12) >> 2] = i47;
          HEAP32[i55 + (i48 + 8) >> 2] = i47;
          break;
         }
         if ((i74 | 0) == 31) {
          i75 = 0;
         } else {
          i75 = 25 - (i74 >>> 1) | 0;
         }
         i42 = i71 << i75;
         i49 = HEAP32[i12 >> 2] | 0;
         while (1) {
          if ((HEAP32[i49 + 4 >> 2] & -8 | 0) == (i71 | 0)) {
           break;
          }
          i76 = i49 + 16 + (i42 >>> 31 << 2) | 0;
          i12 = HEAP32[i76 >> 2] | 0;
          if ((i12 | 0) == 0) {
           i45 = 313;
           break;
          } else {
           i42 = i42 << 1;
           i49 = i12;
          }
         }
         if ((i45 | 0) == 313) {
          if (i76 >>> 0 < (HEAP32[20] | 0) >>> 0) {
           _abort();
           return 0;
          } else {
           HEAP32[i76 >> 2] = i47;
           HEAP32[i55 + (i48 + 24) >> 2] = i49;
           HEAP32[i55 + (i48 + 12) >> 2] = i47;
           HEAP32[i55 + (i48 + 8) >> 2] = i47;
           break;
          }
         }
         i42 = i49 + 8 | 0;
         i12 = HEAP32[i42 >> 2] | 0;
         i53 = HEAP32[20] | 0;
         if (i49 >>> 0 < i53 >>> 0) {
          _abort();
          return 0;
         }
         if (i12 >>> 0 < i53 >>> 0) {
          _abort();
          return 0;
         } else {
          HEAP32[i12 + 12 >> 2] = i47;
          HEAP32[i42 >> 2] = i47;
          HEAP32[i55 + (i48 + 8) >> 2] = i12;
          HEAP32[i55 + (i48 + 12) >> 2] = i49;
          HEAP32[i55 + (i48 + 24) >> 2] = 0;
          break;
         }
        }
       } while (0);
       i13 = i55 + (i64 | 8) | 0;
       return i13 | 0;
      }
     } while (0);
     i50 = i57;
     i48 = 512;
     while (1) {
      i77 = HEAP32[i48 >> 2] | 0;
      if (i77 >>> 0 <= i50 >>> 0) {
       i78 = HEAP32[i48 + 4 >> 2] | 0;
       i79 = i77 + i78 | 0;
       if (i79 >>> 0 > i50 >>> 0) {
        break;
       }
      }
      i48 = HEAP32[i48 + 8 >> 2] | 0;
     }
     i48 = i77 + (i78 - 39) | 0;
     if ((i48 & 7 | 0) == 0) {
      i80 = 0;
     } else {
      i80 = -i48 & 7;
     }
     i48 = i77 + (i78 - 47 + i80) | 0;
     i56 = i48 >>> 0 < (i57 + 16 | 0) >>> 0 ? i50 : i48;
     i48 = i56 + 8 | 0;
     i52 = i55 + 8 | 0;
     if ((i52 & 7 | 0) == 0) {
      i81 = 0;
     } else {
      i81 = -i52 & 7;
     }
     i52 = i54 - 40 - i81 | 0;
     HEAP32[22] = i55 + i81;
     HEAP32[19] = i52;
     HEAP32[i55 + (i81 + 4) >> 2] = i52 | 1;
     HEAP32[i55 + (i54 - 36) >> 2] = 40;
     HEAP32[23] = HEAP32[14];
     HEAP32[i56 + 4 >> 2] = 27;
     HEAP32[i48 >> 2] = HEAP32[128];
     HEAP32[i48 + 4 >> 2] = HEAP32[516 >> 2];
     HEAP32[i48 + 8 >> 2] = HEAP32[520 >> 2];
     HEAP32[i48 + 12 >> 2] = HEAP32[524 >> 2];
     HEAP32[128] = i55;
     HEAP32[129] = i54;
     HEAP32[131] = 0;
     HEAP32[130] = i48;
     i48 = i56 + 28 | 0;
     HEAP32[i48 >> 2] = 7;
     if ((i56 + 32 | 0) >>> 0 < i79 >>> 0) {
      i52 = i48;
      while (1) {
       i48 = i52 + 4 | 0;
       HEAP32[i48 >> 2] = 7;
       if ((i52 + 8 | 0) >>> 0 < i79 >>> 0) {
        i52 = i48;
       } else {
        break;
       }
      }
     }
     if ((i56 | 0) == (i50 | 0)) {
      break;
     }
     i52 = i56 - i57 | 0;
     i48 = i50 + (i52 + 4) | 0;
     HEAP32[i48 >> 2] = HEAP32[i48 >> 2] & -2;
     HEAP32[i57 + 4 >> 2] = i52 | 1;
     HEAP32[i50 + i52 >> 2] = i52;
     i48 = i52 >>> 3;
     if (i52 >>> 0 < 256) {
      i36 = i48 << 1;
      i51 = 104 + (i36 << 2) | 0;
      i44 = HEAP32[16] | 0;
      i12 = 1 << i48;
      do {
       if ((i44 & i12 | 0) == 0) {
        HEAP32[16] = i44 | i12;
        i82 = i51;
        i83 = 104 + (i36 + 2 << 2) | 0;
       } else {
        i48 = 104 + (i36 + 2 << 2) | 0;
        i42 = HEAP32[i48 >> 2] | 0;
        if (i42 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
         i82 = i42;
         i83 = i48;
         break;
        }
        _abort();
        return 0;
       }
      } while (0);
      HEAP32[i83 >> 2] = i57;
      HEAP32[i82 + 12 >> 2] = i57;
      HEAP32[i57 + 8 >> 2] = i82;
      HEAP32[i57 + 12 >> 2] = i51;
      break;
     }
     i36 = i57;
     i12 = i52 >>> 8;
     do {
      if ((i12 | 0) == 0) {
       i84 = 0;
      } else {
       if (i52 >>> 0 > 16777215) {
        i84 = 31;
        break;
       }
       i44 = (i12 + 1048320 | 0) >>> 16 & 8;
       i50 = i12 << i44;
       i56 = (i50 + 520192 | 0) >>> 16 & 4;
       i48 = i50 << i56;
       i50 = (i48 + 245760 | 0) >>> 16 & 2;
       i42 = 14 - (i56 | i44 | i50) + (i48 << i50 >>> 15) | 0;
       i84 = i52 >>> ((i42 + 7 | 0) >>> 0) & 1 | i42 << 1;
      }
     } while (0);
     i12 = 368 + (i84 << 2) | 0;
     HEAP32[i57 + 28 >> 2] = i84;
     HEAP32[i57 + 20 >> 2] = 0;
     HEAP32[i57 + 16 >> 2] = 0;
     i51 = HEAP32[17] | 0;
     i42 = 1 << i84;
     if ((i51 & i42 | 0) == 0) {
      HEAP32[17] = i51 | i42;
      HEAP32[i12 >> 2] = i36;
      HEAP32[i57 + 24 >> 2] = i12;
      HEAP32[i57 + 12 >> 2] = i57;
      HEAP32[i57 + 8 >> 2] = i57;
      break;
     }
     if ((i84 | 0) == 31) {
      i85 = 0;
     } else {
      i85 = 25 - (i84 >>> 1) | 0;
     }
     i42 = i52 << i85;
     i51 = HEAP32[i12 >> 2] | 0;
     while (1) {
      if ((HEAP32[i51 + 4 >> 2] & -8 | 0) == (i52 | 0)) {
       break;
      }
      i86 = i51 + 16 + (i42 >>> 31 << 2) | 0;
      i12 = HEAP32[i86 >> 2] | 0;
      if ((i12 | 0) == 0) {
       i45 = 348;
       break;
      } else {
       i42 = i42 << 1;
       i51 = i12;
      }
     }
     if ((i45 | 0) == 348) {
      if (i86 >>> 0 < (HEAP32[20] | 0) >>> 0) {
       _abort();
       return 0;
      } else {
       HEAP32[i86 >> 2] = i36;
       HEAP32[i57 + 24 >> 2] = i51;
       HEAP32[i57 + 12 >> 2] = i57;
       HEAP32[i57 + 8 >> 2] = i57;
       break;
      }
     }
     i42 = i51 + 8 | 0;
     i52 = HEAP32[i42 >> 2] | 0;
     i12 = HEAP32[20] | 0;
     if (i51 >>> 0 < i12 >>> 0) {
      _abort();
      return 0;
     }
     if (i52 >>> 0 < i12 >>> 0) {
      _abort();
      return 0;
     } else {
      HEAP32[i52 + 12 >> 2] = i36;
      HEAP32[i42 >> 2] = i36;
      HEAP32[i57 + 8 >> 2] = i52;
      HEAP32[i57 + 12 >> 2] = i51;
      HEAP32[i57 + 24 >> 2] = 0;
      break;
     }
    }
   } while (0);
   i57 = HEAP32[19] | 0;
   if (i57 >>> 0 <= i14 >>> 0) {
    break;
   }
   i52 = i57 - i14 | 0;
   HEAP32[19] = i52;
   i57 = HEAP32[22] | 0;
   i42 = i57;
   HEAP32[22] = i42 + i14;
   HEAP32[i42 + (i14 + 4) >> 2] = i52 | 1;
   HEAP32[i57 + 4 >> 2] = i14 | 3;
   i13 = i57 + 8 | 0;
   return i13 | 0;
  }
 } while (0);
 HEAP32[(___errno_location() | 0) >> 2] = 12;
 i13 = 0;
 return i13 | 0;
}
function _free(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0;
 if ((i1 | 0) == 0) {
  return;
 }
 i2 = i1 - 8 | 0;
 i3 = i2;
 i4 = HEAP32[20] | 0;
 if (i2 >>> 0 < i4 >>> 0) {
  _abort();
 }
 i5 = HEAP32[i1 - 4 >> 2] | 0;
 i6 = i5 & 3;
 if ((i6 | 0) == 1) {
  _abort();
 }
 i7 = i5 & -8;
 i8 = i1 + (i7 - 8) | 0;
 i9 = i8;
 L549 : do {
  if ((i5 & 1 | 0) == 0) {
   i10 = HEAP32[i2 >> 2] | 0;
   if ((i6 | 0) == 0) {
    return;
   }
   i11 = -8 - i10 | 0;
   i12 = i1 + i11 | 0;
   i13 = i12;
   i14 = i10 + i7 | 0;
   if (i12 >>> 0 < i4 >>> 0) {
    _abort();
   }
   if ((i13 | 0) == (HEAP32[21] | 0)) {
    i15 = i1 + (i7 - 4) | 0;
    if ((HEAP32[i15 >> 2] & 3 | 0) != 3) {
     i16 = i13;
     i17 = i14;
     break;
    }
    HEAP32[18] = i14;
    HEAP32[i15 >> 2] = HEAP32[i15 >> 2] & -2;
    HEAP32[i1 + (i11 + 4) >> 2] = i14 | 1;
    HEAP32[i8 >> 2] = i14;
    return;
   }
   i15 = i10 >>> 3;
   if (i10 >>> 0 < 256) {
    i10 = HEAP32[i1 + (i11 + 8) >> 2] | 0;
    i18 = HEAP32[i1 + (i11 + 12) >> 2] | 0;
    i19 = 104 + (i15 << 1 << 2) | 0;
    do {
     if ((i10 | 0) != (i19 | 0)) {
      if (i10 >>> 0 < i4 >>> 0) {
       _abort();
      }
      if ((HEAP32[i10 + 12 >> 2] | 0) == (i13 | 0)) {
       break;
      }
      _abort();
     }
    } while (0);
    if ((i18 | 0) == (i10 | 0)) {
     HEAP32[16] = HEAP32[16] & ~(1 << i15);
     i16 = i13;
     i17 = i14;
     break;
    }
    do {
     if ((i18 | 0) == (i19 | 0)) {
      i20 = i18 + 8 | 0;
     } else {
      if (i18 >>> 0 < i4 >>> 0) {
       _abort();
      }
      i21 = i18 + 8 | 0;
      if ((HEAP32[i21 >> 2] | 0) == (i13 | 0)) {
       i20 = i21;
       break;
      }
      _abort();
     }
    } while (0);
    HEAP32[i10 + 12 >> 2] = i18;
    HEAP32[i20 >> 2] = i10;
    i16 = i13;
    i17 = i14;
    break;
   }
   i19 = i12;
   i15 = HEAP32[i1 + (i11 + 24) >> 2] | 0;
   i21 = HEAP32[i1 + (i11 + 12) >> 2] | 0;
   do {
    if ((i21 | 0) == (i19 | 0)) {
     i22 = i1 + (i11 + 20) | 0;
     i23 = HEAP32[i22 >> 2] | 0;
     if ((i23 | 0) == 0) {
      i24 = i1 + (i11 + 16) | 0;
      i25 = HEAP32[i24 >> 2] | 0;
      if ((i25 | 0) == 0) {
       i26 = 0;
       break;
      } else {
       i27 = i25;
       i28 = i24;
      }
     } else {
      i27 = i23;
      i28 = i22;
     }
     while (1) {
      i22 = i27 + 20 | 0;
      i23 = HEAP32[i22 >> 2] | 0;
      if ((i23 | 0) != 0) {
       i27 = i23;
       i28 = i22;
       continue;
      }
      i22 = i27 + 16 | 0;
      i23 = HEAP32[i22 >> 2] | 0;
      if ((i23 | 0) == 0) {
       break;
      } else {
       i27 = i23;
       i28 = i22;
      }
     }
     if (i28 >>> 0 < i4 >>> 0) {
      _abort();
     } else {
      HEAP32[i28 >> 2] = 0;
      i26 = i27;
      break;
     }
    } else {
     i22 = HEAP32[i1 + (i11 + 8) >> 2] | 0;
     if (i22 >>> 0 < i4 >>> 0) {
      _abort();
     }
     i23 = i22 + 12 | 0;
     if ((HEAP32[i23 >> 2] | 0) != (i19 | 0)) {
      _abort();
     }
     i24 = i21 + 8 | 0;
     if ((HEAP32[i24 >> 2] | 0) == (i19 | 0)) {
      HEAP32[i23 >> 2] = i21;
      HEAP32[i24 >> 2] = i22;
      i26 = i21;
      break;
     } else {
      _abort();
     }
    }
   } while (0);
   if ((i15 | 0) == 0) {
    i16 = i13;
    i17 = i14;
    break;
   }
   i21 = i1 + (i11 + 28) | 0;
   i12 = 368 + (HEAP32[i21 >> 2] << 2) | 0;
   do {
    if ((i19 | 0) == (HEAP32[i12 >> 2] | 0)) {
     HEAP32[i12 >> 2] = i26;
     if ((i26 | 0) != 0) {
      break;
     }
     HEAP32[17] = HEAP32[17] & ~(1 << HEAP32[i21 >> 2]);
     i16 = i13;
     i17 = i14;
     break L549;
    } else {
     if (i15 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
     }
     i10 = i15 + 16 | 0;
     if ((HEAP32[i10 >> 2] | 0) == (i19 | 0)) {
      HEAP32[i10 >> 2] = i26;
     } else {
      HEAP32[i15 + 20 >> 2] = i26;
     }
     if ((i26 | 0) == 0) {
      i16 = i13;
      i17 = i14;
      break L549;
     }
    }
   } while (0);
   if (i26 >>> 0 < (HEAP32[20] | 0) >>> 0) {
    _abort();
   }
   HEAP32[i26 + 24 >> 2] = i15;
   i19 = HEAP32[i1 + (i11 + 16) >> 2] | 0;
   do {
    if ((i19 | 0) != 0) {
     if (i19 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
     } else {
      HEAP32[i26 + 16 >> 2] = i19;
      HEAP32[i19 + 24 >> 2] = i26;
      break;
     }
    }
   } while (0);
   i19 = HEAP32[i1 + (i11 + 20) >> 2] | 0;
   if ((i19 | 0) == 0) {
    i16 = i13;
    i17 = i14;
    break;
   }
   if (i19 >>> 0 < (HEAP32[20] | 0) >>> 0) {
    _abort();
   } else {
    HEAP32[i26 + 20 >> 2] = i19;
    HEAP32[i19 + 24 >> 2] = i26;
    i16 = i13;
    i17 = i14;
    break;
   }
  } else {
   i16 = i3;
   i17 = i7;
  }
 } while (0);
 i3 = i16;
 if (i3 >>> 0 >= i8 >>> 0) {
  _abort();
 }
 i26 = i1 + (i7 - 4) | 0;
 i4 = HEAP32[i26 >> 2] | 0;
 if ((i4 & 1 | 0) == 0) {
  _abort();
 }
 do {
  if ((i4 & 2 | 0) == 0) {
   if ((i9 | 0) == (HEAP32[22] | 0)) {
    i27 = (HEAP32[19] | 0) + i17 | 0;
    HEAP32[19] = i27;
    HEAP32[22] = i16;
    HEAP32[i16 + 4 >> 2] = i27 | 1;
    if ((i16 | 0) != (HEAP32[21] | 0)) {
     return;
    }
    HEAP32[21] = 0;
    HEAP32[18] = 0;
    return;
   }
   if ((i9 | 0) == (HEAP32[21] | 0)) {
    i27 = (HEAP32[18] | 0) + i17 | 0;
    HEAP32[18] = i27;
    HEAP32[21] = i16;
    HEAP32[i16 + 4 >> 2] = i27 | 1;
    HEAP32[i3 + i27 >> 2] = i27;
    return;
   }
   i27 = (i4 & -8) + i17 | 0;
   i28 = i4 >>> 3;
   L652 : do {
    if (i4 >>> 0 < 256) {
     i20 = HEAP32[i1 + i7 >> 2] | 0;
     i6 = HEAP32[i1 + (i7 | 4) >> 2] | 0;
     i2 = 104 + (i28 << 1 << 2) | 0;
     do {
      if ((i20 | 0) != (i2 | 0)) {
       if (i20 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       }
       if ((HEAP32[i20 + 12 >> 2] | 0) == (i9 | 0)) {
        break;
       }
       _abort();
      }
     } while (0);
     if ((i6 | 0) == (i20 | 0)) {
      HEAP32[16] = HEAP32[16] & ~(1 << i28);
      break;
     }
     do {
      if ((i6 | 0) == (i2 | 0)) {
       i29 = i6 + 8 | 0;
      } else {
       if (i6 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       }
       i5 = i6 + 8 | 0;
       if ((HEAP32[i5 >> 2] | 0) == (i9 | 0)) {
        i29 = i5;
        break;
       }
       _abort();
      }
     } while (0);
     HEAP32[i20 + 12 >> 2] = i6;
     HEAP32[i29 >> 2] = i20;
    } else {
     i2 = i8;
     i5 = HEAP32[i1 + (i7 + 16) >> 2] | 0;
     i19 = HEAP32[i1 + (i7 | 4) >> 2] | 0;
     do {
      if ((i19 | 0) == (i2 | 0)) {
       i15 = i1 + (i7 + 12) | 0;
       i21 = HEAP32[i15 >> 2] | 0;
       if ((i21 | 0) == 0) {
        i12 = i1 + (i7 + 8) | 0;
        i10 = HEAP32[i12 >> 2] | 0;
        if ((i10 | 0) == 0) {
         i30 = 0;
         break;
        } else {
         i31 = i10;
         i32 = i12;
        }
       } else {
        i31 = i21;
        i32 = i15;
       }
       while (1) {
        i15 = i31 + 20 | 0;
        i21 = HEAP32[i15 >> 2] | 0;
        if ((i21 | 0) != 0) {
         i31 = i21;
         i32 = i15;
         continue;
        }
        i15 = i31 + 16 | 0;
        i21 = HEAP32[i15 >> 2] | 0;
        if ((i21 | 0) == 0) {
         break;
        } else {
         i31 = i21;
         i32 = i15;
        }
       }
       if (i32 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       } else {
        HEAP32[i32 >> 2] = 0;
        i30 = i31;
        break;
       }
      } else {
       i15 = HEAP32[i1 + i7 >> 2] | 0;
       if (i15 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       }
       i21 = i15 + 12 | 0;
       if ((HEAP32[i21 >> 2] | 0) != (i2 | 0)) {
        _abort();
       }
       i12 = i19 + 8 | 0;
       if ((HEAP32[i12 >> 2] | 0) == (i2 | 0)) {
        HEAP32[i21 >> 2] = i19;
        HEAP32[i12 >> 2] = i15;
        i30 = i19;
        break;
       } else {
        _abort();
       }
      }
     } while (0);
     if ((i5 | 0) == 0) {
      break;
     }
     i19 = i1 + (i7 + 20) | 0;
     i20 = 368 + (HEAP32[i19 >> 2] << 2) | 0;
     do {
      if ((i2 | 0) == (HEAP32[i20 >> 2] | 0)) {
       HEAP32[i20 >> 2] = i30;
       if ((i30 | 0) != 0) {
        break;
       }
       HEAP32[17] = HEAP32[17] & ~(1 << HEAP32[i19 >> 2]);
       break L652;
      } else {
       if (i5 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       }
       i6 = i5 + 16 | 0;
       if ((HEAP32[i6 >> 2] | 0) == (i2 | 0)) {
        HEAP32[i6 >> 2] = i30;
       } else {
        HEAP32[i5 + 20 >> 2] = i30;
       }
       if ((i30 | 0) == 0) {
        break L652;
       }
      }
     } while (0);
     if (i30 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
     }
     HEAP32[i30 + 24 >> 2] = i5;
     i2 = HEAP32[i1 + (i7 + 8) >> 2] | 0;
     do {
      if ((i2 | 0) != 0) {
       if (i2 >>> 0 < (HEAP32[20] | 0) >>> 0) {
        _abort();
       } else {
        HEAP32[i30 + 16 >> 2] = i2;
        HEAP32[i2 + 24 >> 2] = i30;
        break;
       }
      }
     } while (0);
     i2 = HEAP32[i1 + (i7 + 12) >> 2] | 0;
     if ((i2 | 0) == 0) {
      break;
     }
     if (i2 >>> 0 < (HEAP32[20] | 0) >>> 0) {
      _abort();
     } else {
      HEAP32[i30 + 20 >> 2] = i2;
      HEAP32[i2 + 24 >> 2] = i30;
      break;
     }
    }
   } while (0);
   HEAP32[i16 + 4 >> 2] = i27 | 1;
   HEAP32[i3 + i27 >> 2] = i27;
   if ((i16 | 0) != (HEAP32[21] | 0)) {
    i33 = i27;
    break;
   }
   HEAP32[18] = i27;
   return;
  } else {
   HEAP32[i26 >> 2] = i4 & -2;
   HEAP32[i16 + 4 >> 2] = i17 | 1;
   HEAP32[i3 + i17 >> 2] = i17;
   i33 = i17;
  }
 } while (0);
 i17 = i33 >>> 3;
 if (i33 >>> 0 < 256) {
  i3 = i17 << 1;
  i4 = 104 + (i3 << 2) | 0;
  i26 = HEAP32[16] | 0;
  i30 = 1 << i17;
  do {
   if ((i26 & i30 | 0) == 0) {
    HEAP32[16] = i26 | i30;
    i34 = i4;
    i35 = 104 + (i3 + 2 << 2) | 0;
   } else {
    i17 = 104 + (i3 + 2 << 2) | 0;
    i7 = HEAP32[i17 >> 2] | 0;
    if (i7 >>> 0 >= (HEAP32[20] | 0) >>> 0) {
     i34 = i7;
     i35 = i17;
     break;
    }
    _abort();
   }
  } while (0);
  HEAP32[i35 >> 2] = i16;
  HEAP32[i34 + 12 >> 2] = i16;
  HEAP32[i16 + 8 >> 2] = i34;
  HEAP32[i16 + 12 >> 2] = i4;
  return;
 }
 i4 = i16;
 i34 = i33 >>> 8;
 do {
  if ((i34 | 0) == 0) {
   i36 = 0;
  } else {
   if (i33 >>> 0 > 16777215) {
    i36 = 31;
    break;
   }
   i35 = (i34 + 1048320 | 0) >>> 16 & 8;
   i3 = i34 << i35;
   i30 = (i3 + 520192 | 0) >>> 16 & 4;
   i26 = i3 << i30;
   i3 = (i26 + 245760 | 0) >>> 16 & 2;
   i17 = 14 - (i30 | i35 | i3) + (i26 << i3 >>> 15) | 0;
   i36 = i33 >>> ((i17 + 7 | 0) >>> 0) & 1 | i17 << 1;
  }
 } while (0);
 i34 = 368 + (i36 << 2) | 0;
 HEAP32[i16 + 28 >> 2] = i36;
 HEAP32[i16 + 20 >> 2] = 0;
 HEAP32[i16 + 16 >> 2] = 0;
 i17 = HEAP32[17] | 0;
 i3 = 1 << i36;
 do {
  if ((i17 & i3 | 0) == 0) {
   HEAP32[17] = i17 | i3;
   HEAP32[i34 >> 2] = i4;
   HEAP32[i16 + 24 >> 2] = i34;
   HEAP32[i16 + 12 >> 2] = i16;
   HEAP32[i16 + 8 >> 2] = i16;
  } else {
   if ((i36 | 0) == 31) {
    i37 = 0;
   } else {
    i37 = 25 - (i36 >>> 1) | 0;
   }
   i26 = i33 << i37;
   i35 = HEAP32[i34 >> 2] | 0;
   while (1) {
    if ((HEAP32[i35 + 4 >> 2] & -8 | 0) == (i33 | 0)) {
     break;
    }
    i38 = i35 + 16 + (i26 >>> 31 << 2) | 0;
    i30 = HEAP32[i38 >> 2] | 0;
    if ((i30 | 0) == 0) {
     i39 = 525;
     break;
    } else {
     i26 = i26 << 1;
     i35 = i30;
    }
   }
   if ((i39 | 0) == 525) {
    if (i38 >>> 0 < (HEAP32[20] | 0) >>> 0) {
     _abort();
    } else {
     HEAP32[i38 >> 2] = i4;
     HEAP32[i16 + 24 >> 2] = i35;
     HEAP32[i16 + 12 >> 2] = i16;
     HEAP32[i16 + 8 >> 2] = i16;
     break;
    }
   }
   i26 = i35 + 8 | 0;
   i27 = HEAP32[i26 >> 2] | 0;
   i30 = HEAP32[20] | 0;
   if (i35 >>> 0 < i30 >>> 0) {
    _abort();
   }
   if (i27 >>> 0 < i30 >>> 0) {
    _abort();
   } else {
    HEAP32[i27 + 12 >> 2] = i4;
    HEAP32[i26 >> 2] = i4;
    HEAP32[i16 + 8 >> 2] = i27;
    HEAP32[i16 + 12 >> 2] = i35;
    HEAP32[i16 + 24 >> 2] = 0;
    break;
   }
  }
 } while (0);
 i16 = (HEAP32[24] | 0) - 1 | 0;
 HEAP32[24] = i16;
 if ((i16 | 0) == 0) {
  i40 = 520;
 } else {
  return;
 }
 while (1) {
  i16 = HEAP32[i40 >> 2] | 0;
  if ((i16 | 0) == 0) {
   break;
  } else {
   i40 = i16 + 8 | 0;
  }
 }
 HEAP32[24] = -1;
 return;
}
function _strlen(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = i1;
 while (HEAP8[i2] | 0) {
  i2 = i2 + 1 | 0;
 }
 return i2 - i1 | 0;
}
function _memcpy(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0;
 i4 = i1 | 0;
 if ((i1 & 3) == (i2 & 3)) {
  while (i1 & 3) {
   if ((i3 | 0) == 0) return i4 | 0;
   HEAP8[i1] = HEAP8[i2] | 0;
   i1 = i1 + 1 | 0;
   i2 = i2 + 1 | 0;
   i3 = i3 - 1 | 0;
  }
  while ((i3 | 0) >= 4) {
   HEAP32[i1 >> 2] = HEAP32[i2 >> 2];
   i1 = i1 + 4 | 0;
   i2 = i2 + 4 | 0;
   i3 = i3 - 4 | 0;
  }
 }
 while ((i3 | 0) > 0) {
  HEAP8[i1] = HEAP8[i2] | 0;
  i1 = i1 + 1 | 0;
  i2 = i2 + 1 | 0;
  i3 = i3 - 1 | 0;
 }
 return i4 | 0;
}
function _memset(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0;
 i4 = i1 + i3 | 0;
 if ((i3 | 0) >= 20) {
  i2 = i2 & 255;
  i3 = i1 & 3;
  i5 = i2 | i2 << 8 | i2 << 16 | i2 << 24;
  i6 = i4 & ~3;
  if (i3) {
   i3 = i1 + 4 - i3 | 0;
   while ((i1 | 0) < (i3 | 0)) {
    HEAP8[i1] = i2;
    i1 = i1 + 1 | 0;
   }
  }
  while ((i1 | 0) < (i6 | 0)) {
   HEAP32[i1 >> 2] = i5;
   i1 = i1 + 4 | 0;
  }
 }
 while ((i1 | 0) < (i4 | 0)) {
  HEAP8[i1] = i2;
  i1 = i1 + 1 | 0;
 }
}
function dynCall_ii(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 return FUNCTION_TABLE_ii[i1 & 1](i2 | 0) | 0;
}
function dynCall_v(i1) {
 i1 = i1 | 0;
 FUNCTION_TABLE_v[i1 & 1]();
}
function dynCall_iii(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return FUNCTION_TABLE_iii[i1 & 1](i2 | 0, i3 | 0) | 0;
}
function dynCall_vi(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 FUNCTION_TABLE_vi[i1 & 1](i2 | 0);
}
function b0(i1) {
 i1 = i1 | 0;
 abort(0);
 return 0;
}
function b1() {
 abort(1);
}
function b2(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(2);
 return 0;
}
function b3(i1) {
 i1 = i1 | 0;
 abort(3);
}
// EMSCRIPTEN_END_FUNCS
  var FUNCTION_TABLE_ii = [b0,b0];
  var FUNCTION_TABLE_v = [b1,b1];
  var FUNCTION_TABLE_iii = [b2,b2];
  var FUNCTION_TABLE_vi = [b3,b3];
  return { _strlen: _strlen, _free: _free, _main: _main, _memset: _memset, _malloc: _malloc, _memcpy: _memcpy, runPostSets: runPostSets, stackAlloc: stackAlloc, stackSave: stackSave, stackRestore: stackRestore, setThrew: setThrew, setTempRet0: setTempRet0, setTempRet1: setTempRet1, setTempRet2: setTempRet2, setTempRet3: setTempRet3, setTempRet4: setTempRet4, setTempRet5: setTempRet5, setTempRet6: setTempRet6, setTempRet7: setTempRet7, setTempRet8: setTempRet8, setTempRet9: setTempRet9, dynCall_ii: dynCall_ii, dynCall_v: dynCall_v, dynCall_iii: dynCall_iii, dynCall_vi: dynCall_vi };
})
// EMSCRIPTEN_END_ASM
({ "Math": Math, "Int8Array": Int8Array, "Int16Array": Int16Array, "Int32Array": Int32Array, "Uint8Array": Uint8Array, "Uint16Array": Uint16Array, "Uint32Array": Uint32Array, "Float32Array": Float32Array, "Float64Array": Float64Array }, { "abort": abort, "assert": assert, "asmPrintInt": asmPrintInt, "asmPrintFloat": asmPrintFloat, "min": Math_min, "invoke_ii": invoke_ii, "invoke_v": invoke_v, "invoke_iii": invoke_iii, "invoke_vi": invoke_vi, "_pwrite": _pwrite, "_sysconf": _sysconf, "_sbrk": _sbrk, "___setErrNo": ___setErrNo, "_fwrite": _fwrite, "__reallyNegative": __reallyNegative, "_time": _time, "__formatString": __formatString, "_send": _send, "_write": _write, "_abort": _abort, "_fprintf": _fprintf, "_printf": _printf, "___errno_location": ___errno_location, "_fflush": _fflush, "STACKTOP": STACKTOP, "STACK_MAX": STACK_MAX, "tempDoublePtr": tempDoublePtr, "ABORT": ABORT, "NaN": NaN, "Infinity": Infinity }, buffer);
var _strlen = Module["_strlen"] = asm["_strlen"];
var _free = Module["_free"] = asm["_free"];
var _main = Module["_main"] = asm["_main"];
var _memset = Module["_memset"] = asm["_memset"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];
var dynCall_ii = Module["dynCall_ii"] = asm["dynCall_ii"];
var dynCall_v = Module["dynCall_v"] = asm["dynCall_v"];
var dynCall_iii = Module["dynCall_iii"] = asm["dynCall_iii"];
var dynCall_vi = Module["dynCall_vi"] = asm["dynCall_vi"];
Runtime.stackAlloc = function(size) { return asm['stackAlloc'](size) };
Runtime.stackSave = function() { return asm['stackSave']() };
Runtime.stackRestore = function(top) { asm['stackRestore'](top) };
// Warning: printing of i64 values may be slightly rounded! No deep i64 math used, so precise i64 code not included
var i64Math = null;
// === Auto-generated postamble setup entry stuff ===
function ExitStatus(status) {
  this.name = "ExitStatus";
  this.message = "Program terminated with exit(" + status + ")";
  this.status = status;
};
ExitStatus.prototype = new Error();
ExitStatus.prototype.constructor = ExitStatus;
var initialStackTop;
var preloadStartTime = null;
Module['callMain'] = Module.callMain = function callMain(args) {
  assert(runDependencies == 0, 'cannot call main when async dependencies remain! (listen on __ATMAIN__)');
  assert(__ATPRERUN__.length == 0, 'cannot call main when preRun functions remain to be called');
  args = args || [];
  if (ENVIRONMENT_IS_WEB && preloadStartTime !== null) {
    //Module.printErr('preload time: ' + (Date.now() - preloadStartTime) + ' ms');
  }
  ensureInitRuntime();
  var argc = args.length+1;
  function pad() {
    for (var i = 0; i < 4-1; i++) {
      argv.push(0);
    }
  }
  var argv = [allocate(intArrayFromString("/bin/this.program"), 'i8', ALLOC_NORMAL) ];
  pad();
  for (var i = 0; i < argc-1; i = i + 1) {
    argv.push(allocate(intArrayFromString(args[i]), 'i8', ALLOC_NORMAL));
    pad();
  }
  argv.push(0);
  argv = allocate(argv, 'i32', ALLOC_NORMAL);
  initialStackTop = STACKTOP;
  try {
    var ret = Module['_main'](argc, argv, 0);
    // if we're not running an evented main loop, it's time to exit
    if (!Module['noExitRuntime']) {
      exit(ret);
    }
  }
  catch(e) {
    if (e instanceof ExitStatus) {
      // exit() throws this once it's done to make sure execution
      // has been stopped completely
      return;
    } else if (e == 'SimulateInfiniteLoop') {
      // running an evented main loop, don't immediately exit
      Module['noExitRuntime'] = true;
      return;
    } else {
      throw e;
    }
  }
}
function run(args) {
  args = args || Module['arguments'];
  if (preloadStartTime === null) preloadStartTime = Date.now();
  if (runDependencies > 0) {
    Module.printErr('run() called, but dependencies remain, so not running');
    return;
  }
  preRun();
  if (runDependencies > 0) {
    // a preRun added a dependency, run will be called later
    return;
  }
  function doRun() {
    ensureInitRuntime();
    preMain();
    calledRun = true;
    if (Module['_main'] && shouldRunNow) {
      Module['callMain'](args);
    }
    postRun();
  }
  if (Module['setStatus']) {
    Module['setStatus']('Running...');
    setTimeout(function() {
      setTimeout(function() {
        Module['setStatus']('');
      }, 1);
      if (!ABORT) doRun();
    }, 1);
  } else {
    doRun();
  }
}
Module['run'] = Module.run = run;
function exit(status) {
  ABORT = true;
  EXITSTATUS = status;
  STACKTOP = initialStackTop;
  // exit the runtime
  exitRuntime();
  // throw an exception to halt the current execution
  throw new ExitStatus(status);
}
Module['exit'] = Module.exit = exit;
function abort(text) {
  if (text) {
    Module.print(text);
    Module.printErr(text);
  }
  ABORT = true;
  EXITSTATUS = 1;
  throw 'abort() at ' + (new Error().stack);
}
Module['abort'] = Module.abort = abort;
// {{PRE_RUN_ADDITIONS}}
if (Module['preInit']) {
  if (typeof Module['preInit'] == 'function') Module['preInit'] = [Module['preInit']];
  while (Module['preInit'].length > 0) {
    Module['preInit'].pop()();
  }
}
// shouldRunNow refers to calling main(), not run().
var shouldRunNow = true;
if (Module['noInitialRun']) {
  shouldRunNow = false;
}
run([].concat(Module["arguments"]));
if (JSRegress_outputBuffer != "final: 400.\n")
    throw "Error: bad result: " + JSRegress_outputBuffer;
// {{POST_RUN_ADDITIONS}}
// {{MODULE_ADDITIONS}}
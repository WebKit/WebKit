//@ slow!
//@ runDefault
//@ runDefaultFTL if $enableFTL

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
  Module['print'] = print;
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
      console.log(x);
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
      code = code & 0xFF;
      if (buffer.length == 0) {
        if ((code & 0x80) == 0x00) {        // 0xxxxxxx
          return String.fromCharCode(code);
        }
        buffer.push(code);
        if ((code & 0xE0) == 0xC0) {        // 110xxxxx
          needed = 1;
        } else if ((code & 0xF0) == 0xE0) { // 1110xxxx
          needed = 2;
        } else {                            // 11110xxx
          needed = 3;
        }
        return '';
      }
      if (needed) {
        buffer.push(code);
        needed--;
        if (needed > 0) return '';
      }
      var c1 = buffer[0];
      var c2 = buffer[1];
      var c3 = buffer[2];
      var c4 = buffer[3];
      var ret;
      if (buffer.length == 2) {
        ret = String.fromCharCode(((c1 & 0x1F) << 6)  | (c2 & 0x3F));
      } else if (buffer.length == 3) {
        ret = String.fromCharCode(((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6)  | (c3 & 0x3F));
      } else {
        // http://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
        var codePoint = ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) |
                        ((c3 & 0x3F) << 6)  | (c4 & 0x3F);
        ret = String.fromCharCode(
          Math.floor((codePoint - 0x10000) / 0x400) + 0xD800,
          (codePoint - 0x10000) % 0x400 + 0xDC00);
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
      case 'i64': (tempI64 = [value>>>0,(tempDouble=value,(+(Math.abs(tempDouble))) >= (+(1)) ? (tempDouble > (+(0)) ? ((Math.min((+(Math.floor((tempDouble)/(+(4294967296))))), (+(4294967295))))|0)>>>0 : (~~((+(Math.ceil((tempDouble - +(((~~(tempDouble)))>>>0))/(+(4294967296)))))))>>>0) : 0)],HEAP32[((ptr)>>2)]=tempI64[0],HEAP32[(((ptr)+(4))>>2)]=tempI64[1]); break;
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
  // TODO: use TextDecoder
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
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 16777216;
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
    if (!calledRun && shouldRunNow) run();
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
STATICTOP = STATIC_BASE + 16392;
var _stdout;
var _stdin;
var _stderr;
/* global initializers */ __ATINIT__.push({ func: function() { runPostSets() } },{ func: function() { __GLOBAL__I_a() } },{ func: function() { __GLOBAL__I_a8() } });
var ___fsmu8;
var ___dso_handle;
var __ZTVN10__cxxabiv120__si_class_type_infoE;
var __ZTVN10__cxxabiv117__class_type_infoE;
var __ZNSt3__112__rs_defaultD1Ev;
var __ZNSt13runtime_errorC1EPKc;
var __ZNSt13runtime_errorD1Ev;
var __ZNSt12length_errorD1Ev;
var __ZNSt3__16localeC1Ev;
var __ZNSt3__16localeC1ERKS0_;
var __ZNSt3__16localeD1Ev;
var __ZNSt8bad_castC1Ev;
var __ZNSt8bad_castD1Ev;
var _stdout = _stdout=allocate([0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC);
var _stdin = _stdin=allocate([0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC);
var _stderr = _stderr=allocate([0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC);
__ZTVN10__cxxabiv120__si_class_type_infoE=allocate([0,0,0,0,200,37,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC);
__ZTVN10__cxxabiv117__class_type_infoE=allocate([0,0,0,0,216,37,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC);
/* memory initializer */ allocate([0,0,0,0,0,0,36,64,0,0,0,0,0,0,89,64,0,0,0,0,0,136,195,64,0,0,0,0,132,215,151,65,0,128,224,55,121,195,65,67,23,110,5,181,181,184,147,70,245,249,63,233,3,79,56,77,50,29,48,249,72,119,130,90,60,191,115,127,221,79,21,117,74,117,108,0,0,0,0,0,74,117,110,0,0,0,0,0,65,112,114,0,0,0,0,0,77,97,114,0,0,0,0,0,70,101,98,0,0,0,0,0,74,97,110,0,0,0,0,0,68,101,99,101,109,98,101,114,0,0,0,0,0,0,0,0,78,111,118,101,109,98,101,114,0,0,0,0,0,0,0,0,79,99,116,111,98,101,114,0,83,101,112,116,101,109,98,101,114,0,0,0,0,0,0,0,117,110,115,117,112,112,111,114,116,101,100,32,108,111,99,97,108,101,32,102,111,114,32,115,116,97,110,100,97,114,100,32,105,110,112,117,116,0,0,0,65,117,103,117,115,116,0,0,74,117,108,121,0,0,0,0,74,117,110,101,0,0,0,0,77,97,121,0,0,0,0,0,65,112,114,105,108,0,0,0,77,97,114,99,104,0,0,0,70,101,98,114,117,97,114,121,0,0,0,0,0,0,0,0,74,97,110,117,97,114,121,0,68,0,0,0,101,0,0,0,99,0,0,0,0,0,0,0,78,0,0,0,111,0,0,0,118,0,0,0,0,0,0,0,79,0,0,0,99,0,0,0,116,0,0,0,0,0,0,0,83,0,0,0,101,0,0,0,112,0,0,0,0,0,0,0,98,97,115,105,99,95,115,116,114,105,110,103,0,0,0,0,65,0,0,0,117,0,0,0,103,0,0,0,0,0,0,0,74,0,0,0,117,0,0,0,108,0,0,0,0,0,0,0,74,0,0,0,117,0,0,0,110,0,0,0,0,0,0,0,77,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,65,0,0,0,112,0,0,0,114,0,0,0,0,0,0,0,77,0,0,0,97,0,0,0,114,0,0,0,0,0,0,0,70,0,0,0,101,0,0,0,98,0,0,0,0,0,0,0,74,0,0,0,97,0,0,0,110,0,0,0,0,0,0,0,68,0,0,0,101,0,0,0,99,0,0,0,101,0,0,0,109,0,0,0,98,0,0,0,101,0,0,0,114,0,0,0,0,0,0,0,0,0,0,0,78,0,0,0,111,0,0,0,118,0,0,0,101,0,0,0,109,0,0,0,98,0,0,0,101,0,0,0,114,0,0,0,0,0,0,0,0,0,0,0,79,0,0,0,99,0,0,0,116,0,0,0,111,0,0,0,98,0,0,0,101,0,0,0,114,0,0,0,0,0,0,0,83,0,0,0,101,0,0,0,112,0,0,0,116,0,0,0,101,0,0,0,109,0,0,0,98,0,0,0,101,0,0,0,114,0,0,0,0,0,0,0,65,0,0,0,117,0,0,0,103,0,0,0,117,0,0,0,115,0,0,0,116,0,0,0,0,0,0,0,0,0,0,0,74,0,0,0,117,0,0,0,108,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,74,0,0,0,117,0,0,0,110,0,0,0,101,0,0,0,0,0,0,0,0,0,0,0,65,0,0,0,112,0,0,0,114,0,0,0,105,0,0,0,108,0,0,0,0,0,0,0,77,0,0,0,97,0,0,0,114,0,0,0,99,0,0,0,104,0,0,0,0,0,0,0,70,0,0,0,101,0,0,0,98,0,0,0,114,0,0,0,117,0,0,0,97,0,0,0,114,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,74,0,0,0,97,0,0,0,110,0,0,0,117,0,0,0,97,0,0,0,114,0,0,0,121,0,0,0,0,0,0,0,80,77,0,0,0,0,0,0,65,77,0,0,0,0,0,0,80,0,0,0,77,0,0,0,0,0,0,0,0,0,0,0,65,0,0,0,77,0,0,0,0,0,0,0,0,0,0,0,108,111,99,97,108,101,32,110,111,116,32,115,117,112,112,111,114,116,101,100,0,0,0,0,37,0,0,0,73,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,32,0,0,0,37,0,0,0,112,0,0,0,0,0,0,0,37,73,58,37,77,58,37,83,32,37,112,0,0,0,0,0,37,0,0,0,97,0,0,0,32,0,0,0,37,0,0,0,98,0,0,0,32,0,0,0,37,0,0,0,100,0,0,0,32,0,0,0,37,0,0,0,72,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,32,0,0,0,37,0,0,0,89,0,0,0,0,0,0,0,0,0,0,0,37,97,32,37,98,32,37,100,32,37,72,58,37,77,58,37,83,32,37,89,0,0,0,0,37,0,0,0,72,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,0,0,0,0,0,0,0,0,37,72,58,37,77,58,37,83,0,0,0,0,0,0,0,0,115,116,100,58,58,98,97,100,95,97,108,108,111,99,0,0,37,0,0,0,109,0,0,0,47,0,0,0,37,0,0,0,100,0,0,0,47,0,0,0,37,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,37,109,47,37,100,47,37,121,0,0,0,0,0,0,0,0,102,0,0,0,97,0,0,0,108,0,0,0,115,0,0,0,101,0,0,0,0,0,0,0,102,97,108,115,101,0,0,0,116,0,0,0,114,0,0,0,117,0,0,0,101,0,0,0,0,0,0,0,0,0,0,0,116,114,117,101,0,0,0,0,58,32,0,0,0,0,0,0,105,111,115,95,98,97,115,101,58,58,99,108,101,97,114,0,37,112,0,0,0,0,0,0,115,116,100,58,58,98,97,100,95,99,97,115,116,0,0,0,67,0,0,0,0,0,0,0,118,101,99,116,111,114,0,0,37,46,48,76,102,0,0,0,109,111,110,101,121,95,103,101,116,32,101,114,114,111,114,0,83,97,116,0,0,0,0,0,70,114,105,0,0,0,0,0,84,104,117,0,0,0,0,0,37,76,102,0,0,0,0,0,105,111,115,116,114,101,97,109,0,0,0,0,0,0,0,0,87,101,100,0,0,0,0,0,84,117,101,0,0,0,0,0,77,111,110,0,0,0,0,0,83,117,110,0,0,0,0,0,83,97,116,117,114,100,97,121,0,0,0,0,0,0,0,0,70,114,105,100,97,121,0,0,84,104,117,114,115,100,97,121,0,0,0,0,0,0,0,0,87,101,100,110,101,115,100,97,121,0,0,0,0,0,0,0,84,117,101,115,100,97,121,0,77,111,110,100,97,121,0,0,83,117,110,100,97,121,0,0,83,0,0,0,97,0,0,0,116,0,0,0,0,0,0,0,70,0,0,0,114,0,0,0,105,0,0,0,0,0,0,0,84,0,0,0,104,0,0,0,117,0,0,0,0,0,0,0,87,0,0,0,101,0,0,0,100,0,0,0,0,0,0,0,84,0,0,0,117,0,0,0,101,0,0,0,0,0,0,0,77,0,0,0,111,0,0,0,110,0,0,0,0,0,0,0,117,110,115,112,101,99,105,102,105,101,100,32,105,111,115,116,114,101,97,109,95,99,97,116,101,103,111,114,121,32,101,114,114,111,114,0,0,0,0,0,83,0,0,0,117,0,0,0,110,0,0,0,0,0,0,0,83,0,0,0,97,0,0,0,116,0,0,0,117,0,0,0,114,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,70,0,0,0,114,0,0,0,105,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,84,0,0,0,104,0,0,0,117,0,0,0,114,0,0,0,115,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,87,0,0,0,101,0,0,0,100,0,0,0,110,0,0,0,101,0,0,0,115,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,84,0,0,0,117,0,0,0,101,0,0,0,115,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,77,0,0,0,111,0,0,0,110,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,83,0,0,0,117,0,0,0,110,0,0,0,100,0,0,0,97,0,0,0,121,0,0,0,0,0,0,0,0,0,0,0,68,101,99,0,0,0,0,0,78,111,118,0,0,0,0,0,79,99,116,0,0,0,0,0,83,101,112,0,0,0,0,0,65,117,103,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,49,50,51,52,53,54,55,56,57,0,0,0,0,0,0,48,49,50,51,52,53,54,55,56,57,0,0,0,0,0,0,37,0,0,0,89,0,0,0,45,0,0,0,37,0,0,0,109,0,0,0,45,0,0,0,37,0,0,0,100,0,0,0,37,0,0,0,72,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,37,0,0,0,72,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,0,0,0,0,37,0,0,0,73,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,32,0,0,0,37,0,0,0,112,0,0,0,0,0,0,0,37,0,0,0,109,0,0,0,47,0,0,0,37,0,0,0,100,0,0,0,47,0,0,0,37,0,0,0,121,0,0,0,37,0,0,0,72,0,0,0,58,0,0,0,37,0,0,0,77,0,0,0,58,0,0,0,37,0,0,0,83,0,0,0,37,72,58,37,77,58,37,83,37,72,58,37,77,0,0,0,37,73,58,37,77,58,37,83,32,37,112,0,0,0,0,0,37,89,45,37,109,45,37,100,37,109,47,37,100,47,37,121,37,72,58,37,77,58,37,83,37,0,0,0,0,0,0,0,37,112,0,0,0,0,0,0,0,0,0,0,224,31,0,0,30,0,0,0,116,0,0,0,98,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,31,0,0,196,0,0,0,164,0,0,0,36,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,70,0,0,0,6,1,0,0,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,32,0,0,94,0,0,0,8,0,0,0,110,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,32,0,0,94,0,0,0,20,0,0,0,110,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,32,0,0,168,0,0,0,84,0,0,0,48,0,0,0,2,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,88,32,0,0,230,0,0,0,186,0,0,0,48,0,0,0,4,0,0,0,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,32,0,0,162,0,0,0,188,0,0,0,48,0,0,0,8,0,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,152,32,0,0,254,0,0,0,140,0,0,0,48,0,0,0,6,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,33,0,0,252,0,0,0,16,0,0,0,48,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,88,33,0,0,160,0,0,0,108,0,0,0,48,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,120,33,0,0,38,0,0,0,110,0,0,0,48,0,0,0,120,0,0,0,4,0,0,0,32,0,0,0,6,0,0,0,20,0,0,0,56,0,0,0,2,0,0,0,248,255,255,255,120,33,0,0,22,0,0,0,8,0,0,0,32,0,0,0,12,0,0,0,2,0,0,0,30,0,0,0,124,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,33,0,0,242,0,0,0,222,0,0,0,48,0,0,0,20,0,0,0,16,0,0,0,60,0,0,0,26,0,0,0,18,0,0,0,2,0,0,0,4,0,0,0,248,255,255,255,160,33,0,0,70,0,0,0,104,0,0,0,116,0,0,0,122,0,0,0,64,0,0,0,44,0,0,0,54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200,33,0,0,78,0,0,0,190,0,0,0,48,0,0,0,48,0,0,0,40,0,0,0,8,0,0,0,38,0,0,0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,216,33,0,0,58,0,0,0,64,0,0,0,48,0,0,0,42,0,0,0,84,0,0,0,12,0,0,0,54,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,232,33,0,0,246,0,0,0,2,0,0,0,48,0,0,0,24,0,0,0,30,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,34,0,0,46,0,0,0,208,0,0,0,48,0,0,0,36,0,0,0,14,0,0,0,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40,34,0,0,210,0,0,0,112,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,34,0,0,28,0,0,0,138,0,0,0,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,34,0,0,6,0,0,0,174,0,0,0,48,0,0,0,8,0,0,0,6,0,0,0,12,0,0,0,4,0,0,0,10,0,0,0,4,0,0,0,2,0,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,34,0,0,98,0,0,0,18,0,0,0,48,0,0,0,20,0,0,0,24,0,0,0,34,0,0,0,22,0,0,0,22,0,0,0,8,0,0,0,6,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,34,0,0,40,0,0,0,24,0,0,0,48,0,0,0,48,0,0,0,46,0,0,0,38,0,0,0,40,0,0,0,30,0,0,0,44,0,0,0,36,0,0,0,54,0,0,0,52,0,0,0,50,0,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,34,0,0,52,0,0,0,4,0,0,0,48,0,0,0,76,0,0,0,70,0,0,0,64,0,0,0,66,0,0,0,58,0,0,0,68,0,0,0,62,0,0,0,28,0,0,0,74,0,0,0,72,0,0,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,34,0,0,74,0,0,0,92,0,0,0,48,0,0,0,16,0,0,0,12,0,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,34,0,0,26,0,0,0,176,0,0,0,48,0,0,0,22,0,0,0,14,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,34,0,0,228,0,0,0,130,0,0,0,48,0,0,0,14,0,0,0,4,0,0,0,20,0,0,0,16,0,0,0,62,0,0,0,4,0,0,0,76,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,35,0,0,180,0,0,0,60,0,0,0,48,0,0,0,2,0,0,0,8,0,0,0,8,0,0,0,106,0,0,0,96,0,0,0,18,0,0,0,94,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,35,0,0,180,0,0,0,132,0,0,0,48,0,0,0,16,0,0,0,6,0,0,0,2,0,0,0,126,0,0,0,46,0,0,0,12,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,35,0,0,180,0,0,0,152,0,0,0,48,0,0,0,10,0,0,0,12,0,0,0,24,0,0,0,34,0,0,0,74,0,0,0,6,0,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,35,0,0,180,0,0,0,34,0,0,0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,112,35,0,0,56,0,0,0,156,0,0,0,48,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,35,0,0,180,0,0,0,80,0,0,0,48,0,0,0,20,0,0,0,2,0,0,0,4,0,0,0,10,0,0,0,16,0,0,0,28,0,0,0,22,0,0,0,6,0,0,0,6,0,0,0,8,0,0,0,10,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,160,35,0,0,4,1,0,0,36,0,0,0,48,0,0,0,2,0,0,0,4,0,0,0,18,0,0,0,34,0,0,0,8,0,0,0,6,0,0,0,26,0,0,0,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,35,0,0,66,0,0,0,218,0,0,0,78,0,0,0,2,0,0,0,14,0,0,0,32,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,35,0,0,180,0,0,0,86,0,0,0,48,0,0,0,10,0,0,0,12,0,0,0,24,0,0,0,34,0,0,0,74,0,0,0,6,0,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,35,0,0,180,0,0,0,238,0,0,0,48,0,0,0,10,0,0,0,12,0,0,0,24,0,0,0,34,0,0,0,74,0,0,0,6,0,0,0,66,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,36,0,0,126,0,0,0,234,0,0,0,16,0,0,0,22,0,0,0,16,0,0,0,10,0,0,0,88,0,0,0,100,0,0,0,30,0,0,0,28,0,0,0,26,0,0,0,28,0,0,0,40,0,0,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,36,0,0,10,0,0,0,118,0,0,0,58,0,0,0,38,0,0,0,28,0,0,0,6,0,0,0,50,0,0,0,86,0,0,0,18,0,0,0,6,0,0,0,10,0,0,0,24,0,0,0,16,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,36,0,0,100,0,0,0,194,0,0,0,2,0,0,0,2,0,0,0,14,0,0,0,32,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,56,36,0,0,44,0,0,0,206,0,0,0,252,255,255,255,252,255,255,255,56,36,0,0,146,0,0,0,124,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,80,36,0,0,212,0,0,0,236,0,0,0,252,255,255,255,252,255,255,255,80,36,0,0,106,0,0,0,200,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,104,36,0,0,88,0,0,0,8,1,0,0,248,255,255,255,248,255,255,255,104,36,0,0,182,0,0,0,232,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,128,36,0,0,104,0,0,0,204,0,0,0,248,255,255,255,248,255,255,255,128,36,0,0,136,0,0,0,50,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,152,36,0,0,202,0,0,0,184,0,0,0,38,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,36,0,0,248,0,0,0,226,0,0,0,4,0,0,0,22,0,0,0,16,0,0,0,10,0,0,0,58,0,0,0,100,0,0,0,30,0,0,0,28,0,0,0,26,0,0,0,28,0,0,0,40,0,0,0,26,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,208,36,0,0,154,0,0,0,178,0,0,0,34,0,0,0,38,0,0,0,28,0,0,0,6,0,0,0,90,0,0,0,86,0,0,0,18,0,0,0,6,0,0,0,10,0,0,0,24,0,0,0,16,0,0,0,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,37,0,0,220,0,0,0,144,0,0,0,48,0,0,0,68,0,0,0,118,0,0,0,40,0,0,0,80,0,0,0,6,0,0,0,28,0,0,0,52,0,0,0,20,0,0,0,36,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,37,0,0,102,0,0,0,54,0,0,0,48,0,0,0,112,0,0,0,4,0,0,0,66,0,0,0,74,0,0,0,76,0,0,0,22,0,0,0,114,0,0,0,50,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,37,0,0,224,0,0,0,114,0,0,0,48,0,0,0,14,0,0,0,60,0,0,0,46,0,0,0,42,0,0,0,78,0,0,0,52,0,0,0,92,0,0,0,56,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,37,0,0,76,0,0,0,172,0,0,0,48,0,0,0,102,0,0,0,108,0,0,0,26,0,0,0,72,0,0,0,24,0,0,0,18,0,0,0,80,0,0,0,70,0,0,0,68,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,152,37,0,0,90,0,0,0,68,0,0,0,62,0,0,0,22,0,0,0,16,0,0,0,10,0,0,0,88,0,0,0,100,0,0,0,30,0,0,0,72,0,0,0,82,0,0,0,12,0,0,0,40,0,0,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,168,37,0,0,14,0,0,0,214,0,0,0,60,0,0,0,38,0,0,0,28,0,0,0,6,0,0,0,50,0,0,0,86,0,0,0,18,0,0,0,56,0,0,0,24,0,0,0,4,0,0,0,16,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,184,37,0,0,250,0,0,0,198,0,0,0,62,0,0,0,150,0,0,0,8,0,0,0,2,0,0,0,6,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,83,116,57,116,121,112,101,95,105,110,102,111,0,0,0,0,83,116,57,101,120,99,101,112,116,105,111,110,0,0,0,0,83,116,57,98,97,100,95,97,108,108,111,99,0,0,0,0,83,116,56,98,97,100,95,99,97,115,116,0,0,0,0,0,83,116,49,51,114,117,110,116,105,109,101,95,101,114,114,111,114,0,0,0,0,0,0,0,83,116,49,50,108,101,110,103,116,104,95,101,114,114,111,114,0,0,0,0,0,0,0,0,83,116,49,49,108,111,103,105,99,95,101,114,114,111,114,0,78,83,116,51,95,95,49,57,116,105,109,101,95,98,97,115,101,69,0,0,0,0,0,0,78,83,116,51,95,95,49,57,109,111,110,101,121,95,112,117,116,73,119,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,78,83,116,51,95,95,49,57,109,111,110,101,121,95,112,117,116,73,99,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,78,83,116,51,95,95,49,57,109,111,110,101,121,95,103,101,116,73,119,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,78,83,116,51,95,95,49,57,109,111,110,101,121,95,103,101,116,73,99,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,78,83,116,51,95,95,49,57,98,97,115,105,99,95,105,111,115,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,57,98,97,115,105,99,95,105,111,115,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,57,95,95,110,117,109,95,112,117,116,73,119,69,69,0,0,0,78,83,116,51,95,95,49,57,95,95,110,117,109,95,112,117,116,73,99,69,69,0,0,0,78,83,116,51,95,95,49,57,95,95,110,117,109,95,103,101,116,73,119,69,69,0,0,0,78,83,116,51,95,95,49,57,95,95,110,117,109,95,103,101,116,73,99,69,69,0,0,0,78,83,116,51,95,95,49,56,116,105,109,101,95,112,117,116,73,119,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,0,78,83,116,51,95,95,49,56,116,105,109,101,95,112,117,116,73,99,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,0,78,83,116,51,95,95,49,56,116,105,109,101,95,103,101,116,73,119,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,0,78,83,116,51,95,95,49,56,116,105,109,101,95,103,101,116,73,99,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,0,78,83,116,51,95,95,49,56,110,117,109,112,117,110,99,116,73,119,69,69,0,0,0,0,78,83,116,51,95,95,49,56,110,117,109,112,117,110,99,116,73,99,69,69,0,0,0,0,78,83,116,51,95,95,49,56,109,101,115,115,97,103,101,115,73,119,69,69,0,0,0,0,78,83,116,51,95,95,49,56,109,101,115,115,97,103,101,115,73,99,69,69,0,0,0,0,78,83,116,51,95,95,49,56,105,111,115,95,98,97,115,101,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,56,105,111,115,95,98,97,115,101,55,102,97,105,108,117,114,101,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,55,110,117,109,95,112,117,116,73,119,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,110,117,109,95,112,117,116,73,99,78,83,95,49,57,111,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,110,117,109,95,103,101,116,73,119,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,110,117,109,95,103,101,116,73,99,78,83,95,49,57,105,115,116,114,101,97,109,98,117,102,95,105,116,101,114,97,116,111,114,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,108,108,97,116,101,73,119,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,108,108,97,116,101,73,99,69,69,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,100,101,99,118,116,73,119,99,49,48,95,109,98,115,116,97,116,101,95,116,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,100,101,99,118,116,73,99,99,49,48,95,109,98,115,116,97,116,101,95,116,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,100,101,99,118,116,73,68,115,99,49,48,95,109,98,115,116,97,116,101,95,116,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,55,99,111,100,101,99,118,116,73,68,105,99,49,48,95,109,98,115,116,97,116,101,95,116,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,54,108,111,99,97,108,101,53,102,97,99,101,116,69,0,0,0,78,83,116,51,95,95,49,54,108,111,99,97,108,101,53,95,95,105,109,112,69,0,0,0,78,83,116,51,95,95,49,53,99,116,121,112,101,73,119,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,53,99,116,121,112,101,73,99,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,50,48,95,95,116,105,109,101,95,103,101,116,95,99,95,115,116,111,114,97,103,101,73,119,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,50,48,95,95,116,105,109,101,95,103,101,116,95,99,95,115,116,111,114,97,103,101,73,99,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,57,95,95,105,111,115,116,114,101,97,109,95,99,97,116,101,103,111,114,121,69,0,0,0,78,83,116,51,95,95,49,49,55,95,95,119,105,100,101,110,95,102,114,111,109,95,117,116,102,56,73,76,106,51,50,69,69,69,0,0,0,0,0,0,78,83,116,51,95,95,49,49,54,95,95,110,97,114,114,111,119,95,116,111,95,117,116,102,56,73,76,106,51,50,69,69,69,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,53,98,97,115,105,99,95,115,116,114,101,97,109,98,117,102,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,53,98,97,115,105,99,95,115,116,114,101,97,109,98,117,102,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,52,101,114,114,111,114,95,99,97,116,101,103,111,114,121,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,52,95,95,115,104,97,114,101,100,95,99,111,117,110,116,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,52,95,95,110,117,109,95,112,117,116,95,98,97,115,101,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,52,95,95,110,117,109,95,103,101,116,95,98,97,115,101,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,51,109,101,115,115,97,103,101,115,95,98,97,115,101,69,0,78,83,116,51,95,95,49,49,51,98,97,115,105,99,95,111,115,116,114,101,97,109,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,0,0,78,83,116,51,95,95,49,49,51,98,97,115,105,99,95,111,115,116,114,101,97,109,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,0,0,78,83,116,51,95,95,49,49,51,98,97,115,105,99,95,105,115,116,114,101,97,109,73,119,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,119,69,69,69,69,0,0,78,83,116,51,95,95,49,49,51,98,97,115,105,99,95,105,115,116,114,101,97,109,73,99,78,83,95,49,49,99,104,97,114,95,116,114,97,105,116,115,73,99,69,69,69,69,0,0,78,83,116,51,95,95,49,49,50,115,121,115,116,101,109,95,101,114,114,111,114,69,0,0,78,83,116,51,95,95,49,49,50,99,111,100,101,99,118,116,95,98,97,115,101,69,0,0,78,83,116,51,95,95,49,49,50,95,95,100,111,95,109,101,115,115,97,103,101,69,0,0,78,83,116,51,95,95,49,49,49,95,95,115,116,100,111,117,116,98,117,102,73,119,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,49,95,95,115,116,100,111,117,116,98,117,102,73,99,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,49,95,95,109,111,110,101,121,95,112,117,116,73,119,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,49,95,95,109,111,110,101,121,95,112,117,116,73,99,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,49,95,95,109,111,110,101,121,95,103,101,116,73,119,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,49,95,95,109,111,110,101,121,95,103,101,116,73,99,69,69,0,0,0,0,0,0,0,0,78,83,116,51,95,95,49,49,48,109,111,110,101,121,112,117,110,99,116,73,119,76,98,49,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,49,48,109,111,110,101,121,112,117,110,99,116,73,119,76,98,48,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,49,48,109,111,110,101,121,112,117,110,99,116,73,99,76,98,49,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,49,48,109,111,110,101,121,112,117,110,99,116,73,99,76,98,48,69,69,69,0,0,0,0,0,78,83,116,51,95,95,49,49,48,109,111,110,101,121,95,98,97,115,101,69,0,0,0,0,78,83,116,51,95,95,49,49,48,99,116,121,112,101,95,98,97,115,101,69,0,0,0,0,78,83,116,51,95,95,49,49,48,95,95,116,105,109,101,95,112,117,116,69,0,0,0,0,78,83,116,51,95,95,49,49,48,95,95,115,116,100,105,110,98,117,102,73,119,69,69,0,78,83,116,51,95,95,49,49,48,95,95,115,116,100,105,110,98,117,102,73,99,69,69,0,78,49,48,95,95,99,120,120,97,98,105,118,49,50,49,95,95,118,109,105,95,99,108,97,115,115,95,116,121,112,101,95,105,110,102,111,69,0,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,50,48,95,95,115,105,95,99,108,97,115,115,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,49,55,95,95,99,108,97,115,115,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,0,0,0,78,49,48,95,95,99,120,120,97,98,105,118,49,49,54,95,95,115,104,105,109,95,116,121,112,101,95,105,110,102,111,69,0,0,0,0,0,0,0,0,0,0,0,0,40,20,0,0,0,0,0,0,56,20,0,0,0,0,0,0,72,20,0,0,216,31,0,0,0,0,0,0,0,0,0,0,88,20,0,0,216,31,0,0,0,0,0,0,0,0,0,0,104,20,0,0,216,31,0,0,0,0,0,0,0,0,0,0,128,20,0,0,32,32,0,0,0,0,0,0,0,0,0,0,152,20,0,0,216,31,0,0,0,0,0,0,0,0,0,0,168,20,0,0,0,20,0,0,192,20,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,224,36,0,0,0,0,0,0,0,20,0,0,8,21,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,232,36,0,0,0,0,0,0,0,20,0,0,80,21,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,240,36,0,0,0,0,0,0,0,20,0,0,152,21,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,248,36,0,0,0,0,0,0,0,0,0,0,224,21,0,0,40,34,0,0,0,0,0,0,0,0,0,0,16,22,0,0,40,34,0,0,0,0,0,0,0,20,0,0,64,22,0,0,0,0,0,0,1,0,0,0,32,36,0,0,0,0,0,0,0,20,0,0,88,22,0,0,0,0,0,0,1,0,0,0,32,36,0,0,0,0,0,0,0,20,0,0,112,22,0,0,0,0,0,0,1,0,0,0,40,36,0,0,0,0,0,0,0,20,0,0,136,22,0,0,0,0,0,0,1,0,0,0,40,36,0,0,0,0,0,0,0,20,0,0,160,22,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,144,37,0,0,0,8,0,0,0,20,0,0,232,22,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,144,37,0,0,0,8,0,0,0,20,0,0,48,23,0,0,0,0,0,0,3,0,0,0,96,35,0,0,2,0,0,0,48,32,0,0,2,0,0,0,192,35,0,0,0,8,0,0,0,20,0,0,120,23,0,0,0,0,0,0,3,0,0,0,96,35,0,0,2,0,0,0,48,32,0,0,2,0,0,0,200,35,0,0,0,8,0,0,0,0,0,0,192,23,0,0,96,35,0,0,0,0,0,0,0,0,0,0,216,23,0,0,96,35,0,0,0,0,0,0,0,20,0,0,240,23,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,48,36,0,0,2,0,0,0,0,20,0,0,8,24,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,48,36,0,0,2,0,0,0,0,0,0,0,32,24,0,0,0,0,0,0,56,24,0,0,152,36,0,0,0,0,0,0,0,20,0,0,88,24,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,216,32,0,0,0,0,0,0,0,20,0,0,160,24,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,240,32,0,0,0,0,0,0,0,20,0,0,232,24,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,8,33,0,0,0,0,0,0,0,20,0,0,48,25,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,32,33,0,0,0,0,0,0,0,0,0,0,120,25,0,0,96,35,0,0,0,0,0,0,0,0,0,0,144,25,0,0,96,35,0,0,0,0,0,0,0,20,0,0,168,25,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,168,36,0,0,2,0,0,0,0,20,0,0,208,25,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,168,36,0,0,2,0,0,0,0,20,0,0,248,25,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,168,36,0,0,2,0,0,0,0,20,0,0,32,26,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,168,36,0,0,2,0,0,0,0,0,0,0,72,26,0,0,24,36,0,0,0,0,0,0,0,0,0,0,96,26,0,0,96,35,0,0,0,0,0,0,0,20,0,0,120,26,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,136,37,0,0,2,0,0,0,0,20,0,0,144,26,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,136,37,0,0,2,0,0,0,0,0,0,0,168,26,0,0,0,0,0,0,208,26,0,0,0,0,0,0,248,26,0,0,176,36,0,0,0,0,0,0,0,0,0,0,24,27,0,0,64,35,0,0,0,0,0,0,0,0,0,0,64,27,0,0,64,35,0,0,0,0,0,0,0,0,0,0,104,27,0,0,0,0,0,0,160,27,0,0,0,0,0,0,216,27,0,0,0,0,0,0,248,27,0,0,0,0,0,0,24,28,0,0,0,0,0,0,56,28,0,0,0,0,0,0,88,28,0,0,0,20,0,0,112,28,0,0,0,0,0,0,1,0,0,0,184,32,0,0,3,244,255,255,0,20,0,0,160,28,0,0,0,0,0,0,1,0,0,0,200,32,0,0,3,244,255,255,0,20,0,0,208,28,0,0,0,0,0,0,1,0,0,0,184,32,0,0,3,244,255,255,0,20,0,0,0,29,0,0,0,0,0,0,1,0,0,0,200,32,0,0,3,244,255,255,0,0,0,0,48,29,0,0,0,32,0,0,0,0,0,0,0,0,0,0,72,29,0,0,0,0,0,0,96,29,0,0,16,36,0,0,0,0,0,0,0,0,0,0,120,29,0,0,0,36,0,0,0,0,0,0,0,0,0,0,152,29,0,0,8,36,0,0,0,0,0,0,0,0,0,0,184,29,0,0,0,0,0,0,216,29,0,0,0,0,0,0,248,29,0,0,0,0,0,0,24,30,0,0,0,20,0,0,56,30,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,128,37,0,0,2,0,0,0,0,20,0,0,88,30,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,128,37,0,0,2,0,0,0,0,20,0,0,120,30,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,128,37,0,0,2,0,0,0,0,20,0,0,152,30,0,0,0,0,0,0,2,0,0,0,96,35,0,0,2,0,0,0,128,37,0,0,2,0,0,0,0,0,0,0,184,30,0,0,0,0,0,0,208,30,0,0,0,0,0,0,232,30,0,0,0,0,0,0,0,31,0,0,0,36,0,0,0,0,0,0,0,0,0,0,24,31,0,0,8,36,0,0,0,0,0,0,0,0,0,0,48,31,0,0,216,37,0,0,0,0,0,0,0,0,0,0,88,31,0,0,216,37,0,0,0,0,0,0,0,0,0,0,128,31,0,0,232,37,0,0,0,0,0,0,0,0,0,0,168,31,0,0,208,31,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,48,49,50,51,52,53,54,55,56,57,97,98,99,100,101,102,65,66,67,68,69,70,120,88,43,45,112,80,105,73,110,78,0,0,0,0,0,0,0,0], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE)
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
  function _atexit(func, arg) {
      __ATEXIT__.unshift({ func: func, arg: arg });
    }var ___cxa_atexit=_atexit;
  function _llvm_umul_with_overflow_i32(x, y) {
      x = x>>>0;
      y = y>>>0;
      return ((asm["setTempRet0"](x*y > 4294967295),(x*y)>>>0)|0);
    }
  function ___gxx_personality_v0() {
    }
  function __exit(status) {
      // void _exit(int status);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/exit.html
      Module.print('exit(' + status + ') called');
      Module['exit'](status);
    }function _exit(status) {
      __exit(status);
    }function __ZSt9terminatev() {
      _exit(-1234);
    }
  Module["_memcpy"] = _memcpy;var _llvm_memcpy_p0i8_p0i8_i32=_memcpy;
  var _log=Math.log;
  var _floor=Math.floor;
  var ctlz_i8 = allocate([8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0], "i8", ALLOC_STATIC); 
  Module["_llvm_ctlz_i32"] = _llvm_ctlz_i32;
  function __ZSt18uncaught_exceptionv() { // std::uncaught_exception()
      return !!__ZSt18uncaught_exceptionv.uncaught_exception;
    }function ___cxa_begin_catch(ptr) {
      __ZSt18uncaught_exceptionv.uncaught_exception--;
      return ptr;
    }
  function _llvm_eh_exception() {
      return HEAP32[((_llvm_eh_exception.buf)>>2)];
    }
  function ___cxa_free_exception(ptr) {
      try {
        return _free(ptr);
      } catch(e) { // XXX FIXME
      }
    }function ___cxa_end_catch() {
      if (___cxa_end_catch.rethrown) {
        ___cxa_end_catch.rethrown = false;
        return;
      }
      // Clear state flag.
      asm['setThrew'](0);
      // Clear type.
      HEAP32[(((_llvm_eh_exception.buf)+(4))>>2)]=0
      // Call destructor if one is registered then clear it.
      var ptr = HEAP32[((_llvm_eh_exception.buf)>>2)];
      var destructor = HEAP32[(((_llvm_eh_exception.buf)+(8))>>2)];
      if (destructor) {
        Runtime.dynCall('vi', destructor, [ptr]);
        HEAP32[(((_llvm_eh_exception.buf)+(8))>>2)]=0
      }
      // Free ptr if it isn't null.
      if (ptr) {
        ___cxa_free_exception(ptr);
        HEAP32[((_llvm_eh_exception.buf)>>2)]=0
      }
    }function ___cxa_rethrow() {
      ___cxa_end_catch.rethrown = true;
      throw HEAP32[((_llvm_eh_exception.buf)>>2)] + " - Exception catching is disabled, this exception cannot be caught. Compile with -s DISABLE_EXCEPTION_CATCHING=0 or DISABLE_EXCEPTION_CATCHING=2 to catch.";;
    }
  Module["_memmove"] = _memmove;var _llvm_memmove_p0i8_p0i8_i32=_memmove;
  Module["_memset"] = _memset;var _llvm_memset_p0i8_i64=_memset;
  function _llvm_lifetime_start() {}
  function _llvm_lifetime_end() {}
  function _pthread_mutex_lock() {}
  function _pthread_mutex_unlock() {}
  function ___cxa_guard_acquire(variable) {
      if (!HEAP8[(variable)]) { // ignore SAFE_HEAP stuff because llvm mixes i64 and i8 here
        HEAP8[(variable)]=1;
        return 1;
      }
      return 0;
    }
  function ___cxa_guard_release() {}
  function _pthread_cond_broadcast() {
      return 0;
    }
  function _pthread_cond_wait() {
      return 0;
    }
  function ___cxa_allocate_exception(size) {
      return _malloc(size);
    }
  function ___cxa_is_number_type(type) {
      var isNumber = false;
      try { if (type == __ZTIi) isNumber = true } catch(e){}
      try { if (type == __ZTIj) isNumber = true } catch(e){}
      try { if (type == __ZTIl) isNumber = true } catch(e){}
      try { if (type == __ZTIm) isNumber = true } catch(e){}
      try { if (type == __ZTIx) isNumber = true } catch(e){}
      try { if (type == __ZTIy) isNumber = true } catch(e){}
      try { if (type == __ZTIf) isNumber = true } catch(e){}
      try { if (type == __ZTId) isNumber = true } catch(e){}
      try { if (type == __ZTIe) isNumber = true } catch(e){}
      try { if (type == __ZTIc) isNumber = true } catch(e){}
      try { if (type == __ZTIa) isNumber = true } catch(e){}
      try { if (type == __ZTIh) isNumber = true } catch(e){}
      try { if (type == __ZTIs) isNumber = true } catch(e){}
      try { if (type == __ZTIt) isNumber = true } catch(e){}
      return isNumber;
    }function ___cxa_does_inherit(definiteType, possibilityType, possibility) {
      if (possibility == 0) return false;
      if (possibilityType == 0 || possibilityType == definiteType)
        return true;
      var possibility_type_info;
      if (___cxa_is_number_type(possibilityType)) {
        possibility_type_info = possibilityType;
      } else {
        var possibility_type_infoAddr = HEAP32[((possibilityType)>>2)] - 8;
        possibility_type_info = HEAP32[((possibility_type_infoAddr)>>2)];
      }
      switch (possibility_type_info) {
      case 0: // possibility is a pointer
        // See if definite type is a pointer
        var definite_type_infoAddr = HEAP32[((definiteType)>>2)] - 8;
        var definite_type_info = HEAP32[((definite_type_infoAddr)>>2)];
        if (definite_type_info == 0) {
          // Also a pointer; compare base types of pointers
          var defPointerBaseAddr = definiteType+8;
          var defPointerBaseType = HEAP32[((defPointerBaseAddr)>>2)];
          var possPointerBaseAddr = possibilityType+8;
          var possPointerBaseType = HEAP32[((possPointerBaseAddr)>>2)];
          return ___cxa_does_inherit(defPointerBaseType, possPointerBaseType, possibility);
        } else
          return false; // one pointer and one non-pointer
      case 1: // class with no base class
        return false;
      case 2: // class with base class
        var parentTypeAddr = possibilityType + 8;
        var parentType = HEAP32[((parentTypeAddr)>>2)];
        return ___cxa_does_inherit(definiteType, parentType, possibility);
      default:
        return false; // some unencountered type
      }
    }
  function ___resumeException(ptr) {
      if (HEAP32[((_llvm_eh_exception.buf)>>2)] == 0) HEAP32[((_llvm_eh_exception.buf)>>2)]=ptr;
      throw ptr + " - Exception catching is disabled, this exception cannot be caught. Compile with -s DISABLE_EXCEPTION_CATCHING=0 or DISABLE_EXCEPTION_CATCHING=2 to catch.";;
    }function ___cxa_find_matching_catch(thrown, throwntype) {
      if (thrown == -1) thrown = HEAP32[((_llvm_eh_exception.buf)>>2)];
      if (throwntype == -1) throwntype = HEAP32[(((_llvm_eh_exception.buf)+(4))>>2)];
      var typeArray = Array.prototype.slice.call(arguments, 2);
      // If throwntype is a pointer, this means a pointer has been
      // thrown. When a pointer is thrown, actually what's thrown
      // is a pointer to the pointer. We'll dereference it.
      if (throwntype != 0 && !___cxa_is_number_type(throwntype)) {
        var throwntypeInfoAddr= HEAP32[((throwntype)>>2)] - 8;
        var throwntypeInfo= HEAP32[((throwntypeInfoAddr)>>2)];
        if (throwntypeInfo == 0)
          thrown = HEAP32[((thrown)>>2)];
      }
      // The different catch blocks are denoted by different types.
      // Due to inheritance, those types may not precisely match the
      // type of the thrown object. Find one which matches, and
      // return the type of the catch block which should be called.
      for (var i = 0; i < typeArray.length; i++) {
        if (___cxa_does_inherit(typeArray[i], throwntype, thrown))
          return ((asm["setTempRet0"](typeArray[i]),thrown)|0);
      }
      // Shouldn't happen unless we have bogus data in typeArray
      // or encounter a type for which emscripten doesn't have suitable
      // typeinfo defined. Best-efforts match just in case.
      return ((asm["setTempRet0"](throwntype),thrown)|0);
    }function ___cxa_throw(ptr, type, destructor) {
      if (!___cxa_throw.initialized) {
        try {
          HEAP32[((__ZTVN10__cxxabiv119__pointer_type_infoE)>>2)]=0; // Workaround for libcxxabi integration bug
        } catch(e){}
        try {
          HEAP32[((__ZTVN10__cxxabiv117__class_type_infoE)>>2)]=1; // Workaround for libcxxabi integration bug
        } catch(e){}
        try {
          HEAP32[((__ZTVN10__cxxabiv120__si_class_type_infoE)>>2)]=2; // Workaround for libcxxabi integration bug
        } catch(e){}
        ___cxa_throw.initialized = true;
      }
      HEAP32[((_llvm_eh_exception.buf)>>2)]=ptr
      HEAP32[(((_llvm_eh_exception.buf)+(4))>>2)]=type
      HEAP32[(((_llvm_eh_exception.buf)+(8))>>2)]=destructor
      if (!("uncaught_exception" in __ZSt18uncaught_exceptionv)) {
        __ZSt18uncaught_exceptionv.uncaught_exception = 1;
      } else {
        __ZSt18uncaught_exceptionv.uncaught_exception++;
      }
      throw ptr + " - Exception catching is disabled, this exception cannot be caught. Compile with -s DISABLE_EXCEPTION_CATCHING=0 or DISABLE_EXCEPTION_CATCHING=2 to catch.";;
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
  function _ungetc(c, stream) {
      // int ungetc(int c, FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/ungetc.html
      stream = FS.getStream(stream);
      if (!stream) {
        return -1;
      }
      if (c === -1) {
        // do nothing for EOF character
        return c;
      }
      c = unSign(c & 0xFF);
      stream.ungotten.push(c);
      stream.eof = false;
      return c;
    }
  function _recv(fd, buf, len, flags) {
      var info = FS.getStream(fd);
      if (!info) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      if (!info.hasData()) {
        if (info.socket.readyState === WebSocket.CLOSING || info.socket.readyState === WebSocket.CLOSED) {
          // socket has closed
          return 0;
        } else {
          // else, our socket is in a valid state but truly has nothing available
          ___setErrNo(ERRNO_CODES.EAGAIN);
          return -1;
        }
      }
      var buffer = info.inQueue.shift();
      if (len < buffer.length) {
        if (info.stream) {
          // This is tcp (reliable), so if not all was read, keep it
          info.inQueue.unshift(buffer.subarray(len));
        }
        buffer = buffer.subarray(0, len);
      }
      HEAPU8.set(buffer, buf);
      return buffer.length;
    }
  function _pread(fildes, buf, nbyte, offset) {
      // ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/read.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      try {
        var slab = HEAP8;
        return FS.read(stream, slab, buf, nbyte, offset);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }function _read(fildes, buf, nbyte) {
      // ssize_t read(int fildes, void *buf, size_t nbyte);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/read.html
      var stream = FS.getStream(fildes);
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
      if (stream && ('socket' in stream)) {
        return _recv(fildes, buf, nbyte, 0);
      }
      try {
        var slab = HEAP8;
        return FS.read(stream, slab, buf, nbyte);
      } catch (e) {
        FS.handleFSError(e);
        return -1;
      }
    }function _fread(ptr, size, nitems, stream) {
      // size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fread.html
      var bytesToRead = nitems * size;
      if (bytesToRead == 0) {
        return 0;
      }
      var bytesRead = 0;
      var streamObj = FS.getStream(stream);
      while (streamObj.ungotten.length && bytesToRead > 0) {
        HEAP8[((ptr++)|0)]=streamObj.ungotten.pop()
        bytesToRead--;
        bytesRead++;
      }
      var err = _read(stream, ptr, bytesToRead);
      if (err == -1) {
        if (streamObj) streamObj.error = true;
        return 0;
      }
      bytesRead += err;
      if (bytesRead < bytesToRead) streamObj.eof = true;
      return Math.floor(bytesRead / size);
    }function _fgetc(stream) {
      // int fgetc(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fgetc.html
      var streamObj = FS.getStream(stream);
      if (!streamObj) return -1;
      if (streamObj.eof || streamObj.error) return -1;
      var ret = _fread(_fgetc.ret, 1, 1, stream);
      if (ret == 0) {
        streamObj.eof = true;
        return -1;
      } else if (ret == -1) {
        streamObj.error = true;
        return -1;
      } else {
        return HEAPU8[((_fgetc.ret)|0)];
      }
    }var _getc=_fgetc;
  function ___cxa_pure_virtual() {
      ABORT = true;
      throw 'Pure virtual function called!';
    }
  function ___errno_location() {
      return ___errno_state;
    }var ___errno=___errno_location;
  var _llvm_memset_p0i8_i32=_memset;
  Module["_strlen"] = _strlen;
  function _strerror_r(errnum, strerrbuf, buflen) {
      if (errnum in ERRNO_MESSAGES) {
        if (ERRNO_MESSAGES[errnum].length > buflen - 1) {
          return ___setErrNo(ERRNO_CODES.ERANGE);
        } else {
          var msg = ERRNO_MESSAGES[errnum];
          for (var i = 0; i < msg.length; i++) {
            HEAP8[(((strerrbuf)+(i))|0)]=msg.charCodeAt(i)
          }
          HEAP8[(((strerrbuf)+(i))|0)]=0
          return 0;
        }
      } else {
        return ___setErrNo(ERRNO_CODES.EINVAL);
      }
    }function _strerror(errnum) {
      if (!_strerror.buffer) _strerror.buffer = _malloc(256);
      _strerror_r(errnum, _strerror.buffer, 256);
      return _strerror.buffer;
    }
  function _abort() {
      Module['abort']();
    }
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
              var origArg = currArg;
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
                if (argSize == 8 && i64Math) argText = i64Math.stringify(origArg[0], origArg[1], null); else
                argText = reSign(currArg, 8 * argSize, 1).toString(10);
              } else if (next == 117) {
                if (argSize == 8 && i64Math) argText = i64Math.stringify(origArg[0], origArg[1], true); else
                argText = unSign(currArg, 8 * argSize, 1).toString(10);
                currArg = Math.abs(currArg);
              } else if (next == 111) {
                argText = (flagAlternative ? '0' : '') + currAbsArg.toString(8);
              } else if (next == 120 || next == 88) {
                prefix = (flagAlternative && currArg != 0) ? '0x' : '';
                if (argSize == 8 && i64Math) {
                  if (origArg[1]) {
                    argText = (origArg[1]>>>0).toString(16);
                    var lower = (origArg[0]>>>0).toString(16);
                    while (lower.length < 8) lower = '0' + lower;
                    argText += lower;
                  } else {
                    argText = (origArg[0]>>>0).toString(16);
                  }
                } else
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
    }function _snprintf(s, n, format, varargs) {
      // int snprintf(char *restrict s, size_t n, const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var result = __formatString(format, varargs);
      var limit = (n === undefined) ? result.length
                                    : Math.min(result.length, Math.max(n - 1, 0));
      if (s < 0) {
        s = -s;
        var buf = _malloc(limit+1);
        HEAP32[((s)>>2)]=buf;
        s = buf;
      }
      for (var i = 0; i < limit; i++) {
        HEAP8[(((s)+(i))|0)]=result[i];
      }
      if (limit < n || (n === undefined)) HEAP8[(((s)+(i))|0)]=0;
      return result.length;
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
  function ___cxa_guard_abort() {}
  function _isxdigit(chr) {
      return (chr >= 48 && chr <= 57) ||
             (chr >= 97 && chr <= 102) ||
             (chr >= 65 && chr <= 70);
    }var _isxdigit_l=_isxdigit;
  function _isdigit(chr) {
      return chr >= 48 && chr <= 57;
    }var _isdigit_l=_isdigit;
  function __isFloat(text) {
      return !!(/^[+-]?[0-9]*\.?[0-9]+([eE][+-]?[0-9]+)?$/.exec(text));
    }function __scanString(format, get, unget, varargs) {
      if (!__scanString.whiteSpace) {
        __scanString.whiteSpace = {};
        __scanString.whiteSpace[32] = 1;
        __scanString.whiteSpace[9] = 1;
        __scanString.whiteSpace[10] = 1;
        __scanString.whiteSpace[11] = 1;
        __scanString.whiteSpace[12] = 1;
        __scanString.whiteSpace[13] = 1;
        __scanString.whiteSpace[' '] = 1;
        __scanString.whiteSpace['\t'] = 1;
        __scanString.whiteSpace['\n'] = 1;
        __scanString.whiteSpace['\v'] = 1;
        __scanString.whiteSpace['\f'] = 1;
        __scanString.whiteSpace['\r'] = 1;
      }
      // Supports %x, %4x, %d.%d, %lld, %s, %f, %lf.
      // TODO: Support all format specifiers.
      format = Pointer_stringify(format);
      var soFar = 0;
      if (format.indexOf('%n') >= 0) {
        // need to track soFar
        var _get = get;
        get = function() {
          soFar++;
          return _get();
        }
        var _unget = unget;
        unget = function() {
          soFar--;
          return _unget();
        }
      }
      var formatIndex = 0;
      var argsi = 0;
      var fields = 0;
      var argIndex = 0;
      var next;
      mainLoop:
      for (var formatIndex = 0; formatIndex < format.length;) {
        if (format[formatIndex] === '%' && format[formatIndex+1] == 'n') {
          var argPtr = HEAP32[(((varargs)+(argIndex))>>2)];
          argIndex += Runtime.getAlignSize('void*', null, true);
          HEAP32[((argPtr)>>2)]=soFar;
          formatIndex += 2;
          continue;
        }
        if (format[formatIndex] === '%') {
          var nextC = format.indexOf('c', formatIndex+1);
          if (nextC > 0) {
            var maxx = 1;
            if (nextC > formatIndex+1) {
              var sub = format.substring(formatIndex+1, nextC)
              maxx = parseInt(sub);
              if (maxx != sub) maxx = 0;
            }
            if (maxx) {
              var argPtr = HEAP32[(((varargs)+(argIndex))>>2)];
              argIndex += Runtime.getAlignSize('void*', null, true);
              fields++;
              for (var i = 0; i < maxx; i++) {
                next = get();
                HEAP8[((argPtr++)|0)]=next;
              }
              formatIndex += nextC - formatIndex + 1;
              continue;
            }
          }
        }
        // remove whitespace
        while (1) {
          next = get();
          if (next == 0) return fields;
          if (!(next in __scanString.whiteSpace)) break;
        }
        unget();
        if (format[formatIndex] === '%') {
          formatIndex++;
          var suppressAssignment = false;
          if (format[formatIndex] == '*') {
            suppressAssignment = true;
            formatIndex++;
          }
          var maxSpecifierStart = formatIndex;
          while (format[formatIndex].charCodeAt(0) >= 48 &&
                 format[formatIndex].charCodeAt(0) <= 57) {
            formatIndex++;
          }
          var max_;
          if (formatIndex != maxSpecifierStart) {
            max_ = parseInt(format.slice(maxSpecifierStart, formatIndex), 10);
          }
          var long_ = false;
          var half = false;
          var longLong = false;
          if (format[formatIndex] == 'l') {
            long_ = true;
            formatIndex++;
            if (format[formatIndex] == 'l') {
              longLong = true;
              formatIndex++;
            }
          } else if (format[formatIndex] == 'h') {
            half = true;
            formatIndex++;
          }
          var type = format[formatIndex];
          formatIndex++;
          var curr = 0;
          var buffer = [];
          // Read characters according to the format. floats are trickier, they may be in an unfloat state in the middle, then be a valid float later
          if (type == 'f' || type == 'e' || type == 'g' ||
              type == 'F' || type == 'E' || type == 'G') {
            var last = 0;
            next = get();
            while (next > 0) {
              buffer.push(String.fromCharCode(next));
              if (__isFloat(buffer.join(''))) {
                last = buffer.length;
              }
              next = get();
            }
            for (var i = 0; i < buffer.length - last + 1; i++) {
              unget();
            }
            buffer.length = last;
          } else {
            next = get();
            var first = true;
            while ((curr < max_ || isNaN(max_)) && next > 0) {
              if (!(next in __scanString.whiteSpace) && // stop on whitespace
                  (type == 's' ||
                   ((type === 'd' || type == 'u' || type == 'i') && ((next >= 48 && next <= 57) ||
                                                                     (first && next == 45))) ||
                   ((type === 'x' || type === 'X') && (next >= 48 && next <= 57 ||
                                     next >= 97 && next <= 102 ||
                                     next >= 65 && next <= 70))) &&
                  (formatIndex >= format.length || next !== format[formatIndex].charCodeAt(0))) { // Stop when we read something that is coming up
                buffer.push(String.fromCharCode(next));
                next = get();
                curr++;
                first = false;
              } else {
                break;
              }
            }
            unget();
          }
          if (buffer.length === 0) return 0;  // Failure.
          if (suppressAssignment) continue;
          var text = buffer.join('');
          var argPtr = HEAP32[(((varargs)+(argIndex))>>2)];
          argIndex += Runtime.getAlignSize('void*', null, true);
          switch (type) {
            case 'd': case 'u': case 'i':
              if (half) {
                HEAP16[((argPtr)>>1)]=parseInt(text, 10);
              } else if (longLong) {
                (tempI64 = [parseInt(text, 10)>>>0,(tempDouble=parseInt(text, 10),(+(Math.abs(tempDouble))) >= (+(1)) ? (tempDouble > (+(0)) ? ((Math.min((+(Math.floor((tempDouble)/(+(4294967296))))), (+(4294967295))))|0)>>>0 : (~~((+(Math.ceil((tempDouble - +(((~~(tempDouble)))>>>0))/(+(4294967296)))))))>>>0) : 0)],HEAP32[((argPtr)>>2)]=tempI64[0],HEAP32[(((argPtr)+(4))>>2)]=tempI64[1]);
              } else {
                HEAP32[((argPtr)>>2)]=parseInt(text, 10);
              }
              break;
            case 'X':
            case 'x':
              HEAP32[((argPtr)>>2)]=parseInt(text, 16)
              break;
            case 'F':
            case 'f':
            case 'E':
            case 'e':
            case 'G':
            case 'g':
            case 'E':
              // fallthrough intended
              if (long_) {
                HEAPF64[((argPtr)>>3)]=parseFloat(text)
              } else {
                HEAPF32[((argPtr)>>2)]=parseFloat(text)
              }
              break;
            case 's':
              var array = intArrayFromString(text);
              for (var j = 0; j < array.length; j++) {
                HEAP8[(((argPtr)+(j))|0)]=array[j]
              }
              break;
          }
          fields++;
        } else if (format[formatIndex] in __scanString.whiteSpace) {
          next = get();
          while (next in __scanString.whiteSpace) {
            if (next <= 0) break mainLoop;  // End of input.
            next = get();
          }
          unget(next);
          formatIndex++;
        } else {
          // Not a specifier.
          next = get();
          if (format[formatIndex].charCodeAt(0) !== next) {
            unget(next);
            break mainLoop;
          }
          formatIndex++;
        }
      }
      return fields;
    }function _sscanf(s, format, varargs) {
      // int sscanf(const char *restrict s, const char *restrict format, ... );
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/scanf.html
      var index = 0;
      var get = function() { return HEAP8[(((s)+(index++))|0)]; };
      var unget = function() { index--; };
      return __scanString(format, get, unget, varargs);
    }
  function __Z7catopenPKci() { throw 'catopen not implemented' }
  function __Z7catgetsP8_nl_catdiiPKc() { throw 'catgets not implemented' }
  function __Z8catcloseP8_nl_catd() { throw 'catclose not implemented' }
  function _newlocale(mask, locale, base) {
      return 0;
    }
  function _freelocale(locale) {}
  function ___ctype_b_loc() {
      // http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/baselib---ctype-b-loc.html
      var me = ___ctype_b_loc;
      if (!me.ret) {
        var values = [
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,8195,8194,8194,8194,8194,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,24577,49156,49156,49156,
          49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,55304,55304,55304,55304,55304,55304,55304,55304,
          55304,55304,49156,49156,49156,49156,49156,49156,49156,54536,54536,54536,54536,54536,54536,50440,50440,50440,50440,50440,
          50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,49156,49156,49156,49156,49156,
          49156,54792,54792,54792,54792,54792,54792,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,
          50696,50696,50696,50696,50696,50696,50696,49156,49156,49156,49156,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        ];
        var i16size = 2;
        var arr = _malloc(values.length * i16size);
        for (var i = 0; i < values.length; i++) {
          HEAP16[(((arr)+(i * i16size))>>1)]=values[i]
        }
        me.ret = allocate([arr + 128 * i16size], 'i16*', ALLOC_NORMAL);
      }
      return me.ret;
    }
  function ___ctype_tolower_loc() {
      // http://refspecs.freestandards.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/libutil---ctype-tolower-loc.html
      var me = ___ctype_tolower_loc;
      if (!me.ret) {
        var values = [
          128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,
          158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,
          188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,
          218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,
          248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
          33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,
          104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,
          104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,
          134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,
          164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,
          194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
          224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,
          254,255
        ];
        var i32size = 4;
        var arr = _malloc(values.length * i32size);
        for (var i = 0; i < values.length; i++) {
          HEAP32[(((arr)+(i * i32size))>>2)]=values[i]
        }
        me.ret = allocate([arr + 128 * i32size], 'i32*', ALLOC_NORMAL);
      }
      return me.ret;
    }
  function ___ctype_toupper_loc() {
      // http://refspecs.freestandards.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/libutil---ctype-toupper-loc.html
      var me = ___ctype_toupper_loc;
      if (!me.ret) {
        var values = [
          128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,
          158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,
          188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,
          218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,
          248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
          33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,
          73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
          81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,
          145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,
          175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,
          205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,
          235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
        ];
        var i32size = 4;
        var arr = _malloc(values.length * i32size);
        for (var i = 0; i < values.length; i++) {
          HEAP32[(((arr)+(i * i32size))>>2)]=values[i]
        }
        me.ret = allocate([arr + 128 * i32size], 'i32*', ALLOC_NORMAL);
      }
      return me.ret;
    }
  var ___tm_struct_layout={__size__:44,tm_sec:0,tm_min:4,tm_hour:8,tm_mday:12,tm_mon:16,tm_year:20,tm_wday:24,tm_yday:28,tm_isdst:32,tm_gmtoff:36,tm_zone:40};
  function __isLeapYear(year) {
        return year%4 === 0 && (year%100 !== 0 || year%400 === 0);
    }
  function __arraySum(array, index) {
      var sum = 0;
      for (var i = 0; i <= index; sum += array[i++]);
      return sum;
    }
  var __MONTH_DAYS_LEAP=[31,29,31,30,31,30,31,31,30,31,30,31];
  var __MONTH_DAYS_REGULAR=[31,28,31,30,31,30,31,31,30,31,30,31];function __addDays(date, days) {
      var newDate = new Date(date.getTime());
      while(days > 0) {
        var leap = __isLeapYear(newDate.getFullYear());
        var currentMonth = newDate.getMonth();
        var daysInCurrentMonth = (leap ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR)[currentMonth];
        if (days > daysInCurrentMonth-newDate.getDate()) {
          // we spill over to next month
          days -= (daysInCurrentMonth-newDate.getDate()+1);
          newDate.setDate(1);
          if (currentMonth < 11) {
            newDate.setMonth(currentMonth+1)
          } else {
            newDate.setMonth(0);
            newDate.setFullYear(newDate.getFullYear()+1);
          }
        } else {
          // we stay in current month 
          newDate.setDate(newDate.getDate()+days);
          return newDate;
        }
      }
      return newDate;
    }function _strftime(s, maxsize, format, tm) {
      // size_t strftime(char *restrict s, size_t maxsize, const char *restrict format, const struct tm *restrict timeptr);
      // http://pubs.opengroup.org/onlinepubs/009695399/functions/strftime.html
      var date = {
        tm_sec: HEAP32[(((tm)+(___tm_struct_layout.tm_sec))>>2)],
        tm_min: HEAP32[(((tm)+(___tm_struct_layout.tm_min))>>2)],
        tm_hour: HEAP32[(((tm)+(___tm_struct_layout.tm_hour))>>2)],
        tm_mday: HEAP32[(((tm)+(___tm_struct_layout.tm_mday))>>2)],
        tm_mon: HEAP32[(((tm)+(___tm_struct_layout.tm_mon))>>2)],
        tm_year: HEAP32[(((tm)+(___tm_struct_layout.tm_year))>>2)],
        tm_wday: HEAP32[(((tm)+(___tm_struct_layout.tm_wday))>>2)],
        tm_yday: HEAP32[(((tm)+(___tm_struct_layout.tm_yday))>>2)],
        tm_isdst: HEAP32[(((tm)+(___tm_struct_layout.tm_isdst))>>2)]
      };
      var pattern = Pointer_stringify(format);
      // expand format
      var EXPANSION_RULES_1 = {
        '%c': '%a %b %d %H:%M:%S %Y',     // Replaced by the locale's appropriate date and time representation - e.g., Mon Aug  3 14:02:01 2013
        '%D': '%m/%d/%y',                 // Equivalent to %m / %d / %y
        '%F': '%Y-%m-%d',                 // Equivalent to %Y - %m - %d
        '%h': '%b',                       // Equivalent to %b
        '%r': '%I:%M:%S %p',              // Replaced by the time in a.m. and p.m. notation
        '%R': '%H:%M',                    // Replaced by the time in 24-hour notation
        '%T': '%H:%M:%S',                 // Replaced by the time
        '%x': '%m/%d/%y',                 // Replaced by the locale's appropriate date representation
        '%X': '%H:%M:%S',                 // Replaced by the locale's appropriate date representation
      };
      for (var rule in EXPANSION_RULES_1) {
        pattern = pattern.replace(new RegExp(rule, 'g'), EXPANSION_RULES_1[rule]);
      }
      var WEEKDAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
      var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
      var leadingSomething = function(value, digits, character) {
        var str = typeof value === 'number' ? value.toString() : (value || '');
        while (str.length < digits) {
          str = character[0]+str;
        }
        return str;
      };
      var leadingNulls = function(value, digits) {
        return leadingSomething(value, digits, '0');
      };
      var compareByDay = function(date1, date2) {
        var sgn = function(value) {
          return value < 0 ? -1 : (value > 0 ? 1 : 0);
        };
        var compare;
        if ((compare = sgn(date1.getFullYear()-date2.getFullYear())) === 0) {
          if ((compare = sgn(date1.getMonth()-date2.getMonth())) === 0) {
            compare = sgn(date1.getDate()-date2.getDate());
          }
        }
        return compare;
      };
      var getFirstWeekStartDate = function(janFourth) {
          switch (janFourth.getDay()) {
            case 0: // Sunday
              return new Date(janFourth.getFullYear()-1, 11, 29);
            case 1: // Monday
              return janFourth;
            case 2: // Tuesday
              return new Date(janFourth.getFullYear(), 0, 3);
            case 3: // Wednesday
              return new Date(janFourth.getFullYear(), 0, 2);
            case 4: // Thursday
              return new Date(janFourth.getFullYear(), 0, 1);
            case 5: // Friday
              return new Date(janFourth.getFullYear()-1, 11, 31);
            case 6: // Saturday
              return new Date(janFourth.getFullYear()-1, 11, 30);
          }
      };
      var getWeekBasedYear = function(date) {
          var thisDate = __addDays(new Date(date.tm_year+1900, 0, 1), date.tm_yday);
          var janFourthThisYear = new Date(thisDate.getFullYear(), 0, 4);
          var janFourthNextYear = new Date(thisDate.getFullYear()+1, 0, 4);
          var firstWeekStartThisYear = getFirstWeekStartDate(janFourthThisYear);
          var firstWeekStartNextYear = getFirstWeekStartDate(janFourthNextYear);
          if (compareByDay(firstWeekStartThisYear, thisDate) <= 0) {
            // this date is after the start of the first week of this year
            if (compareByDay(firstWeekStartNextYear, thisDate) <= 0) {
              return thisDate.getFullYear()+1;
            } else {
              return thisDate.getFullYear();
            }
          } else { 
            return thisDate.getFullYear()-1;
          }
      };
      var EXPANSION_RULES_2 = {
        '%a': function(date) {
          return WEEKDAYS[date.tm_wday].substring(0,3);
        },
        '%A': function(date) {
          return WEEKDAYS[date.tm_wday];
        },
        '%b': function(date) {
          return MONTHS[date.tm_mon].substring(0,3);
        },
        '%B': function(date) {
          return MONTHS[date.tm_mon];
        },
        '%C': function(date) {
          var year = date.tm_year+1900;
          return leadingNulls(Math.floor(year/100),2);
        },
        '%d': function(date) {
          return leadingNulls(date.tm_mday, 2);
        },
        '%e': function(date) {
          return leadingSomething(date.tm_mday, 2, ' ');
        },
        '%g': function(date) {
          // %g, %G, and %V give values according to the ISO 8601:2000 standard week-based year. 
          // In this system, weeks begin on a Monday and week 1 of the year is the week that includes 
          // January 4th, which is also the week that includes the first Thursday of the year, and 
          // is also the first week that contains at least four days in the year. 
          // If the first Monday of January is the 2nd, 3rd, or 4th, the preceding days are part of 
          // the last week of the preceding year; thus, for Saturday 2nd January 1999, 
          // %G is replaced by 1998 and %V is replaced by 53. If December 29th, 30th, 
          // or 31st is a Monday, it and any following days are part of week 1 of the following year. 
          // Thus, for Tuesday 30th December 1997, %G is replaced by 1998 and %V is replaced by 01.
          return getWeekBasedYear(date).toString().substring(2);
        },
        '%G': function(date) {
          return getWeekBasedYear(date);
        },
        '%H': function(date) {
          return leadingNulls(date.tm_hour, 2);
        },
        '%I': function(date) {
          return leadingNulls(date.tm_hour < 13 ? date.tm_hour : date.tm_hour-12, 2);
        },
        '%j': function(date) {
          // Day of the year (001-366)
          return leadingNulls(date.tm_mday+__arraySum(__isLeapYear(date.tm_year+1900) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, date.tm_mon-1), 3);
        },
        '%m': function(date) {
          return leadingNulls(date.tm_mon+1, 2);
        },
        '%M': function(date) {
          return leadingNulls(date.tm_min, 2);
        },
        '%n': function() {
          return '\n';
        },
        '%p': function(date) {
          if (date.tm_hour > 0 && date.tm_hour < 13) {
            return 'AM';
          } else {
            return 'PM';
          }
        },
        '%S': function(date) {
          return leadingNulls(date.tm_sec, 2);
        },
        '%t': function() {
          return '\t';
        },
        '%u': function(date) {
          var day = new Date(date.tm_year+1900, date.tm_mon+1, date.tm_mday, 0, 0, 0, 0);
          return day.getDay() || 7;
        },
        '%U': function(date) {
          // Replaced by the week number of the year as a decimal number [00,53]. 
          // The first Sunday of January is the first day of week 1; 
          // days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday]
          var janFirst = new Date(date.tm_year+1900, 0, 1);
          var firstSunday = janFirst.getDay() === 0 ? janFirst : __addDays(janFirst, 7-janFirst.getDay());
          var endDate = new Date(date.tm_year+1900, date.tm_mon, date.tm_mday);
          // is target date after the first Sunday?
          if (compareByDay(firstSunday, endDate) < 0) {
            // calculate difference in days between first Sunday and endDate
            var februaryFirstUntilEndMonth = __arraySum(__isLeapYear(endDate.getFullYear()) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, endDate.getMonth()-1)-31;
            var firstSundayUntilEndJanuary = 31-firstSunday.getDate();
            var days = firstSundayUntilEndJanuary+februaryFirstUntilEndMonth+endDate.getDate();
            return leadingNulls(Math.ceil(days/7), 2);
          }
          return compareByDay(firstSunday, janFirst) === 0 ? '01': '00';
        },
        '%V': function(date) {
          // Replaced by the week number of the year (Monday as the first day of the week) 
          // as a decimal number [01,53]. If the week containing 1 January has four 
          // or more days in the new year, then it is considered week 1. 
          // Otherwise, it is the last week of the previous year, and the next week is week 1. 
          // Both January 4th and the first Thursday of January are always in week 1. [ tm_year, tm_wday, tm_yday]
          var janFourthThisYear = new Date(date.tm_year+1900, 0, 4);
          var janFourthNextYear = new Date(date.tm_year+1901, 0, 4);
          var firstWeekStartThisYear = getFirstWeekStartDate(janFourthThisYear);
          var firstWeekStartNextYear = getFirstWeekStartDate(janFourthNextYear);
          var endDate = __addDays(new Date(date.tm_year+1900, 0, 1), date.tm_yday);
          if (compareByDay(endDate, firstWeekStartThisYear) < 0) {
            // if given date is before this years first week, then it belongs to the 53rd week of last year
            return '53';
          } 
          if (compareByDay(firstWeekStartNextYear, endDate) <= 0) {
            // if given date is after next years first week, then it belongs to the 01th week of next year
            return '01';
          }
          // given date is in between CW 01..53 of this calendar year
          var daysDifference;
          if (firstWeekStartThisYear.getFullYear() < date.tm_year+1900) {
            // first CW of this year starts last year
            daysDifference = date.tm_yday+32-firstWeekStartThisYear.getDate()
          } else {
            // first CW of this year starts this year
            daysDifference = date.tm_yday+1-firstWeekStartThisYear.getDate();
          }
          return leadingNulls(Math.ceil(daysDifference/7), 2);
        },
        '%w': function(date) {
          var day = new Date(date.tm_year+1900, date.tm_mon+1, date.tm_mday, 0, 0, 0, 0);
          return day.getDay();
        },
        '%W': function(date) {
          // Replaced by the week number of the year as a decimal number [00,53]. 
          // The first Monday of January is the first day of week 1; 
          // days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday]
          var janFirst = new Date(date.tm_year, 0, 1);
          var firstMonday = janFirst.getDay() === 1 ? janFirst : __addDays(janFirst, janFirst.getDay() === 0 ? 1 : 7-janFirst.getDay()+1);
          var endDate = new Date(date.tm_year+1900, date.tm_mon, date.tm_mday);
          // is target date after the first Monday?
          if (compareByDay(firstMonday, endDate) < 0) {
            var februaryFirstUntilEndMonth = __arraySum(__isLeapYear(endDate.getFullYear()) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, endDate.getMonth()-1)-31;
            var firstMondayUntilEndJanuary = 31-firstMonday.getDate();
            var days = firstMondayUntilEndJanuary+februaryFirstUntilEndMonth+endDate.getDate();
            return leadingNulls(Math.ceil(days/7), 2);
          }
          return compareByDay(firstMonday, janFirst) === 0 ? '01': '00';
        },
        '%y': function(date) {
          // Replaced by the last two digits of the year as a decimal number [00,99]. [ tm_year]
          return (date.tm_year+1900).toString().substring(2);
        },
        '%Y': function(date) {
          // Replaced by the year as a decimal number (for example, 1997). [ tm_year]
          return date.tm_year+1900;
        },
        '%z': function(date) {
          // Replaced by the offset from UTC in the ISO 8601:2000 standard format ( +hhmm or -hhmm ),
          // or by no characters if no timezone is determinable. 
          // For example, "-0430" means 4 hours 30 minutes behind UTC (west of Greenwich). 
          // If tm_isdst is zero, the standard time offset is used. 
          // If tm_isdst is greater than zero, the daylight savings time offset is used. 
          // If tm_isdst is negative, no characters are returned. 
          // FIXME: we cannot determine time zone (or can we?)
          return '';
        },
        '%Z': function(date) {
          // Replaced by the timezone name or abbreviation, or by no bytes if no timezone information exists. [ tm_isdst]
          // FIXME: we cannot determine time zone (or can we?)
          return '';
        },
        '%%': function() {
          return '%';
        }
      };
      for (var rule in EXPANSION_RULES_2) {
        if (pattern.indexOf(rule) >= 0) {
          pattern = pattern.replace(new RegExp(rule, 'g'), EXPANSION_RULES_2[rule](date));
        }
      }
      var bytes = intArrayFromString(pattern, false);
      if (bytes.length > maxsize) {
        return 0;
      } 
      writeArrayToMemory(bytes, s);
      return bytes.length-1;
    }var _strftime_l=_strftime;
  function _isspace(chr) {
      return (chr == 32) || (chr >= 9 && chr <= 13);
    }
  function __parseInt64(str, endptr, base, min, max, unsign) {
      var isNegative = false;
      // Skip space.
      while (_isspace(HEAP8[(str)])) str++;
      // Check for a plus/minus sign.
      if (HEAP8[(str)] == 45) {
        str++;
        isNegative = true;
      } else if (HEAP8[(str)] == 43) {
        str++;
      }
      // Find base.
      var ok = false;
      var finalBase = base;
      if (!finalBase) {
        if (HEAP8[(str)] == 48) {
          if (HEAP8[((str+1)|0)] == 120 ||
              HEAP8[((str+1)|0)] == 88) {
            finalBase = 16;
            str += 2;
          } else {
            finalBase = 8;
            ok = true; // we saw an initial zero, perhaps the entire thing is just "0"
          }
        }
      } else if (finalBase==16) {
        if (HEAP8[(str)] == 48) {
          if (HEAP8[((str+1)|0)] == 120 ||
              HEAP8[((str+1)|0)] == 88) {
            str += 2;
          }
        }
      }
      if (!finalBase) finalBase = 10;
      start = str;
      // Get digits.
      var chr;
      while ((chr = HEAP8[(str)]) != 0) {
        var digit = parseInt(String.fromCharCode(chr), finalBase);
        if (isNaN(digit)) {
          break;
        } else {
          str++;
          ok = true;
        }
      }
      if (!ok) {
        ___setErrNo(ERRNO_CODES.EINVAL);
        return ((asm["setTempRet0"](0),0)|0);
      }
      // Set end pointer.
      if (endptr) {
        HEAP32[((endptr)>>2)]=str
      }
      try {
        var numberString = isNegative ? '-'+Pointer_stringify(start, str - start) : Pointer_stringify(start, str - start);
        i64Math.fromString(numberString, finalBase, min, max, unsign);
      } catch(e) {
        ___setErrNo(ERRNO_CODES.ERANGE); // not quite correct
      }
      return ((asm["setTempRet0"](((HEAP32[(((tempDoublePtr)+(4))>>2)])|0)),((HEAP32[((tempDoublePtr)>>2)])|0))|0);
    }function _strtoull(str, endptr, base) {
      return __parseInt64(str, endptr, base, 0, '18446744073709551615', true);  // ULONG_MAX.
    }var _strtoull_l=_strtoull;
  function _strtoll(str, endptr, base) {
      return __parseInt64(str, endptr, base, '-9223372036854775808', '9223372036854775807');  // LLONG_MIN, LLONG_MAX.
    }var _strtoll_l=_strtoll;
  function _uselocale(locale) {
      return 0;
    }
  function ___locale_mb_cur_max() { throw '__locale_mb_cur_max not implemented' }
  var _llvm_va_start=undefined;
  function _sprintf(s, format, varargs) {
      // int sprintf(char *restrict s, const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      return _snprintf(s, undefined, format, varargs);
    }function _asprintf(s, format, varargs) {
      return _sprintf(-s, format, varargs);
    }function _vasprintf(s, format, va_arg) {
      return _asprintf(s, format, HEAP32[((va_arg)>>2)]);
    }
  function _llvm_va_end() {}
  function _vsnprintf(s, n, format, va_arg) {
      return _snprintf(s, n, format, HEAP32[((va_arg)>>2)]);
    }
  function _vsscanf(s, format, va_arg) {
      return _sscanf(s, format, HEAP32[((va_arg)>>2)]);
    }
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
  function _time(ptr) {
      var ret = Math.floor(Date.now()/1000);
      if (ptr) {
        HEAP32[((ptr)>>2)]=ret
      }
      return ret;
    }
  function ___cxa_call_unexpected(exception) {
      Module.printErr('Unexpected exception thrown, this is not properly supported - aborting');
      ABORT = true;
      throw exception;
    }
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
          var x, y;
          if (event.type == 'touchstart' ||
              event.type == 'touchend' ||
              event.type == 'touchmove') {
            var t = event.touches.item(0);
            if (t) {
              x = t.pageX - (window.scrollX + rect.left);
              y = t.pageY - (window.scrollY + rect.top);
            } else {
              return;
            }
          } else {
            x = event.pageX - (window.scrollX + rect.left);
            y = event.pageY - (window.scrollY + rect.top);
          }
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
_llvm_eh_exception.buf = allocate(12, "void*", ALLOC_STATIC);
FS.staticInit();__ATINIT__.unshift({ func: function() { if (!Module["noFSInit"] && !FS.init.initialized) FS.init() } });__ATMAIN__.push({ func: function() { FS.ignorePermissions = false } });__ATEXIT__.push({ func: function() { FS.quit() } });Module["FS_createFolder"] = FS.createFolder;Module["FS_createPath"] = FS.createPath;Module["FS_createDataFile"] = FS.createDataFile;Module["FS_createPreloadedFile"] = FS.createPreloadedFile;Module["FS_createLazyFile"] = FS.createLazyFile;Module["FS_createLink"] = FS.createLink;Module["FS_createDevice"] = FS.createDevice;
___errno_state = Runtime.staticAlloc(4); HEAP32[((___errno_state)>>2)]=0;
_fgetc.ret = allocate([0], "i8", ALLOC_STATIC);
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
 var cttz_i8 = allocate([8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0], "i8", ALLOC_DYNAMIC);
var Math_min = Math.min;
function invoke_viiiii(index,a1,a2,a3,a4,a5) {
  try {
    Module["dynCall_viiiii"](index,a1,a2,a3,a4,a5);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiiii(index,a1,a2,a3,a4,a5,a6,a7) {
  try {
    Module["dynCall_viiiiiii"](index,a1,a2,a3,a4,a5,a6,a7);
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
function invoke_vii(index,a1,a2) {
  try {
    Module["dynCall_vii"](index,a1,a2);
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
function invoke_iiiiii(index,a1,a2,a3,a4,a5) {
  try {
    return Module["dynCall_iiiiii"](index,a1,a2,a3,a4,a5);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_ii(index,a1) {
  try {
    return Module["dynCall_ii"](index,a1);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_iiii(index,a1,a2,a3) {
  try {
    return Module["dynCall_iiii"](index,a1,a2,a3);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiif(index,a1,a2,a3,a4,a5,a6) {
  try {
    Module["dynCall_viiiiif"](index,a1,a2,a3,a4,a5,a6);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viii(index,a1,a2,a3) {
  try {
    Module["dynCall_viii"](index,a1,a2,a3);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiiiii(index,a1,a2,a3,a4,a5,a6,a7,a8) {
  try {
    Module["dynCall_viiiiiiii"](index,a1,a2,a3,a4,a5,a6,a7,a8);
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
function invoke_iiiiiiiii(index,a1,a2,a3,a4,a5,a6,a7,a8) {
  try {
    return Module["dynCall_iiiiiiiii"](index,a1,a2,a3,a4,a5,a6,a7,a8);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiiiiii(index,a1,a2,a3,a4,a5,a6,a7,a8,a9) {
  try {
    Module["dynCall_viiiiiiiii"](index,a1,a2,a3,a4,a5,a6,a7,a8,a9);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiiif(index,a1,a2,a3,a4,a5,a6,a7) {
  try {
    Module["dynCall_viiiiiif"](index,a1,a2,a3,a4,a5,a6,a7);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiiiii(index,a1,a2,a3,a4,a5,a6) {
  try {
    Module["dynCall_viiiiii"](index,a1,a2,a3,a4,a5,a6);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_iiiii(index,a1,a2,a3,a4) {
  try {
    return Module["dynCall_iiiii"](index,a1,a2,a3,a4);
  } catch(e) {
    if (typeof e !== 'number' && e !== 'longjmp') throw e;
    asm["setThrew"](1, 0);
  }
}
function invoke_viiii(index,a1,a2,a3,a4) {
  try {
    Module["dynCall_viiii"](index,a1,a2,a3,a4);
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
var asm=(function(global,env,buffer){"use asm";var a=new global.Int8Array(buffer);var b=new global.Int16Array(buffer);var c=new global.Int32Array(buffer);var d=new global.Uint8Array(buffer);var e=new global.Uint16Array(buffer);var f=new global.Uint32Array(buffer);var g=new global.Float32Array(buffer);var h=new global.Float64Array(buffer);var i=env.STACKTOP|0;var j=env.STACK_MAX|0;var k=env.tempDoublePtr|0;var l=env.ABORT|0;var m=env.cttz_i8|0;var n=env.ctlz_i8|0;var o=env._stdin|0;var p=env.__ZTVN10__cxxabiv117__class_type_infoE|0;var q=env.__ZTVN10__cxxabiv120__si_class_type_infoE|0;var r=env._stderr|0;var s=env.___fsmu8|0;var t=env._stdout|0;var u=env.___dso_handle|0;var v=+env.NaN;var w=+env.Infinity;var x=0;var y=0;var z=0;var A=0;var B=0,C=0,D=0,E=0,F=0.0,G=0,H=0,I=0,J=0.0;var K=0;var L=0;var M=0;var N=0;var O=0;var P=0;var Q=0;var R=0;var S=0;var T=0;var U=global.Math.floor;var V=global.Math.abs;var W=global.Math.sqrt;var X=global.Math.pow;var Y=global.Math.cos;var Z=global.Math.sin;var _=global.Math.tan;var $=global.Math.acos;var aa=global.Math.asin;var ab=global.Math.atan;var ac=global.Math.atan2;var ad=global.Math.exp;var ae=global.Math.log;var af=global.Math.ceil;var ag=global.Math.imul;var ah=env.abort;var ai=env.assert;var aj=env.asmPrintInt;var ak=env.asmPrintFloat;var al=env.min;var am=env.invoke_viiiii;var an=env.invoke_viiiiiii;var ao=env.invoke_vi;var ap=env.invoke_vii;var aq=env.invoke_iii;var ar=env.invoke_iiiiii;var as=env.invoke_ii;var at=env.invoke_iiii;var au=env.invoke_viiiiif;var av=env.invoke_viii;var aw=env.invoke_viiiiiiii;var ax=env.invoke_v;var ay=env.invoke_iiiiiiiii;var az=env.invoke_viiiiiiiii;var aA=env.invoke_viiiiiif;var aB=env.invoke_viiiiii;var aC=env.invoke_iiiii;var aD=env.invoke_viiii;var aE=env._llvm_lifetime_end;var aF=env.__scanString;var aG=env._pthread_mutex_lock;var aH=env.___cxa_end_catch;var aI=env._strtoull;var aJ=env.__isFloat;var aK=env._fflush;var aL=env.__isLeapYear;var aM=env._fwrite;var aN=env._send;var aO=env._llvm_umul_with_overflow_i32;var aP=env._isspace;var aQ=env._read;var aR=env.___cxa_guard_abort;var aS=env._newlocale;var aT=env.___gxx_personality_v0;var aU=env._pthread_cond_wait;var aV=env.___cxa_rethrow;var aW=env.___resumeException;var aX=env._llvm_va_end;var aY=env._vsscanf;var aZ=env._snprintf;var a_=env._fgetc;var a$=env._atexit;var a0=env.___cxa_free_exception;var a1=env.__Z8catcloseP8_nl_catd;var a2=env.___setErrNo;var a3=env._isxdigit;var a4=env._exit;var a5=env._sprintf;var a6=env.___ctype_b_loc;var a7=env._freelocale;var a8=env.__Z7catopenPKci;var a9=env._asprintf;var ba=env.___cxa_is_number_type;var bb=env.___cxa_does_inherit;var bc=env.___cxa_guard_acquire;var bd=env.___locale_mb_cur_max;var be=env.___cxa_begin_catch;var bf=env._recv;var bg=env.__parseInt64;var bh=env.__ZSt18uncaught_exceptionv;var bi=env.___cxa_call_unexpected;var bj=env.__exit;var bk=env._strftime;var bl=env.___cxa_throw;var bm=env._llvm_eh_exception;var bn=env._pread;var bo=env.__arraySum;var bp=env._log;var bq=env.___cxa_find_matching_catch;var br=env.__formatString;var bs=env._pthread_cond_broadcast;var bt=env.__ZSt9terminatev;var bu=env._pthread_mutex_unlock;var bv=env._sbrk;var bw=env.___errno_location;var bx=env._strerror;var by=env._llvm_lifetime_start;var bz=env.___cxa_guard_release;var bA=env._ungetc;var bB=env._uselocale;var bC=env._vsnprintf;var bD=env._sscanf;var bE=env._sysconf;var bF=env._fread;var bG=env._abort;var bH=env._isdigit;var bI=env._strtoll;var bJ=env.__addDays;var bK=env._floor;var bL=env.__reallyNegative;var bM=env.__Z7catgetsP8_nl_catdiiPKc;var bN=env._write;var bO=env.___cxa_allocate_exception;var bP=env.___cxa_pure_virtual;var bQ=env._vasprintf;var bR=env.___ctype_toupper_loc;var bS=env.___ctype_tolower_loc;var bT=env._pwrite;var bU=env._strerror_r;var bV=env._time;
// EMSCRIPTEN_START_FUNCS
function cc(a){a=a|0;var b=0;b=i;i=i+a|0;i=i+7>>3<<3;return b|0}function cd(){return i|0}function ce(a){a=a|0;i=a}function cf(a,b){a=a|0;b=b|0;if((x|0)==0){x=a;y=b}}function cg(b){b=b|0;a[k]=a[b];a[k+1|0]=a[b+1|0];a[k+2|0]=a[b+2|0];a[k+3|0]=a[b+3|0]}function ch(b){b=b|0;a[k]=a[b];a[k+1|0]=a[b+1|0];a[k+2|0]=a[b+2|0];a[k+3|0]=a[b+3|0];a[k+4|0]=a[b+4|0];a[k+5|0]=a[b+5|0];a[k+6|0]=a[b+6|0];a[k+7|0]=a[b+7|0]}function ci(a){a=a|0;K=a}function cj(a){a=a|0;L=a}function ck(a){a=a|0;M=a}function cl(a){a=a|0;N=a}function cm(a){a=a|0;O=a}function cn(a){a=a|0;P=a}function co(a){a=a|0;Q=a}function cp(a){a=a|0;R=a}function cq(a){a=a|0;S=a}function cr(a){a=a|0;T=a}function cs(){c[q+8>>2]=250;c[q+12>>2]=128;c[q+16>>2]=62;c[q+20>>2]=150;c[q+24>>2]=8;c[q+28>>2]=10;c[q+32>>2]=2;c[q+36>>2]=2;c[p+8>>2]=250;c[p+12>>2]=244;c[p+16>>2]=62;c[p+20>>2]=150;c[p+24>>2]=8;c[p+28>>2]=26;c[p+32>>2]=4;c[p+36>>2]=8;c[2036]=p+8;c[2038]=p+8;c[2040]=q+8;c[2044]=q+8;c[2048]=q+8;c[2052]=q+8;c[2056]=q+8;c[2060]=p+8;c[2094]=q+8;c[2098]=q+8;c[2162]=q+8;c[2166]=q+8;c[2186]=p+8;c[2188]=q+8;c[2224]=q+8;c[2228]=q+8;c[2264]=q+8;c[2268]=q+8;c[2288]=p+8;c[2290]=p+8;c[2292]=q+8;c[2296]=q+8;c[2300]=q+8;c[2304]=p+8;c[2306]=p+8;c[2308]=p+8;c[2310]=p+8;c[2312]=p+8;c[2314]=p+8;c[2316]=p+8;c[2342]=q+8;c[2346]=p+8;c[2348]=q+8;c[2352]=q+8;c[2356]=q+8;c[2360]=p+8;c[2362]=p+8;c[2364]=p+8;c[2366]=p+8;c[2400]=p+8;c[2402]=p+8;c[2404]=p+8;c[2406]=q+8;c[2410]=q+8;c[2414]=q+8;c[2418]=q+8;c[2422]=q+8;c[2426]=q+8}function ct(a){a=a|0;var b=0,d=0;b=c[a>>2]|0;if((b|0)==0){return}d=a+4|0;a=c[d>>2]|0;if((b|0)!=(a|0)){c[d>>2]=a+(~((a-8+(-b|0)|0)>>>3)<<3)}kS(b);return}function cu(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0.0,w=0,x=0,y=0.0,z=0.0;d=i;i=i+24|0;e=d|0;f=d+8|0;g=f|0;c[g>>2]=0;j=f+4|0;c[j>>2]=0;k=f+8|0;c[k>>2]=0;l=b-a|0;m=l>>3;do{if((m|0)==0){n=0;o=0}else{if(m>>>0>536870911){ir(f)}p=kO(l)|0;c[j>>2]=p;c[g>>2]=p;c[k>>2]=p+(m<<3);if((a|0)==(b|0)){n=p;o=p;break}else{q=a;r=p}do{if((r|0)==0){s=0}else{h[r>>3]=+h[q>>3];s=c[j>>2]|0}r=s+8|0;c[j>>2]=r;q=q+8|0;}while((q|0)!=(b|0));n=c[g>>2]|0;o=r}}while(0);cU(n,o,e);e=c[g>>2]|0;o=c[j>>2]|0;L23:do{if((e|0)==(o|0)){t=e}else{n=e;while(1){u=n+8|0;if((u|0)==(o|0)){t=e;break L23}v=+h[n>>3];if(v==+h[u>>3]){break}else{n=u}}if((n|0)==(o|0)){t=e;break}else{w=u;x=n;y=v}L28:while(1){r=w;do{r=r+8|0;if((r|0)==(o|0)){break L28}z=+h[r>>3];}while(y==z);b=x+8|0;h[b>>3]=z;w=r;x=b;y=z}t=c[g>>2]|0}}while(0);if((t|0)==0){i=d;return}g=c[j>>2]|0;if((t|0)!=(g|0)){c[j>>2]=g+(~((g-8+(-t|0)|0)>>>3)<<3)}kS(t);i=d;return}function cv(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0.0,x=0.0,y=0,z=0.0;d=i;i=i+24|0;e=d|0;f=d+8|0;g=f|0;c[g>>2]=0;j=f+4|0;c[j>>2]=0;k=f+8|0;c[k>>2]=0;l=b-a|0;m=l>>3;do{if((m|0)==0){n=0;o=0}else{if(m>>>0>536870911){ir(f)}p=kO(l)|0;c[j>>2]=p;c[g>>2]=p;c[k>>2]=p+(m<<3);if((a|0)==(b|0)){n=p;o=p;break}else{q=a;r=p}do{if((r|0)==0){s=0}else{h[r>>3]=+h[q>>3];s=c[j>>2]|0}r=s+8|0;c[j>>2]=r;q=q+8|0;}while((q|0)!=(b|0));n=c[g>>2]|0;o=r}}while(0);cU(n,o,e);e=c[g>>2]|0;o=c[j>>2]|0;L58:do{if((e|0)==(o|0)){t=e;u=62}else{n=e;while(1){r=n+8|0;if((r|0)==(o|0)){v=e;break L58}if(+h[n>>3]==+h[r>>3]){t=n;u=62;break}else{n=r}}}}while(0);do{if((u|0)==62){if((t|0)==(o|0)){v=e;break}n=t+16|0;if((n|0)==(o|0)){v=e;break}r=n;n=t;w=+h[t>>3];while(1){x=+h[r>>3];if(w==x){y=n;z=w}else{b=n+8|0;h[b>>3]=x;y=b;z=x}b=r+8|0;if((b|0)==(o|0)){break}else{r=b;n=y;w=z}}v=c[g>>2]|0}}while(0);if((v|0)==0){i=d;return}g=c[j>>2]|0;if((v|0)!=(g|0)){c[j>>2]=g+(~((g-8+(-v|0)|0)>>>3)<<3)}kS(v);i=d;return}function cw(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0;d=i;i=i+8|0;e=d|0;f=e;g=i;i=i+8|0;j=g;k=i;i=i+1|0;i=i+7>>3<<3;l=i;i=i+8|0;m=i;i=i+8|0;n=i;i=i+1|0;i=i+7>>3<<3;o=i;i=i+24|0;p=i;i=i+8|0;q=i;i=i+8|0;r=b;s=r-a>>3;h[p>>3]=0.0;t=o|0;u=o+4|0;v=o+8|0;k$(o|0,0,24);do{if((s|0)==0){w=0;x=o+16|0;y=0;z=0;A=0;B=0}else{cO(o,s,p);C=c[u>>2]|0;D=o+16|0;E=c[D>>2]|0;F=c[v>>2]|0;G=C+(E>>>9<<2)|0;if((F|0)==(C|0)){w=0;x=D;y=F;z=E;A=C;B=G;break}w=(c[G>>2]|0)+((E&511)<<3)|0;x=D;y=F;z=E;A=C;B=G}}while(0);if((a|0)==(b|0)){H=A;I=z;J=y}else{y=a;a=w;w=B;while(1){B=a;z=(c[w>>2]|0)+4096-B>>3;A=y;p=r-A>>3;if((p|0)>(z|0)){K=y+(z<<3)|0;L=z}else{K=b;L=p}k_(a|0,y|0,K-A|0);do{if((L|0)==0){M=a;N=w}else{A=(B-(c[w>>2]|0)>>3)+L|0;if((A|0)>0){p=w+(((A|0)/512|0)<<2)|0;M=(c[p>>2]|0)+(((A|0)%512|0)<<3)|0;N=p;break}else{p=511-A|0;A=w+(((p|0)/-512|0)<<2)|0;M=(c[A>>2]|0)+(511-((p|0)%512|0)<<3)|0;N=A;break}}}while(0);if((K|0)==(b|0)){break}else{y=K;a=M;w=N}}H=c[u>>2]|0;I=c[x>>2]|0;J=c[v>>2]|0}N=H+(I>>>9<<2)|0;if((J|0)==(H|0)){J=o+20|0;O=0;P=0;Q=0;R=0;S=J;T=H+((I+(c[J>>2]|0)|0)>>>9<<2)|0}else{J=o+20|0;o=I+(c[J>>2]|0)|0;w=H+(o>>>9<<2)|0;O=(c[w>>2]|0)+((o&511)<<3)|0;P=0;Q=(c[N>>2]|0)+((I&511)<<3)|0;R=0;S=J;T=w}c[l>>2]=R|N;c[l+4>>2]=Q;c[m>>2]=P|T;c[m+4>>2]=O;cK(l,m,n);n=c[u>>2]|0;m=c[x>>2]|0;l=n+(m>>>9<<2)|0;if((c[v>>2]|0)==(n|0)){U=0;V=0;W=0;X=0;Y=n+((m+(c[S>>2]|0)|0)>>>9<<2)|0}else{O=m+(c[S>>2]|0)|0;T=n+(O>>>9<<2)|0;U=(c[T>>2]|0)+((O&511)<<3)|0;V=0;W=(c[l>>2]|0)+((m&511)<<3)|0;X=0;Y=T}c[e>>2]=X|l;c[e+4>>2]=W;c[g>>2]=V|Y;c[g+4>>2]=U;cI(q,f,j,k);k=c[u>>2]|0;j=c[x>>2]|0;f=k+(j>>>9<<2)|0;q=c[v>>2]|0;if((q|0)==(k|0)){Z=0;_=0}else{U=(c[S>>2]|0)+j|0;Z=(c[k+(U>>>9<<2)>>2]|0)+((U&511)<<3)|0;_=(c[f>>2]|0)+((j&511)<<3)|0}j=f;f=_;L112:while(1){_=f;do{if((_|0)==(Z|0)){break L112}_=_+8|0;}while((_-(c[j>>2]|0)|0)!=4096);_=j+4|0;j=_;f=c[_>>2]|0}c[S>>2]=0;S=q-k>>2;if(S>>>0>2){f=k;while(1){kS(c[f>>2]|0);j=(c[u>>2]|0)+4|0;c[u>>2]=j;Z=c[v>>2]|0;_=Z-j>>2;if(_>>>0>2){f=j}else{$=_;aa=j;ab=Z;break}}}else{$=S;aa=k;ab=q}switch($|0){case 1:{c[x>>2]=256;break};case 2:{c[x>>2]=512;break};default:{}}do{if((aa|0)!=(ab|0)){x=aa;do{kS(c[x>>2]|0);x=x+4|0;}while((x|0)!=(ab|0));x=c[u>>2]|0;$=c[v>>2]|0;if((x|0)==($|0)){break}c[v>>2]=$+(~(($-4+(-x|0)|0)>>>2)<<2)}}while(0);v=c[t>>2]|0;if((v|0)==0){i=d;return}kS(v);i=d;return}function cx(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0.0,y=0,z=0;d=i;i=i+48|0;e=d|0;f=d+8|0;g=d+16|0;j=d+24|0;k=d+32|0;l=k|0;m=k;c[l>>2]=m;n=k+4|0;c[n>>2]=m;o=k+8|0;c[o>>2]=0;if((a|0)==(b|0)){p=m;q=0}else{k=a;a=m;r=0;while(1){s=kO(16)|0;t=s;u=s+8|0;if((u|0)!=0){h[u>>3]=+h[k>>3]}c[a+4>>2]=t;c[s>>2]=c[l>>2];c[l>>2]=t;c[s+4>>2]=m;v=(c[o>>2]|0)+1|0;c[o>>2]=v;s=k+8|0;if((s|0)==(b|0)){break}else{k=s;a=t;r=v}}p=c[n>>2]|0;q=v}c[e>>2]=p;c[f>>2]=m;cJ(g,e,f,q,j);j=c[n>>2]|0;if((j|0)!=(m|0)){q=j;while(1){j=c[q+4>>2]|0;L150:do{if((j|0)==(m|0)){w=m}else{x=+h[q+8>>3];f=j;while(1){if(x!=+h[f+8>>3]){w=f;break L150}e=c[f+4>>2]|0;if((e|0)==(m|0)){w=m;break}else{f=e}}}}while(0);if((j|0)==(w|0)){y=j}else{f=(c[w>>2]|0)+4|0;e=j|0;c[(c[e>>2]|0)+4>>2]=c[f>>2];c[c[f>>2]>>2]=c[e>>2];e=j;while(1){f=c[e+4>>2]|0;c[o>>2]=(c[o>>2]|0)-1;kS(e);if((f|0)==(w|0)){y=w;break}else{e=f}}}if((y|0)==(m|0)){break}else{q=y}}}if((c[o>>2]|0)==0){i=d;return}y=c[n>>2]|0;n=(c[l>>2]|0)+4|0;l=y|0;c[(c[l>>2]|0)+4>>2]=c[n>>2];c[c[n>>2]>>2]=c[l>>2];c[o>>2]=0;if((y|0)==(m|0)){i=d;return}else{z=y}while(1){y=c[z+4>>2]|0;kS(z);if((y|0)==(m|0)){break}else{z=y}}i=d;return}function cy(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;d=i;i=i+32|0;e=d|0;f=d+8|0;g=d+16|0;j=g|0;k=g+4|0;c[k>>2]=0;l=g+8|0;c[l>>2]=0;m=k;k=g|0;c[k>>2]=m;if((a|0)==(b|0)){n=0;cE(j,n);i=d;return}o=f|0;p=g+4|0;g=a;do{c[o>>2]=m;a=cH(j,f,e,g)|0;if((c[a>>2]|0)==0){q=kO(24)|0;r=q+16|0;if((r|0)!=0){h[r>>3]=+h[g>>3]}r=c[e>>2]|0;s=q;c[q>>2]=0;c[q+4>>2]=0;c[q+8>>2]=r;c[a>>2]=s;r=c[c[k>>2]>>2]|0;if((r|0)==0){t=s}else{c[k>>2]=r;t=c[a>>2]|0}cG(c[p>>2]|0,t);c[l>>2]=(c[l>>2]|0)+1}g=g+8|0;}while((g|0)!=(b|0));n=c[p>>2]|0;cE(j,n);i=d;return}function cz(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0;d=i;i=i+32|0;e=d|0;f=d+8|0;g=d+16|0;j=g|0;k=g+4|0;c[k>>2]=0;l=g+8|0;c[l>>2]=0;m=k;n=g|0;c[n>>2]=m;do{if((a|0)!=(b|0)){o=f|0;p=g+4|0;q=a;do{c[o>>2]=m;r=cC(j,f,e,q)|0;s=kO(24)|0;t=s+16|0;if((t|0)!=0){h[t>>3]=+h[q>>3]}t=c[e>>2]|0;u=s;c[s>>2]=0;c[s+4>>2]=0;c[s+8>>2]=t;c[r>>2]=u;t=c[c[n>>2]>>2]|0;if((t|0)==0){v=u}else{c[n>>2]=t;v=c[r>>2]|0}cG(c[p>>2]|0,v);c[l>>2]=(c[l>>2]|0)+1;q=q+8|0;}while((q|0)!=(b|0));q=c[n>>2]|0;if((q|0)==(m|0)){break}p=k;o=g+4|0;r=q;L202:while(1){q=r|0;t=r+4|0;u=r+16|0;while(1){w=c[t>>2]|0;if((w|0)==0){s=q;while(1){x=c[s+8>>2]|0;if((s|0)==(c[x>>2]|0)){y=x;break}else{s=x}}}else{s=w;while(1){x=c[s>>2]|0;if((x|0)==0){y=s;break}else{s=x}}}s=y;if((y|0)==(p|0)){z=o;A=213;break L202}if(+h[u>>3]!=+h[y+16>>3]){break}x=c[y+4>>2]|0;if((x|0)==0){B=y;while(1){C=c[B+8>>2]|0;if((B|0)==(c[C>>2]|0)){D=C;break}else{B=C}}}else{B=x;while(1){C=c[B>>2]|0;if((C|0)==0){D=B;break}else{B=C}}}if((c[n>>2]|0)==(s|0)){c[n>>2]=D}c[l>>2]=(c[l>>2]|0)-1;cB(c[o>>2]|0,y);kS(y)}if((w|0)==0){u=q;while(1){t=c[u+8>>2]|0;if((u|0)==(c[t>>2]|0)){E=t;break}else{u=t}}}else{u=w;while(1){q=c[u>>2]|0;if((q|0)==0){E=u;break}else{u=q}}}if((E|0)==(p|0)){z=o;A=214;break}else{r=E}}if((A|0)==214){F=c[z>>2]|0;G=F;cE(j,G);i=d;return}else if((A|0)==213){F=c[z>>2]|0;G=F;cE(j,G);i=d;return}}}while(0);z=g+4|0;F=c[z>>2]|0;G=F;cE(j,G);i=d;return}function cA(a){a=a|0;var b=0,d=0,e=0,f=0.0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0.0,x=0,y=0,z=0.0,A=0,B=0.0,C=0,D=0,E=0,F=0.0,G=0,H=0,I=0.0,J=0.0,L=0,M=0,N=0,O=0,P=0;b=i;i=i+24|0;d=b|0;e=b+8|0;f=+(a|0);g=~~+U(+(19931568.569324173/(f*(+ae(+f)/.6931471805599453))));j=a<<1;k=c[2446]|0;l=c[2447]|0;if((k|0)!=(l|0)){c[2447]=l+(~((l-8+(-k|0)|0)>>>3)<<3)}k=e|0;c[k>>2]=0;l=e+4|0;c[l>>2]=0;m=e+8|0;c[m>>2]=0;if((j|0)==0){n=0}else{if(j>>>0>536870911){ir(e)}e=kO(a<<4)|0;c[l>>2]=e;c[k>>2]=e;c[m>>2]=e+(j<<3);m=j;o=e;do{if((o|0)==0){p=0}else{h[o>>3]=0.0;p=c[l>>2]|0}o=p+8|0;c[l>>2]=o;m=m-1|0;}while((m|0)!=0);n=c[k>>2]|0}m=n+(j<<3)|0;o=n+(a<<3)|0;if((a|0)!=0){p=n;f=0.0;while(1){e=p+8|0;h[p>>3]=f;if((e|0)==(o|0)){break}else{p=e;f=f+1.0}}}if((j|0)!=(a|0)){j=o;f=0.0;while(1){o=j+8|0;h[j>>3]=f;if((o|0)==(m|0)){break}else{j=o;f=f+1.0}}}cF(n,m);if((g|0)>0){j=a<<4;a=j>>3;o=aO(a|0,8)|0;p=n;if(K){e=g;while(1){q=e-1|0;r=kP(-1)|0;s=r;kY(r|0,p|0,j)|0;t=s+(a<<3)|0;cU(s,t,d);L268:do{if((s|0)!=(t|0)){u=s;while(1){v=u+8|0;if((v|0)==(t|0)){break L268}w=+h[u>>3];if(w==+h[v>>3]){break}else{u=v}}if((u|0)==(t|0)){break}else{x=v;y=u;z=w}while(1){A=x;do{A=A+8|0;if((A|0)==(t|0)){break L268}B=+h[A>>3];}while(z==B);C=y+8|0;h[C>>3]=B;x=A;y=C;z=B}}}while(0);if((r|0)!=0){kT(r)}if((q|0)>0){e=q}else{D=g;break}}}else{e=g;while(1){y=e-1|0;x=kP(o)|0;v=x;kY(x|0,p|0,j)|0;t=v+(a<<3)|0;cU(v,t,d);L287:do{if((v|0)!=(t|0)){s=v;while(1){E=s+8|0;if((E|0)==(t|0)){break L287}F=+h[s>>3];if(F==+h[E>>3]){break}else{s=E}}if((s|0)==(t|0)){break}else{G=E;H=s;I=F}while(1){u=G;do{u=u+8|0;if((u|0)==(t|0)){break L287}J=+h[u>>3];}while(I==J);A=H+8|0;h[A>>3]=J;G=u;H=A;I=J}}}while(0);if((x|0)!=0){kT(x)}if((y|0)>0){e=y}else{D=g;break}}}while(1){e=D-1|0;cu(n,m,e);if((e|0)>0){D=e}else{L=g;break}}while(1){D=L-1|0;cv(n,m,D);if((D|0)>0){L=D}else{M=g;break}}while(1){L=M-1|0;cw(n,m,L);if((L|0)>0){M=L}else{N=g;break}}while(1){M=N-1|0;cx(n,m,M);if((M|0)>0){N=M}else{O=g;break}}while(1){N=O-1|0;cy(n,m,N);if((N|0)>0){O=N}else{P=g;break}}do{P=P-1|0;cz(n,m,P);}while((P|0)>0)}P=c[k>>2]|0;if((P|0)==0){i=b;return}k=c[l>>2]|0;if((P|0)!=(k|0)){c[l>>2]=k+(~((k-8+(-P|0)|0)>>>3)<<3)}kS(P);i=b;return}function cB(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;e=d|0;f=c[e>>2]|0;do{if((f|0)==0){g=d;h=301}else{i=c[d+4>>2]|0;if((i|0)==0){j=f;k=d;l=d|0;h=303;break}else{m=i;while(1){i=c[m>>2]|0;if((i|0)==0){g=m;h=301;break}else{m=i}}}}}while(0);do{if((h|0)==301){f=g|0;m=c[g+4>>2]|0;if((m|0)!=0){j=m;k=g;l=f;h=303;break}n=0;o=0;p=g+8|0;q=g;r=f}}while(0);if((h|0)==303){g=k+8|0;c[j+8>>2]=c[g>>2];n=j;o=1;p=g;q=k;r=l}l=c[p>>2]|0;k=l|0;do{if((q|0)==(c[k>>2]|0)){c[k>>2]=n;if((q|0)==(b|0)){s=n;t=0;break}s=b;t=c[(c[p>>2]|0)+4>>2]|0}else{c[l+4>>2]=n;s=b;t=c[c[p>>2]>>2]|0}}while(0);b=q+12|0;l=(a[b]&1)==0;if((q|0)==(d|0)){u=s}else{k=d+8|0;g=c[k>>2]|0;c[p>>2]=g;if((c[c[k>>2]>>2]|0)==(d|0)){c[g>>2]=q}else{c[g+4>>2]=q}g=c[e>>2]|0;c[r>>2]=g;c[g+8>>2]=q;g=c[d+4>>2]|0;c[q+4>>2]=g;if((g|0)!=0){c[g+8>>2]=q}a[b]=a[d+12|0]&1;u=(s|0)==(d|0)?q:s}if(l|(u|0)==0){return}if(o){a[n+12|0]=1;return}else{v=u;w=t}while(1){t=w+8|0;u=c[t>>2]|0;n=w+12|0;o=(a[n]&1)!=0;if((w|0)==(c[u>>2]|0)){if(o){x=v;y=w}else{a[n]=1;a[u+12|0]=0;l=c[t>>2]|0;s=l|0;q=c[s>>2]|0;d=q+4|0;b=c[d>>2]|0;c[s>>2]=b;if((b|0)!=0){c[b+8>>2]=l}b=l+8|0;c[q+8>>2]=c[b>>2];s=c[b>>2]|0;g=s|0;if((c[g>>2]|0)==(l|0)){c[g>>2]=q}else{c[s+4>>2]=q}c[d>>2]=l;c[b>>2]=q;q=c[w+4>>2]|0;x=(v|0)==(q|0)?w:v;y=c[q>>2]|0}z=c[y>>2]|0;A=(z|0)==0;if(!A){if((a[z+12|0]&1)==0){h=365;break}}q=c[y+4>>2]|0;if((q|0)!=0){if((a[q+12|0]&1)==0){h=364;break}}a[y+12|0]=0;q=c[y+8>>2]|0;B=q+12|0;if((a[B]&1)==0|(q|0)==(x|0)){h=361;break}b=c[q+8>>2]|0;l=c[b>>2]|0;if((q|0)!=(l|0)){v=x;w=l;continue}v=x;w=c[b+4>>2]|0;continue}if(o){C=v;D=w}else{a[n]=1;a[u+12|0]=0;u=c[t>>2]|0;t=u+4|0;n=c[t>>2]|0;o=n|0;b=c[o>>2]|0;c[t>>2]=b;if((b|0)!=0){c[b+8>>2]=u}b=u+8|0;c[n+8>>2]=c[b>>2];t=c[b>>2]|0;l=t|0;if((c[l>>2]|0)==(u|0)){c[l>>2]=n}else{c[t+4>>2]=n}c[o>>2]=u;c[b>>2]=n;n=c[w>>2]|0;C=(v|0)==(n|0)?w:v;D=c[n+4>>2]|0}E=D|0;F=c[E>>2]|0;if((F|0)!=0){if((a[F+12|0]&1)==0){h=335;break}}n=c[D+4>>2]|0;if((n|0)!=0){if((a[n+12|0]&1)==0){G=n;h=336;break}}a[D+12|0]=0;n=c[D+8>>2]|0;if((n|0)==(C|0)){H=C;h=332;break}if((a[n+12|0]&1)==0){H=n;h=332;break}b=c[n+8>>2]|0;u=c[b>>2]|0;if((n|0)!=(u|0)){v=C;w=u;continue}v=C;w=c[b+4>>2]|0}if((h|0)==332){a[H+12|0]=1;return}else if((h|0)==335){H=c[D+4>>2]|0;if((H|0)==0){h=337}else{G=H;h=336}}else if((h|0)==361){a[B]=1;return}else if((h|0)==364){if(A){h=366}else{h=365}}if((h|0)==336){if((a[G+12|0]&1)==0){I=D;h=343}else{h=337}}else if((h|0)==365){if((a[z+12|0]&1)==0){J=y;h=372}else{h=366}}if((h|0)==337){a[F+12|0]=1;a[D+12|0]=0;F=c[E>>2]|0;z=F+4|0;G=c[z>>2]|0;c[E>>2]=G;if((G|0)!=0){c[G+8>>2]=D}G=D+8|0;c[F+8>>2]=c[G>>2];E=c[G>>2]|0;A=E|0;if((c[A>>2]|0)==(D|0)){c[A>>2]=F}else{c[E+4>>2]=F}c[z>>2]=D;c[G>>2]=F;I=F;h=343}else if((h|0)==366){F=y+4|0;a[(c[F>>2]|0)+12|0]=1;a[y+12|0]=0;G=c[F>>2]|0;D=G|0;z=c[D>>2]|0;c[F>>2]=z;if((z|0)!=0){c[z+8>>2]=y}z=y+8|0;c[G+8>>2]=c[z>>2];F=c[z>>2]|0;E=F|0;if((c[E>>2]|0)==(y|0)){c[E>>2]=G}else{c[F+4>>2]=G}c[D>>2]=y;c[z>>2]=G;J=G;h=372}if((h|0)==343){G=I+8|0;z=(c[G>>2]|0)+12|0;a[I+12|0]=a[z]&1;a[z]=1;a[(c[I+4>>2]|0)+12|0]=1;I=c[G>>2]|0;G=I+4|0;z=c[G>>2]|0;y=z|0;D=c[y>>2]|0;c[G>>2]=D;if((D|0)!=0){c[D+8>>2]=I}D=I+8|0;c[z+8>>2]=c[D>>2];G=c[D>>2]|0;F=G|0;if((c[F>>2]|0)==(I|0)){c[F>>2]=z}else{c[G+4>>2]=z}c[y>>2]=I;c[D>>2]=z;return}else if((h|0)==372){h=J+8|0;z=(c[h>>2]|0)+12|0;a[J+12|0]=a[z]&1;a[z]=1;a[(c[J>>2]|0)+12|0]=1;J=c[h>>2]|0;h=J|0;z=c[h>>2]|0;D=z+4|0;I=c[D>>2]|0;c[h>>2]=I;if((I|0)!=0){c[I+8>>2]=J}I=J+8|0;c[z+8>>2]=c[I>>2];h=c[I>>2]|0;y=h|0;if((c[y>>2]|0)==(J|0)){c[y>>2]=z}else{c[h+4>>2]=z}c[D>>2]=J;c[I>>2]=z;return}}function cC(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,j=0,k=0.0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;f=i;g=b;b=i;i=i+4|0;i=i+7>>3<<3;c[b>>2]=c[g>>2];g=a+4|0;j=c[b>>2]|0;do{if((j|0)!=(g|0)){k=+h[e>>3];if(+h[j+16>>3]>=k){break}b=g|0;l=c[b>>2]|0;if((l|0)==0){c[d>>2]=g;m=b;i=f;return m|0}else{n=l}while(1){if(+h[n+16>>3]<k){o=n+4|0;l=c[o>>2]|0;if((l|0)==0){p=406;break}else{n=l;continue}}else{q=n|0;l=c[q>>2]|0;if((l|0)==0){p=408;break}else{n=l;continue}}}if((p|0)==408){c[d>>2]=n;m=q;i=f;return m|0}else if((p|0)==406){c[d>>2]=n;m=o;i=f;return m|0}}}while(0);o=c[j>>2]|0;do{if((j|0)==(c[a>>2]|0)){r=j}else{if((o|0)==0){n=j|0;while(1){q=c[n+8>>2]|0;if((n|0)==(c[q>>2]|0)){n=q}else{s=q;break}}}else{n=o;while(1){q=c[n+4>>2]|0;if((q|0)==0){s=n;break}else{n=q}}}k=+h[e>>3];if(k>=+h[s+16>>3]){r=s;break}n=g|0;q=c[n>>2]|0;if((q|0)==0){c[d>>2]=g;m=n;i=f;return m|0}else{t=q}while(1){if(k<+h[t+16>>3]){u=t|0;q=c[u>>2]|0;if((q|0)==0){p=399;break}else{t=q;continue}}else{v=t+4|0;q=c[v>>2]|0;if((q|0)==0){p=401;break}else{t=q;continue}}}if((p|0)==399){c[d>>2]=t;m=u;i=f;return m|0}else if((p|0)==401){c[d>>2]=t;m=v;i=f;return m|0}}}while(0);if((o|0)==0){c[d>>2]=j;m=j|0;i=f;return m|0}else{c[d>>2]=r;m=r+4|0;i=f;return m|0}return 0}function cD(){cA(1e5);return 0}function cE(a,b){a=a|0;b=b|0;if((b|0)==0){return}else{cE(a,c[b>>2]|0);cE(a,c[b+4>>2]|0);kS(b);return}}function cF(a,b){a=a|0;b=b|0;var c=0,d=0,e=0,f=0,g=0,j=0,k=0,l=0,m=0.0;c=i;i=i+8|0;d=c|0;e=b-a|0;if((e|0)<=8){i=c;return}cY(d);f=b-8|0;if(f>>>0>a>>>0){b=a;a=e>>3;while(1){e=a-1|0;do{if((e|0)!=0){if((a|0)==0){g=cX(d)|0}else{j=32-(kZ(a|0)|0)|0;k=(((-1>>>((33-j|0)>>>0)&a|0)==0)<<31>>31)+j|0;j=(k>>>0)/((((k&31|0)!=0)+(k>>>5)|0)>>>0)|0;if((j|0)==0){l=0}else{l=-1>>>((32-j|0)>>>0)}while(1){j=(cX(d)|0)&l;if(j>>>0<a>>>0){g=j;break}}}if((g|0)==0){break}j=b+(g<<3)|0;m=+h[b>>3];h[b>>3]=+h[j>>3];h[j>>3]=m}}while(0);j=b+8|0;if(j>>>0<f>>>0){b=j;a=e}else{break}}}cW(d);i=c;return}function cG(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;e=(d|0)==(b|0);a[d+12|0]=e&1;if(e){return}else{f=d}while(1){g=f+8|0;h=c[g>>2]|0;d=h+12|0;if((a[d]&1)!=0){i=487;break}j=h+8|0;k=c[j>>2]|0;e=c[k>>2]|0;if((h|0)==(e|0)){l=c[k+4>>2]|0;if((l|0)==0){i=452;break}m=l+12|0;if((a[m]&1)!=0){i=452;break}a[d]=1;a[k+12|0]=(k|0)==(b|0)|0;a[m]=1}else{if((e|0)==0){i=469;break}m=e+12|0;if((a[m]&1)!=0){i=469;break}a[d]=1;a[k+12|0]=(k|0)==(b|0)|0;a[m]=1}if((k|0)==(b|0)){i=484;break}else{f=k}}if((i|0)==469){b=h|0;if((f|0)==(c[b>>2]|0)){m=f+4|0;d=c[m>>2]|0;c[b>>2]=d;if((d|0)==0){n=k}else{c[d+8>>2]=h;n=c[j>>2]|0}c[g>>2]=n;n=c[j>>2]|0;d=n|0;if((c[d>>2]|0)==(h|0)){c[d>>2]=f}else{c[n+4>>2]=f}c[m>>2]=h;c[j>>2]=f;o=f;p=c[g>>2]|0}else{o=h;p=k}a[o+12|0]=1;a[p+12|0]=0;o=p+4|0;g=c[o>>2]|0;m=g|0;n=c[m>>2]|0;c[o>>2]=n;if((n|0)!=0){c[n+8>>2]=p}n=p+8|0;c[g+8>>2]=c[n>>2];o=c[n>>2]|0;d=o|0;if((c[d>>2]|0)==(p|0)){c[d>>2]=g}else{c[o+4>>2]=g}c[m>>2]=p;c[n>>2]=g;return}else if((i|0)==484){return}else if((i|0)==487){return}else if((i|0)==452){if((f|0)==(c[h>>2]|0)){q=h;r=k}else{f=h+4|0;i=c[f>>2]|0;g=i|0;n=c[g>>2]|0;c[f>>2]=n;if((n|0)==0){s=k}else{c[n+8>>2]=h;s=c[j>>2]|0}n=i+8|0;c[n>>2]=s;s=c[j>>2]|0;k=s|0;if((c[k>>2]|0)==(h|0)){c[k>>2]=i}else{c[s+4>>2]=i}c[g>>2]=h;c[j>>2]=i;q=i;r=c[n>>2]|0}a[q+12|0]=1;a[r+12|0]=0;q=r|0;n=c[q>>2]|0;i=n+4|0;j=c[i>>2]|0;c[q>>2]=j;if((j|0)!=0){c[j+8>>2]=r}j=r+8|0;c[n+8>>2]=c[j>>2];q=c[j>>2]|0;h=q|0;if((c[h>>2]|0)==(r|0)){c[h>>2]=n}else{c[q+4>>2]=n}c[i>>2]=r;c[j>>2]=n;return}}function cH(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,j=0,k=0.0,l=0.0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;f=i;g=b;b=i;i=i+4|0;i=i+7>>3<<3;c[b>>2]=c[g>>2];g=a+4|0;j=c[b>>2]|0;do{if((j|0)!=(g|0)){k=+h[e>>3];l=+h[j+16>>3];if(k<l){break}if(l>=k){c[d>>2]=j;m=d;i=f;return m|0}b=j+4|0;n=c[b>>2]|0;o=(n|0)==0;if(o){p=j|0;while(1){q=c[p+8>>2]|0;if((p|0)==(c[q>>2]|0)){r=q;break}else{p=q}}}else{p=n;while(1){q=c[p>>2]|0;if((q|0)==0){r=p;break}else{p=q}}}p=g;do{if((r|0)!=(p|0)){if(k<+h[r+16>>3]){break}n=g|0;q=c[n>>2]|0;if((q|0)==0){c[d>>2]=p;m=n;i=f;return m|0}else{s=q}while(1){l=+h[s+16>>3];if(k<l){t=s|0;q=c[t>>2]|0;if((q|0)==0){u=521;break}else{s=q;continue}}if(l>=k){u=525;break}v=s+4|0;q=c[v>>2]|0;if((q|0)==0){u=524;break}else{s=q}}if((u|0)==521){c[d>>2]=s;m=t;i=f;return m|0}else if((u|0)==524){c[d>>2]=s;m=v;i=f;return m|0}else if((u|0)==525){c[d>>2]=s;m=d;i=f;return m|0}}}while(0);if(o){c[d>>2]=j;m=b;i=f;return m|0}else{c[d>>2]=r;m=r|0;i=f;return m|0}}}while(0);r=c[j>>2]|0;do{if((j|0)==(c[a>>2]|0)){w=j}else{if((r|0)==0){s=j|0;while(1){v=c[s+8>>2]|0;if((s|0)==(c[v>>2]|0)){s=v}else{x=v;break}}}else{s=r;while(1){b=c[s+4>>2]|0;if((b|0)==0){x=s;break}else{s=b}}}k=+h[e>>3];if(+h[x+16>>3]<k){w=x;break}s=g|0;b=c[s>>2]|0;if((b|0)==0){c[d>>2]=g;m=s;i=f;return m|0}else{y=b}while(1){l=+h[y+16>>3];if(k<l){z=y|0;b=c[z>>2]|0;if((b|0)==0){u=502;break}else{y=b;continue}}if(l>=k){u=506;break}A=y+4|0;b=c[A>>2]|0;if((b|0)==0){u=505;break}else{y=b}}if((u|0)==505){c[d>>2]=y;m=A;i=f;return m|0}else if((u|0)==506){c[d>>2]=y;m=d;i=f;return m|0}else if((u|0)==502){c[d>>2]=y;m=z;i=f;return m|0}}}while(0);if((r|0)==0){c[d>>2]=j;m=j|0;i=f;return m|0}else{c[d>>2]=w;m=w+4|0;i=f;return m|0}return 0}function cI(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0.0,H=0,I=0;g=i;j=d;d=i;i=i+8|0;c[d>>2]=c[j>>2];c[d+4>>2]=c[j+4>>2];j=e;e=i;i=i+8|0;c[e>>2]=c[j>>2];c[e+4>>2]=c[j+4>>2];j=f;f=i;i=i+1|0;i=i+7>>3<<3;a[f]=a[j]|0;j=d|0;f=d+4|0;k=c[f>>2]|0;l=c[e>>2]|0;m=c[e+4>>2]|0;L655:do{if((k|0)==(m|0)){n=k;o=l;p=549}else{e=c[j>>2]|0;q=e;r=k;s=c[e>>2]|0;while(1){e=r+8|0;if((e-s|0)==4096){t=q+4|0;u=c[t>>2]|0;v=t;w=u;x=u}else{v=q;w=e;x=s}if((w|0)==(m|0)){break}if(+h[r>>3]==+h[w>>3]){n=r;o=q;p=549;break L655}else{q=v;r=w;s=x}}c[j>>2]=l;c[f>>2]=m}}while(0);do{if((p|0)==549){c[j>>2]=o;c[f>>2]=n;if((n|0)==(m|0)){break}l=n+8|0;x=c[o>>2]|0;if((l-x|0)==4096){w=o+4|0;v=c[w>>2]|0;y=w;z=v;A=v;B=n;C=o}else{y=o;z=l;A=x;B=n;C=o}while(1){x=z+8|0;if((x-A|0)==4096){l=y+4|0;v=c[l>>2]|0;D=l;E=v;F=v}else{D=y;E=x;F=A}if((E|0)==(m|0)){break}G=+h[E>>3];if(+h[B>>3]==G){y=D;z=E;A=F;B=B;C=C;continue}x=B+8|0;c[f>>2]=x;if((x-(c[C>>2]|0)|0)==4096){v=C+4|0;c[j>>2]=v;l=c[v>>2]|0;c[f>>2]=l;H=v;I=l}else{H=C;I=x}h[I>>3]=G;y=D;z=E;A=c[D>>2]|0;B=I;C=H}x=B+8|0;c[f>>2]=x;if((x-(c[C>>2]|0)|0)!=4096){break}x=C+4|0;c[j>>2]=x;c[f>>2]=c[x>>2]}}while(0);f=d;d=b;b=c[f+4>>2]|0;c[d>>2]=c[f>>2];c[d+4>>2]=b;i=g;return}function cJ(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0.0,x=0,y=0,z=0,A=0,B=0,C=0.0,D=0;g=i;i=i+48|0;j=b;b=i;i=i+4|0;i=i+7>>3<<3;c[b>>2]=c[j>>2];j=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[j>>2];j=g|0;k=g+8|0;l=g+16|0;m=g+24|0;n=g+32|0;o=g+40|0;switch(e|0){case 2:{p=d|0;q=c[c[p>>2]>>2]|0;c[p>>2]=q;p=c[b>>2]|0;if(+h[q+8>>3]<+h[p+8>>3]){r=q+4|0;s=q|0;c[(c[s>>2]|0)+4>>2]=c[r>>2];c[c[r>>2]>>2]=c[s>>2];t=p|0;c[(c[t>>2]|0)+4>>2]=q;c[s>>2]=c[t>>2];c[t>>2]=q;c[r>>2]=p;c[a>>2]=q;i=g;return}else{c[a>>2]=p;i=g;return}break};case 0:case 1:{c[a>>2]=c[b>>2];i=g;return};default:{p=e>>>1;q=b|0;b=c[q>>2]|0;if((p|0)==0){u=b}else{r=p;t=b;while(1){s=r-1|0;v=c[t+4>>2]|0;if((s|0)>0){r=s;t=v}else{u=v;break}}}c[k>>2]=b;c[l>>2]=u;cJ(j,k,l,p,f);l=c[j>>2]|0;c[q>>2]=l;c[n>>2]=u;u=d|0;d=c[u>>2]|0;c[o>>2]=d;cJ(m,n,o,e-p|0,f);f=c[m>>2]|0;w=+h[l+8>>3];if(+h[f+8>>3]<w){m=c[f+4>>2]|0;L697:do{if((m|0)==(d|0)){x=d}else{p=m;while(1){if(+h[p+8>>3]>=w){x=p;break L697}e=c[p+4>>2]|0;if((e|0)==(d|0)){x=d;break}else{p=e}}}}while(0);m=c[x>>2]|0;p=m+4|0;e=f|0;c[(c[e>>2]|0)+4>>2]=c[p>>2];c[c[p>>2]>>2]=c[e>>2];o=c[l+4>>2]|0;n=l|0;c[(c[n>>2]|0)+4>>2]=f;c[e>>2]=c[n>>2];c[n>>2]=m;c[p>>2]=l;y=f;z=x;A=o}else{y=l;z=f;A=c[l+4>>2]|0}c[q>>2]=A;L704:do{if((A|0)!=(z|0)){l=z;f=z;o=A;x=d;while(1){p=l+8|0;if((l|0)==(x|0)){break L704}else{B=o}while(1){C=+h[B+8>>3];if(+h[p>>3]<C){break}m=c[B+4>>2]|0;c[q>>2]=m;if((m|0)==(f|0)){break L704}else{B=m}}p=c[l+4>>2]|0;L711:do{if((p|0)==(x|0)){D=x}else{m=p;while(1){if(+h[m+8>>3]>=C){D=m;break L711}n=c[m+4>>2]|0;if((n|0)==(x|0)){D=x;break}else{m=n}}}}while(0);p=c[D>>2]|0;m=p+4|0;n=l|0;c[(c[n>>2]|0)+4>>2]=c[m>>2];c[c[m>>2]>>2]=c[n>>2];e=c[B+4>>2]|0;j=(f|0)==(l|0)?D:f;k=B|0;c[(c[k>>2]|0)+4>>2]=l;c[n>>2]=c[k>>2];c[k>>2]=p;c[m>>2]=B;c[q>>2]=e;if((e|0)==(j|0)){break L704}l=D;f=j;o=e;x=c[u>>2]|0}}}while(0);c[a>>2]=y;i=g;return}}}function cK(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0,az=0,aA=0,aB=0,aC=0,aD=0,aE=0,aF=0,aG=0.0,aH=0.0,aI=0.0,aJ=0.0,aK=0.0,aL=0,aM=0,aN=0,aO=0,aP=0,aQ=0,aR=0,aS=0.0,aT=0,aU=0,aV=0,aW=0,aX=0,aY=0,aZ=0,a_=0,a$=0,a0=0.0,a1=0,a2=0,a3=0,a4=0,a5=0,a6=0,a7=0,a8=0.0,a9=0,ba=0,bb=0,bc=0,bd=0.0,be=0,bf=0,bg=0,bh=0,bi=0,bj=0,bk=0,bl=0,bm=0,bn=0,bo=0,bp=0,bq=0,br=0,bs=0,bt=0,bu=0.0,bv=0,bw=0,bx=0,by=0,bz=0.0,bA=0,bB=0,bC=0,bD=0,bE=0,bF=0,bG=0,bH=0,bI=0,bJ=0,bK=0,bL=0,bM=0,bN=0,bO=0,bP=0,bQ=0.0,bR=0,bS=0,bT=0;e=i;i=i+8|0;f=a;a=i;i=i+8|0;c[a>>2]=c[f>>2];c[a+4>>2]=c[f+4>>2];f=b;b=i;i=i+8|0;c[b>>2]=c[f>>2];c[b+4>>2]=c[f+4>>2];f=e|0;g=f;j=i;i=i+8|0;k=j;l=i;i=i+8|0;m=l;n=i;i=i+8|0;o=n;p=i;i=i+8|0;q=p;r=i;i=i+8|0;s=r;t=i;i=i+8|0;u=t;v=i;i=i+8|0;w=v;x=i;i=i+8|0;y=x;z=i;i=i+8|0;A=i;i=i+8|0;B=i;i=i+8|0;C=i;i=i+8|0;D=C;E=i;i=i+8|0;F=i;i=i+8|0;G=i;i=i+8|0;H=G;I=i;i=i+8|0;J=I;K=i;i=i+8|0;L=i;i=i+8|0;M=i;i=i+8|0;N=M;O=b+4|0;P=c[O>>2]|0;Q=a+4|0;R=c[Q>>2]|0;if((P|0)==(R|0)){i=e;return}S=b|0;T=a|0;U=a;a=y+4|0;V=x;W=z|0;X=z+4|0;Y=A|0;Z=A+4|0;_=B|0;$=B+4|0;aa=E|0;ab=E+4|0;ac=F|0;ad=F+4|0;ae=b;b=K|0;af=K+4|0;ag=L|0;ah=L+4|0;ai=P;P=R;L725:while(1){aj=c[S>>2]|0;R=c[T>>2]|0;ak=c[aj>>2]|0;al=c[R>>2]|0;am=P-al>>3;an=(ai-ak>>3)+(aj-R<<7)-am|0;switch(an|0){case 5:{ao=630;break L725;break};case 4:{ao=610;break L725;break};case 2:{ao=593;break L725;break};case 3:{ao=597;break L725;break};case 0:case 1:{ao=760;break L725;break};default:{}}if((an|0)<31){ao=643;break}if((ai|0)==(ak|0)){ap=aj-4|0;aq=ap;ar=(c[ap>>2]|0)+4096|0}else{aq=aj;ar=ai}ap=ar-8|0;as=(an|0)/2|0;at=(an+1|0)>>>0>2;do{if((an|0)>999){do{if(at){au=am+as|0;if((au|0)>0){av=R+(((au|0)/512|0)<<2)|0;aw=c[av>>2]|0;ax=aw+(((au|0)%512|0)<<3)|0;ay=av;az=aw;break}else{aw=511-au|0;au=R+(((aw|0)/-512|0)<<2)|0;av=c[au>>2]|0;ax=av+(511-((aw|0)%512|0)<<3)|0;ay=au;az=av;break}}else{ax=P;ay=R;az=al}}while(0);av=(an|0)/4|0;au=c[U>>2]|0;aw=c[U+4>>2]|0;c[v>>2]=au;c[v+4>>2]=aw;c[x>>2]=au;c[x+4>>2]=aw;aA=(an+3|0)>>>0>6;aB=au;do{if(aA){au=(aw-(c[aB>>2]|0)>>3)+av|0;if((au|0)>0){aC=aB+(((au|0)/512|0)<<2)|0;c[V>>2]=aC;c[a>>2]=(c[aC>>2]|0)+(((au|0)%512|0)<<3);c[W>>2]=ay;c[X>>2]=ax;c[Y>>2]=ay;c[Z>>2]=ax;if(!aA){break}}else{aC=511-au|0;au=aB+(((aC|0)/-512|0)<<2)|0;c[V>>2]=au;c[a>>2]=(c[au>>2]|0)+(511-((aC|0)%512|0)<<3);c[W>>2]=ay;c[X>>2]=ax;c[Y>>2]=ay;c[Z>>2]=ax}aC=(ax-az>>3)+av|0;if((aC|0)>0){au=ay+(((aC|0)/512|0)<<2)|0;c[Y>>2]=au;c[Z>>2]=(c[au>>2]|0)+(((aC|0)%512|0)<<3);break}else{au=511-aC|0;aC=ay+(((au|0)/-512|0)<<2)|0;c[Y>>2]=aC;c[Z>>2]=(c[aC>>2]|0)+(511-((au|0)%512|0)<<3);break}}else{c[W>>2]=ay;c[X>>2]=ax;c[Y>>2]=ay;c[Z>>2]=ax}}while(0);c[_>>2]=aq;c[$>>2]=ap;aD=cL(w,y,z,A,B,0)|0;aE=ax}else{do{if(at){av=am+as|0;if((av|0)>0){aF=(c[R+(((av|0)/512|0)<<2)>>2]|0)+(((av|0)%512|0)<<3)|0;break}else{aB=511-av|0;aF=(c[R+(((aB|0)/-512|0)<<2)>>2]|0)+(511-((aB|0)%512|0)<<3)|0;break}}else{aF=P}}while(0);aB=c[U+4>>2]|0;aG=+h[aF>>3];aH=+h[aB>>3];aI=+h[ap>>3];av=aI<aG;if(aG>=aH){if(!av){aD=0;aE=aF;break}h[aF>>3]=aI;h[ap>>3]=aG;aJ=+h[aF>>3];aK=+h[aB>>3];if(aJ>=aK){aD=1;aE=aF;break}h[aB>>3]=aJ;h[aF>>3]=aK;aD=2;aE=aF;break}if(av){h[aB>>3]=aI;h[ap>>3]=aH;aD=1;aE=aF;break}h[aB>>3]=aG;h[aF>>3]=aH;aG=+h[ap>>3];if(aG>=aH){aD=1;aE=aF;break}h[aF>>3]=aG;h[ap>>3]=aH;aD=2;aE=aF}}while(0);R=c[T>>2]|0;as=c[Q>>2]|0;aH=+h[as>>3];aG=+h[aE>>3];do{if(aH<aG){aL=aD;aM=aq;aN=ap;ao=707}else{am=aq;at=ap;an=c[aq>>2]|0;while(1){if((at|0)==(an|0)){al=am-4|0;aB=c[al>>2]|0;aO=al;aP=aB+4096|0;aQ=aB}else{aO=am;aP=at;aQ=an}aR=aP-8|0;if((as|0)==(aR|0)){break}aS=+h[aR>>3];if(aS<aG){ao=706;break}else{am=aO;at=aR;an=aQ}}if((ao|0)==706){ao=0;h[as>>3]=aS;h[aR>>3]=aH;aL=aD+1|0;aM=aO;aN=aR;ao=707;break}an=as+8|0;at=c[R>>2]|0;if((an-at|0)==4096){am=R+4|0;aB=c[am>>2]|0;aT=aB;aU=am;aV=aB}else{aT=an;aU=R;aV=at}at=c[S>>2]|0;an=c[O>>2]|0;if((an|0)==(c[at>>2]|0)){aB=at-4|0;aW=aB;aX=(c[aB>>2]|0)+4096|0}else{aW=at;aX=an}an=aX-8|0;aI=+h[an>>3];do{if(aH<aI){aY=aT;aZ=aU;a_=aV}else{at=aT;aB=aU;am=aV;L786:while(1){a$=at;while(1){if((a$|0)==(an|0)){ao=767;break L725}a0=+h[a$>>3];if(aH<a0){break L786}al=a$+8|0;if((al-am|0)==4096){break}else{a$=al}}al=aB+4|0;av=c[al>>2]|0;at=av;aB=al;am=av}h[a$>>3]=aI;h[an>>3]=a0;am=a$+8|0;at=c[aB>>2]|0;if((am-at|0)!=4096){aY=am;aZ=aB;a_=at;break}at=aB+4|0;am=c[at>>2]|0;aY=am;aZ=at;a_=am}}while(0);if((aY|0)==(an|0)){ao=768;break L725}else{a1=aY;a2=aZ;a3=aW;a4=an;a5=a_}while(1){aI=+h[c[Q>>2]>>3];am=a1;a6=a2;at=a5;L798:while(1){a7=am;while(1){a8=+h[a7>>3];if(aI<a8){break L798}av=a7+8|0;if((av-at|0)==4096){break}else{a7=av}}av=a6+4|0;al=c[av>>2]|0;am=al;a6=av;at=al}at=a3;am=a4;aB=c[a3>>2]|0;while(1){if((am|0)==(aB|0)){al=at-4|0;av=c[al>>2]|0;a9=al;ba=av+4096|0;bb=av}else{a9=at;ba=am;bb=aB}bc=ba-8|0;bd=+h[bc>>3];if(aI<bd){at=a9;am=bc;aB=bb}else{break}}if(a6>>>0>=a9>>>0){if(a7>>>0>=bc>>>0|(a6|0)!=(a9|0)){break}}h[a7>>3]=bd;h[bc>>3]=a8;aB=a7+8|0;am=c[a6>>2]|0;if((aB-am|0)!=4096){a1=aB;a2=a6;a3=a9;a4=bc;a5=am;continue}am=a6+4|0;aB=c[am>>2]|0;a1=aB;a2=am;a3=a9;a4=bc;a5=aB}c[T>>2]=a6;c[Q>>2]=a7;be=a7}}while(0);L816:do{if((ao|0)==707){ao=0;ap=as+8|0;an=c[R>>2]|0;if((ap-an|0)==4096){aB=R+4|0;am=c[aB>>2]|0;bf=am;bg=aB;bh=am}else{bf=ap;bg=R;bh=an}if(bg>>>0<aM>>>0){bi=aL;bj=bf;bk=bg;bl=aE;bm=aM;bn=aN;bo=bh;ao=711}else{if((bg|0)==(aM|0)&bf>>>0<aN>>>0){bi=aL;bj=bf;bk=bg;bl=aE;bm=aM;bn=aN;bo=bh;ao=711}else{bp=aL;bq=bf;br=bg;bs=aE}}L823:do{if((ao|0)==711){while(1){ao=0;aH=+h[bl>>3];an=bj;ap=bk;am=bo;L826:while(1){bt=an;while(1){bu=+h[bt>>3];if(bu>=aH){break L826}aB=bt+8|0;if((aB-am|0)==4096){break}else{bt=aB}}aB=ap+4|0;at=c[aB>>2]|0;an=at;ap=aB;am=at}am=bm;an=bn;at=c[bm>>2]|0;while(1){if((an|0)==(at|0)){aB=am-4|0;av=c[aB>>2]|0;bv=aB;bw=av+4096|0;bx=av}else{bv=am;bw=an;bx=at}by=bw-8|0;bz=+h[by>>3];if(bz>=aH){am=bv;an=by;at=bx}else{break}}if(bv>>>0<ap>>>0){bp=bi;bq=bt;br=ap;bs=bl;break L823}if((bv|0)==(ap|0)&by>>>0<bt>>>0){bp=bi;bq=bt;br=ap;bs=bl;break L823}h[bt>>3]=bz;h[by>>3]=bu;at=bi+1|0;an=(bl|0)==(bt|0)?by:bl;am=bt+8|0;av=c[ap>>2]|0;if((am-av|0)!=4096){bi=at;bj=am;bk=ap;bl=an;bm=bv;bn=by;bo=av;ao=711;continue}av=ap+4|0;am=c[av>>2]|0;bi=at;bj=am;bk=av;bl=an;bm=bv;bn=by;bo=am;ao=711}}}while(0);do{if((bq|0)==(bs|0)){bA=bp}else{aH=+h[bs>>3];aG=+h[bq>>3];if(aH>=aG){bA=bp;break}h[bq>>3]=aH;h[bs>>3]=aG;bA=bp+1|0}}while(0);do{if((bA|0)==0){am=c[U+4>>2]|0;c[C>>2]=c[U>>2];c[C+4>>2]=am;c[aa>>2]=br;c[ab>>2]=bq;am=cN(D,E,0)|0;c[ac>>2]=br;c[ad>>2]=bq;an=bq-(c[br>>2]|0)>>3;av=an+1|0;if((av|0)>0){at=br+(((av|0)/512|0)<<2)|0;c[ac>>2]=at;bB=(c[at>>2]|0)+(((av|0)%512|0)<<3)|0}else{av=510-an|0;an=br+(((av|0)/-512|0)<<2)|0;c[ac>>2]=an;bB=(c[an>>2]|0)+(511-((av|0)%512|0)<<3)|0}c[ad>>2]=bB;av=c[ae+4>>2]|0;c[G>>2]=c[ae>>2];c[G+4>>2]=av;if(cN(F,H,0)|0){if(am){ao=770;break L725}c[S>>2]=br;c[O>>2]=bq;be=c[Q>>2]|0;break L816}if(!am){break}am=bq+8|0;if((am-(c[br>>2]|0)|0)==4096){av=br+4|0;bC=c[av>>2]|0;bD=av}else{bC=am;bD=br}c[T>>2]=bD;c[Q>>2]=bC;be=bC;break L816}}while(0);am=c[Q>>2]|0;if((bq|0)==(am|0)){bE=0}else{av=c[T>>2]|0;bE=(bq-(c[br>>2]|0)>>3)+(br-av<<7)-(am-(c[av>>2]|0)>>3)|0}av=c[O>>2]|0;if((av|0)==(bq|0)){bF=0}else{an=c[S>>2]|0;bF=(av-(c[an>>2]|0)>>3)+(an-br<<7)-(bq-(c[br>>2]|0)>>3)|0}if((bE|0)<(bF|0)){an=c[U+4>>2]|0;c[I>>2]=c[U>>2];c[I+4>>2]=an;c[b>>2]=br;c[af>>2]=bq;cK(J,K,d);an=bq+8|0;if((an-(c[br>>2]|0)|0)==4096){av=br+4|0;bG=c[av>>2]|0;bH=av}else{bG=an;bH=br}c[T>>2]=bH;c[Q>>2]=bG;be=bG;break}c[ag>>2]=br;c[ah>>2]=bq;an=bq-(c[br>>2]|0)>>3;av=an+1|0;if((av|0)>0){at=br+(((av|0)/512|0)<<2)|0;c[ag>>2]=at;bI=(c[at>>2]|0)+(((av|0)%512|0)<<3)|0}else{av=510-an|0;an=br+(((av|0)/-512|0)<<2)|0;c[ag>>2]=an;bI=(c[an>>2]|0)+(511-((av|0)%512|0)<<3)|0}c[ah>>2]=bI;av=c[ae+4>>2]|0;c[M>>2]=c[ae>>2];c[M+4>>2]=av;cK(L,N,d);c[S>>2]=br;c[O>>2]=bq;be=am}}while(0);R=c[O>>2]|0;if((R|0)==(be|0)){ao=769;break}else{ai=R;P=be}}if((ao|0)==630){be=c[U>>2]|0;bq=c[U+4>>2]|0;c[f>>2]=be;c[f+4>>2]=bq;c[j>>2]=be;c[j+4>>2]=bq;f=bq;br=j;j=be;d=f-(c[j>>2]|0)>>3;N=d+1|0;if((N|0)>0){L=j+(((N|0)/512|0)<<2)|0;c[br>>2]=L;bJ=(c[L>>2]|0)+(((N|0)%512|0)<<3)|0}else{N=510-d|0;d=j+(((N|0)/-512|0)<<2)|0;c[br>>2]=d;bJ=(c[d>>2]|0)+(511-((N|0)%512|0)<<3)|0}c[k+4>>2]=bJ;c[l>>2]=be;c[l+4>>2]=bq;bJ=l;l=f-(c[j>>2]|0)>>3;N=l+2|0;if((N|0)>0){d=j+(((N|0)/512|0)<<2)|0;c[bJ>>2]=d;bK=(c[d>>2]|0)+(((N|0)%512|0)<<3)|0}else{N=509-l|0;l=j+(((N|0)/-512|0)<<2)|0;c[bJ>>2]=l;bK=(c[l>>2]|0)+(511-((N|0)%512|0)<<3)|0}c[m+4>>2]=bK;c[n>>2]=be;c[n+4>>2]=bq;bq=n;n=f-(c[j>>2]|0)>>3;f=n+3|0;if((f|0)>0){be=j+(((f|0)/512|0)<<2)|0;c[bq>>2]=be;bL=(c[be>>2]|0)+(((f|0)%512|0)<<3)|0}else{f=508-n|0;n=j+(((f|0)/-512|0)<<2)|0;c[bq>>2]=n;bL=(c[n>>2]|0)+(511-((f|0)%512|0)<<3)|0}c[o+4>>2]=bL;if((ai|0)==(ak|0)){bL=aj-4|0;c[S>>2]=bL;f=(c[bL>>2]|0)+4096|0;c[O>>2]=f;bM=f}else{bM=ai}c[O>>2]=bM-8;bM=c[ae+4>>2]|0;c[p>>2]=c[ae>>2];c[p+4>>2]=bM;cL(g,k,m,o,q,0)|0;i=e;return}else if((ao|0)==610){q=c[U>>2]|0;o=c[U+4>>2]|0;m=o;k=o-(c[q>>2]|0)>>3;o=k+1|0;if((o|0)>0){bN=(c[q+(((o|0)/512|0)<<2)>>2]|0)+(((o|0)%512|0)<<3)|0}else{o=510-k|0;bN=(c[q+(((o|0)/-512|0)<<2)>>2]|0)+(511-((o|0)%512|0)<<3)|0}o=k+2|0;if((o|0)>0){bO=(c[q+(((o|0)/512|0)<<2)>>2]|0)+(((o|0)%512|0)<<3)|0}else{o=509-k|0;bO=(c[q+(((o|0)/-512|0)<<2)>>2]|0)+(511-((o|0)%512|0)<<3)|0}if((ai|0)==(ak|0)){o=aj-4|0;c[S>>2]=o;q=(c[o>>2]|0)+4096|0;c[O>>2]=q;bP=q}else{bP=ai}c[O>>2]=bP-8;bP=c[ae+4>>2]|0;bu=+h[bN>>3];bz=+h[m>>3];a8=+h[bO>>3];q=a8<bu;do{if(bu<bz){if(q){h[m>>3]=a8;h[bO>>3]=bz;bQ=bz;break}h[m>>3]=bu;h[bN>>3]=bz;bd=+h[bO>>3];if(bd>=bz){bQ=bd;break}h[bN>>3]=bd;h[bO>>3]=bz;bQ=bz}else{if(!q){bQ=a8;break}h[bN>>3]=a8;h[bO>>3]=bu;bd=+h[bN>>3];a0=+h[m>>3];if(bd>=a0){bQ=bu;break}h[m>>3]=bd;h[bN>>3]=a0;bQ=+h[bO>>3]}}while(0);bu=+h[bP>>3];if(bu>=bQ){i=e;return}h[bO>>3]=bu;h[bP>>3]=bQ;bQ=+h[bO>>3];bu=+h[bN>>3];if(bQ>=bu){i=e;return}h[bN>>3]=bQ;h[bO>>3]=bu;bu=+h[bN>>3];bQ=+h[m>>3];if(bu>=bQ){i=e;return}h[m>>3]=bu;h[bN>>3]=bQ;i=e;return}else if((ao|0)==643){bN=c[U+4>>2]|0;c[r>>2]=c[U>>2];c[r+4>>2]=bN;bN=c[ae+4>>2]|0;c[t>>2]=c[ae>>2];c[t+4>>2]=bN;cM(s,u,0);i=e;return}else if((ao|0)==593){if((ai|0)==(ak|0)){u=aj-4|0;c[S>>2]=u;s=(c[u>>2]|0)+4096|0;c[O>>2]=s;bR=s}else{bR=ai}s=bR-8|0;c[O>>2]=s;bQ=+h[s>>3];bu=+h[P>>3];if(bQ>=bu){i=e;return}h[P>>3]=bQ;h[s>>3]=bu;i=e;return}else if((ao|0)==597){s=c[U>>2]|0;P=c[U+4>>2]|0;U=P;bR=P-(c[s>>2]|0)>>3;P=bR+1|0;if((P|0)>0){bS=(c[s+(((P|0)/512|0)<<2)>>2]|0)+(((P|0)%512|0)<<3)|0}else{P=510-bR|0;bS=(c[s+(((P|0)/-512|0)<<2)>>2]|0)+(511-((P|0)%512|0)<<3)|0}if((ai|0)==(ak|0)){ak=aj-4|0;c[S>>2]=ak;S=(c[ak>>2]|0)+4096|0;c[O>>2]=S;bT=S}else{bT=ai}c[O>>2]=bT-8;bT=c[ae+4>>2]|0;bu=+h[bS>>3];bQ=+h[U>>3];a8=+h[bT>>3];ae=a8<bu;if(bu>=bQ){if(!ae){i=e;return}h[bS>>3]=a8;h[bT>>3]=bu;bz=+h[bS>>3];a0=+h[U>>3];if(bz>=a0){i=e;return}h[U>>3]=bz;h[bS>>3]=a0;i=e;return}if(ae){h[U>>3]=a8;h[bT>>3]=bQ;i=e;return}h[U>>3]=bu;h[bS>>3]=bQ;bu=+h[bT>>3];if(bu>=bQ){i=e;return}h[bS>>3]=bu;h[bT>>3]=bQ;i=e;return}else if((ao|0)==760){i=e;return}else if((ao|0)==767){i=e;return}else if((ao|0)==768){i=e;return}else if((ao|0)==769){i=e;return}else if((ao|0)==770){i=e;return}}function cL(a,b,d,e,f,g){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var j=0,k=0,l=0,m=0,n=0.0,o=0.0,p=0.0,q=0,r=0,s=0.0,t=0.0,u=0.0,v=0,w=0;g=i;j=a;a=i;i=i+8|0;c[a>>2]=c[j>>2];c[a+4>>2]=c[j+4>>2];j=b;b=i;i=i+8|0;c[b>>2]=c[j>>2];c[b+4>>2]=c[j+4>>2];j=d;d=i;i=i+8|0;c[d>>2]=c[j>>2];c[d+4>>2]=c[j+4>>2];j=e;e=i;i=i+8|0;c[e>>2]=c[j>>2];c[e+4>>2]=c[j+4>>2];j=f;f=i;i=i+8|0;c[f>>2]=c[j>>2];c[f+4>>2]=c[j+4>>2];j=c[a+4>>2]|0;k=c[b+4>>2]|0;l=c[d+4>>2]|0;m=c[e+4>>2]|0;n=+h[k>>3];o=+h[j>>3];p=+h[l>>3];q=p<n;do{if(n<o){if(q){h[j>>3]=p;h[l>>3]=o;r=1;s=o;break}h[j>>3]=n;h[k>>3]=o;t=+h[l>>3];if(t>=o){r=1;s=t;break}h[k>>3]=t;h[l>>3]=o;r=2;s=o}else{if(!q){r=0;s=p;break}h[k>>3]=p;h[l>>3]=n;t=+h[k>>3];u=+h[j>>3];if(t>=u){r=1;s=n;break}h[j>>3]=t;h[k>>3]=u;r=2;s=+h[l>>3]}}while(0);n=+h[m>>3];do{if(n<s){h[l>>3]=n;h[m>>3]=s;p=+h[l>>3];o=+h[k>>3];if(p>=o){v=r+1|0;break}h[k>>3]=p;h[l>>3]=o;o=+h[k>>3];p=+h[j>>3];if(o>=p){v=r+2|0;break}h[j>>3]=o;h[k>>3]=p;v=r+3|0}else{v=r}}while(0);r=c[f+4>>2]|0;f=c[e+4>>2]|0;s=+h[r>>3];n=+h[f>>3];if(s>=n){w=v;i=g;return w|0}h[f>>3]=s;h[r>>3]=n;r=c[d+4>>2]|0;n=+h[f>>3];s=+h[r>>3];if(n>=s){w=v+1|0;i=g;return w|0}h[r>>3]=n;h[f>>3]=s;f=c[b+4>>2]|0;s=+h[r>>3];n=+h[f>>3];if(s>=n){w=v+2|0;i=g;return w|0}h[f>>3]=s;h[r>>3]=n;r=c[a+4>>2]|0;n=+h[f>>3];s=+h[r>>3];if(n>=s){w=v+3|0;i=g;return w|0}h[r>>3]=n;h[f>>3]=s;w=v+4|0;i=g;return w|0}function cM(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0.0,p=0.0,q=0.0,r=0.0,s=0.0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;d=i;e=a;a=i;i=i+8|0;c[a>>2]=c[e>>2];c[a+4>>2]=c[e+4>>2];e=b;b=i;i=i+8|0;c[b>>2]=c[e>>2];c[b+4>>2]=c[e+4>>2];e=c[a>>2]|0;f=a+4|0;g=(c[f>>2]|0)-(c[e>>2]|0)>>3;j=g+2|0;if((j|0)>0){k=e+(((j|0)/512|0)<<2)|0;l=(c[k>>2]|0)+(((j|0)%512|0)<<3)|0;m=k}else{k=509-g|0;g=e+(((k|0)/-512|0)<<2)|0;l=(c[g>>2]|0)+(511-((k|0)%512|0)<<3)|0;m=g}g=a;a=c[g>>2]|0;k=c[g+4>>2]|0;g=k;e=k-(c[a>>2]|0)>>3;k=e+1|0;if((k|0)>0){n=(c[a+(((k|0)/512|0)<<2)>>2]|0)+(((k|0)%512|0)<<3)|0}else{k=510-e|0;n=(c[a+(((k|0)/-512|0)<<2)>>2]|0)+(511-((k|0)%512|0)<<3)|0}o=+h[n>>3];p=+h[g>>3];q=+h[l>>3];k=q<o;do{if(o<p){if(k){h[g>>3]=q;h[l>>3]=p;break}h[g>>3]=o;h[n>>3]=p;r=+h[l>>3];if(r>=p){break}h[n>>3]=r;h[l>>3]=p}else{if(!k){break}h[n>>3]=q;h[l>>3]=o;r=+h[n>>3];s=+h[g>>3];if(r>=s){break}h[g>>3]=r;h[n>>3]=s}}while(0);n=l-(c[m>>2]|0)>>3;g=n+1|0;if((g|0)>0){k=m+(((g|0)/512|0)<<2)|0;a=c[k>>2]|0;t=k;u=a+(((g|0)%512|0)<<3)|0;v=a}else{a=510-n|0;n=m+(((a|0)/-512|0)<<2)|0;g=c[n>>2]|0;t=n;u=g+(511-((a|0)%512|0)<<3)|0;v=g}g=b+4|0;b=l;l=t;t=u;u=m;m=v;L1022:while(1){v=b;a=t;n=u;k=m;while(1){if((a|0)==(c[g>>2]|0)){break L1022}o=+h[a>>3];q=+h[v>>3];if(o<q){e=n;j=v;w=a;p=q;while(1){h[w>>3]=p;if((j|0)==(c[f>>2]|0)){break}if((j|0)==(c[e>>2]|0)){x=e-4|0;y=x;z=(c[x>>2]|0)+4096|0}else{y=e;z=j}x=z-8|0;q=+h[x>>3];if(o<q){e=y;w=j;j=x;p=q}else{break}}h[j>>3]=o;A=c[l>>2]|0}else{A=k}w=a+8|0;if((w-A|0)==4096){break}else{v=a;a=w;n=l;k=A}}k=l+4|0;n=c[k>>2]|0;b=a;u=l;l=k;t=n;m=n}i=d;return}function cN(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0.0,E=0.0,F=0,G=0,H=0.0,I=0.0,J=0.0,K=0,L=0,M=0,N=0.0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0;d=i;i=i+8|0;e=a;a=i;i=i+8|0;c[a>>2]=c[e>>2];c[a+4>>2]=c[e+4>>2];e=b;b=i;i=i+8|0;c[b>>2]=c[e>>2];c[b+4>>2]=c[e+4>>2];e=d|0;f=e;g=i;i=i+8|0;j=g;k=i;i=i+8|0;l=k;m=i;i=i+8|0;n=m;o=i;i=i+8|0;p=o;q=b+4|0;r=c[q>>2]|0;s=a+4|0;t=c[s>>2]|0;if((r|0)==(t|0)){u=1;i=d;return u|0}v=b|0;w=c[v>>2]|0;x=c[a>>2]|0;y=c[w>>2]|0;z=t-(c[x>>2]|0)>>3;switch((r-y>>3)+(w-x<<7)-z|0){case 2:{if((r|0)==(y|0)){A=w-4|0;c[v>>2]=A;B=(c[A>>2]|0)+4096|0;c[q>>2]=B;C=B}else{C=r}B=C-8|0;c[q>>2]=B;D=+h[B>>3];E=+h[t>>3];if(D>=E){u=1;i=d;return u|0}h[t>>3]=D;h[B>>3]=E;u=1;i=d;return u|0};case 0:case 1:{u=1;i=d;return u|0};case 3:{B=a;t=c[B>>2]|0;C=c[B+4>>2]|0;B=C;A=C-(c[t>>2]|0)>>3;C=A+1|0;if((C|0)>0){F=(c[t+(((C|0)/512|0)<<2)>>2]|0)+(((C|0)%512|0)<<3)|0}else{C=510-A|0;F=(c[t+(((C|0)/-512|0)<<2)>>2]|0)+(511-((C|0)%512|0)<<3)|0}if((r|0)==(y|0)){C=w-4|0;c[v>>2]=C;t=(c[C>>2]|0)+4096|0;c[q>>2]=t;G=t}else{G=r}c[q>>2]=G-8;G=c[b+4>>2]|0;E=+h[F>>3];D=+h[B>>3];H=+h[G>>3];t=H<E;if(E>=D){if(!t){u=1;i=d;return u|0}h[F>>3]=H;h[G>>3]=E;I=+h[F>>3];J=+h[B>>3];if(I>=J){u=1;i=d;return u|0}h[B>>3]=I;h[F>>3]=J;u=1;i=d;return u|0}if(t){h[B>>3]=H;h[G>>3]=D;u=1;i=d;return u|0}h[B>>3]=E;h[F>>3]=D;E=+h[G>>3];if(E>=D){u=1;i=d;return u|0}h[F>>3]=E;h[G>>3]=D;u=1;i=d;return u|0};case 4:{G=a;F=c[G>>2]|0;B=c[G+4>>2]|0;G=B;t=B-(c[F>>2]|0)>>3;B=t+1|0;if((B|0)>0){K=(c[F+(((B|0)/512|0)<<2)>>2]|0)+(((B|0)%512|0)<<3)|0}else{B=510-t|0;K=(c[F+(((B|0)/-512|0)<<2)>>2]|0)+(511-((B|0)%512|0)<<3)|0}B=t+2|0;if((B|0)>0){L=(c[F+(((B|0)/512|0)<<2)>>2]|0)+(((B|0)%512|0)<<3)|0}else{B=509-t|0;L=(c[F+(((B|0)/-512|0)<<2)>>2]|0)+(511-((B|0)%512|0)<<3)|0}if((r|0)==(y|0)){B=w-4|0;c[v>>2]=B;F=(c[B>>2]|0)+4096|0;c[q>>2]=F;M=F}else{M=r}c[q>>2]=M-8;M=c[b+4>>2]|0;D=+h[K>>3];E=+h[G>>3];H=+h[L>>3];F=H<D;do{if(D<E){if(F){h[G>>3]=H;h[L>>3]=E;N=E;break}h[G>>3]=D;h[K>>3]=E;J=+h[L>>3];if(J>=E){N=J;break}h[K>>3]=J;h[L>>3]=E;N=E}else{if(!F){N=H;break}h[K>>3]=H;h[L>>3]=D;J=+h[K>>3];I=+h[G>>3];if(J>=I){N=D;break}h[G>>3]=J;h[K>>3]=I;N=+h[L>>3]}}while(0);D=+h[M>>3];if(D>=N){u=1;i=d;return u|0}h[L>>3]=D;h[M>>3]=N;N=+h[L>>3];D=+h[K>>3];if(N>=D){u=1;i=d;return u|0}h[K>>3]=N;h[L>>3]=D;D=+h[K>>3];N=+h[G>>3];if(D>=N){u=1;i=d;return u|0}h[G>>3]=D;h[K>>3]=N;u=1;i=d;return u|0};case 5:{K=a;G=c[K>>2]|0;L=c[K+4>>2]|0;c[e>>2]=G;c[e+4>>2]=L;c[g>>2]=G;c[g+4>>2]=L;e=L;K=g;g=G;M=e-(c[g>>2]|0)>>3;F=M+1|0;if((F|0)>0){B=g+(((F|0)/512|0)<<2)|0;c[K>>2]=B;O=(c[B>>2]|0)+(((F|0)%512|0)<<3)|0}else{F=510-M|0;M=g+(((F|0)/-512|0)<<2)|0;c[K>>2]=M;O=(c[M>>2]|0)+(511-((F|0)%512|0)<<3)|0}c[j+4>>2]=O;c[k>>2]=G;c[k+4>>2]=L;O=k;k=e-(c[g>>2]|0)>>3;F=k+2|0;if((F|0)>0){M=g+(((F|0)/512|0)<<2)|0;c[O>>2]=M;P=(c[M>>2]|0)+(((F|0)%512|0)<<3)|0}else{F=509-k|0;k=g+(((F|0)/-512|0)<<2)|0;c[O>>2]=k;P=(c[k>>2]|0)+(511-((F|0)%512|0)<<3)|0}c[l+4>>2]=P;c[m>>2]=G;c[m+4>>2]=L;L=m;m=e-(c[g>>2]|0)>>3;e=m+3|0;if((e|0)>0){G=g+(((e|0)/512|0)<<2)|0;c[L>>2]=G;Q=(c[G>>2]|0)+(((e|0)%512|0)<<3)|0}else{e=508-m|0;m=g+(((e|0)/-512|0)<<2)|0;c[L>>2]=m;Q=(c[m>>2]|0)+(511-((e|0)%512|0)<<3)|0}c[n+4>>2]=Q;if((r|0)==(y|0)){y=w-4|0;c[v>>2]=y;v=(c[y>>2]|0)+4096|0;c[q>>2]=v;R=v}else{R=r}c[q>>2]=R-8;R=b;b=c[R+4>>2]|0;c[o>>2]=c[R>>2];c[o+4>>2]=b;cL(f,j,l,n,p,0)|0;u=1;i=d;return u|0};default:{p=z+2|0;if((p|0)>0){n=x+(((p|0)/512|0)<<2)|0;S=n;T=(c[n>>2]|0)+(((p|0)%512|0)<<3)|0}else{p=509-z|0;z=x+(((p|0)/-512|0)<<2)|0;S=z;T=(c[z>>2]|0)+(511-((p|0)%512|0)<<3)|0}p=a;a=c[p>>2]|0;z=c[p+4>>2]|0;p=z;x=z-(c[a>>2]|0)>>3;z=x+1|0;if((z|0)>0){U=(c[a+(((z|0)/512|0)<<2)>>2]|0)+(((z|0)%512|0)<<3)|0}else{z=510-x|0;U=(c[a+(((z|0)/-512|0)<<2)>>2]|0)+(511-((z|0)%512|0)<<3)|0}N=+h[U>>3];D=+h[p>>3];H=+h[T>>3];z=H<N;do{if(N<D){if(z){h[p>>3]=H;h[T>>3]=D;break}h[p>>3]=N;h[U>>3]=D;E=+h[T>>3];if(E>=D){break}h[U>>3]=E;h[T>>3]=D}else{if(!z){break}h[U>>3]=H;h[T>>3]=N;E=+h[U>>3];I=+h[p>>3];if(E>=I){break}h[p>>3]=E;h[U>>3]=I}}while(0);U=T-(c[S>>2]|0)>>3;p=U+1|0;if((p|0)>0){z=S+(((p|0)/512|0)<<2)|0;a=c[z>>2]|0;V=0;W=a+(((p|0)%512|0)<<3)|0;X=z;Y=S;Z=T;_=a}else{a=510-U|0;U=S+(((a|0)/-512|0)<<2)|0;z=c[U>>2]|0;V=0;W=z+(511-((a|0)%512|0)<<3)|0;X=U;Y=S;Z=T;_=z}L1065:while(1){z=V;$=W;T=Y;S=Z;U=_;while(1){if(($|0)==(c[q>>2]|0)){u=1;aa=906;break L1065}N=+h[$>>3];H=+h[S>>3];if(N<H){a=T;p=S;x=$;D=H;while(1){h[x>>3]=D;if((p|0)==(c[s>>2]|0)){break}if((p|0)==(c[a>>2]|0)){n=a-4|0;ab=n;ac=(c[n>>2]|0)+4096|0}else{ab=a;ac=p}n=ac-8|0;H=+h[n>>3];if(N<H){a=ab;x=p;p=n;D=H}else{break}}h[p>>3]=N;x=z+1|0;if((x|0)==8){break L1065}ad=x;ae=c[X>>2]|0}else{ad=z;ae=U}x=$+8|0;if((x-ae|0)==4096){break}else{z=ad;S=$;$=x;T=X;U=ae}}U=X+4|0;T=c[U>>2]|0;V=ad;W=T;Y=X;X=U;Z=$;_=T}if((aa|0)==906){i=d;return u|0}aa=$+8|0;if((aa-(c[X>>2]|0)|0)==4096){af=c[X+4>>2]|0}else{af=aa}u=(af|0)==(c[q>>2]|0);i=d;return u|0}}return 0}function cO(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;e=a+8|0;f=c[e>>2]|0;g=a+4|0;i=c[g>>2]|0;if((f|0)==(i|0)){j=0}else{j=(f-i<<7)-1|0}k=a+16|0;l=c[k>>2]|0;m=a+20|0;n=c[m>>2]|0;o=j-l-n|0;if(o>>>0<b>>>0){cQ(a,b-o|0);p=c[m>>2]|0;q=c[k>>2]|0;r=c[g>>2]|0;s=c[e>>2]|0}else{p=n;q=l;r=i;s=f}f=q+p|0;p=r+(f>>>9<<2)|0;if((s|0)==(r|0)){t=0}else{t=(c[p>>2]|0)+((f&511)<<3)|0}if((b|0)==0){return}else{u=t;v=p;w=b}while(1){if((u|0)!=0){h[u>>3]=+h[d>>3]}b=w-1|0;p=u+8|0;if((p-(c[v>>2]|0)|0)==4096){t=v+4|0;x=t;y=c[t>>2]|0}else{x=v;y=p}c[m>>2]=(c[m>>2]|0)+1;if((b|0)==0){break}else{u=y;v=x;w=b}}return}function cP(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;d=a+4|0;e=c[d>>2]|0;f=a|0;do{if((e|0)==(c[f>>2]|0)){g=a+8|0;h=c[g>>2]|0;i=a+12|0;j=c[i>>2]|0;k=j;if(h>>>0<j>>>0){j=h;l=((k-j>>2)+1|0)/2|0;m=j-e|0;j=h+(l-(m>>2)<<2)|0;k_(j|0,e|0,m|0);c[d>>2]=j;c[g>>2]=(c[g>>2]|0)+(l<<2);n=j;break}j=k-e>>1;k=(j|0)==0?1:j;j=kO(k<<2)|0;l=j+((k+3|0)>>>2<<2)|0;m=j+(k<<2)|0;if((e|0)==(h|0)){o=l;p=e}else{k=e;q=l;do{if((q|0)==0){r=0}else{c[q>>2]=c[k>>2];r=q}q=r+4|0;k=k+4|0;}while((k|0)!=(h|0));o=q;p=c[f>>2]|0}c[f>>2]=j;c[d>>2]=l;c[g>>2]=o;c[i>>2]=m;if((p|0)==0){n=l;break}kS(p);n=c[d>>2]|0}else{n=e}}while(0);e=n-4|0;if((e|0)==0){s=n;t=s-4|0;c[d>>2]=t;return}c[e>>2]=c[b>>2];s=c[d>>2]|0;t=s-4|0;c[d>>2]=t;return}function cQ(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0;d=i;i=i+32|0;e=d|0;f=d+8|0;g=a|0;h=a+8|0;j=c[h>>2]|0;k=a+4|0;l=c[k>>2]|0;m=((j|0)==(l|0))+b|0;b=((m&511|0)!=0)+(m>>>9)|0;m=a+16|0;n=c[m>>2]|0;o=n>>>9;p=b>>>0<o>>>0?b:o;q=b-p|0;if((b|0)==(p|0)){c[m>>2]=n-(b<<9);if((b|0)==0){i=d;return}n=a+12|0;r=a|0;s=b;t=j;u=l;while(1){v=c[u>>2]|0;w=u+4|0;c[k>>2]=w;do{if((t|0)==(c[n>>2]|0)){x=c[r>>2]|0;if(w>>>0>x>>>0){y=w;z=((y-x>>2)+1|0)/-2|0;A=z+1|0;B=t-y|0;k_(u+(A<<2)|0,w|0,B|0);y=u+(A+(B>>2)<<2)|0;c[h>>2]=y;c[k>>2]=(c[k>>2]|0)+(z<<2);C=y;break}y=t-x>>1;z=(y|0)==0?1:y;y=kO(z<<2)|0;B=y+(z>>>2<<2)|0;A=y+(z<<2)|0;if((w|0)==(t|0)){D=B;E=x}else{x=w;z=B;do{if((z|0)==0){F=0}else{c[z>>2]=c[x>>2];F=z}z=F+4|0;x=x+4|0;}while((x|0)!=(t|0));D=z;E=c[r>>2]|0}c[r>>2]=y;c[k>>2]=B;c[h>>2]=D;c[n>>2]=A;if((E|0)==0){C=D;break}kS(E);C=c[h>>2]|0}else{C=t}}while(0);if((C|0)==0){G=0}else{c[C>>2]=v;G=c[h>>2]|0}w=G+4|0;c[h>>2]=w;x=s-1|0;if((x|0)==0){break}s=x;t=w;u=c[k>>2]|0}i=d;return}u=a+12|0;t=u|0;s=c[t>>2]|0;G=a|0;a=s-(c[G>>2]|0)|0;C=j-l>>2;if(q>>>0<=((a>>2)-C|0)>>>0){l=q;E=j;j=s;while(1){if((j|0)==(E|0)){H=971;break}s=kO(4096)|0;if((E|0)==0){I=0}else{c[E>>2]=s;I=c[h>>2]|0}s=I+4|0;c[h>>2]=s;D=l-1|0;if((D|0)==0){J=s;H=972;break}l=D;E=s;j=c[t>>2]|0}do{if((H|0)==971){if((l|0)==0){J=j;H=972;break}E=~o;I=~b;s=E>>>0>I>>>0?E:I;I=l;do{c[e>>2]=kO(4096)|0;cP(g,e);I=I-1|0;K=c[h>>2]|0;L=(c[m>>2]|0)+512-((K-(c[k>>2]|0)|0)==4)|0;c[m>>2]=L;}while((I|0)!=0);M=l-1-s|0;N=K;O=L}}while(0);if((H|0)==972){M=p;N=J;O=c[m>>2]|0}c[m>>2]=O-(M<<9);if((M|0)==0){i=d;return}else{P=M;Q=N}do{N=c[k>>2]|0;M=c[N>>2]|0;O=N+4|0;c[k>>2]=O;do{if((Q|0)==(c[t>>2]|0)){J=c[G>>2]|0;if(O>>>0>J>>>0){H=O;L=((H-J>>2)+1|0)/-2|0;K=L+1|0;l=Q-H|0;k_(N+(K<<2)|0,O|0,l|0);H=N+(K+(l>>2)<<2)|0;c[h>>2]=H;c[k>>2]=(c[k>>2]|0)+(L<<2);R=H;break}H=Q-J>>1;L=(H|0)==0?1:H;H=kO(L<<2)|0;l=H+(L>>>2<<2)|0;K=H+(L<<2)|0;if((O|0)==(Q|0)){S=l;T=J}else{J=O;L=l;do{if((L|0)==0){U=0}else{c[L>>2]=c[J>>2];U=L}L=U+4|0;J=J+4|0;}while((J|0)!=(Q|0));S=L;T=c[G>>2]|0}c[G>>2]=H;c[k>>2]=l;c[h>>2]=S;c[t>>2]=K;if((T|0)==0){R=S;break}kS(T);R=c[h>>2]|0}else{R=Q}}while(0);if((R|0)==0){V=0}else{c[R>>2]=M;V=c[h>>2]|0}Q=V+4|0;c[h>>2]=Q;P=P-1|0;}while((P|0)!=0);i=d;return}P=p<<9;Q=a>>1;a=q+C|0;V=Q>>>0<a>>>0?a:Q;Q=f+12|0;c[Q>>2]=0;c[f+16>>2]=u;if((V|0)==0){W=0}else{W=kO(V<<2)|0}u=f|0;c[u>>2]=W;a=W+(C-p<<2)|0;C=f+8|0;c[C>>2]=a;R=f+4|0;c[R>>2]=a;T=W+(V<<2)|0;c[Q>>2]=T;V=q;q=W;W=a;S=T;T=a;while(1){a=kO(4096)|0;do{if((W|0)==(S|0)){if(T>>>0>q>>>0){U=T;O=((U-q>>2)+1|0)/-2|0;N=S-U|0;k_(T+(O<<2)|0,T|0,N|0);U=T+(O+(N>>2)<<2)|0;c[C>>2]=U;N=(c[R>>2]|0)+(O<<2)|0;c[R>>2]=N;X=U;Y=S;Z=q;_=N;break}N=S-q>>1;U=(N|0)==0?1:N;N=kO(U<<2)|0;O=N+(U>>>2<<2)|0;s=N+(U<<2)|0;if((T|0)==(S|0)){$=O;aa=q}else{U=T;J=O;do{if((J|0)==0){ab=0}else{c[J>>2]=c[U>>2];ab=J}J=ab+4|0;U=U+4|0;}while((U|0)!=(S|0));$=J;aa=c[u>>2]|0}c[u>>2]=N;c[R>>2]=O;c[C>>2]=$;c[Q>>2]=s;if((aa|0)==0){X=$;Y=s;Z=N;_=O;break}kS(aa);X=$;Y=s;Z=N;_=O}else{X=W;Y=S;Z=q;_=T}}while(0);if((X|0)==0){ac=0}else{c[X>>2]=a;ac=c[C>>2]|0}ad=ac+4|0;c[C>>2]=ad;M=V-1|0;if((M|0)==0){break}else{V=M;q=Z;W=ad;S=Y;T=_}}if((p|0)!=0){_=p;p=c[k>>2]|0;T=ad;ad=Y;Y=Z;while(1){do{if((T|0)==(ad|0)){Z=c[R>>2]|0;if(Z>>>0>Y>>>0){S=Z;W=((S-Y>>2)+1|0)/-2|0;q=Z+(W<<2)|0;V=ad-S|0;k_(q|0,Z|0,V|0);S=Z+(W+(V>>2)<<2)|0;c[C>>2]=S;c[R>>2]=q;ae=S;af=ad;ag=Y;break}S=ad-Y>>1;q=(S|0)==0?1:S;S=kO(q<<2)|0;V=S+(q>>>2<<2)|0;W=S+(q<<2)|0;if((Z|0)==(ad|0)){ah=V;ai=Y}else{q=Z;Z=V;do{if((Z|0)==0){aj=0}else{c[Z>>2]=c[q>>2];aj=Z}Z=aj+4|0;q=q+4|0;}while((q|0)!=(ad|0));ah=Z;ai=c[u>>2]|0}c[u>>2]=S;c[R>>2]=V;c[C>>2]=ah;c[Q>>2]=W;if((ai|0)==0){ae=ah;af=W;ag=S;break}kS(ai);ae=ah;af=W;ag=S}else{ae=T;af=ad;ag=Y}}while(0);if((ae|0)==0){ak=0}else{c[ae>>2]=c[p>>2];ak=c[C>>2]|0}a=ak+4|0;c[C>>2]=a;q=(c[k>>2]|0)+4|0;c[k>>2]=q;O=_-1|0;if((O|0)==0){break}else{_=O;p=q;T=a;ad=af;Y=ag}}}ag=c[h>>2]|0;while(1){if((ag|0)==(c[k>>2]|0)){break}Y=ag-4|0;cS(f,Y);ag=Y}f=c[G>>2]|0;c[G>>2]=c[u>>2];c[u>>2]=f;c[k>>2]=c[R>>2];c[R>>2]=ag;R=c[h>>2]|0;c[h>>2]=c[C>>2];c[C>>2]=R;h=c[t>>2]|0;c[t>>2]=c[Q>>2];c[Q>>2]=h;c[m>>2]=(c[m>>2]|0)-P;if((ag|0)!=(R|0)){c[C>>2]=R+(~((R-4+(-ag|0)|0)>>>2)<<2)}if((f|0)==0){i=d;return}kS(f);i=d;return}function cR(a,b,c,d,e,f){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;var g=0.0,i=0.0,j=0.0,k=0,l=0.0,m=0.0,n=0.0,o=0,p=0;g=+h[b>>3];i=+h[a>>3];j=+h[c>>3];f=j<g;do{if(g<i){if(f){h[a>>3]=j;h[c>>3]=i;k=1;l=i;break}h[a>>3]=g;h[b>>3]=i;m=+h[c>>3];if(m>=i){k=1;l=m;break}h[b>>3]=m;h[c>>3]=i;k=2;l=i}else{if(!f){k=0;l=j;break}h[b>>3]=j;h[c>>3]=g;m=+h[b>>3];n=+h[a>>3];if(m>=n){k=1;l=g;break}h[a>>3]=m;h[b>>3]=n;k=2;l=+h[c>>3]}}while(0);g=+h[d>>3];do{if(g<l){h[c>>3]=g;h[d>>3]=l;j=+h[c>>3];i=+h[b>>3];if(j>=i){o=k+1|0;break}h[b>>3]=j;h[c>>3]=i;i=+h[b>>3];j=+h[a>>3];if(i<j){h[a>>3]=i;h[b>>3]=j;o=k+3|0;break}else{o=k+2|0;break}}else{o=k}}while(0);l=+h[e>>3];g=+h[d>>3];if(l>=g){p=o;return p|0}h[d>>3]=l;h[e>>3]=g;g=+h[d>>3];l=+h[c>>3];if(g>=l){p=o+1|0;return p|0}h[c>>3]=g;h[d>>3]=l;l=+h[c>>3];g=+h[b>>3];if(l>=g){p=o+2|0;return p|0}h[b>>3]=l;h[c>>3]=g;g=+h[b>>3];l=+h[a>>3];if(g>=l){p=o+3|0;return p|0}h[a>>3]=g;h[b>>3]=l;p=o+4|0;return p|0}function cS(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0;d=a+4|0;e=c[d>>2]|0;f=a|0;do{if((e|0)==(c[f>>2]|0)){g=a+8|0;h=c[g>>2]|0;i=a+12|0;j=c[i>>2]|0;k=j;if(h>>>0<j>>>0){j=h;l=((k-j>>2)+1|0)/2|0;m=j-e|0;j=h+(l-(m>>2)<<2)|0;k_(j|0,e|0,m|0);c[d>>2]=j;c[g>>2]=(c[g>>2]|0)+(l<<2);n=j;break}j=k-e>>1;k=(j|0)==0?1:j;j=kO(k<<2)|0;l=j+((k+3|0)>>>2<<2)|0;m=j+(k<<2)|0;if((e|0)==(h|0)){o=l;p=e}else{k=e;q=l;do{if((q|0)==0){r=0}else{c[q>>2]=c[k>>2];r=q}q=r+4|0;k=k+4|0;}while((k|0)!=(h|0));o=q;p=c[f>>2]|0}c[f>>2]=j;c[d>>2]=l;c[g>>2]=o;c[i>>2]=m;if((p|0)==0){n=l;break}kS(p);n=c[d>>2]|0}else{n=e}}while(0);e=n-4|0;if((e|0)==0){s=n;t=s-4|0;c[d>>2]=t;return}c[e>>2]=c[b>>2];s=c[d>>2]|0;t=s-4|0;c[d>>2]=t;return}function cT(){c[2446]=0;c[2447]=0;c[2448]=0;a$(72,9784,u|0)|0;return}function cU(a,b,c){a=a|0;b=b|0;c=c|0;var d=0,e=0,f=0,g=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0.0,q=0.0,r=0.0,s=0.0,t=0.0,u=0,v=0,w=0,x=0.0,y=0,z=0.0,A=0,B=0,C=0,D=0.0,E=0,F=0,G=0.0,H=0,I=0,J=0,K=0.0,L=0,M=0,N=0.0,O=0,P=0,Q=0,R=0,S=0,T=0.0,U=0,V=0,W=0.0,X=0,Y=0.0;d=a;a=b;L1397:while(1){b=a;e=a-8|0;f=d;L1399:while(1){g=f;i=b-g|0;j=i>>3;switch(j|0){case 3:{k=1102;break L1397;break};case 5:{k=1122;break L1397;break};case 2:{k=1100;break L1397;break};case 0:case 1:{k=1185;break L1397;break};case 4:{k=1110;break L1397;break};default:{}}if((i|0)<248){k=1124;break L1397}l=(j|0)/2|0;m=f+(l<<3)|0;do{if((i|0)>7992){n=(j|0)/4|0;o=cR(f,f+(n<<3)|0,m,f+(n+l<<3)|0,e,0)|0}else{p=+h[m>>3];q=+h[f>>3];r=+h[e>>3];n=r<p;if(p>=q){if(!n){o=0;break}h[m>>3]=r;h[e>>3]=p;s=+h[m>>3];t=+h[f>>3];if(s>=t){o=1;break}h[f>>3]=s;h[m>>3]=t;o=2;break}if(n){h[f>>3]=r;h[e>>3]=q;o=1;break}h[f>>3]=p;h[m>>3]=q;p=+h[e>>3];if(p>=q){o=1;break}h[m>>3]=p;h[e>>3]=q;o=2}}while(0);q=+h[f>>3];p=+h[m>>3];do{if(q<p){u=e;v=o}else{l=e;while(1){w=l-8|0;if((f|0)==(w|0)){break}x=+h[w>>3];if(x<p){k=1162;break}else{l=w}}if((k|0)==1162){k=0;h[f>>3]=x;h[w>>3]=q;u=w;v=o+1|0;break}l=f+8|0;r=+h[e>>3];if(q<r){y=l}else{j=l;while(1){if((j|0)==(e|0)){k=1197;break L1397}z=+h[j>>3];A=j+8|0;if(q<z){break}else{j=A}}h[j>>3]=r;h[e>>3]=z;y=A}if((y|0)==(e|0)){k=1192;break L1397}else{B=e;C=y}while(1){t=+h[f>>3];l=C;while(1){D=+h[l>>3];E=l+8|0;if(t>=D){l=E}else{F=B;break}}do{F=F-8|0;G=+h[F>>3];}while(t<G);if(l>>>0>=F>>>0){f=l;continue L1399}h[l>>3]=G;h[F>>3]=D;B=F;C=E}}}while(0);j=f+8|0;L1438:do{if(j>>>0<u>>>0){i=u;n=j;H=v;I=m;while(1){q=+h[I>>3];J=n;while(1){K=+h[J>>3];L=J+8|0;if(K<q){J=L}else{M=i;break}}do{M=M-8|0;N=+h[M>>3];}while(N>=q);if(J>>>0>M>>>0){O=J;P=H;Q=I;break L1438}h[J>>3]=N;h[M>>3]=K;i=M;n=L;H=H+1|0;I=(I|0)==(J|0)?M:I}}else{O=j;P=v;Q=m}}while(0);do{if((O|0)==(Q|0)){R=P}else{q=+h[Q>>3];p=+h[O>>3];if(q>=p){R=P;break}h[O>>3]=q;h[Q>>3]=p;R=P+1|0}}while(0);if((R|0)==0){S=cV(f,O,0)|0;m=O+8|0;if(cV(m,a,0)|0){k=1174;break}if(S){f=m;continue}}m=O;if((m-g|0)>=(b-m|0)){k=1178;break}cU(f,O,c);f=O+8|0}if((k|0)==1174){k=0;if(S){k=1186;break}else{d=f;a=O;continue}}else if((k|0)==1178){k=0;cU(O+8|0,a,c);d=f;a=O;continue}}if((k|0)==1102){O=f+8|0;K=+h[O>>3];N=+h[f>>3];D=+h[e>>3];d=D<K;if(K>=N){if(!d){return}h[O>>3]=D;h[e>>3]=K;G=+h[O>>3];z=+h[f>>3];if(G>=z){return}h[f>>3]=G;h[O>>3]=z;return}if(d){h[f>>3]=D;h[e>>3]=N;return}h[f>>3]=K;h[O>>3]=N;K=+h[e>>3];if(K>=N){return}h[O>>3]=K;h[e>>3]=N;return}else if((k|0)==1122){cR(f,f+8|0,f+16|0,f+24|0,e,0)|0;return}else if((k|0)==1124){O=f+16|0;d=f+8|0;N=+h[d>>3];K=+h[f>>3];D=+h[O>>3];c=D<N;do{if(N<K){if(c){h[f>>3]=D;h[O>>3]=K;T=K;break}h[f>>3]=N;h[d>>3]=K;if(D>=K){T=D;break}h[d>>3]=D;h[O>>3]=K;T=K}else{if(!c){T=D;break}h[d>>3]=D;h[O>>3]=N;if(D>=K){T=N;break}h[f>>3]=D;h[d>>3]=K;T=N}}while(0);d=f+24|0;if((d|0)==(a|0)){return}else{U=O;V=d;W=T}while(1){T=+h[V>>3];if(T<W){d=U;O=V;N=W;while(1){h[O>>3]=N;if((d|0)==(f|0)){X=f;break}c=d-8|0;K=+h[c>>3];if(T<K){O=d;d=c;N=K}else{X=d;break}}h[X>>3]=T}d=V+8|0;if((d|0)==(a|0)){break}N=+h[V>>3];U=V;V=d;W=N}return}else if((k|0)==1197){return}else if((k|0)==1100){W=+h[e>>3];N=+h[f>>3];if(W>=N){return}h[f>>3]=W;h[e>>3]=N;return}else if((k|0)==1185){return}else if((k|0)==1186){return}else if((k|0)==1192){return}else if((k|0)==1110){k=f+8|0;V=f+16|0;N=+h[k>>3];W=+h[f>>3];K=+h[V>>3];U=K<N;do{if(N<W){if(U){h[f>>3]=K;h[V>>3]=W;Y=W;break}h[f>>3]=N;h[k>>3]=W;if(K>=W){Y=K;break}h[k>>3]=K;h[V>>3]=W;Y=W}else{if(!U){Y=K;break}h[k>>3]=K;h[V>>3]=N;if(K>=W){Y=N;break}h[f>>3]=K;h[k>>3]=W;Y=N}}while(0);N=+h[e>>3];if(N>=Y){return}h[V>>3]=N;h[e>>3]=Y;Y=+h[V>>3];N=+h[k>>3];if(Y>=N){return}h[k>>3]=Y;h[V>>3]=N;N=+h[f>>3];if(Y>=N){return}h[f>>3]=Y;h[k>>3]=N;return}}function cV(a,b,c){a=a|0;b=b|0;c=c|0;var d=0,e=0,f=0.0,g=0.0,i=0.0,j=0,k=0.0,l=0.0,m=0,n=0.0,o=0.0,p=0,q=0,r=0,s=0.0,t=0,u=0,v=0;switch(b-a>>3|0){case 0:case 1:{d=1;return d|0};case 3:{c=a+8|0;e=b-8|0;f=+h[c>>3];g=+h[a>>3];i=+h[e>>3];j=i<f;if(f>=g){if(!j){d=1;return d|0}h[c>>3]=i;h[e>>3]=f;k=+h[c>>3];l=+h[a>>3];if(k>=l){d=1;return d|0}h[a>>3]=k;h[c>>3]=l;d=1;return d|0}if(j){h[a>>3]=i;h[e>>3]=g;d=1;return d|0}h[a>>3]=f;h[c>>3]=g;f=+h[e>>3];if(f>=g){d=1;return d|0}h[c>>3]=f;h[e>>3]=g;d=1;return d|0};case 2:{e=b-8|0;g=+h[e>>3];f=+h[a>>3];if(g>=f){d=1;return d|0}h[a>>3]=g;h[e>>3]=f;d=1;return d|0};case 4:{e=a+8|0;c=a+16|0;j=b-8|0;f=+h[e>>3];g=+h[a>>3];i=+h[c>>3];m=i<f;do{if(f<g){if(m){h[a>>3]=i;h[c>>3]=g;n=g;break}h[a>>3]=f;h[e>>3]=g;if(i>=g){n=i;break}h[e>>3]=i;h[c>>3]=g;n=g}else{if(!m){n=i;break}h[e>>3]=i;h[c>>3]=f;if(i>=g){n=f;break}h[a>>3]=i;h[e>>3]=g;n=f}}while(0);f=+h[j>>3];if(f>=n){d=1;return d|0}h[c>>3]=f;h[j>>3]=n;n=+h[c>>3];f=+h[e>>3];if(n>=f){d=1;return d|0}h[e>>3]=n;h[c>>3]=f;f=+h[a>>3];if(n>=f){d=1;return d|0}h[a>>3]=n;h[e>>3]=f;d=1;return d|0};case 5:{cR(a,a+8|0,a+16|0,a+24|0,b-8|0,0)|0;d=1;return d|0};default:{e=a+16|0;c=a+8|0;f=+h[c>>3];n=+h[a>>3];g=+h[e>>3];j=g<f;do{if(f<n){if(j){h[a>>3]=g;h[e>>3]=n;o=n;break}h[a>>3]=f;h[c>>3]=n;if(g>=n){o=g;break}h[c>>3]=g;h[e>>3]=n;o=n}else{if(!j){o=g;break}h[c>>3]=g;h[e>>3]=f;if(g>=n){o=f;break}h[a>>3]=g;h[c>>3]=n;o=f}}while(0);c=a+24|0;if((c|0)==(b|0)){d=1;return d|0}else{p=e;q=0;r=c;s=o}while(1){o=+h[r>>3];if(o<s){c=p;e=r;f=s;while(1){h[e>>3]=f;if((c|0)==(a|0)){t=a;break}j=c-8|0;n=+h[j>>3];if(o<n){e=c;c=j;f=n}else{t=c;break}}h[t>>3]=o;c=q+1|0;if((c|0)==8){break}else{u=c}}else{u=q}c=r+8|0;if((c|0)==(b|0)){d=1;v=1256;break}f=+h[r>>3];p=r;q=u;r=c;s=f}if((v|0)==1256){return d|0}d=(r+8|0)==(b|0);return d|0}}return 0}function cW(a){a=a|0;var b=0;a=(c[4038]|0)-1|0;c[4038]=a;if((a|0)!=0){return}b;return}function cX(b){b=b|0;var d=0,e=0,f=0;do{if((a[16280]|0)==0){if((bc(16280)|0)==0){break}c[2962]=5489;b=1;d=5489;do{d=(ag(d>>>30^d,1812433253)|0)+b|0;c[11848+(b<<2)>>2]=d;b=b+1|0;}while(b>>>0<624);c[3586]=0}}while(0);b=c[3586]|0;d=((b+1|0)>>>0)%624|0;e=11848+(b<<2)|0;f=c[11848+(d<<2)>>2]|0;c[e>>2]=-(f&1)&-1727483681^c[11848+((((b+397|0)>>>0)%624|0)<<2)>>2]^(f&2147483646|c[e>>2]&-2147483648)>>>1;e=c[11848+(c[3586]<<2)>>2]|0;f=e>>>11^e;c[3586]=d;d=f<<7&-1658038656^f;f=d<<15&-272236544^d;return f>>>18^f|0}function cY(a){a=a|0;var b=0;b;c[4038]=1;return}function cZ(b){b=b|0;var d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0;b=i;i=i+32|0;d=b|0;e=b+8|0;f=b+16|0;g=b+24|0;dq(15136,c[o>>2]|0,15192);c[4014]=4500;c[4016]=4520;c[4015]=0;h=c[1122]|0;ek(16056+h|0,15136);c[h+16128>>2]=0;c[h+16132>>2]=-1;h=c[t>>2]|0;ep(15040);c[3760]=4648;c[3768]=h;iN(g,15044);h=iX(g,15392)|0;j=h;iO(g);c[3769]=j;c[3770]=15200;a[15084]=(b0[c[(c[h>>2]|0)+28>>2]&127](j)|0)&1;c[3948]=4404;c[3949]=4424;j=c[1098]|0;ek(15792+j|0,15040);h=j+72|0;c[15792+h>>2]=0;g=j+76|0;c[15792+g>>2]=-1;k=c[r>>2]|0;ep(15088);c[3772]=4648;c[3780]=k;iN(f,15092);k=iX(f,15392)|0;l=k;iO(f);c[3781]=l;c[3782]=15208;a[15132]=(b0[c[(c[k>>2]|0)+28>>2]&127](l)|0)&1;c[3992]=4404;c[3993]=4424;ek(15968+j|0,15088);c[15968+h>>2]=0;c[15968+g>>2]=-1;l=c[(c[(c[3992]|0)-12>>2]|0)+15992>>2]|0;c[3970]=4404;c[3971]=4424;ek(15880+j|0,l);c[15880+h>>2]=0;c[15880+g>>2]=-1;c[(c[(c[4014]|0)-12>>2]|0)+16128>>2]=15792;g=(c[(c[3992]|0)-12>>2]|0)+15972|0;c[g>>2]=c[g>>2]|8192;c[(c[(c[3992]|0)-12>>2]|0)+16040>>2]=15792;c3(14984,c[o>>2]|0,15216);c[3926]=4452;c[3928]=4472;c[3927]=0;g=c[1110]|0;ek(15704+g|0,14984);c[g+15776>>2]=0;c[g+15780>>2]=-1;g=c[t>>2]|0;ew(14888);c[3722]=4576;c[3730]=g;iN(e,14892);g=iX(e,15384)|0;h=g;iO(e);c[3731]=h;c[3732]=15224;a[14932]=(b0[c[(c[g>>2]|0)+28>>2]&127](h)|0)&1;c[3856]=4356;c[3857]=4376;h=c[1086]|0;ek(15424+h|0,14888);g=h+72|0;c[15424+g>>2]=0;e=h+76|0;c[15424+e>>2]=-1;l=c[r>>2]|0;ew(14936);c[3734]=4576;c[3742]=l;iN(d,14940);l=iX(d,15384)|0;j=l;iO(d);c[3743]=j;c[3744]=15232;a[14980]=(b0[c[(c[l>>2]|0)+28>>2]&127](j)|0)&1;c[3900]=4356;c[3901]=4376;ek(15600+h|0,14936);c[15600+g>>2]=0;c[15600+e>>2]=-1;j=c[(c[(c[3900]|0)-12>>2]|0)+15624>>2]|0;c[3878]=4356;c[3879]=4376;ek(15512+h|0,j);c[15512+g>>2]=0;c[15512+e>>2]=-1;c[(c[(c[3926]|0)-12>>2]|0)+15776>>2]=15424;e=(c[(c[3900]|0)-12>>2]|0)+15604|0;c[e>>2]=c[e>>2]|8192;c[(c[(c[3900]|0)-12>>2]|0)+15672>>2]=15424;i=b;return}function c_(a){a=a|0;ev(a|0);return}function c$(a){a=a|0;ev(a|0);kS(a);return}function c0(b,d){b=b|0;d=d|0;var e=0;b0[c[(c[b>>2]|0)+24>>2]&127](b)|0;e=iX(d,15384)|0;d=e;c[b+36>>2]=d;a[b+44|0]=(b0[c[(c[e>>2]|0)+28>>2]&127](d)|0)&1;return}function c1(a){a=a|0;var b=0,d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0;b=i;i=i+16|0;d=b|0;e=b+8|0;f=a+36|0;g=a+40|0;h=d|0;j=d+8|0;k=d;d=a+32|0;L1642:while(1){a=c[f>>2]|0;l=b$[c[(c[a>>2]|0)+20>>2]&31](a,c[g>>2]|0,h,j,e)|0;a=(c[e>>2]|0)-k|0;if((aM(h|0,1,a|0,c[d>>2]|0)|0)!=(a|0)){m=-1;n=1304;break}switch(l|0){case 2:{m=-1;n=1305;break L1642;break};case 1:{break};default:{n=1301;break L1642}}}if((n|0)==1304){i=b;return m|0}else if((n|0)==1305){i=b;return m|0}else if((n|0)==1301){m=((aK(c[d>>2]|0)|0)!=0)<<31>>31;i=b;return m|0}return 0}function c2(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0;e=i;i=i+32|0;f=e|0;g=e+8|0;h=e+16|0;j=e+24|0;k=(d|0)==-1;L1651:do{if(!k){c[g>>2]=d;if((a[b+44|0]&1)!=0){if((aM(g|0,4,1,c[b+32>>2]|0)|0)==1){break}else{l=-1}i=e;return l|0}m=f|0;c[h>>2]=m;n=g+4|0;o=b+36|0;p=b+40|0;q=f+8|0;r=f;s=b+32|0;t=g;while(1){u=c[o>>2]|0;v=b6[c[(c[u>>2]|0)+12>>2]&31](u,c[p>>2]|0,t,n,j,m,q,h)|0;if((c[j>>2]|0)==(t|0)){l=-1;w=1320;break}if((v|0)==3){w=1312;break}u=(v|0)==1;if(v>>>0>=2){l=-1;w=1322;break}v=(c[h>>2]|0)-r|0;if((aM(m|0,1,v|0,c[s>>2]|0)|0)!=(v|0)){l=-1;w=1321;break}if(u){t=u?c[j>>2]|0:t}else{break L1651}}if((w|0)==1321){i=e;return l|0}else if((w|0)==1322){i=e;return l|0}else if((w|0)==1312){if((aM(t|0,1,1,c[s>>2]|0)|0)==1){break}else{l=-1}i=e;return l|0}else if((w|0)==1320){i=e;return l|0}}}while(0);l=k?0:d;i=e;return l|0}function c3(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,j=0;f=i;i=i+8|0;g=f|0;ew(b|0);c[b>>2]=4976;c[b+32>>2]=d;c[b+40>>2]=e;c[b+48>>2]=-1;a[b+52|0]=0;iN(g,b+4|0);e=iX(g,15384)|0;d=e;h=b+36|0;c[h>>2]=d;j=b+44|0;c[j>>2]=b0[c[(c[e>>2]|0)+24>>2]&127](d)|0;d=c[h>>2]|0;a[b+53|0]=(b0[c[(c[d>>2]|0)+28>>2]&127](d)|0)&1;if((c[j>>2]|0)<=8){iO(g);i=f;return}hV(184);iO(g);i=f;return}function c4(a){a=a|0;ev(a|0);return}function c5(a){a=a|0;ev(a|0);kS(a);return}function c6(b,d){b=b|0;d=d|0;var e=0,f=0,g=0;e=iX(d,15384)|0;d=e;f=b+36|0;c[f>>2]=d;g=b+44|0;c[g>>2]=b0[c[(c[e>>2]|0)+24>>2]&127](d)|0;d=c[f>>2]|0;a[b+53|0]=(b0[c[(c[d>>2]|0)+28>>2]&127](d)|0)&1;if((c[g>>2]|0)<=8){return}hV(184);return}function c7(a){a=a|0;return da(a,0)|0}function c8(a){a=a|0;return da(a,1)|0}function c9(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;e=i;i=i+32|0;f=e|0;g=e+8|0;h=e+16|0;j=e+24|0;k=b+52|0;l=(a[k]&1)!=0;if((d|0)==-1){if(l){m=-1;i=e;return m|0}n=c[b+48>>2]|0;a[k]=(n|0)!=-1|0;m=n;i=e;return m|0}n=b+48|0;L1694:do{if(l){c[h>>2]=c[n>>2];o=c[b+36>>2]|0;p=f|0;switch(b6[c[(c[o>>2]|0)+12>>2]&31](o,c[b+40>>2]|0,h,h+4|0,j,p,f+8|0,g)|0){case 2:case 1:{m=-1;i=e;return m|0};case 3:{a[p]=c[n>>2]&255;c[g>>2]=f+1;break};default:{}}o=b+32|0;while(1){q=c[g>>2]|0;if(q>>>0<=p>>>0){break L1694}r=q-1|0;c[g>>2]=r;if((bA(a[r]|0,c[o>>2]|0)|0)==-1){m=-1;break}}i=e;return m|0}}while(0);c[n>>2]=d;a[k]=1;m=d;i=e;return m|0}function da(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;e=i;i=i+32|0;f=e|0;g=e+8|0;h=e+16|0;j=e+24|0;k=b+52|0;if((a[k]&1)!=0){l=b+48|0;m=c[l>>2]|0;if(!d){n=m;i=e;return n|0}c[l>>2]=-1;a[k]=0;n=m;i=e;return n|0}m=c[b+44>>2]|0;k=(m|0)>1?m:1;L1714:do{if((k|0)>0){m=b+32|0;l=0;while(1){o=a_(c[m>>2]|0)|0;if((o|0)==-1){n=-1;break}a[f+l|0]=o&255;l=l+1|0;if((l|0)>=(k|0)){break L1714}}i=e;return n|0}}while(0);L1721:do{if((a[b+53|0]&1)==0){l=b+40|0;m=b+36|0;o=f|0;p=g+4|0;q=b+32|0;r=k;L1723:while(1){s=c[l>>2]|0;t=s;u=c[t>>2]|0;v=c[t+4>>2]|0;t=c[m>>2]|0;w=f+r|0;switch(b6[c[(c[t>>2]|0)+16>>2]&31](t,s,o,w,h,g,p,j)|0){case 1:{break};case 2:{n=-1;x=1380;break L1723;break};case 3:{x=1369;break L1723;break};default:{y=r;break L1721}}s=c[l>>2]|0;c[s>>2]=u;c[s+4>>2]=v;if((r|0)==8){n=-1;x=1379;break}v=a_(c[q>>2]|0)|0;if((v|0)==-1){n=-1;x=1385;break}a[w]=v&255;r=r+1|0}if((x|0)==1379){i=e;return n|0}else if((x|0)==1380){i=e;return n|0}else if((x|0)==1385){i=e;return n|0}else if((x|0)==1369){c[g>>2]=a[o]|0;y=r;break}}else{c[g>>2]=a[f|0]|0;y=k}}while(0);if(d){d=c[g>>2]|0;c[b+48>>2]=d;n=d;i=e;return n|0}d=b+32|0;b=y;while(1){if((b|0)<=0){break}y=b-1|0;if((bA(a[f+y|0]|0,c[d>>2]|0)|0)==-1){n=-1;x=1382;break}else{b=y}}if((x|0)==1382){i=e;return n|0}n=c[g>>2]|0;i=e;return n|0}function db(a){a=a|0;eo(a|0);return}function dc(a){a=a|0;eo(a|0);kS(a);return}function dd(a){a=a|0;fg(15792)|0;fg(15880)|0;fh(15424)|0;fh(15512)|0;return}function de(a){a=a|0;return}function df(a){a=a|0;return}function dg(a){a=a|0;var b=0;b=a+4|0;I=c[b>>2]|0,c[b>>2]=I+1,I;return}function dh(a){a=a|0;return c[a+4>>2]|0}function di(a){a=a|0;return c[a+4>>2]|0}function dj(a){a=a|0;c[a>>2]=4304;return}function dk(a,b,d){a=a|0;b=b|0;d=d|0;c[a>>2]=d;c[a+4>>2]=b;return}function dl(a,b,d){a=a|0;b=b|0;d=d|0;var e=0;if((c[b+4>>2]|0)!=(a|0)){e=0;return e|0}e=(c[b>>2]|0)==(d|0);return e|0}function dm(b,d){b=b|0;d=d|0;var e=0;b0[c[(c[b>>2]|0)+24>>2]&127](b)|0;e=iX(d,15392)|0;d=e;c[b+36>>2]=d;a[b+44|0]=(b0[c[(c[e>>2]|0)+28>>2]&127](d)|0)&1;return}function dn(a){a=a|0;var b=0,d=0,e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0;b=i;i=i+16|0;d=b|0;e=b+8|0;f=a+36|0;g=a+40|0;h=d|0;j=d+8|0;k=d;d=a+32|0;L1767:while(1){a=c[f>>2]|0;l=b$[c[(c[a>>2]|0)+20>>2]&31](a,c[g>>2]|0,h,j,e)|0;a=(c[e>>2]|0)-k|0;if((aM(h|0,1,a|0,c[d>>2]|0)|0)!=(a|0)){m=-1;n=1414;break}switch(l|0){case 2:{m=-1;n=1415;break L1767;break};case 1:{break};default:{n=1411;break L1767}}}if((n|0)==1415){i=b;return m|0}else if((n|0)==1411){m=((aK(c[d>>2]|0)|0)!=0)<<31>>31;i=b;return m|0}else if((n|0)==1414){i=b;return m|0}return 0}function dp(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0;e=i;i=i+32|0;f=e|0;g=e+8|0;h=e+16|0;j=e+24|0;k=(d|0)==-1;L1776:do{if(!k){a[g]=d&255;if((a[b+44|0]&1)!=0){if((aM(g|0,1,1,c[b+32>>2]|0)|0)==1){break}else{l=-1}i=e;return l|0}m=f|0;c[h>>2]=m;n=g+1|0;o=b+36|0;p=b+40|0;q=f+8|0;r=f;s=b+32|0;t=g;while(1){u=c[o>>2]|0;v=b6[c[(c[u>>2]|0)+12>>2]&31](u,c[p>>2]|0,t,n,j,m,q,h)|0;if((c[j>>2]|0)==(t|0)){l=-1;w=1433;break}if((v|0)==3){w=1422;break}u=(v|0)==1;if(v>>>0>=2){l=-1;w=1432;break}v=(c[h>>2]|0)-r|0;if((aM(m|0,1,v|0,c[s>>2]|0)|0)!=(v|0)){l=-1;w=1430;break}if(u){t=u?c[j>>2]|0:t}else{break L1776}}if((w|0)==1430){i=e;return l|0}else if((w|0)==1422){if((aM(t|0,1,1,c[s>>2]|0)|0)==1){break}else{l=-1}i=e;return l|0}else if((w|0)==1432){i=e;return l|0}else if((w|0)==1433){i=e;return l|0}}}while(0);l=k?0:d;i=e;return l|0}function dq(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,j=0;f=i;i=i+8|0;g=f|0;ep(b|0);c[b>>2]=5048;c[b+32>>2]=d;c[b+40>>2]=e;c[b+48>>2]=-1;a[b+52|0]=0;iN(g,b+4|0);e=iX(g,15392)|0;d=e;h=b+36|0;c[h>>2]=d;j=b+44|0;c[j>>2]=b0[c[(c[e>>2]|0)+24>>2]&127](d)|0;d=c[h>>2]|0;a[b+53|0]=(b0[c[(c[d>>2]|0)+28>>2]&127](d)|0)&1;if((c[j>>2]|0)<=8){iO(g);i=f;return}hV(184);iO(g);i=f;return}function dr(a){a=a|0;eo(a|0);return}function ds(a){a=a|0;eo(a|0);kS(a);return}function dt(b,d){b=b|0;d=d|0;var e=0,f=0,g=0;e=iX(d,15392)|0;d=e;f=b+36|0;c[f>>2]=d;g=b+44|0;c[g>>2]=b0[c[(c[e>>2]|0)+24>>2]&127](d)|0;d=c[f>>2]|0;a[b+53|0]=(b0[c[(c[d>>2]|0)+28>>2]&127](d)|0)&1;if((c[g>>2]|0)<=8){return}hV(184);return}function du(a){a=a|0;return dx(a,0)|0}function dv(a){a=a|0;return dx(a,1)|0}function dw(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;e=i;i=i+32|0;f=e|0;g=e+8|0;h=e+16|0;j=e+24|0;k=b+52|0;l=(a[k]&1)!=0;if((d|0)==-1){if(l){m=-1;i=e;return m|0}n=c[b+48>>2]|0;a[k]=(n|0)!=-1|0;m=n;i=e;return m|0}n=b+48|0;L1819:do{if(l){a[h]=c[n>>2]&255;o=c[b+36>>2]|0;p=f|0;switch(b6[c[(c[o>>2]|0)+12>>2]&31](o,c[b+40>>2]|0,h,h+1|0,j,p,f+8|0,g)|0){case 3:{a[p]=c[n>>2]&255;c[g>>2]=f+1;break};case 2:case 1:{m=-1;i=e;return m|0};default:{}}o=b+32|0;while(1){q=c[g>>2]|0;if(q>>>0<=p>>>0){break L1819}r=q-1|0;c[g>>2]=r;if((bA(a[r]|0,c[o>>2]|0)|0)==-1){m=-1;break}}i=e;return m|0}}while(0);c[n>>2]=d;a[k]=1;m=d;i=e;return m|0}function dx(b,e){b=b|0;e=e|0;var f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;f=i;i=i+32|0;g=f|0;h=f+8|0;j=f+16|0;k=f+24|0;l=b+52|0;if((a[l]&1)!=0){m=b+48|0;n=c[m>>2]|0;if(!e){o=n;i=f;return o|0}c[m>>2]=-1;a[l]=0;o=n;i=f;return o|0}n=c[b+44>>2]|0;l=(n|0)>1?n:1;L1839:do{if((l|0)>0){n=b+32|0;m=0;while(1){p=a_(c[n>>2]|0)|0;if((p|0)==-1){o=-1;break}a[g+m|0]=p&255;m=m+1|0;if((m|0)>=(l|0)){break L1839}}i=f;return o|0}}while(0);L1846:do{if((a[b+53|0]&1)==0){m=b+40|0;n=b+36|0;p=g|0;q=h+1|0;r=b+32|0;s=l;L1848:while(1){t=c[m>>2]|0;u=t;v=c[u>>2]|0;w=c[u+4>>2]|0;u=c[n>>2]|0;x=g+s|0;switch(b6[c[(c[u>>2]|0)+16>>2]&31](u,t,p,x,j,h,q,k)|0){case 1:{break};case 2:{o=-1;y=1490;break L1848;break};case 3:{y=1479;break L1848;break};default:{z=s;break L1846}}t=c[m>>2]|0;c[t>>2]=v;c[t+4>>2]=w;if((s|0)==8){o=-1;y=1491;break}w=a_(c[r>>2]|0)|0;if((w|0)==-1){o=-1;y=1495;break}a[x]=w&255;s=s+1|0}if((y|0)==1490){i=f;return o|0}else if((y|0)==1495){i=f;return o|0}else if((y|0)==1479){a[h]=a[p]|0;z=s;break}else if((y|0)==1491){i=f;return o|0}}else{a[h]=a[g|0]|0;z=l}}while(0);do{if(e){l=a[h]|0;c[b+48>>2]=l&255;A=l}else{l=b+32|0;k=z;while(1){if((k|0)<=0){y=1486;break}j=k-1|0;if((bA(d[g+j|0]|0|0,c[l>>2]|0)|0)==-1){o=-1;y=1494;break}else{k=j}}if((y|0)==1486){A=a[h]|0;break}else if((y|0)==1494){i=f;return o|0}}}while(0);o=A&255;i=f;return o|0}function dy(){cZ(0);a$(142,16144|0,u|0)|0;return}function dz(a){a=a|0;var b=0,d=0;b=a+4|0;if(((I=c[b>>2]|0,c[b>>2]=I+ -1,I)|0)!=0){d=0;return d|0}bY[c[(c[a>>2]|0)+8>>2]&511](a);d=1;return d|0}function dA(a,b){a=a|0;b=b|0;var d=0,e=0,f=0;c[a>>2]=2536;d=a+4|0;if((d|0)==0){return}a=k0(b|0)|0;e=a+1|0;f=kP(a+13|0)|0;c[f+4>>2]=a;c[f>>2]=a;a=f+12|0;c[d>>2]=a;c[f+8>>2]=0;kY(a|0,b|0,e)|0;return}function dB(a){a=a|0;var b=0,d=0,e=0;c[a>>2]=2536;b=a+4|0;d=(c[b>>2]|0)-4|0;if(((I=c[d>>2]|0,c[d>>2]=I+ -1,I)-1|0)>=0){e=a;kS(e);return}d=(c[b>>2]|0)-12|0;if((d|0)==0){e=a;kS(e);return}kT(d);e=a;kS(e);return}function dC(a){a=a|0;var b=0;c[a>>2]=2536;b=a+4|0;a=(c[b>>2]|0)-4|0;if(((I=c[a>>2]|0,c[a>>2]=I+ -1,I)-1|0)>=0){return}a=(c[b>>2]|0)-12|0;if((a|0)==0){return}kT(a);return}function dD(b,d){b=b|0;d=d|0;var e=0,f=0,g=0;c[b>>2]=2472;e=b+4|0;if((e|0)==0){return}if((a[d]&1)==0){f=d+1|0}else{f=c[d+8>>2]|0}d=k0(f|0)|0;b=d+1|0;g=kP(d+13|0)|0;c[g+4>>2]=d;c[g>>2]=d;d=g+12|0;c[e>>2]=d;c[g+8>>2]=0;kY(d|0,f|0,b)|0;return}function dE(a,b){a=a|0;b=b|0;var d=0,e=0,f=0;c[a>>2]=2472;d=a+4|0;if((d|0)==0){return}a=k0(b|0)|0;e=a+1|0;f=kP(a+13|0)|0;c[f+4>>2]=a;c[f>>2]=a;a=f+12|0;c[d>>2]=a;c[f+8>>2]=0;kY(a|0,b|0,e)|0;return}function dF(a){a=a|0;var b=0,d=0,e=0;c[a>>2]=2472;b=a+4|0;d=(c[b>>2]|0)-4|0;if(((I=c[d>>2]|0,c[d>>2]=I+ -1,I)-1|0)>=0){e=a;kS(e);return}d=(c[b>>2]|0)-12|0;if((d|0)==0){e=a;kS(e);return}kT(d);e=a;kS(e);return}function dG(a){a=a|0;var b=0;c[a>>2]=2472;b=a+4|0;a=(c[b>>2]|0)-4|0;if(((I=c[a>>2]|0,c[a>>2]=I+ -1,I)-1|0)>=0){return}a=(c[b>>2]|0)-12|0;if((a|0)==0){return}kT(a);return}function dH(a){a=a|0;var b=0,d=0,e=0;c[a>>2]=2536;b=a+4|0;d=(c[b>>2]|0)-4|0;if(((I=c[d>>2]|0,c[d>>2]=I+ -1,I)-1|0)>=0){e=a;kS(e);return}d=(c[b>>2]|0)-12|0;if((d|0)==0){e=a;kS(e);return}kT(d);e=a;kS(e);return}function dI(a){a=a|0;kS(a);return}function dJ(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0;e=i;i=i+8|0;f=e|0;b3[c[(c[a>>2]|0)+12>>2]&7](f,a,b);if((c[f+4>>2]|0)!=(c[d+4>>2]|0)){g=0;i=e;return g|0}g=(c[f>>2]|0)==(c[d>>2]|0);i=e;return g|0}function dK(a,b,c){a=a|0;b=b|0;c=c|0;b=bx(c|0)|0;dZ(a,b,k0(b|0)|0);return}function dL(b,e,f){b=b|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;g=i;h=f;j=i;i=i+12|0;i=i+7>>3<<3;k=e|0;l=c[k>>2]|0;if((l|0)==0){m=b;c[m>>2]=c[h>>2];c[m+4>>2]=c[h+4>>2];c[m+8>>2]=c[h+8>>2];k$(h|0,0,12);i=g;return}n=d[h]|0;if((n&1|0)==0){o=n>>>1}else{o=c[f+4>>2]|0}if((o|0)==0){p=l}else{dT(f,1296)|0;p=c[k>>2]|0}k=c[e+4>>2]|0;b3[c[(c[k>>2]|0)+24>>2]&7](j,k,p);p=a[j]|0;if((p&1)==0){q=j+1|0}else{q=c[j+8>>2]|0}k=p&255;if((k&1|0)==0){r=k>>>1}else{r=c[j+4>>2]|0}dV(f,q,r)|0;dP(j);m=b;c[m>>2]=c[h>>2];c[m+4>>2]=c[h+4>>2];c[m+8>>2]=c[h+8>>2];k$(h|0,0,12);i=g;return}function dM(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0;e=i;i=i+32|0;f=b;b=i;i=i+8|0;c[b>>2]=c[f>>2];c[b+4>>2]=c[f+4>>2];f=e|0;g=e+16|0;dZ(g,d,k0(d|0)|0);dL(f,b,g);dD(a|0,f);dP(f);dP(g);c[a>>2]=4544;g=b;b=a+8|0;a=c[g+4>>2]|0;c[b>>2]=c[g>>2];c[b+4>>2]=a;i=e;return}function dN(a){a=a|0;dG(a|0);kS(a);return}function dO(a){a=a|0;dG(a|0);return}function dP(b){b=b|0;if((a[b]&1)==0){return}kS(c[b+8>>2]|0);return}function dQ(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0;e=k0(d|0)|0;f=b;g=b;h=a[g]|0;if((h&1)==0){i=10;j=h}else{h=c[b>>2]|0;i=(h&-2)-1|0;j=h&255}if(i>>>0<e>>>0){h=j&255;if((h&1|0)==0){k=h>>>1}else{k=c[b+4>>2]|0}d0(b,i,e-i|0,k,0,k,e,d);return b|0}if((j&1)==0){l=f+1|0}else{l=c[b+8>>2]|0}k_(l|0,d|0,e|0);a[l+e|0]=0;if((a[g]&1)==0){a[g]=e<<1&255;return b|0}else{c[b+4>>2]=e;return b|0}return 0}function dR(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0;f=b;g=a[f]|0;h=g&255;if((h&1|0)==0){i=h>>>1}else{i=c[b+4>>2]|0}if(i>>>0<d>>>0){h=d-i|0;dS(b,h,e)|0;return}if((g&1)==0){a[b+1+d|0]=0;a[f]=d<<1&255;return}else{a[(c[b+8>>2]|0)+d|0]=0;c[b+4>>2]=d;return}}function dS(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0,l=0;if((d|0)==0){return b|0}f=b;g=a[f]|0;if((g&1)==0){h=10;i=g}else{g=c[b>>2]|0;h=(g&-2)-1|0;i=g&255}g=i&255;if((g&1|0)==0){j=g>>>1}else{j=c[b+4>>2]|0}if((h-j|0)>>>0<d>>>0){d1(b,h,d-h+j|0,j,j,0,0);k=a[f]|0}else{k=i}if((k&1)==0){l=b+1|0}else{l=c[b+8>>2]|0}k$(l+j|0,e|0,d|0);e=j+d|0;if((a[f]&1)==0){a[f]=e<<1&255}else{c[b+4>>2]=e}a[l+e|0]=0;return b|0}function dT(a,b){a=a|0;b=b|0;return dV(a,b,k0(b|0)|0)|0}function dU(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0;e=b;f=a[e]|0;if((f&1)==0){g=(f&255)>>>1;h=10}else{g=c[b+4>>2]|0;h=(c[b>>2]&-2)-1|0}if((g|0)==(h|0)){d1(b,h,1,h,h,0,0);i=a[e]|0}else{i=f}if((i&1)==0){a[e]=(g<<1)+2&255;j=b+1|0;k=g+1|0;l=j+g|0;a[l]=d;m=j+k|0;a[m]=0;return}else{e=c[b+8>>2]|0;i=g+1|0;c[b+4>>2]=i;j=e;k=i;l=j+g|0;a[l]=d;m=j+k|0;a[m]=0;return}}function dV(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0;f=b;g=a[f]|0;if((g&1)==0){h=10;i=g}else{g=c[b>>2]|0;h=(g&-2)-1|0;i=g&255}g=i&255;if((g&1|0)==0){j=g>>>1}else{j=c[b+4>>2]|0}if((h-j|0)>>>0<e>>>0){d0(b,h,e-h+j|0,j,j,0,e,d);return b|0}if((e|0)==0){return b|0}if((i&1)==0){k=b+1|0}else{k=c[b+8>>2]|0}i=k+j|0;kY(i|0,d|0,e)|0;d=j+e|0;if((a[f]&1)==0){a[f]=d<<1&255}else{c[b+4>>2]=d}a[k+d|0]=0;return b|0}function dW(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0;e;if((c[a>>2]|0)==1){do{aU(9736,9728)|0;}while((c[a>>2]|0)==1)}if((c[a>>2]|0)!=0){f;return}c[a>>2]=1;g;bY[d&511](b);h;c[a>>2]=-1;i;bs(9736)|0;return}function dX(a){a=a|0;a=bO(8)|0;dA(a,360);c[a>>2]=2504;bl(a|0,8208,32)}function dY(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0;e=d;if((a[e]&1)==0){f=b;c[f>>2]=c[e>>2];c[f+4>>2]=c[e+4>>2];c[f+8>>2]=c[e+8>>2];return}e=c[d+8>>2]|0;f=c[d+4>>2]|0;if((f|0)==-1){dX(0)}if(f>>>0<11){a[b]=f<<1&255;g=b+1|0}else{d=f+16&-16;h=kO(d)|0;c[b+8>>2]=h;c[b>>2]=d|1;c[b+4>>2]=f;g=h}kY(g|0,e|0,f)|0;a[g+f|0]=0;return}function dZ(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0;if((e|0)==-1){dX(0)}if(e>>>0<11){a[b]=e<<1&255;f=b+1|0;kY(f|0,d|0,e)|0;g=f+e|0;a[g]=0;return}else{h=e+16&-16;i=kO(h)|0;c[b+8>>2]=i;c[b>>2]=h|1;c[b+4>>2]=e;f=i;kY(f|0,d|0,e)|0;g=f+e|0;a[g]=0;return}}function d_(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;if((d|0)==-1){dX(0)}if(d>>>0<11){a[b]=d<<1&255;f=b+1|0}else{g=d+16&-16;h=kO(g)|0;c[b+8>>2]=h;c[b>>2]=g|1;c[b+4>>2]=d;f=h}k$(f|0,e|0,d|0);a[f+d|0]=0;return}function d$(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;if((d|0)==-1){dX(0)}e=b;f=b;g=a[f]|0;if((g&1)==0){h=10;i=g}else{g=c[b>>2]|0;h=(g&-2)-1|0;i=g&255}g=i&255;if((g&1|0)==0){j=g>>>1}else{j=c[b+4>>2]|0}g=j>>>0>d>>>0?j:d;if(g>>>0<11){k=11}else{k=g+16&-16}g=k-1|0;if((g|0)==(h|0)){return}if((g|0)==10){l=e+1|0;m=c[b+8>>2]|0;n=1;o=0}else{if(g>>>0>h>>>0){p=kO(k)|0}else{p=kO(k)|0}h=i&1;if(h<<24>>24==0){q=e+1|0}else{q=c[b+8>>2]|0}l=p;m=q;n=h<<24>>24!=0;o=1}h=i&255;if((h&1|0)==0){r=h>>>1}else{r=c[b+4>>2]|0}h=r+1|0;kY(l|0,m|0,h)|0;if(n){kS(m)}if(o){c[b>>2]=k|1;c[b+4>>2]=j;c[b+8>>2]=l;return}else{a[f]=j<<1&255;return}}function d0(b,d,e,f,g,h,i,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;if((-3-d|0)>>>0<e>>>0){dX(0)}if((a[b]&1)==0){k=b+1|0}else{k=c[b+8>>2]|0}do{if(d>>>0<2147483631){l=e+d|0;m=d<<1;n=l>>>0<m>>>0?m:l;if(n>>>0<11){o=11;break}o=n+16&-16}else{o=-2}}while(0);e=kO(o)|0;if((g|0)!=0){kY(e|0,k|0,g)|0}if((i|0)!=0){n=e+g|0;kY(n|0,j|0,i)|0}j=f-h|0;if((j|0)!=(g|0)){f=j-g|0;n=e+(i+g)|0;l=k+(h+g)|0;kY(n|0,l|0,f)|0}if((d|0)==10){p=b+8|0;c[p>>2]=e;q=o|1;r=b|0;c[r>>2]=q;s=j+i|0;t=b+4|0;c[t>>2]=s;u=e+s|0;a[u]=0;return}kS(k);p=b+8|0;c[p>>2]=e;q=o|1;r=b|0;c[r>>2]=q;s=j+i|0;t=b+4|0;c[t>>2]=s;u=e+s|0;a[u]=0;return}function d1(b,d,e,f,g,h,i){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;if((-3-d|0)>>>0<e>>>0){dX(0)}if((a[b]&1)==0){j=b+1|0}else{j=c[b+8>>2]|0}do{if(d>>>0<2147483631){k=e+d|0;l=d<<1;m=k>>>0<l>>>0?l:k;if(m>>>0<11){n=11;break}n=m+16&-16}else{n=-2}}while(0);e=kO(n)|0;if((g|0)!=0){kY(e|0,j|0,g)|0}m=f-h|0;if((m|0)!=(g|0)){f=m-g|0;m=e+(i+g)|0;i=j+(h+g)|0;kY(m|0,i|0,f)|0}if((d|0)==10){o=b+8|0;c[o>>2]=e;p=n|1;q=b|0;c[q>>2]=p;return}kS(j);o=b+8|0;c[o>>2]=e;p=n|1;q=b|0;c[q>>2]=p;return}function d2(a,b){a=a|0;b=b|0;return}function d3(a,b,c){a=a|0;b=b|0;c=c|0;return a|0}function d4(a){a=a|0;return 0}function d5(a){a=a|0;return 0}function d6(a){a=a|0;return-1|0}function d7(a,b){a=a|0;b=b|0;return-1|0}function d8(a,b){a=a|0;b=b|0;return-1|0}function d9(a,b){a=a|0;b=b|0;return}function ea(a,b,c){a=a|0;b=b|0;c=c|0;return a|0}function eb(a,b,d,e,f,g){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;g=a;c[g>>2]=0;c[g+4>>2]=0;g=a+8|0;c[g>>2]=-1;c[g+4>>2]=-1;return}function ec(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;e=i;b=d;d=i;i=i+16|0;c[d>>2]=c[b>>2];c[d+4>>2]=c[b+4>>2];c[d+8>>2]=c[b+8>>2];c[d+12>>2]=c[b+12>>2];b=a;c[b>>2]=0;c[b+4>>2]=0;b=a+8|0;c[b>>2]=-1;c[b+4>>2]=-1;i=e;return}function ed(a,b,d,e,f,g){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;g=a;c[g>>2]=0;c[g+4>>2]=0;g=a+8|0;c[g>>2]=-1;c[g+4>>2]=-1;return}function ee(b){b=b|0;if((a[b]&1)==0){return}kS(c[b+8>>2]|0);return}function ef(a,b){a=a|0;b=b|0;return eg(a,b,km(b)|0)|0}function eg(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0;f=b;g=a[f]|0;if((g&1)==0){h=1;i=g}else{g=c[b>>2]|0;h=(g&-2)-1|0;i=g&255}if(h>>>0<e>>>0){g=i&255;if((g&1|0)==0){j=g>>>1}else{j=c[b+4>>2]|0}eA(b,h,e-h|0,j,0,j,e,d);return b|0}if((i&1)==0){k=b+4|0}else{k=c[b+8>>2]|0}ko(k,d,e)|0;c[k+(e<<2)>>2]=0;if((a[f]&1)==0){a[f]=e<<1&255;return b|0}else{c[b+4>>2]=e;return b|0}return 0}function eh(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0;e=b;f=a[e]|0;if((f&1)==0){g=(f&255)>>>1;h=1}else{g=c[b+4>>2]|0;h=(c[b>>2]&-2)-1|0}if((g|0)==(h|0)){eB(b,h,1,h,h,0,0);i=a[e]|0}else{i=f}if((i&1)==0){a[e]=(g<<1)+2&255;j=b+4|0;k=g+1|0;l=j+(g<<2)|0;c[l>>2]=d;m=j+(k<<2)|0;c[m>>2]=0;return}else{e=c[b+8>>2]|0;i=g+1|0;c[b+4>>2]=i;j=e;k=i;l=j+(g<<2)|0;c[l>>2]=d;m=j+(k<<2)|0;c[m>>2]=0;return}}function ei(a){a=a|0;eD(a|0);return}function ej(a,b){a=a|0;b=b|0;iN(a,b+28|0);return}function ek(a,b){a=a|0;b=b|0;c[a+24>>2]=b;c[a+16>>2]=(b|0)==0;c[a+20>>2]=0;c[a+4>>2]=4098;c[a+12>>2]=0;c[a+8>>2]=6;b=a+28|0;k$(a+32|0,0,40);if((b|0)==0){return}iW(b);return}function el(a){a=a|0;eD(a|0);return}function em(a){a=a|0;c[a>>2]=4232;iO(a+4|0);kS(a);return}function en(a){a=a|0;c[a>>2]=4232;iO(a+4|0);return}function eo(a){a=a|0;c[a>>2]=4232;iO(a+4|0);return}function ep(a){a=a|0;c[a>>2]=4232;iW(a+4|0);k$(a+8|0,0,24);return}function eq(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0;f=b;if((e|0)<=0){g=0;return g|0}h=b+12|0;i=b+16|0;j=d;d=0;while(1){k=c[h>>2]|0;if(k>>>0<(c[i>>2]|0)>>>0){c[h>>2]=k+1;l=a[k]|0}else{k=b0[c[(c[f>>2]|0)+40>>2]&127](b)|0;if((k|0)==-1){g=d;m=1854;break}l=k&255}a[j]=l;k=d+1|0;if((k|0)<(e|0)){j=j+1|0;d=k}else{g=k;m=1856;break}}if((m|0)==1854){return g|0}else if((m|0)==1856){return g|0}return 0}function er(a){a=a|0;var b=0,e=0;if((b0[c[(c[a>>2]|0)+36>>2]&127](a)|0)==-1){b=-1;return b|0}e=a+12|0;a=c[e>>2]|0;c[e>>2]=a+1;b=d[a]|0;return b|0}function es(b,e,f){b=b|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0;g=b;if((f|0)<=0){h=0;return h|0}i=b+24|0;j=b+28|0;k=0;l=e;while(1){e=c[i>>2]|0;if(e>>>0<(c[j>>2]|0)>>>0){m=a[l]|0;c[i>>2]=e+1;a[e]=m}else{if((b_[c[(c[g>>2]|0)+52>>2]&31](b,d[l]|0)|0)==-1){h=k;n=1869;break}}m=k+1|0;if((m|0)<(f|0)){k=m;l=l+1|0}else{h=m;n=1871;break}}if((n|0)==1869){return h|0}else if((n|0)==1871){return h|0}return 0}function et(a){a=a|0;c[a>>2]=4160;iO(a+4|0);kS(a);return}function eu(a){a=a|0;c[a>>2]=4160;iO(a+4|0);return}function ev(a){a=a|0;c[a>>2]=4160;iO(a+4|0);return}function ew(a){a=a|0;c[a>>2]=4160;iW(a+4|0);k$(a+8|0,0,24);return}function ex(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0;if(e>>>0>1073741822){dX(0)}if(e>>>0<2){a[b]=e<<1&255;f=b+4|0;g=kn(f,d,e)|0;h=f+(e<<2)|0;c[h>>2]=0;return}else{i=e+4&-4;j=kO(i<<2)|0;c[b+8>>2]=j;c[b>>2]=i|1;c[b+4>>2]=e;f=j;g=kn(f,d,e)|0;h=f+(e<<2)|0;c[h>>2]=0;return}}function ey(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0;if(d>>>0>1073741822){dX(0)}if(d>>>0<2){a[b]=d<<1&255;f=b+4|0;g=kp(f,e,d)|0;h=f+(d<<2)|0;c[h>>2]=0;return}else{i=d+4&-4;j=kO(i<<2)|0;c[b+8>>2]=j;c[b>>2]=i|1;c[b+4>>2]=d;f=j;g=kp(f,e,d)|0;h=f+(d<<2)|0;c[h>>2]=0;return}}function ez(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;if(d>>>0>1073741822){dX(0)}e=b;f=a[e]|0;if((f&1)==0){g=1;h=f}else{f=c[b>>2]|0;g=(f&-2)-1|0;h=f&255}f=h&255;if((f&1|0)==0){i=f>>>1}else{i=c[b+4>>2]|0}f=i>>>0>d>>>0?i:d;if(f>>>0<2){j=2}else{j=f+4&-4}f=j-1|0;if((f|0)==(g|0)){return}if((f|0)==1){k=b+4|0;l=c[b+8>>2]|0;m=1;n=0}else{d=j<<2;if(f>>>0>g>>>0){o=kO(d)|0}else{o=kO(d)|0}d=h&1;if(d<<24>>24==0){p=b+4|0}else{p=c[b+8>>2]|0}k=o;l=p;m=d<<24>>24!=0;n=1}d=k;k=h&255;if((k&1|0)==0){q=k>>>1}else{q=c[b+4>>2]|0}kn(d,l,q+1|0)|0;if(m){kS(l)}if(n){c[b>>2]=j|1;c[b+4>>2]=i;c[b+8>>2]=d;return}else{a[e]=i<<1&255;return}}function eA(b,d,e,f,g,h,i,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;if((1073741821-d|0)>>>0<e>>>0){dX(0)}if((a[b]&1)==0){k=b+4|0}else{k=c[b+8>>2]|0}do{if(d>>>0<536870895){l=e+d|0;m=d<<1;n=l>>>0<m>>>0?m:l;if(n>>>0<2){o=2;break}o=n+4&-4}else{o=1073741822}}while(0);e=kO(o<<2)|0;if((g|0)!=0){kn(e,k,g)|0}if((i|0)!=0){n=e+(g<<2)|0;kn(n,j,i)|0}j=f-h|0;if((j|0)!=(g|0)){f=j-g|0;n=e+(i+g<<2)|0;l=k+(h+g<<2)|0;kn(n,l,f)|0}if((d|0)==1){p=b+8|0;c[p>>2]=e;q=o|1;r=b|0;c[r>>2]=q;s=j+i|0;t=b+4|0;c[t>>2]=s;u=e+(s<<2)|0;c[u>>2]=0;return}kS(k);p=b+8|0;c[p>>2]=e;q=o|1;r=b|0;c[r>>2]=q;s=j+i|0;t=b+4|0;c[t>>2]=s;u=e+(s<<2)|0;c[u>>2]=0;return}function eB(b,d,e,f,g,h,i){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;if((1073741821-d|0)>>>0<e>>>0){dX(0)}if((a[b]&1)==0){j=b+4|0}else{j=c[b+8>>2]|0}do{if(d>>>0<536870895){k=e+d|0;l=d<<1;m=k>>>0<l>>>0?l:k;if(m>>>0<2){n=2;break}n=m+4&-4}else{n=1073741822}}while(0);e=kO(n<<2)|0;if((g|0)!=0){kn(e,j,g)|0}m=f-h|0;if((m|0)!=(g|0)){f=m-g|0;m=e+(i+g<<2)|0;i=j+(h+g<<2)|0;kn(m,i,f)|0}if((d|0)==1){o=b+8|0;c[o>>2]=e;p=n|1;q=b|0;c[q>>2]=p;return}kS(j);o=b+8|0;c[o>>2]=e;p=n|1;q=b|0;c[q>>2]=p;return}function eC(b,d){b=b|0;d=d|0;var e=0,f=0,g=0;e=i;i=i+8|0;f=e|0;g=(c[b+24>>2]|0)==0;if(g){c[b+16>>2]=d|1}else{c[b+16>>2]=d}if(((g&1|d)&c[b+20>>2]|0)==0){i=e;return}e=bO(16)|0;do{if((a[16272]|0)==0){if((bc(16272)|0)==0){break}dj(11840);c[2960]=4e3;a$(66,11840,u|0)|0}}while(0);b=k3(11840,0,32)|0;d=K;c[f>>2]=b&0|1;c[f+4>>2]=d|0;dM(e,f,1304);c[e>>2]=3184;bl(e|0,8752,28)}function eD(a){a=a|0;var b=0,d=0,e=0,f=0;c[a>>2]=3160;b=c[a+40>>2]|0;d=a+32|0;e=a+36|0;if((b|0)!=0){f=b;do{f=f-1|0;b3[c[(c[d>>2]|0)+(f<<2)>>2]&7](0,a,c[(c[e>>2]|0)+(f<<2)>>2]|0);}while((f|0)!=0)}iO(a+28|0);kK(c[d>>2]|0);kK(c[e>>2]|0);kK(c[a+48>>2]|0);kK(c[a+60>>2]|0);return}function eE(a){a=a|0;return 0}function eF(a){a=a|0;return 0}function eG(a){a=a|0;return-1|0}function eH(a,b){a=a|0;b=b|0;return-1|0}function eI(a,b){a=a|0;b=b|0;return-1|0}function eJ(a){a=a|0;return 1416|0}function eK(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;e=i;b=d;d=i;i=i+16|0;c[d>>2]=c[b>>2];c[d+4>>2]=c[b+4>>2];c[d+8>>2]=c[b+8>>2];c[d+12>>2]=c[b+12>>2];b=a;c[b>>2]=0;c[b+4>>2]=0;b=a+8|0;c[b>>2]=-1;c[b+4>>2]=-1;i=e;return}function eL(b,c,d,e,f){b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0;L2437:do{if((e|0)==(f|0)){g=c}else{b=c;h=e;while(1){if((b|0)==(d|0)){i=-1;j=1990;break}k=a[b]|0;l=a[h]|0;if(k<<24>>24<l<<24>>24){i=-1;j=1993;break}if(l<<24>>24<k<<24>>24){i=1;j=1992;break}k=b+1|0;l=h+1|0;if((l|0)==(f|0)){g=k;break L2437}else{b=k;h=l}}if((j|0)==1993){return i|0}else if((j|0)==1992){return i|0}else if((j|0)==1990){return i|0}}}while(0);i=(g|0)!=(d|0)|0;return i|0}function eM(b,c,d){b=b|0;c=c|0;d=d|0;var e=0,f=0,g=0,h=0;if((c|0)==(d|0)){e=0;return e|0}else{f=c;g=0}while(1){c=(a[f]|0)+(g<<4)|0;b=c&-268435456;h=(b>>>24|b)^c;c=f+1|0;if((c|0)==(d|0)){e=h;break}else{f=c;g=h}}return e|0}function eN(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0;e=a;if((d|0)<=0){f=0;return f|0}g=a+12|0;h=a+16|0;i=b;b=0;while(1){j=c[g>>2]|0;if(j>>>0<(c[h>>2]|0)>>>0){c[g>>2]=j+4;k=c[j>>2]|0}else{j=b0[c[(c[e>>2]|0)+40>>2]&127](a)|0;if((j|0)==-1){f=b;l=2007;break}else{k=j}}c[i>>2]=k;j=b+1|0;if((j|0)<(d|0)){i=i+4|0;b=j}else{f=j;l=2008;break}}if((l|0)==2008){return f|0}else if((l|0)==2007){return f|0}return 0}function eO(a){a=a|0;var b=0,d=0;if((b0[c[(c[a>>2]|0)+36>>2]&127](a)|0)==-1){b=-1;return b|0}d=a+12|0;a=c[d>>2]|0;c[d>>2]=a+4;b=c[a>>2]|0;return b|0}function eP(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0;e=a;if((d|0)<=0){f=0;return f|0}g=a+24|0;h=a+28|0;i=0;j=b;while(1){b=c[g>>2]|0;if(b>>>0<(c[h>>2]|0)>>>0){k=c[j>>2]|0;c[g>>2]=b+4;c[b>>2]=k}else{if((b_[c[(c[e>>2]|0)+52>>2]&31](a,c[j>>2]|0)|0)==-1){f=i;l=2023;break}}k=i+1|0;if((k|0)<(d|0)){i=k;j=j+4|0}else{f=k;l=2022;break}}if((l|0)==2023){return f|0}else if((l|0)==2022){return f|0}return 0}function eQ(a){a=a|0;eD(a+8|0);kS(a);return}function eR(a){a=a|0;eD(a+8|0);return}function eS(a){a=a|0;var b=0,d=0;b=a;d=c[(c[a>>2]|0)-12>>2]|0;eD(b+(d+8)|0);kS(b+d|0);return}function eT(a){a=a|0;eD(a+((c[(c[a>>2]|0)-12>>2]|0)+8)|0);return}function eU(a){a=a|0;eD(a+8|0);kS(a);return}function eV(a){a=a|0;eD(a+8|0);return}function eW(a){a=a|0;var b=0,d=0;b=a;d=c[(c[a>>2]|0)-12>>2]|0;eD(b+(d+8)|0);kS(b+d|0);return}function eX(a){a=a|0;eD(a+((c[(c[a>>2]|0)-12>>2]|0)+8)|0);return}function eY(a){a=a|0;eD(a+4|0);kS(a);return}function eZ(a){a=a|0;eD(a+4|0);return}function e_(a){a=a|0;var b=0,d=0;b=a;d=c[(c[a>>2]|0)-12>>2]|0;eD(b+(d+4)|0);kS(b+d|0);return}function e$(a){a=a|0;eD(a+((c[(c[a>>2]|0)-12>>2]|0)+4)|0);return}function e0(a){a=a|0;eD(a+4|0);kS(a);return}function e1(a){a=a|0;eD(a+4|0);return}function e2(a){a=a|0;var b=0,d=0;b=a;d=c[(c[a>>2]|0)-12>>2]|0;eD(b+(d+4)|0);kS(b+d|0);return}function e3(a){a=a|0;eD(a+((c[(c[a>>2]|0)-12>>2]|0)+4)|0);return}function e4(a,b,c){a=a|0;b=b|0;c=c|0;if((c|0)==1){dZ(a,1640,35);return}else{dK(a,b|0,c);return}}function e5(a){a=a|0;df(a|0);return}function e6(a){a=a|0;dO(a|0);kS(a);return}function e7(a){a=a|0;dO(a|0);return}function e8(a){a=a|0;eD(a);kS(a);return}function e9(a){a=a|0;df(a|0);kS(a);return}function fa(a){a=a|0;de(a|0);kS(a);return}function fb(a){a=a|0;de(a|0);return}function fc(a){a=a|0;de(a|0);return}function fd(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0;d=e;g=f-d|0;do{if((g|0)==-1){dX(b);h=2058}else{if(g>>>0>=11){h=2058;break}a[b]=g<<1&255;i=b+1|0}}while(0);if((h|0)==2058){h=g+16&-16;j=kO(h)|0;c[b+8>>2]=j;c[b>>2]=h|1;c[b+4>>2]=g;i=j}if((e|0)==(f|0)){k=i;a[k]=0;return}j=f+(-d|0)|0;d=i;g=e;while(1){a[d]=a[g]|0;e=g+1|0;if((e|0)==(f|0)){break}else{d=d+1|0;g=e}}k=i+j|0;a[k]=0;return}function fe(a){a=a|0;de(a|0);kS(a);return}function ff(a){a=a|0;de(a|0);return}function fg(b){b=b|0;var d=0,e=0,f=0,g=0,h=0,j=0,k=0;d=i;i=i+8|0;e=d|0;f=b;g=c[(c[f>>2]|0)-12>>2]|0;h=b;if((c[h+(g+24)>>2]|0)==0){i=d;return b|0}j=e|0;a[j]=0;c[e+4>>2]=b;do{if((c[h+(g+16)>>2]|0)==0){k=c[h+(g+72)>>2]|0;if((k|0)!=0){fg(k)|0}a[j]=1;k=c[h+((c[(c[f>>2]|0)-12>>2]|0)+24)>>2]|0;if((b0[c[(c[k>>2]|0)+24>>2]&127](k)|0)!=-1){break}k=c[(c[f>>2]|0)-12>>2]|0;eC(h+k|0,c[h+(k+16)>>2]|1)}}while(0);fi(e);i=d;return b|0}function fh(b){b=b|0;var d=0,e=0,f=0,g=0,h=0,j=0,k=0;d=i;i=i+8|0;e=d|0;f=b;g=c[(c[f>>2]|0)-12>>2]|0;h=b;if((c[h+(g+24)>>2]|0)==0){i=d;return b|0}j=e|0;a[j]=0;c[e+4>>2]=b;do{if((c[h+(g+16)>>2]|0)==0){k=c[h+(g+72)>>2]|0;if((k|0)!=0){fh(k)|0}a[j]=1;k=c[h+((c[(c[f>>2]|0)-12>>2]|0)+24)>>2]|0;if((b0[c[(c[k>>2]|0)+24>>2]&127](k)|0)!=-1){break}k=c[(c[f>>2]|0)-12>>2]|0;eC(h+k|0,c[h+(k+16)>>2]|1)}}while(0);fj(e);i=d;return b|0}function fi(a){a=a|0;var b=0,d=0,e=0;b=a+4|0;a=c[b>>2]|0;d=c[(c[a>>2]|0)-12>>2]|0;e=a;if((c[e+(d+24)>>2]|0)==0){return}if((c[e+(d+16)>>2]|0)!=0){return}if((c[e+(d+4)>>2]&8192|0)==0){return}if(bh()|0){return}d=c[b>>2]|0;e=c[d+((c[(c[d>>2]|0)-12>>2]|0)+24)>>2]|0;if((b0[c[(c[e>>2]|0)+24>>2]&127](e)|0)!=-1){return}e=c[b>>2]|0;b=c[(c[e>>2]|0)-12>>2]|0;d=e;eC(d+b|0,c[d+(b+16)>>2]|1);return}function fj(a){a=a|0;var b=0,d=0,e=0;b=a+4|0;a=c[b>>2]|0;d=c[(c[a>>2]|0)-12>>2]|0;e=a;if((c[e+(d+24)>>2]|0)==0){return}if((c[e+(d+16)>>2]|0)!=0){return}if((c[e+(d+4)>>2]&8192|0)==0){return}if(bh()|0){return}d=c[b>>2]|0;e=c[d+((c[(c[d>>2]|0)-12>>2]|0)+24)>>2]|0;if((b0[c[(c[e>>2]|0)+24>>2]&127](e)|0)!=-1){return}e=c[b>>2]|0;b=c[(c[e>>2]|0)-12>>2]|0;d=e;eC(d+b|0,c[d+(b+16)>>2]|1);return}function fk(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0;L2596:do{if((e|0)==(f|0)){g=b}else{a=b;h=e;while(1){if((a|0)==(d|0)){i=-1;j=2147;break}k=c[a>>2]|0;l=c[h>>2]|0;if((k|0)<(l|0)){i=-1;j=2150;break}if((l|0)<(k|0)){i=1;j=2148;break}k=a+4|0;l=h+4|0;if((l|0)==(f|0)){g=k;break L2596}else{a=k;h=l}}if((j|0)==2148){return i|0}else if((j|0)==2147){return i|0}else if((j|0)==2150){return i|0}}}while(0);i=(g|0)!=(d|0)|0;return i|0}function fl(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0;if((b|0)==(d|0)){e=0;return e|0}else{f=b;g=0}while(1){b=(c[f>>2]|0)+(g<<4)|0;a=b&-268435456;h=(a>>>24|a)^b;b=f+4|0;if((b|0)==(d|0)){e=h;break}else{f=b;g=h}}return e|0}function fm(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0;d=e;g=f-d|0;h=g>>2;if(h>>>0>1073741822){dX(b)}if(h>>>0<2){a[b]=g>>>1&255;i=b+4|0}else{g=h+4&-4;j=kO(g<<2)|0;c[b+8>>2]=j;c[b>>2]=g|1;c[b+4>>2]=h;i=j}if((e|0)==(f|0)){k=i;c[k>>2]=0;return}j=(f-4+(-d|0)|0)>>>2;d=i;h=e;while(1){c[d>>2]=c[h>>2];e=h+4|0;if((e|0)==(f|0)){break}else{d=d+4|0;h=e}}k=i+(j+1<<2)|0;c[k>>2]=0;return}function fn(a){a=a|0;de(a|0);kS(a);return}function fo(a){a=a|0;de(a|0);return}function fp(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;k=i;i=i+112|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=k|0;m=k+16|0;n=k+32|0;o=k+40|0;p=k+48|0;q=k+56|0;r=k+64|0;s=k+72|0;t=k+80|0;u=k+104|0;if((c[g+4>>2]&1|0)==0){c[n>>2]=-1;v=c[(c[d>>2]|0)+16>>2]|0;w=e|0;c[p>>2]=c[w>>2];c[q>>2]=c[f>>2];bX[v&127](o,d,p,q,g,h,n);q=c[o>>2]|0;c[w>>2]=q;switch(c[n>>2]|0){case 1:{a[j]=1;break};case 0:{a[j]=0;break};default:{a[j]=1;c[h>>2]=4}}c[b>>2]=q;i=k;return}ej(r,g);q=r|0;r=c[q>>2]|0;if((c[3924]|0)!=-1){c[m>>2]=15696;c[m+4>>2]=12;c[m+8>>2]=0;dW(15696,m,96)}m=(c[3925]|0)-1|0;n=c[r+8>>2]|0;do{if((c[r+12>>2]|0)-n>>2>>>0>m>>>0){w=c[n+(m<<2)>>2]|0;if((w|0)==0){break}o=w;w=c[q>>2]|0;dz(w)|0;ej(s,g);w=s|0;p=c[w>>2]|0;if((c[3828]|0)!=-1){c[l>>2]=15312;c[l+4>>2]=12;c[l+8>>2]=0;dW(15312,l,96)}d=(c[3829]|0)-1|0;v=c[p+8>>2]|0;do{if((c[p+12>>2]|0)-v>>2>>>0>d>>>0){x=c[v+(d<<2)>>2]|0;if((x|0)==0){break}y=x;z=c[w>>2]|0;dz(z)|0;z=t|0;A=x;bZ[c[(c[A>>2]|0)+24>>2]&127](z,y);bZ[c[(c[A>>2]|0)+28>>2]&127](t+12|0,y);c[u>>2]=c[f>>2];a[j]=(fq(e,u,z,t+24|0,o,h,1)|0)==(z|0)|0;c[b>>2]=c[e>>2];dP(t+12|0);dP(t|0);i=k;return}}while(0);o=bO(4)|0;kq(o);bl(o|0,8176,134)}}while(0);k=bO(4)|0;kq(k);bl(k|0,8176,134)}function fq(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0;l=i;i=i+104|0;m=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[m>>2];m=(g-f|0)/12|0;n=l|0;do{if(m>>>0>100){o=kJ(m)|0;if((o|0)!=0){p=o;q=o;break}kX();p=0;q=0}else{p=n;q=0}}while(0);n=(f|0)==(g|0);if(n){r=m;s=0}else{o=m;m=0;t=p;u=f;while(1){v=d[u]|0;if((v&1|0)==0){w=v>>>1}else{w=c[u+4>>2]|0}if((w|0)==0){a[t]=2;x=m+1|0;y=o-1|0}else{a[t]=1;x=m;y=o}v=u+12|0;if((v|0)==(g|0)){r=y;s=x;break}else{o=y;m=x;t=t+1|0;u=v}}}u=b|0;b=e|0;e=h;t=0;x=s;s=r;while(1){r=c[u>>2]|0;do{if((r|0)==0){z=0}else{if((c[r+12>>2]|0)!=(c[r+16>>2]|0)){z=r;break}if((b0[c[(c[r>>2]|0)+36>>2]&127](r)|0)==-1){c[u>>2]=0;z=0;break}else{z=c[u>>2]|0;break}}}while(0);r=(z|0)==0;m=c[b>>2]|0;if((m|0)==0){A=z;B=0}else{do{if((c[m+12>>2]|0)==(c[m+16>>2]|0)){if((b0[c[(c[m>>2]|0)+36>>2]&127](m)|0)!=-1){C=m;break}c[b>>2]=0;C=0}else{C=m}}while(0);A=c[u>>2]|0;B=C}D=(B|0)==0;if(!((r^D)&(s|0)!=0)){break}m=c[A+12>>2]|0;if((m|0)==(c[A+16>>2]|0)){E=(b0[c[(c[A>>2]|0)+36>>2]&127](A)|0)&255}else{E=a[m]|0}if(k){F=E}else{F=b_[c[(c[e>>2]|0)+12>>2]&31](h,E)|0}do{if(n){G=x;H=s}else{m=t+1|0;L2707:do{if(k){y=s;o=x;w=p;v=0;I=f;while(1){do{if((a[w]|0)==1){J=I;if((a[J]&1)==0){K=I+1|0}else{K=c[I+8>>2]|0}if(F<<24>>24!=(a[K+t|0]|0)){a[w]=0;L=v;M=o;N=y-1|0;break}O=d[J]|0;if((O&1|0)==0){P=O>>>1}else{P=c[I+4>>2]|0}if((P|0)!=(m|0)){L=1;M=o;N=y;break}a[w]=2;L=1;M=o+1|0;N=y-1|0}else{L=v;M=o;N=y}}while(0);O=I+12|0;if((O|0)==(g|0)){Q=N;R=M;S=L;break L2707}y=N;o=M;w=w+1|0;v=L;I=O}}else{I=s;v=x;w=p;o=0;y=f;while(1){do{if((a[w]|0)==1){O=y;if((a[O]&1)==0){T=y+1|0}else{T=c[y+8>>2]|0}if(F<<24>>24!=(b_[c[(c[e>>2]|0)+12>>2]&31](h,a[T+t|0]|0)|0)<<24>>24){a[w]=0;U=o;V=v;W=I-1|0;break}J=d[O]|0;if((J&1|0)==0){X=J>>>1}else{X=c[y+4>>2]|0}if((X|0)!=(m|0)){U=1;V=v;W=I;break}a[w]=2;U=1;V=v+1|0;W=I-1|0}else{U=o;V=v;W=I}}while(0);J=y+12|0;if((J|0)==(g|0)){Q=W;R=V;S=U;break L2707}I=W;v=V;w=w+1|0;o=U;y=J}}}while(0);if(!S){G=R;H=Q;break}m=c[u>>2]|0;y=m+12|0;o=c[y>>2]|0;if((o|0)==(c[m+16>>2]|0)){w=c[(c[m>>2]|0)+40>>2]|0;b0[w&127](m)|0}else{c[y>>2]=o+1}if((R+Q|0)>>>0<2|n){G=R;H=Q;break}o=t+1|0;y=R;m=p;w=f;while(1){do{if((a[m]|0)==2){v=d[w]|0;if((v&1|0)==0){Y=v>>>1}else{Y=c[w+4>>2]|0}if((Y|0)==(o|0)){Z=y;break}a[m]=0;Z=y-1|0}else{Z=y}}while(0);v=w+12|0;if((v|0)==(g|0)){G=Z;H=Q;break}else{y=Z;m=m+1|0;w=v}}}}while(0);t=t+1|0;x=G;s=H}do{if((A|0)==0){_=0}else{if((c[A+12>>2]|0)!=(c[A+16>>2]|0)){_=A;break}if((b0[c[(c[A>>2]|0)+36>>2]&127](A)|0)==-1){c[u>>2]=0;_=0;break}else{_=c[u>>2]|0;break}}}while(0);u=(_|0)==0;do{if(D){$=2293}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){if(u){break}else{$=2295;break}}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)==-1){c[b>>2]=0;$=2293;break}else{if(u^(B|0)==0){break}else{$=2295;break}}}}while(0);if(($|0)==2293){if(u){$=2295}}if(($|0)==2295){c[j>>2]=c[j>>2]|2}L2786:do{if(n){$=2300}else{u=f;B=p;while(1){if((a[B]|0)==2){aa=u;break L2786}b=u+12|0;if((b|0)==(g|0)){$=2300;break L2786}u=b;B=B+1|0}}}while(0);if(($|0)==2300){c[j>>2]=c[j>>2]|4;aa=g}if((q|0)==0){i=l;return aa|0}kK(q);i=l;return aa|0}function fr(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;e=i;i=i+72|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+32|0;m=e+40|0;n=e+56|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 64:{t=8;break};case 8:{t=16;break};case 0:{t=0;break};default:{t=10}}u=e|0;fx(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=a[l]|0;l=w;w=c[h>>2]|0;L2809:while(1){do{if((w|0)==0){B=0}else{if((c[w+12>>2]|0)!=(c[w+16>>2]|0)){B=w;break}if((b0[c[(c[w>>2]|0)+36>>2]&127](w)|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);C=(B|0)==0;D=c[f>>2]|0;do{if((D|0)==0){E=2327}else{if((c[D+12>>2]|0)!=(c[D+16>>2]|0)){if(C){F=D;G=0;break}else{H=l;I=D;J=0;break L2809}}if((b0[c[(c[D>>2]|0)+36>>2]&127](D)|0)==-1){c[f>>2]=0;E=2327;break}else{K=(D|0)==0;if(C^K){F=D;G=K;break}else{H=l;I=D;J=K;break L2809}}}}while(0);if((E|0)==2327){E=0;if(C){H=l;I=0;J=1;break}else{F=0;G=1}}D=d[o]|0;K=(D&1|0)==0;if(((c[p>>2]|0)-l|0)==((K?D>>>1:c[z>>2]|0)|0)){if(K){L=D>>>1;M=D>>>1}else{D=c[z>>2]|0;L=D;M=D}dR(n,L<<1,0);if((a[o]&1)==0){N=10}else{N=(c[g>>2]&-2)-1|0}dR(n,N,0);if((a[o]&1)==0){O=x}else{O=c[y>>2]|0}c[p>>2]=O+M;P=O}else{P=l}D=B+12|0;K=c[D>>2]|0;Q=B+16|0;if((K|0)==(c[Q>>2]|0)){R=(b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)&255}else{R=a[K]|0}if((fs(R,t,P,p,s,A,m,v,r,u)|0)!=0){H=P;I=F;J=G;break}K=c[D>>2]|0;if((K|0)==(c[Q>>2]|0)){Q=c[(c[B>>2]|0)+40>>2]|0;b0[Q&127](B)|0;l=P;w=B;continue}else{c[D>>2]=K+1;l=P;w=B;continue}}w=d[m]|0;if((w&1|0)==0){S=w>>>1}else{S=c[m+4>>2]|0}do{if((S|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}P=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=P}}while(0);c[k>>2]=fu(H,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(C){T=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){T=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){T=B;break}c[h>>2]=0;T=0}}while(0);h=(T|0)==0;L2869:do{if(J){E=2368}else{do{if((c[I+12>>2]|0)==(c[I+16>>2]|0)){if((b0[c[(c[I>>2]|0)+36>>2]&127](I)|0)!=-1){break}c[f>>2]=0;E=2368;break L2869}}while(0);if(!(h^(I|0)==0)){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);do{if((E|0)==2368){if(h){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}function fs(b,e,f,g,h,i,j,k,l,m){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0;n=c[g>>2]|0;o=(n|0)==(f|0);do{if(o){p=(a[m+24|0]|0)==b<<24>>24;if(!p){if((a[m+25|0]|0)!=b<<24>>24){break}}c[g>>2]=f+1;a[f]=p?43:45;c[h>>2]=0;q=0;return q|0}}while(0);p=d[j]|0;if((p&1|0)==0){r=p>>>1}else{r=c[j+4>>2]|0}if((r|0)!=0&b<<24>>24==i<<24>>24){i=c[l>>2]|0;if((i-k|0)>=160){q=0;return q|0}k=c[h>>2]|0;c[l>>2]=i+4;c[i>>2]=k;c[h>>2]=0;q=0;return q|0}k=m+26|0;i=m;while(1){if((i|0)==(k|0)){s=k;break}if((a[i]|0)==b<<24>>24){s=i;break}else{i=i+1|0}}i=s-m|0;if((i|0)>23){q=-1;return q|0}L2908:do{switch(e|0){case 8:case 10:{if((i|0)<(e|0)){break L2908}else{q=-1}return q|0};case 16:{if((i|0)<22){break L2908}if(o){q=-1;return q|0}if((n-f|0)>=3){q=-1;return q|0}if((a[n-1|0]|0)!=48){q=-1;return q|0}c[h>>2]=0;m=a[9744+i|0]|0;s=c[g>>2]|0;c[g>>2]=s+1;a[s]=m;q=0;return q|0};default:{}}}while(0);f=a[9744+i|0]|0;c[g>>2]=n+1;a[n]=f;c[h>>2]=(c[h>>2]|0)+1;q=0;return q|0}function ft(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;g=b;h=b;i=a[h]|0;j=i&255;if((j&1|0)==0){k=j>>>1}else{k=c[b+4>>2]|0}if((k|0)==0){return}do{if((d|0)==(e|0)){l=i}else{k=e-4|0;if(k>>>0>d>>>0){m=d;n=k}else{l=i;break}do{k=c[m>>2]|0;c[m>>2]=c[n>>2];c[n>>2]=k;m=m+4|0;n=n-4|0;}while(m>>>0<n>>>0);l=a[h]|0}}while(0);if((l&1)==0){o=g+1|0}else{o=c[b+8>>2]|0}g=l&255;if((g&1|0)==0){p=g>>>1}else{p=c[b+4>>2]|0}b=e-4|0;e=a[o]|0;g=e<<24>>24;l=e<<24>>24<1|e<<24>>24==127;L2947:do{if(b>>>0>d>>>0){e=o+p|0;h=o;n=d;m=g;i=l;while(1){if(!i){if((m|0)!=(c[n>>2]|0)){break}}k=(e-h|0)>1?h+1|0:h;j=n+4|0;q=a[k]|0;r=q<<24>>24;s=q<<24>>24<1|q<<24>>24==127;if(j>>>0<b>>>0){h=k;n=j;m=r;i=s}else{t=r;u=s;break L2947}}c[f>>2]=4;return}else{t=g;u=l}}while(0);if(u){return}u=c[b>>2]|0;if(!(t>>>0<u>>>0|(u|0)==0)){return}c[f>>2]=4;return}function fu(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0;g=i;i=i+8|0;h=g|0;if((b|0)==(d|0)){c[e>>2]=4;j=0;i=g;return j|0}k=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=bI(b|0,h|0,f|0,c[2958]|0)|0;f=K;b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=k}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;i=g;return j|0}d=-1;h=0;if((b|0)==34|((f|0)<(d|0)|(f|0)==(d|0)&l>>>0<-2147483648>>>0)|((f|0)>(h|0)|(f|0)==(h|0)&l>>>0>2147483647>>>0)){c[e>>2]=4;e=0;j=(f|0)>(e|0)|(f|0)==(e|0)&l>>>0>0>>>0?2147483647:-2147483648;i=g;return j|0}else{j=l;i=g;return j|0}return 0}function fv(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0;e=i;i=i+72|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+32|0;m=e+40|0;n=e+56|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 64:{t=8;break};case 0:{t=0;break};default:{t=10}}u=e|0;fx(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=a[l]|0;l=w;w=c[h>>2]|0;L2997:while(1){do{if((w|0)==0){B=0}else{if((c[w+12>>2]|0)!=(c[w+16>>2]|0)){B=w;break}if((b0[c[(c[w>>2]|0)+36>>2]&127](w)|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);C=(B|0)==0;D=c[f>>2]|0;do{if((D|0)==0){E=2476}else{if((c[D+12>>2]|0)!=(c[D+16>>2]|0)){if(C){F=D;G=0;break}else{H=l;I=D;J=0;break L2997}}if((b0[c[(c[D>>2]|0)+36>>2]&127](D)|0)==-1){c[f>>2]=0;E=2476;break}else{L=(D|0)==0;if(C^L){F=D;G=L;break}else{H=l;I=D;J=L;break L2997}}}}while(0);if((E|0)==2476){E=0;if(C){H=l;I=0;J=1;break}else{F=0;G=1}}D=d[o]|0;L=(D&1|0)==0;if(((c[p>>2]|0)-l|0)==((L?D>>>1:c[z>>2]|0)|0)){if(L){M=D>>>1;N=D>>>1}else{D=c[z>>2]|0;M=D;N=D}dR(n,M<<1,0);if((a[o]&1)==0){O=10}else{O=(c[g>>2]&-2)-1|0}dR(n,O,0);if((a[o]&1)==0){P=x}else{P=c[y>>2]|0}c[p>>2]=P+N;Q=P}else{Q=l}D=B+12|0;L=c[D>>2]|0;R=B+16|0;if((L|0)==(c[R>>2]|0)){S=(b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)&255}else{S=a[L]|0}if((fs(S,t,Q,p,s,A,m,v,r,u)|0)!=0){H=Q;I=F;J=G;break}L=c[D>>2]|0;if((L|0)==(c[R>>2]|0)){R=c[(c[B>>2]|0)+40>>2]|0;b0[R&127](B)|0;l=Q;w=B;continue}else{c[D>>2]=L+1;l=Q;w=B;continue}}w=d[m]|0;if((w&1|0)==0){T=w>>>1}else{T=c[m+4>>2]|0}do{if((T|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}Q=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=Q}}while(0);s=fw(H,c[p>>2]|0,j,t)|0;c[k>>2]=s;c[k+4>>2]=K;ft(m,v,c[r>>2]|0,j);do{if(C){U=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){U=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){U=B;break}c[h>>2]=0;U=0}}while(0);h=(U|0)==0;L3057:do{if(J){E=2517}else{do{if((c[I+12>>2]|0)==(c[I+16>>2]|0)){if((b0[c[(c[I>>2]|0)+36>>2]&127](I)|0)!=-1){break}c[f>>2]=0;E=2517;break L3057}}while(0);if(!(h^(I|0)==0)){break}V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}}while(0);do{if((E|0)==2517){if(h){break}V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}function fw(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0;g=i;i=i+8|0;h=g|0;if((b|0)==(d|0)){c[e>>2]=4;j=0;k=0;i=g;return(K=j,k)|0}l=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);m=bI(b|0,h|0,f|0,c[2958]|0)|0;f=K;b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=l}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;k=0;i=g;return(K=j,k)|0}if((b|0)!=34){j=f;k=m;i=g;return(K=j,k)|0}c[e>>2]=4;e=0;b=(f|0)>(e|0)|(f|0)==(e|0)&m>>>0>0>>>0;j=b?2147483647:-2147483648;k=b?-1:0;i=g;return(K=j,k)|0}function fx(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;g=i;i=i+40|0;h=g|0;j=g+16|0;k=g+32|0;ej(k,d);d=k|0;k=c[d>>2]|0;if((c[3924]|0)!=-1){c[j>>2]=15696;c[j+4>>2]=12;c[j+8>>2]=0;dW(15696,j,96)}j=(c[3925]|0)-1|0;l=c[k+8>>2]|0;do{if((c[k+12>>2]|0)-l>>2>>>0>j>>>0){m=c[l+(j<<2)>>2]|0;if((m|0)==0){break}n=m;o=c[(c[m>>2]|0)+32>>2]|0;ca[o&15](n,9744,9770,e)|0;n=c[d>>2]|0;if((c[3828]|0)!=-1){c[h>>2]=15312;c[h+4>>2]=12;c[h+8>>2]=0;dW(15312,h,96)}o=(c[3829]|0)-1|0;m=c[n+8>>2]|0;do{if((c[n+12>>2]|0)-m>>2>>>0>o>>>0){p=c[m+(o<<2)>>2]|0;if((p|0)==0){break}q=p;a[f]=b0[c[(c[p>>2]|0)+16>>2]&127](q)|0;bZ[c[(c[p>>2]|0)+20>>2]&127](b,q);q=c[d>>2]|0;dz(q)|0;i=g;return}}while(0);o=bO(4)|0;kq(o);bl(o|0,8176,134)}}while(0);g=bO(4)|0;kq(g);bl(g|0,8176,134)}function fy(e,f,g,h,j,k,l){e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0;f=i;i=i+72|0;m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=h;h=i;i=i+4|0;i=i+7>>3<<3;c[h>>2]=c[m>>2];m=f+32|0;n=f+40|0;o=f+56|0;p=o;q=i;i=i+4|0;i=i+7>>3<<3;r=i;i=i+160|0;s=i;i=i+4|0;i=i+7>>3<<3;t=i;i=i+4|0;i=i+7>>3<<3;switch(c[j+4>>2]&74|0){case 0:{u=0;break};case 8:{u=16;break};case 64:{u=8;break};default:{u=10}}v=f|0;fx(n,j,v,m);k$(p|0,0,12);j=o;dR(o,10,0);if((a[p]&1)==0){w=j+1|0;x=w;y=w;z=o+8|0}else{w=o+8|0;x=c[w>>2]|0;y=j+1|0;z=w}c[q>>2]=x;w=r|0;c[s>>2]=w;c[t>>2]=0;j=g|0;g=h|0;h=o|0;A=o+4|0;B=a[m]|0;m=x;x=c[j>>2]|0;L3122:while(1){do{if((x|0)==0){C=0}else{if((c[x+12>>2]|0)!=(c[x+16>>2]|0)){C=x;break}if((b0[c[(c[x>>2]|0)+36>>2]&127](x)|0)!=-1){C=x;break}c[j>>2]=0;C=0}}while(0);D=(C|0)==0;E=c[g>>2]|0;do{if((E|0)==0){F=2579}else{if((c[E+12>>2]|0)!=(c[E+16>>2]|0)){if(D){G=E;H=0;break}else{I=m;J=E;K=0;break L3122}}if((b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)==-1){c[g>>2]=0;F=2579;break}else{L=(E|0)==0;if(D^L){G=E;H=L;break}else{I=m;J=E;K=L;break L3122}}}}while(0);if((F|0)==2579){F=0;if(D){I=m;J=0;K=1;break}else{G=0;H=1}}E=d[p]|0;L=(E&1|0)==0;if(((c[q>>2]|0)-m|0)==((L?E>>>1:c[A>>2]|0)|0)){if(L){M=E>>>1;N=E>>>1}else{E=c[A>>2]|0;M=E;N=E}dR(o,M<<1,0);if((a[p]&1)==0){O=10}else{O=(c[h>>2]&-2)-1|0}dR(o,O,0);if((a[p]&1)==0){P=y}else{P=c[z>>2]|0}c[q>>2]=P+N;Q=P}else{Q=m}E=C+12|0;L=c[E>>2]|0;R=C+16|0;if((L|0)==(c[R>>2]|0)){S=(b0[c[(c[C>>2]|0)+36>>2]&127](C)|0)&255}else{S=a[L]|0}if((fs(S,u,Q,q,t,B,n,w,s,v)|0)!=0){I=Q;J=G;K=H;break}L=c[E>>2]|0;if((L|0)==(c[R>>2]|0)){R=c[(c[C>>2]|0)+40>>2]|0;b0[R&127](C)|0;m=Q;x=C;continue}else{c[E>>2]=L+1;m=Q;x=C;continue}}x=d[n]|0;if((x&1|0)==0){T=x>>>1}else{T=c[n+4>>2]|0}do{if((T|0)!=0){x=c[s>>2]|0;if((x-r|0)>=160){break}Q=c[t>>2]|0;c[s>>2]=x+4;c[x>>2]=Q}}while(0);b[l>>1]=fz(I,c[q>>2]|0,k,u)|0;ft(n,w,c[s>>2]|0,k);do{if(D){U=0}else{if((c[C+12>>2]|0)!=(c[C+16>>2]|0)){U=C;break}if((b0[c[(c[C>>2]|0)+36>>2]&127](C)|0)!=-1){U=C;break}c[j>>2]=0;U=0}}while(0);j=(U|0)==0;L3182:do{if(K){F=2620}else{do{if((c[J+12>>2]|0)==(c[J+16>>2]|0)){if((b0[c[(c[J>>2]|0)+36>>2]&127](J)|0)!=-1){break}c[g>>2]=0;F=2620;break L3182}}while(0);if(!(j^(J|0)==0)){break}V=e|0;c[V>>2]=U;dP(o);dP(n);i=f;return}}while(0);do{if((F|0)==2620){if(j){break}V=e|0;c[V>>2]=U;dP(o);dP(n);i=f;return}}while(0);c[k>>2]=c[k>>2]|2;V=e|0;c[V>>2]=U;dP(o);dP(n);i=f;return}function fz(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0;g=i;i=i+8|0;h=g|0;if((b|0)==(d|0)){c[e>>2]=4;j=0;i=g;return j|0}if((a[b]|0)==45){c[e>>2]=4;j=0;i=g;return j|0}k=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=aI(b|0,h|0,f|0,c[2958]|0)|0;f=K;b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=k}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;i=g;return j|0}d=0;if((b|0)==34|(f>>>0>d>>>0|f>>>0==d>>>0&l>>>0>65535>>>0)){c[e>>2]=4;j=-1;i=g;return j|0}else{j=l&65535;i=g;return j|0}return 0}function fA(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;e=i;i=i+72|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+32|0;m=e+40|0;n=e+56|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 64:{t=8;break};case 0:{t=0;break};case 8:{t=16;break};default:{t=10}}u=e|0;fx(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=a[l]|0;l=w;w=c[h>>2]|0;L3232:while(1){do{if((w|0)==0){B=0}else{if((c[w+12>>2]|0)!=(c[w+16>>2]|0)){B=w;break}if((b0[c[(c[w>>2]|0)+36>>2]&127](w)|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);C=(B|0)==0;D=c[f>>2]|0;do{if((D|0)==0){E=2669}else{if((c[D+12>>2]|0)!=(c[D+16>>2]|0)){if(C){F=D;G=0;break}else{H=l;I=D;J=0;break L3232}}if((b0[c[(c[D>>2]|0)+36>>2]&127](D)|0)==-1){c[f>>2]=0;E=2669;break}else{K=(D|0)==0;if(C^K){F=D;G=K;break}else{H=l;I=D;J=K;break L3232}}}}while(0);if((E|0)==2669){E=0;if(C){H=l;I=0;J=1;break}else{F=0;G=1}}D=d[o]|0;K=(D&1|0)==0;if(((c[p>>2]|0)-l|0)==((K?D>>>1:c[z>>2]|0)|0)){if(K){L=D>>>1;M=D>>>1}else{D=c[z>>2]|0;L=D;M=D}dR(n,L<<1,0);if((a[o]&1)==0){N=10}else{N=(c[g>>2]&-2)-1|0}dR(n,N,0);if((a[o]&1)==0){O=x}else{O=c[y>>2]|0}c[p>>2]=O+M;P=O}else{P=l}D=B+12|0;K=c[D>>2]|0;Q=B+16|0;if((K|0)==(c[Q>>2]|0)){R=(b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)&255}else{R=a[K]|0}if((fs(R,t,P,p,s,A,m,v,r,u)|0)!=0){H=P;I=F;J=G;break}K=c[D>>2]|0;if((K|0)==(c[Q>>2]|0)){Q=c[(c[B>>2]|0)+40>>2]|0;b0[Q&127](B)|0;l=P;w=B;continue}else{c[D>>2]=K+1;l=P;w=B;continue}}w=d[m]|0;if((w&1|0)==0){S=w>>>1}else{S=c[m+4>>2]|0}do{if((S|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}P=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=P}}while(0);c[k>>2]=fB(H,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(C){T=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){T=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){T=B;break}c[h>>2]=0;T=0}}while(0);h=(T|0)==0;L3292:do{if(J){E=2710}else{do{if((c[I+12>>2]|0)==(c[I+16>>2]|0)){if((b0[c[(c[I>>2]|0)+36>>2]&127](I)|0)!=-1){break}c[f>>2]=0;E=2710;break L3292}}while(0);if(!(h^(I|0)==0)){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);do{if((E|0)==2710){if(h){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}function fB(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0;g=i;i=i+8|0;h=g|0;if((b|0)==(d|0)){c[e>>2]=4;j=0;i=g;return j|0}if((a[b]|0)==45){c[e>>2]=4;j=0;i=g;return j|0}k=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=aI(b|0,h|0,f|0,c[2958]|0)|0;f=K;b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=k}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;i=g;return j|0}d=0;if((b|0)==34|(f>>>0>d>>>0|f>>>0==d>>>0&l>>>0>-1>>>0)){c[e>>2]=4;j=-1;i=g;return j|0}else{j=l;i=g;return j|0}return 0}function fC(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;e=i;i=i+72|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+32|0;m=e+40|0;n=e+56|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 0:{t=0;break};case 8:{t=16;break};case 64:{t=8;break};default:{t=10}}u=e|0;fx(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=a[l]|0;l=w;w=c[h>>2]|0;L3342:while(1){do{if((w|0)==0){B=0}else{if((c[w+12>>2]|0)!=(c[w+16>>2]|0)){B=w;break}if((b0[c[(c[w>>2]|0)+36>>2]&127](w)|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);C=(B|0)==0;D=c[f>>2]|0;do{if((D|0)==0){E=2759}else{if((c[D+12>>2]|0)!=(c[D+16>>2]|0)){if(C){F=D;G=0;break}else{H=l;I=D;J=0;break L3342}}if((b0[c[(c[D>>2]|0)+36>>2]&127](D)|0)==-1){c[f>>2]=0;E=2759;break}else{K=(D|0)==0;if(C^K){F=D;G=K;break}else{H=l;I=D;J=K;break L3342}}}}while(0);if((E|0)==2759){E=0;if(C){H=l;I=0;J=1;break}else{F=0;G=1}}D=d[o]|0;K=(D&1|0)==0;if(((c[p>>2]|0)-l|0)==((K?D>>>1:c[z>>2]|0)|0)){if(K){L=D>>>1;M=D>>>1}else{D=c[z>>2]|0;L=D;M=D}dR(n,L<<1,0);if((a[o]&1)==0){N=10}else{N=(c[g>>2]&-2)-1|0}dR(n,N,0);if((a[o]&1)==0){O=x}else{O=c[y>>2]|0}c[p>>2]=O+M;P=O}else{P=l}D=B+12|0;K=c[D>>2]|0;Q=B+16|0;if((K|0)==(c[Q>>2]|0)){R=(b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)&255}else{R=a[K]|0}if((fs(R,t,P,p,s,A,m,v,r,u)|0)!=0){H=P;I=F;J=G;break}K=c[D>>2]|0;if((K|0)==(c[Q>>2]|0)){Q=c[(c[B>>2]|0)+40>>2]|0;b0[Q&127](B)|0;l=P;w=B;continue}else{c[D>>2]=K+1;l=P;w=B;continue}}w=d[m]|0;if((w&1|0)==0){S=w>>>1}else{S=c[m+4>>2]|0}do{if((S|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}P=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=P}}while(0);c[k>>2]=fD(H,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(C){T=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){T=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){T=B;break}c[h>>2]=0;T=0}}while(0);h=(T|0)==0;L3402:do{if(J){E=2800}else{do{if((c[I+12>>2]|0)==(c[I+16>>2]|0)){if((b0[c[(c[I>>2]|0)+36>>2]&127](I)|0)!=-1){break}c[f>>2]=0;E=2800;break L3402}}while(0);if(!(h^(I|0)==0)){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);do{if((E|0)==2800){if(h){break}U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;U=b|0;c[U>>2]=T;dP(n);dP(m);i=e;return}function fD(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0;g=i;i=i+8|0;h=g|0;if((b|0)==(d|0)){c[e>>2]=4;j=0;i=g;return j|0}if((a[b]|0)==45){c[e>>2]=4;j=0;i=g;return j|0}k=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=aI(b|0,h|0,f|0,c[2958]|0)|0;f=K;b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=k}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;i=g;return j|0}d=0;if((b|0)==34|(f>>>0>d>>>0|f>>>0==d>>>0&l>>>0>-1>>>0)){c[e>>2]=4;j=-1;i=g;return j|0}else{j=l;i=g;return j|0}return 0}function fE(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0;e=i;i=i+72|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+32|0;m=e+40|0;n=e+56|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 0:{t=0;break};case 64:{t=8;break};default:{t=10}}u=e|0;fx(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=a[l]|0;l=w;w=c[h>>2]|0;L3452:while(1){do{if((w|0)==0){B=0}else{if((c[w+12>>2]|0)!=(c[w+16>>2]|0)){B=w;break}if((b0[c[(c[w>>2]|0)+36>>2]&127](w)|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);C=(B|0)==0;D=c[f>>2]|0;do{if((D|0)==0){E=2849}else{if((c[D+12>>2]|0)!=(c[D+16>>2]|0)){if(C){F=D;G=0;break}else{H=l;I=D;J=0;break L3452}}if((b0[c[(c[D>>2]|0)+36>>2]&127](D)|0)==-1){c[f>>2]=0;E=2849;break}else{L=(D|0)==0;if(C^L){F=D;G=L;break}else{H=l;I=D;J=L;break L3452}}}}while(0);if((E|0)==2849){E=0;if(C){H=l;I=0;J=1;break}else{F=0;G=1}}D=d[o]|0;L=(D&1|0)==0;if(((c[p>>2]|0)-l|0)==((L?D>>>1:c[z>>2]|0)|0)){if(L){M=D>>>1;N=D>>>1}else{D=c[z>>2]|0;M=D;N=D}dR(n,M<<1,0);if((a[o]&1)==0){O=10}else{O=(c[g>>2]&-2)-1|0}dR(n,O,0);if((a[o]&1)==0){P=x}else{P=c[y>>2]|0}c[p>>2]=P+N;Q=P}else{Q=l}D=B+12|0;L=c[D>>2]|0;R=B+16|0;if((L|0)==(c[R>>2]|0)){S=(b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)&255}else{S=a[L]|0}if((fs(S,t,Q,p,s,A,m,v,r,u)|0)!=0){H=Q;I=F;J=G;break}L=c[D>>2]|0;if((L|0)==(c[R>>2]|0)){R=c[(c[B>>2]|0)+40>>2]|0;b0[R&127](B)|0;l=Q;w=B;continue}else{c[D>>2]=L+1;l=Q;w=B;continue}}w=d[m]|0;if((w&1|0)==0){T=w>>>1}else{T=c[m+4>>2]|0}do{if((T|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}Q=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=Q}}while(0);s=fF(H,c[p>>2]|0,j,t)|0;c[k>>2]=s;c[k+4>>2]=K;ft(m,v,c[r>>2]|0,j);do{if(C){U=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){U=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){U=B;break}c[h>>2]=0;U=0}}while(0);h=(U|0)==0;L3512:do{if(J){E=2890}else{do{if((c[I+12>>2]|0)==(c[I+16>>2]|0)){if((b0[c[(c[I>>2]|0)+36>>2]&127](I)|0)!=-1){break}c[f>>2]=0;E=2890;break L3512}}while(0);if(!(h^(I|0)==0)){break}V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}}while(0);do{if((E|0)==2890){if(h){break}V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;V=b|0;c[V>>2]=U;dP(n);dP(m);i=e;return}function fF(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0;g=i;i=i+8|0;h=g|0;do{if((b|0)==(d|0)){c[e>>2]=4;j=0;k=0}else{if((a[b]|0)==45){c[e>>2]=4;j=0;k=0;break}l=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);m=aI(b|0,h|0,f|0,c[2958]|0)|0;n=K;o=c[(bw()|0)>>2]|0;if((o|0)==0){c[(bw()|0)>>2]=l}if((c[h>>2]|0)!=(d|0)){c[e>>2]=4;j=0;k=0;break}if((o|0)!=34){j=n;k=m;break}c[e>>2]=4;j=-1;k=-1}}while(0);i=g;return(K=j,k)|0}function fG(b,e,f,g,h,i,j,k,l,m,n,o){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;var p=0,q=0,r=0,s=0,t=0;if(b<<24>>24==i<<24>>24){if((a[e]&1)==0){p=-1;return p|0}a[e]=0;i=c[h>>2]|0;c[h>>2]=i+1;a[i]=46;i=d[k]|0;if((i&1|0)==0){q=i>>>1}else{q=c[k+4>>2]|0}if((q|0)==0){p=0;return p|0}q=c[m>>2]|0;if((q-l|0)>=160){p=0;return p|0}i=c[n>>2]|0;c[m>>2]=q+4;c[q>>2]=i;p=0;return p|0}do{if(b<<24>>24==j<<24>>24){i=d[k]|0;if((i&1|0)==0){r=i>>>1}else{r=c[k+4>>2]|0}if((r|0)==0){break}if((a[e]&1)==0){p=-1;return p|0}i=c[m>>2]|0;if((i-l|0)>=160){p=0;return p|0}q=c[n>>2]|0;c[m>>2]=i+4;c[i>>2]=q;c[n>>2]=0;p=0;return p|0}}while(0);r=o+32|0;j=o;while(1){if((j|0)==(r|0)){s=r;break}if((a[j]|0)==b<<24>>24){s=j;break}else{j=j+1|0}}j=s-o|0;if((j|0)>31){p=-1;return p|0}o=a[9744+j|0]|0;switch(j|0){case 25:case 24:{s=c[h>>2]|0;do{if((s|0)!=(g|0)){if((a[s-1|0]&95|0)==(a[f]&127|0)){break}else{p=-1}return p|0}}while(0);c[h>>2]=s+1;a[s]=o;p=0;return p|0};case 22:case 23:{a[f]=80;s=c[h>>2]|0;c[h>>2]=s+1;a[s]=o;p=0;return p|0};default:{s=a[f]|0;do{if((o&95|0)==(s<<24>>24|0)){a[f]=s|-128;if((a[e]&1)==0){break}a[e]=0;g=d[k]|0;if((g&1|0)==0){t=g>>>1}else{t=c[k+4>>2]|0}if((t|0)==0){break}g=c[m>>2]|0;if((g-l|0)>=160){break}b=c[n>>2]|0;c[m>>2]=g+4;c[g>>2]=b}}while(0);m=c[h>>2]|0;c[h>>2]=m+1;a[m]=o;if((j|0)>21){p=0;return p|0}c[n>>2]=(c[n>>2]|0)+1;p=0;return p|0}}return 0}function fH(b,e,f,h,j,k,l){b=b|0;e=e|0;f=f|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0;e=i;i=i+80|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=h;h=i;i=i+4|0;i=i+7>>3<<3;c[h>>2]=c[m>>2];m=e+32|0;n=e+40|0;o=e+48|0;p=e+64|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;fJ(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=h|0;h=p|0;C=p+4|0;D=a[m]|0;m=a[n]|0;n=z;z=c[j>>2]|0;L3615:while(1){do{if((z|0)==0){E=0}else{if((c[z+12>>2]|0)!=(c[z+16>>2]|0)){E=z;break}if((b0[c[(c[z>>2]|0)+36>>2]&127](z)|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);F=(E|0)==0;G=c[f>>2]|0;do{if((G|0)==0){H=2978}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){if(F){I=G;J=0;break}else{K=n;L=G;M=0;break L3615}}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)==-1){c[f>>2]=0;H=2978;break}else{N=(G|0)==0;if(F^N){I=G;J=N;break}else{K=n;L=G;M=N;break L3615}}}}while(0);if((H|0)==2978){H=0;if(F){K=n;L=0;M=1;break}else{I=0;J=1}}G=d[q]|0;N=(G&1|0)==0;if(((c[r>>2]|0)-n|0)==((N?G>>>1:c[C>>2]|0)|0)){if(N){O=G>>>1;P=G>>>1}else{G=c[C>>2]|0;O=G;P=G}dR(p,O<<1,0);if((a[q]&1)==0){Q=10}else{Q=(c[h>>2]&-2)-1|0}dR(p,Q,0);if((a[q]&1)==0){R=A}else{R=c[B>>2]|0}c[r>>2]=R+P;S=R}else{S=n}G=E+12|0;N=c[G>>2]|0;T=E+16|0;if((N|0)==(c[T>>2]|0)){U=(b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)&255}else{U=a[N]|0}if((fG(U,v,w,S,r,D,m,o,y,t,u,x)|0)!=0){K=S;L=I;M=J;break}N=c[G>>2]|0;if((N|0)==(c[T>>2]|0)){T=c[(c[E>>2]|0)+40>>2]|0;b0[T&127](E)|0;n=S;z=E;continue}else{c[G>>2]=N+1;n=S;z=E;continue}}z=d[o]|0;if((z&1|0)==0){V=z>>>1}else{V=c[o+4>>2]|0}do{if((V|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}S=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=S}}while(0);g[l>>2]=+fI(K,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(F){W=0}else{if((c[E+12>>2]|0)!=(c[E+16>>2]|0)){W=E;break}if((b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)!=-1){W=E;break}c[j>>2]=0;W=0}}while(0);j=(W|0)==0;L3676:do{if(M){H=3020}else{do{if((c[L+12>>2]|0)==(c[L+16>>2]|0)){if((b0[c[(c[L>>2]|0)+36>>2]&127](L)|0)!=-1){break}c[f>>2]=0;H=3020;break L3676}}while(0);if(!(j^(L|0)==0)){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);do{if((H|0)==3020){if(j){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}function fI(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0.0,j=0,k=0.0;f=i;i=i+8|0;g=f|0;if((b|0)==(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}j=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);k=+kW(b,g,c[2958]|0);b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=j}if((c[g>>2]|0)!=(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}if((b|0)==34){c[e>>2]=4}h=k;i=f;return+h}function fJ(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;h=i;i=i+40|0;j=h|0;k=h+16|0;l=h+32|0;ej(l,d);d=l|0;l=c[d>>2]|0;if((c[3924]|0)!=-1){c[k>>2]=15696;c[k+4>>2]=12;c[k+8>>2]=0;dW(15696,k,96)}k=(c[3925]|0)-1|0;m=c[l+8>>2]|0;do{if((c[l+12>>2]|0)-m>>2>>>0>k>>>0){n=c[m+(k<<2)>>2]|0;if((n|0)==0){break}o=n;p=c[(c[n>>2]|0)+32>>2]|0;ca[p&15](o,9744,9776,e)|0;o=c[d>>2]|0;if((c[3828]|0)!=-1){c[j>>2]=15312;c[j+4>>2]=12;c[j+8>>2]=0;dW(15312,j,96)}p=(c[3829]|0)-1|0;n=c[o+8>>2]|0;do{if((c[o+12>>2]|0)-n>>2>>>0>p>>>0){q=c[n+(p<<2)>>2]|0;if((q|0)==0){break}r=q;s=q;a[f]=b0[c[(c[s>>2]|0)+12>>2]&127](r)|0;a[g]=b0[c[(c[s>>2]|0)+16>>2]&127](r)|0;bZ[c[(c[q>>2]|0)+20>>2]&127](b,r);r=c[d>>2]|0;dz(r)|0;i=h;return}}while(0);p=bO(4)|0;kq(p);bl(p|0,8176,134)}}while(0);h=bO(4)|0;kq(h);bl(h|0,8176,134)}function fK(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0;e=i;i=i+80|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=e+32|0;n=e+40|0;o=e+48|0;p=e+64|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;fJ(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=g|0;g=p|0;C=p+4|0;D=a[m]|0;m=a[n]|0;n=z;z=c[j>>2]|0;L3737:while(1){do{if((z|0)==0){E=0}else{if((c[z+12>>2]|0)!=(c[z+16>>2]|0)){E=z;break}if((b0[c[(c[z>>2]|0)+36>>2]&127](z)|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);F=(E|0)==0;G=c[f>>2]|0;do{if((G|0)==0){H=3079}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){if(F){I=G;J=0;break}else{K=n;L=G;M=0;break L3737}}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)==-1){c[f>>2]=0;H=3079;break}else{N=(G|0)==0;if(F^N){I=G;J=N;break}else{K=n;L=G;M=N;break L3737}}}}while(0);if((H|0)==3079){H=0;if(F){K=n;L=0;M=1;break}else{I=0;J=1}}G=d[q]|0;N=(G&1|0)==0;if(((c[r>>2]|0)-n|0)==((N?G>>>1:c[C>>2]|0)|0)){if(N){O=G>>>1;P=G>>>1}else{G=c[C>>2]|0;O=G;P=G}dR(p,O<<1,0);if((a[q]&1)==0){Q=10}else{Q=(c[g>>2]&-2)-1|0}dR(p,Q,0);if((a[q]&1)==0){R=A}else{R=c[B>>2]|0}c[r>>2]=R+P;S=R}else{S=n}G=E+12|0;N=c[G>>2]|0;T=E+16|0;if((N|0)==(c[T>>2]|0)){U=(b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)&255}else{U=a[N]|0}if((fG(U,v,w,S,r,D,m,o,y,t,u,x)|0)!=0){K=S;L=I;M=J;break}N=c[G>>2]|0;if((N|0)==(c[T>>2]|0)){T=c[(c[E>>2]|0)+40>>2]|0;b0[T&127](E)|0;n=S;z=E;continue}else{c[G>>2]=N+1;n=S;z=E;continue}}z=d[o]|0;if((z&1|0)==0){V=z>>>1}else{V=c[o+4>>2]|0}do{if((V|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}S=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=S}}while(0);h[l>>3]=+fL(K,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(F){W=0}else{if((c[E+12>>2]|0)!=(c[E+16>>2]|0)){W=E;break}if((b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)!=-1){W=E;break}c[j>>2]=0;W=0}}while(0);j=(W|0)==0;L3798:do{if(M){H=3121}else{do{if((c[L+12>>2]|0)==(c[L+16>>2]|0)){if((b0[c[(c[L>>2]|0)+36>>2]&127](L)|0)!=-1){break}c[f>>2]=0;H=3121;break L3798}}while(0);if(!(j^(L|0)==0)){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);do{if((H|0)==3121){if(j){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}function fL(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0.0,j=0,k=0.0;f=i;i=i+8|0;g=f|0;if((b|0)==(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}j=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);k=+kW(b,g,c[2958]|0);b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=j}if((c[g>>2]|0)!=(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}if((b|0)!=34){h=k;i=f;return+h}c[e>>2]=4;h=k;i=f;return+h}function fM(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0;e=i;i=i+80|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=e+32|0;n=e+40|0;o=e+48|0;p=e+64|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;fJ(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=g|0;g=p|0;C=p+4|0;D=a[m]|0;m=a[n]|0;n=z;z=c[j>>2]|0;L6:while(1){do{if((z|0)==0){E=0}else{if((c[z+12>>2]|0)!=(c[z+16>>2]|0)){E=z;break}if((b0[c[(c[z>>2]|0)+36>>2]&127](z)|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);F=(E|0)==0;G=c[f>>2]|0;do{if((G|0)==0){H=17}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){if(F){I=G;J=0;break}else{K=n;L=G;M=0;break L6}}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)==-1){c[f>>2]=0;H=17;break}else{N=(G|0)==0;if(F^N){I=G;J=N;break}else{K=n;L=G;M=N;break L6}}}}while(0);if((H|0)==17){H=0;if(F){K=n;L=0;M=1;break}else{I=0;J=1}}G=d[q]|0;N=(G&1|0)==0;if(((c[r>>2]|0)-n|0)==((N?G>>>1:c[C>>2]|0)|0)){if(N){O=G>>>1;P=G>>>1}else{G=c[C>>2]|0;O=G;P=G}dR(p,O<<1,0);if((a[q]&1)==0){Q=10}else{Q=(c[g>>2]&-2)-1|0}dR(p,Q,0);if((a[q]&1)==0){R=A}else{R=c[B>>2]|0}c[r>>2]=R+P;S=R}else{S=n}G=E+12|0;N=c[G>>2]|0;T=E+16|0;if((N|0)==(c[T>>2]|0)){U=(b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)&255}else{U=a[N]|0}if((fG(U,v,w,S,r,D,m,o,y,t,u,x)|0)!=0){K=S;L=I;M=J;break}N=c[G>>2]|0;if((N|0)==(c[T>>2]|0)){T=c[(c[E>>2]|0)+40>>2]|0;b0[T&127](E)|0;n=S;z=E;continue}else{c[G>>2]=N+1;n=S;z=E;continue}}z=d[o]|0;if((z&1|0)==0){V=z>>>1}else{V=c[o+4>>2]|0}do{if((V|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}S=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=S}}while(0);h[l>>3]=+fN(K,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(F){W=0}else{if((c[E+12>>2]|0)!=(c[E+16>>2]|0)){W=E;break}if((b0[c[(c[E>>2]|0)+36>>2]&127](E)|0)!=-1){W=E;break}c[j>>2]=0;W=0}}while(0);j=(W|0)==0;L67:do{if(M){H=59}else{do{if((c[L+12>>2]|0)==(c[L+16>>2]|0)){if((b0[c[(c[L>>2]|0)+36>>2]&127](L)|0)!=-1){break}c[f>>2]=0;H=59;break L67}}while(0);if(!(j^(L|0)==0)){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);do{if((H|0)==59){if(j){break}X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;X=b|0;c[X>>2]=W;dP(p);dP(o);i=e;return}function fN(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0.0,j=0,k=0.0;f=i;i=i+8|0;g=f|0;if((b|0)==(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}j=c[(bw()|0)>>2]|0;c[(bw()|0)>>2]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);k=+kW(b,g,c[2958]|0);b=c[(bw()|0)>>2]|0;if((b|0)==0){c[(bw()|0)>>2]=j}if((c[g>>2]|0)!=(d|0)){c[e>>2]=4;h=0.0;i=f;return+h}if((b|0)!=34){h=k;i=f;return+h}c[e>>2]=4;h=k;i=f;return+h}function fO(a){a=a|0;de(a|0);kS(a);return}function fP(a){a=a|0;de(a|0);return}function fQ(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0;e=i;i=i+64|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e|0;m=e+16|0;n=e+48|0;o=i;i=i+4|0;i=i+7>>3<<3;p=i;i=i+12|0;i=i+7>>3<<3;q=i;i=i+4|0;i=i+7>>3<<3;r=i;i=i+160|0;s=i;i=i+4|0;i=i+7>>3<<3;t=i;i=i+4|0;i=i+7>>3<<3;k$(n|0,0,12);u=p;ej(o,h);h=o|0;o=c[h>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;v=c[o+8>>2]|0;do{if((c[o+12>>2]|0)-v>>2>>>0>l>>>0){w=c[v+(l<<2)>>2]|0;if((w|0)==0){break}x=w;y=m|0;z=c[(c[w>>2]|0)+32>>2]|0;ca[z&15](x,9744,9770,y)|0;x=c[h>>2]|0;dz(x)|0;k$(u|0,0,12);x=p;dR(p,10,0);if((a[u]&1)==0){z=x+1|0;A=z;B=z;C=p+8|0}else{z=p+8|0;A=c[z>>2]|0;B=x+1|0;C=z}c[q>>2]=A;z=r|0;c[s>>2]=z;c[t>>2]=0;x=f|0;w=g|0;D=p|0;E=p+4|0;F=A;G=c[x>>2]|0;L117:while(1){do{if((G|0)==0){H=0}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){H=G;break}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)!=-1){H=G;break}c[x>>2]=0;H=0}}while(0);I=(H|0)==0;J=c[w>>2]|0;do{if((J|0)==0){K=110}else{if((c[J+12>>2]|0)!=(c[J+16>>2]|0)){if(I){break}else{L=F;break L117}}if((b0[c[(c[J>>2]|0)+36>>2]&127](J)|0)==-1){c[w>>2]=0;K=110;break}else{if(I^(J|0)==0){break}else{L=F;break L117}}}}while(0);if((K|0)==110){K=0;if(I){L=F;break}}J=d[u]|0;M=(J&1|0)==0;if(((c[q>>2]|0)-F|0)==((M?J>>>1:c[E>>2]|0)|0)){if(M){N=J>>>1;O=J>>>1}else{J=c[E>>2]|0;N=J;O=J}dR(p,N<<1,0);if((a[u]&1)==0){P=10}else{P=(c[D>>2]&-2)-1|0}dR(p,P,0);if((a[u]&1)==0){Q=B}else{Q=c[C>>2]|0}c[q>>2]=Q+O;R=Q}else{R=F}J=H+12|0;M=c[J>>2]|0;S=H+16|0;if((M|0)==(c[S>>2]|0)){T=(b0[c[(c[H>>2]|0)+36>>2]&127](H)|0)&255}else{T=a[M]|0}if((fs(T,16,R,q,t,0,n,z,s,y)|0)!=0){L=R;break}M=c[J>>2]|0;if((M|0)==(c[S>>2]|0)){S=c[(c[H>>2]|0)+40>>2]|0;b0[S&127](H)|0;F=R;G=H;continue}else{c[J>>2]=M+1;F=R;G=H;continue}}a[L+3|0]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);G=fR(L,c[2958]|0,1320,(F=i,i=i+8|0,c[F>>2]=k,F)|0)|0;i=F;if((G|0)!=1){c[j>>2]=4}G=c[x>>2]|0;do{if((G|0)==0){U=0}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){U=G;break}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)!=-1){U=G;break}c[x>>2]=0;U=0}}while(0);x=(U|0)==0;G=c[w>>2]|0;do{if((G|0)==0){K=155}else{if((c[G+12>>2]|0)!=(c[G+16>>2]|0)){if(!x){break}V=b|0;c[V>>2]=U;dP(p);dP(n);i=e;return}if((b0[c[(c[G>>2]|0)+36>>2]&127](G)|0)==-1){c[w>>2]=0;K=155;break}if(!(x^(G|0)==0)){break}V=b|0;c[V>>2]=U;dP(p);dP(n);i=e;return}}while(0);do{if((K|0)==155){if(x){break}V=b|0;c[V>>2]=U;dP(p);dP(n);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;V=b|0;c[V>>2]=U;dP(p);dP(n);i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function fR(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;f=i;i=i+16|0;g=f|0;h=g;c[h>>2]=e;c[h+4>>2]=0;h=bB(b|0)|0;b=aY(a|0,d|0,g|0)|0;if((h|0)==0){i=f;return b|0}bB(h|0)|0;i=f;return b|0}function fS(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0;l=i;i=i+104|0;m=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[m>>2];m=(g-f|0)/12|0;n=l|0;do{if(m>>>0>100){o=kJ(m)|0;if((o|0)!=0){p=o;q=o;break}kX();p=0;q=0}else{p=n;q=0}}while(0);n=(f|0)==(g|0);if(n){r=m;s=0}else{o=m;m=0;t=p;u=f;while(1){v=d[u]|0;if((v&1|0)==0){w=v>>>1}else{w=c[u+4>>2]|0}if((w|0)==0){a[t]=2;x=m+1|0;y=o-1|0}else{a[t]=1;x=m;y=o}v=u+12|0;if((v|0)==(g|0)){r=y;s=x;break}else{o=y;m=x;t=t+1|0;u=v}}}u=b|0;b=e|0;e=h;t=0;x=s;s=r;while(1){r=c[u>>2]|0;do{if((r|0)==0){z=0}else{m=c[r+12>>2]|0;if((m|0)==(c[r+16>>2]|0)){A=b0[c[(c[r>>2]|0)+36>>2]&127](r)|0}else{A=c[m>>2]|0}if((A|0)==-1){c[u>>2]=0;z=0;break}else{z=c[u>>2]|0;break}}}while(0);r=(z|0)==0;m=c[b>>2]|0;if((m|0)==0){B=z;C=0}else{y=c[m+12>>2]|0;if((y|0)==(c[m+16>>2]|0)){D=b0[c[(c[m>>2]|0)+36>>2]&127](m)|0}else{D=c[y>>2]|0}if((D|0)==-1){c[b>>2]=0;E=0}else{E=m}B=c[u>>2]|0;C=E}F=(C|0)==0;if(!((r^F)&(s|0)!=0)){break}r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){G=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{G=c[r>>2]|0}if(k){H=G}else{H=b_[c[(c[e>>2]|0)+28>>2]&31](h,G)|0}do{if(n){I=x;J=s}else{r=t+1|0;L249:do{if(k){m=s;y=x;o=p;w=0;v=f;while(1){do{if((a[o]|0)==1){K=v;if((a[K]&1)==0){L=v+4|0}else{L=c[v+8>>2]|0}if((H|0)!=(c[L+(t<<2)>>2]|0)){a[o]=0;M=w;N=y;O=m-1|0;break}P=d[K]|0;if((P&1|0)==0){Q=P>>>1}else{Q=c[v+4>>2]|0}if((Q|0)!=(r|0)){M=1;N=y;O=m;break}a[o]=2;M=1;N=y+1|0;O=m-1|0}else{M=w;N=y;O=m}}while(0);P=v+12|0;if((P|0)==(g|0)){R=O;S=N;T=M;break L249}m=O;y=N;o=o+1|0;w=M;v=P}}else{v=s;w=x;o=p;y=0;m=f;while(1){do{if((a[o]|0)==1){P=m;if((a[P]&1)==0){U=m+4|0}else{U=c[m+8>>2]|0}if((H|0)!=(b_[c[(c[e>>2]|0)+28>>2]&31](h,c[U+(t<<2)>>2]|0)|0)){a[o]=0;V=y;W=w;X=v-1|0;break}K=d[P]|0;if((K&1|0)==0){Y=K>>>1}else{Y=c[m+4>>2]|0}if((Y|0)!=(r|0)){V=1;W=w;X=v;break}a[o]=2;V=1;W=w+1|0;X=v-1|0}else{V=y;W=w;X=v}}while(0);K=m+12|0;if((K|0)==(g|0)){R=X;S=W;T=V;break L249}v=X;w=W;o=o+1|0;y=V;m=K}}}while(0);if(!T){I=S;J=R;break}r=c[u>>2]|0;m=r+12|0;y=c[m>>2]|0;if((y|0)==(c[r+16>>2]|0)){o=c[(c[r>>2]|0)+40>>2]|0;b0[o&127](r)|0}else{c[m>>2]=y+4}if((S+R|0)>>>0<2|n){I=S;J=R;break}y=t+1|0;m=S;r=p;o=f;while(1){do{if((a[r]|0)==2){w=d[o]|0;if((w&1|0)==0){Z=w>>>1}else{Z=c[o+4>>2]|0}if((Z|0)==(y|0)){_=m;break}a[r]=0;_=m-1|0}else{_=m}}while(0);w=o+12|0;if((w|0)==(g|0)){I=_;J=R;break}else{m=_;r=r+1|0;o=w}}}}while(0);t=t+1|0;x=I;s=J}do{if((B|0)==0){$=1}else{J=c[B+12>>2]|0;if((J|0)==(c[B+16>>2]|0)){aa=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{aa=c[J>>2]|0}if((aa|0)==-1){c[u>>2]=0;$=1;break}else{$=(c[u>>2]|0)==0;break}}}while(0);do{if(F){ab=264}else{u=c[C+12>>2]|0;if((u|0)==(c[C+16>>2]|0)){ac=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{ac=c[u>>2]|0}if((ac|0)==-1){c[b>>2]=0;ab=264;break}else{if($^(C|0)==0){break}else{ab=266;break}}}}while(0);if((ab|0)==264){if($){ab=266}}if((ab|0)==266){c[j>>2]=c[j>>2]|2}L330:do{if(n){ab=271}else{$=f;C=p;while(1){if((a[C]|0)==2){ad=$;break L330}b=$+12|0;if((b|0)==(g|0)){ab=271;break L330}$=b;C=C+1|0}}}while(0);if((ab|0)==271){c[j>>2]=c[j>>2]|4;ad=g}if((q|0)==0){i=l;return ad|0}kK(q);i=l;return ad|0}function fT(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;k=i;i=i+112|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=k|0;m=k+16|0;n=k+32|0;o=k+40|0;p=k+48|0;q=k+56|0;r=k+64|0;s=k+72|0;t=k+80|0;u=k+104|0;if((c[g+4>>2]&1|0)==0){c[n>>2]=-1;v=c[(c[d>>2]|0)+16>>2]|0;w=e|0;c[p>>2]=c[w>>2];c[q>>2]=c[f>>2];bX[v&127](o,d,p,q,g,h,n);q=c[o>>2]|0;c[w>>2]=q;switch(c[n>>2]|0){case 1:{a[j]=1;break};case 0:{a[j]=0;break};default:{a[j]=1;c[h>>2]=4}}c[b>>2]=q;i=k;return}ej(r,g);q=r|0;r=c[q>>2]|0;if((c[3922]|0)!=-1){c[m>>2]=15688;c[m+4>>2]=12;c[m+8>>2]=0;dW(15688,m,96)}m=(c[3923]|0)-1|0;n=c[r+8>>2]|0;do{if((c[r+12>>2]|0)-n>>2>>>0>m>>>0){w=c[n+(m<<2)>>2]|0;if((w|0)==0){break}o=w;w=c[q>>2]|0;dz(w)|0;ej(s,g);w=s|0;p=c[w>>2]|0;if((c[3826]|0)!=-1){c[l>>2]=15304;c[l+4>>2]=12;c[l+8>>2]=0;dW(15304,l,96)}d=(c[3827]|0)-1|0;v=c[p+8>>2]|0;do{if((c[p+12>>2]|0)-v>>2>>>0>d>>>0){x=c[v+(d<<2)>>2]|0;if((x|0)==0){break}y=x;z=c[w>>2]|0;dz(z)|0;z=t|0;A=x;bZ[c[(c[A>>2]|0)+24>>2]&127](z,y);bZ[c[(c[A>>2]|0)+28>>2]&127](t+12|0,y);c[u>>2]=c[f>>2];a[j]=(fS(e,u,z,t+24|0,o,h,1)|0)==(z|0)|0;c[b>>2]=c[e>>2];ee(t+12|0);ee(t|0);i=k;return}}while(0);o=bO(4)|0;kq(o);bl(o|0,8176,134)}}while(0);k=bO(4)|0;kq(k);bl(k|0,8176,134)}function fU(b,e,f,g,h,i,j,k,l,m){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0;n=c[g>>2]|0;o=(n|0)==(f|0);do{if(o){p=(c[m+96>>2]|0)==(b|0);if(!p){if((c[m+100>>2]|0)!=(b|0)){break}}c[g>>2]=f+1;a[f]=p?43:45;c[h>>2]=0;q=0;return q|0}}while(0);p=d[j]|0;if((p&1|0)==0){r=p>>>1}else{r=c[j+4>>2]|0}if((r|0)!=0&(b|0)==(i|0)){i=c[l>>2]|0;if((i-k|0)>=160){q=0;return q|0}k=c[h>>2]|0;c[l>>2]=i+4;c[i>>2]=k;c[h>>2]=0;q=0;return q|0}k=m+104|0;i=m;while(1){if((i|0)==(k|0)){s=k;break}if((c[i>>2]|0)==(b|0)){s=i;break}else{i=i+4|0}}i=s-m|0;m=i>>2;if((i|0)>92){q=-1;return q|0}L398:do{switch(e|0){case 8:case 10:{if((m|0)<(e|0)){break L398}else{q=-1}return q|0};case 16:{if((i|0)<88){break L398}if(o){q=-1;return q|0}if((n-f|0)>=3){q=-1;return q|0}if((a[n-1|0]|0)!=48){q=-1;return q|0}c[h>>2]=0;s=a[9744+m|0]|0;b=c[g>>2]|0;c[g>>2]=b+1;a[b]=s;q=0;return q|0};default:{}}}while(0);f=a[9744+m|0]|0;c[g>>2]=n+1;a[n]=f;c[h>>2]=(c[h>>2]|0)+1;q=0;return q|0}function fV(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0;e=i;i=i+144|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+104|0;m=e+112|0;n=e+128|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 0:{t=0;break};case 64:{t=8;break};default:{t=10}}u=e|0;fW(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=c[l>>2]|0;l=w;w=c[h>>2]|0;L426:while(1){do{if((w|0)==0){B=0}else{C=c[w+12>>2]|0;if((C|0)==(c[w+16>>2]|0)){D=b0[c[(c[w>>2]|0)+36>>2]&127](w)|0}else{D=c[C>>2]|0}if((D|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);E=(B|0)==0;C=c[f>>2]|0;do{if((C|0)==0){F=363}else{G=c[C+12>>2]|0;if((G|0)==(c[C+16>>2]|0)){H=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{H=c[G>>2]|0}if((H|0)==-1){c[f>>2]=0;F=363;break}else{G=(C|0)==0;if(E^G){I=C;J=G;break}else{K=l;L=C;M=G;break L426}}}}while(0);if((F|0)==363){F=0;if(E){K=l;L=0;M=1;break}else{I=0;J=1}}C=d[o]|0;G=(C&1|0)==0;if(((c[p>>2]|0)-l|0)==((G?C>>>1:c[z>>2]|0)|0)){if(G){N=C>>>1;O=C>>>1}else{C=c[z>>2]|0;N=C;O=C}dR(n,N<<1,0);if((a[o]&1)==0){P=10}else{P=(c[g>>2]&-2)-1|0}dR(n,P,0);if((a[o]&1)==0){Q=x}else{Q=c[y>>2]|0}c[p>>2]=Q+O;R=Q}else{R=l}C=B+12|0;G=c[C>>2]|0;S=B+16|0;if((G|0)==(c[S>>2]|0)){T=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{T=c[G>>2]|0}if((fU(T,t,R,p,s,A,m,v,r,u)|0)!=0){K=R;L=I;M=J;break}G=c[C>>2]|0;if((G|0)==(c[S>>2]|0)){S=c[(c[B>>2]|0)+40>>2]|0;b0[S&127](B)|0;l=R;w=B;continue}else{c[C>>2]=G+4;l=R;w=B;continue}}w=d[m]|0;if((w&1|0)==0){U=w>>>1}else{U=c[m+4>>2]|0}do{if((U|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}R=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=R}}while(0);c[k>>2]=fu(K,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(E){V=0}else{r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){W=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{W=c[r>>2]|0}if((W|0)!=-1){V=B;break}c[h>>2]=0;V=0}}while(0);h=(V|0)==0;do{if(M){F=405}else{B=c[L+12>>2]|0;if((B|0)==(c[L+16>>2]|0)){X=b0[c[(c[L>>2]|0)+36>>2]&127](L)|0}else{X=c[B>>2]|0}if((X|0)==-1){c[f>>2]=0;F=405;break}if(!(h^(L|0)==0)){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);do{if((F|0)==405){if(h){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}function fW(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0;f=i;i=i+40|0;g=f|0;h=f+16|0;j=f+32|0;ej(j,b);b=j|0;j=c[b>>2]|0;if((c[3922]|0)!=-1){c[h>>2]=15688;c[h+4>>2]=12;c[h+8>>2]=0;dW(15688,h,96)}h=(c[3923]|0)-1|0;k=c[j+8>>2]|0;do{if((c[j+12>>2]|0)-k>>2>>>0>h>>>0){l=c[k+(h<<2)>>2]|0;if((l|0)==0){break}m=l;n=c[(c[l>>2]|0)+48>>2]|0;ca[n&15](m,9744,9770,d)|0;m=c[b>>2]|0;if((c[3826]|0)!=-1){c[g>>2]=15304;c[g+4>>2]=12;c[g+8>>2]=0;dW(15304,g,96)}n=(c[3827]|0)-1|0;l=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-l>>2>>>0>n>>>0){o=c[l+(n<<2)>>2]|0;if((o|0)==0){break}p=o;c[e>>2]=b0[c[(c[o>>2]|0)+16>>2]&127](p)|0;bZ[c[(c[o>>2]|0)+20>>2]&127](a,p);p=c[b>>2]|0;dz(p)|0;i=f;return}}while(0);n=bO(4)|0;kq(n);bl(n|0,8176,134)}}while(0);f=bO(4)|0;kq(f);bl(f|0,8176,134)}function fX(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0;e=i;i=i+144|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+104|0;m=e+112|0;n=e+128|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 0:{t=0;break};case 64:{t=8;break};default:{t=10}}u=e|0;fW(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=c[l>>2]|0;l=w;w=c[h>>2]|0;L535:while(1){do{if((w|0)==0){B=0}else{C=c[w+12>>2]|0;if((C|0)==(c[w+16>>2]|0)){D=b0[c[(c[w>>2]|0)+36>>2]&127](w)|0}else{D=c[C>>2]|0}if((D|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);E=(B|0)==0;C=c[f>>2]|0;do{if((C|0)==0){F=450}else{G=c[C+12>>2]|0;if((G|0)==(c[C+16>>2]|0)){H=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{H=c[G>>2]|0}if((H|0)==-1){c[f>>2]=0;F=450;break}else{G=(C|0)==0;if(E^G){I=C;J=G;break}else{L=l;M=C;N=G;break L535}}}}while(0);if((F|0)==450){F=0;if(E){L=l;M=0;N=1;break}else{I=0;J=1}}C=d[o]|0;G=(C&1|0)==0;if(((c[p>>2]|0)-l|0)==((G?C>>>1:c[z>>2]|0)|0)){if(G){O=C>>>1;P=C>>>1}else{C=c[z>>2]|0;O=C;P=C}dR(n,O<<1,0);if((a[o]&1)==0){Q=10}else{Q=(c[g>>2]&-2)-1|0}dR(n,Q,0);if((a[o]&1)==0){R=x}else{R=c[y>>2]|0}c[p>>2]=R+P;S=R}else{S=l}C=B+12|0;G=c[C>>2]|0;T=B+16|0;if((G|0)==(c[T>>2]|0)){U=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{U=c[G>>2]|0}if((fU(U,t,S,p,s,A,m,v,r,u)|0)!=0){L=S;M=I;N=J;break}G=c[C>>2]|0;if((G|0)==(c[T>>2]|0)){T=c[(c[B>>2]|0)+40>>2]|0;b0[T&127](B)|0;l=S;w=B;continue}else{c[C>>2]=G+4;l=S;w=B;continue}}w=d[m]|0;if((w&1|0)==0){V=w>>>1}else{V=c[m+4>>2]|0}do{if((V|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}S=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=S}}while(0);s=fw(L,c[p>>2]|0,j,t)|0;c[k>>2]=s;c[k+4>>2]=K;ft(m,v,c[r>>2]|0,j);do{if(E){W=0}else{r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){X=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{X=c[r>>2]|0}if((X|0)!=-1){W=B;break}c[h>>2]=0;W=0}}while(0);h=(W|0)==0;do{if(N){F=492}else{B=c[M+12>>2]|0;if((B|0)==(c[M+16>>2]|0)){Y=b0[c[(c[M>>2]|0)+36>>2]&127](M)|0}else{Y=c[B>>2]|0}if((Y|0)==-1){c[f>>2]=0;F=492;break}if(!(h^(M|0)==0)){break}Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}}while(0);do{if((F|0)==492){if(h){break}Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}function fY(e,f,g,h,j,k,l){e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0;f=i;i=i+144|0;m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=h;h=i;i=i+4|0;i=i+7>>3<<3;c[h>>2]=c[m>>2];m=f+104|0;n=f+112|0;o=f+128|0;p=o;q=i;i=i+4|0;i=i+7>>3<<3;r=i;i=i+160|0;s=i;i=i+4|0;i=i+7>>3<<3;t=i;i=i+4|0;i=i+7>>3<<3;switch(c[j+4>>2]&74|0){case 8:{u=16;break};case 64:{u=8;break};case 0:{u=0;break};default:{u=10}}v=f|0;fW(n,j,v,m);k$(p|0,0,12);j=o;dR(o,10,0);if((a[p]&1)==0){w=j+1|0;x=w;y=w;z=o+8|0}else{w=o+8|0;x=c[w>>2]|0;y=j+1|0;z=w}c[q>>2]=x;w=r|0;c[s>>2]=w;c[t>>2]=0;j=g|0;g=h|0;h=o|0;A=o+4|0;B=c[m>>2]|0;m=x;x=c[j>>2]|0;L624:while(1){do{if((x|0)==0){C=0}else{D=c[x+12>>2]|0;if((D|0)==(c[x+16>>2]|0)){E=b0[c[(c[x>>2]|0)+36>>2]&127](x)|0}else{E=c[D>>2]|0}if((E|0)!=-1){C=x;break}c[j>>2]=0;C=0}}while(0);F=(C|0)==0;D=c[g>>2]|0;do{if((D|0)==0){G=520}else{H=c[D+12>>2]|0;if((H|0)==(c[D+16>>2]|0)){I=b0[c[(c[D>>2]|0)+36>>2]&127](D)|0}else{I=c[H>>2]|0}if((I|0)==-1){c[g>>2]=0;G=520;break}else{H=(D|0)==0;if(F^H){J=D;K=H;break}else{L=m;M=D;N=H;break L624}}}}while(0);if((G|0)==520){G=0;if(F){L=m;M=0;N=1;break}else{J=0;K=1}}D=d[p]|0;H=(D&1|0)==0;if(((c[q>>2]|0)-m|0)==((H?D>>>1:c[A>>2]|0)|0)){if(H){O=D>>>1;P=D>>>1}else{D=c[A>>2]|0;O=D;P=D}dR(o,O<<1,0);if((a[p]&1)==0){Q=10}else{Q=(c[h>>2]&-2)-1|0}dR(o,Q,0);if((a[p]&1)==0){R=y}else{R=c[z>>2]|0}c[q>>2]=R+P;S=R}else{S=m}D=C+12|0;H=c[D>>2]|0;T=C+16|0;if((H|0)==(c[T>>2]|0)){U=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{U=c[H>>2]|0}if((fU(U,u,S,q,t,B,n,w,s,v)|0)!=0){L=S;M=J;N=K;break}H=c[D>>2]|0;if((H|0)==(c[T>>2]|0)){T=c[(c[C>>2]|0)+40>>2]|0;b0[T&127](C)|0;m=S;x=C;continue}else{c[D>>2]=H+4;m=S;x=C;continue}}x=d[n]|0;if((x&1|0)==0){V=x>>>1}else{V=c[n+4>>2]|0}do{if((V|0)!=0){x=c[s>>2]|0;if((x-r|0)>=160){break}S=c[t>>2]|0;c[s>>2]=x+4;c[x>>2]=S}}while(0);b[l>>1]=fz(L,c[q>>2]|0,k,u)|0;ft(n,w,c[s>>2]|0,k);do{if(F){W=0}else{s=c[C+12>>2]|0;if((s|0)==(c[C+16>>2]|0)){X=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{X=c[s>>2]|0}if((X|0)!=-1){W=C;break}c[j>>2]=0;W=0}}while(0);j=(W|0)==0;do{if(N){G=562}else{C=c[M+12>>2]|0;if((C|0)==(c[M+16>>2]|0)){Y=b0[c[(c[M>>2]|0)+36>>2]&127](M)|0}else{Y=c[C>>2]|0}if((Y|0)==-1){c[g>>2]=0;G=562;break}if(!(j^(M|0)==0)){break}Z=e|0;c[Z>>2]=W;dP(o);dP(n);i=f;return}}while(0);do{if((G|0)==562){if(j){break}Z=e|0;c[Z>>2]=W;dP(o);dP(n);i=f;return}}while(0);c[k>>2]=c[k>>2]|2;Z=e|0;c[Z>>2]=W;dP(o);dP(n);i=f;return}function fZ(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0;e=i;i=i+144|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+104|0;m=e+112|0;n=e+128|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 0:{t=0;break};case 64:{t=8;break};default:{t=10}}u=e|0;fW(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=c[l>>2]|0;l=w;w=c[h>>2]|0;L713:while(1){do{if((w|0)==0){B=0}else{C=c[w+12>>2]|0;if((C|0)==(c[w+16>>2]|0)){D=b0[c[(c[w>>2]|0)+36>>2]&127](w)|0}else{D=c[C>>2]|0}if((D|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);E=(B|0)==0;C=c[f>>2]|0;do{if((C|0)==0){F=590}else{G=c[C+12>>2]|0;if((G|0)==(c[C+16>>2]|0)){H=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{H=c[G>>2]|0}if((H|0)==-1){c[f>>2]=0;F=590;break}else{G=(C|0)==0;if(E^G){I=C;J=G;break}else{K=l;L=C;M=G;break L713}}}}while(0);if((F|0)==590){F=0;if(E){K=l;L=0;M=1;break}else{I=0;J=1}}C=d[o]|0;G=(C&1|0)==0;if(((c[p>>2]|0)-l|0)==((G?C>>>1:c[z>>2]|0)|0)){if(G){N=C>>>1;O=C>>>1}else{C=c[z>>2]|0;N=C;O=C}dR(n,N<<1,0);if((a[o]&1)==0){P=10}else{P=(c[g>>2]&-2)-1|0}dR(n,P,0);if((a[o]&1)==0){Q=x}else{Q=c[y>>2]|0}c[p>>2]=Q+O;R=Q}else{R=l}C=B+12|0;G=c[C>>2]|0;S=B+16|0;if((G|0)==(c[S>>2]|0)){T=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{T=c[G>>2]|0}if((fU(T,t,R,p,s,A,m,v,r,u)|0)!=0){K=R;L=I;M=J;break}G=c[C>>2]|0;if((G|0)==(c[S>>2]|0)){S=c[(c[B>>2]|0)+40>>2]|0;b0[S&127](B)|0;l=R;w=B;continue}else{c[C>>2]=G+4;l=R;w=B;continue}}w=d[m]|0;if((w&1|0)==0){U=w>>>1}else{U=c[m+4>>2]|0}do{if((U|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}R=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=R}}while(0);c[k>>2]=fB(K,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(E){V=0}else{r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){W=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{W=c[r>>2]|0}if((W|0)!=-1){V=B;break}c[h>>2]=0;V=0}}while(0);h=(V|0)==0;do{if(M){F=632}else{B=c[L+12>>2]|0;if((B|0)==(c[L+16>>2]|0)){X=b0[c[(c[L>>2]|0)+36>>2]&127](L)|0}else{X=c[B>>2]|0}if((X|0)==-1){c[f>>2]=0;F=632;break}if(!(h^(L|0)==0)){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);do{if((F|0)==632){if(h){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}function f_(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0;e=i;i=i+144|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+104|0;m=e+112|0;n=e+128|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 64:{t=8;break};case 0:{t=0;break};default:{t=10}}u=e|0;fW(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=c[l>>2]|0;l=w;w=c[h>>2]|0;L802:while(1){do{if((w|0)==0){B=0}else{C=c[w+12>>2]|0;if((C|0)==(c[w+16>>2]|0)){D=b0[c[(c[w>>2]|0)+36>>2]&127](w)|0}else{D=c[C>>2]|0}if((D|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);E=(B|0)==0;C=c[f>>2]|0;do{if((C|0)==0){F=660}else{G=c[C+12>>2]|0;if((G|0)==(c[C+16>>2]|0)){H=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{H=c[G>>2]|0}if((H|0)==-1){c[f>>2]=0;F=660;break}else{G=(C|0)==0;if(E^G){I=C;J=G;break}else{K=l;L=C;M=G;break L802}}}}while(0);if((F|0)==660){F=0;if(E){K=l;L=0;M=1;break}else{I=0;J=1}}C=d[o]|0;G=(C&1|0)==0;if(((c[p>>2]|0)-l|0)==((G?C>>>1:c[z>>2]|0)|0)){if(G){N=C>>>1;O=C>>>1}else{C=c[z>>2]|0;N=C;O=C}dR(n,N<<1,0);if((a[o]&1)==0){P=10}else{P=(c[g>>2]&-2)-1|0}dR(n,P,0);if((a[o]&1)==0){Q=x}else{Q=c[y>>2]|0}c[p>>2]=Q+O;R=Q}else{R=l}C=B+12|0;G=c[C>>2]|0;S=B+16|0;if((G|0)==(c[S>>2]|0)){T=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{T=c[G>>2]|0}if((fU(T,t,R,p,s,A,m,v,r,u)|0)!=0){K=R;L=I;M=J;break}G=c[C>>2]|0;if((G|0)==(c[S>>2]|0)){S=c[(c[B>>2]|0)+40>>2]|0;b0[S&127](B)|0;l=R;w=B;continue}else{c[C>>2]=G+4;l=R;w=B;continue}}w=d[m]|0;if((w&1|0)==0){U=w>>>1}else{U=c[m+4>>2]|0}do{if((U|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}R=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=R}}while(0);c[k>>2]=fD(K,c[p>>2]|0,j,t)|0;ft(m,v,c[r>>2]|0,j);do{if(E){V=0}else{r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){W=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{W=c[r>>2]|0}if((W|0)!=-1){V=B;break}c[h>>2]=0;V=0}}while(0);h=(V|0)==0;do{if(M){F=702}else{B=c[L+12>>2]|0;if((B|0)==(c[L+16>>2]|0)){X=b0[c[(c[L>>2]|0)+36>>2]&127](L)|0}else{X=c[B>>2]|0}if((X|0)==-1){c[f>>2]=0;F=702;break}if(!(h^(L|0)==0)){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);do{if((F|0)==702){if(h){break}Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Y=b|0;c[Y>>2]=V;dP(n);dP(m);i=e;return}function f$(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0;e=i;i=i+144|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e+104|0;m=e+112|0;n=e+128|0;o=n;p=i;i=i+4|0;i=i+7>>3<<3;q=i;i=i+160|0;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+4|0;i=i+7>>3<<3;switch(c[h+4>>2]&74|0){case 8:{t=16;break};case 0:{t=0;break};case 64:{t=8;break};default:{t=10}}u=e|0;fW(m,h,u,l);k$(o|0,0,12);h=n;dR(n,10,0);if((a[o]&1)==0){v=h+1|0;w=v;x=v;y=n+8|0}else{v=n+8|0;w=c[v>>2]|0;x=h+1|0;y=v}c[p>>2]=w;v=q|0;c[r>>2]=v;c[s>>2]=0;h=f|0;f=g|0;g=n|0;z=n+4|0;A=c[l>>2]|0;l=w;w=c[h>>2]|0;L891:while(1){do{if((w|0)==0){B=0}else{C=c[w+12>>2]|0;if((C|0)==(c[w+16>>2]|0)){D=b0[c[(c[w>>2]|0)+36>>2]&127](w)|0}else{D=c[C>>2]|0}if((D|0)!=-1){B=w;break}c[h>>2]=0;B=0}}while(0);E=(B|0)==0;C=c[f>>2]|0;do{if((C|0)==0){F=730}else{G=c[C+12>>2]|0;if((G|0)==(c[C+16>>2]|0)){H=b0[c[(c[C>>2]|0)+36>>2]&127](C)|0}else{H=c[G>>2]|0}if((H|0)==-1){c[f>>2]=0;F=730;break}else{G=(C|0)==0;if(E^G){I=C;J=G;break}else{L=l;M=C;N=G;break L891}}}}while(0);if((F|0)==730){F=0;if(E){L=l;M=0;N=1;break}else{I=0;J=1}}C=d[o]|0;G=(C&1|0)==0;if(((c[p>>2]|0)-l|0)==((G?C>>>1:c[z>>2]|0)|0)){if(G){O=C>>>1;P=C>>>1}else{C=c[z>>2]|0;O=C;P=C}dR(n,O<<1,0);if((a[o]&1)==0){Q=10}else{Q=(c[g>>2]&-2)-1|0}dR(n,Q,0);if((a[o]&1)==0){R=x}else{R=c[y>>2]|0}c[p>>2]=R+P;S=R}else{S=l}C=B+12|0;G=c[C>>2]|0;T=B+16|0;if((G|0)==(c[T>>2]|0)){U=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{U=c[G>>2]|0}if((fU(U,t,S,p,s,A,m,v,r,u)|0)!=0){L=S;M=I;N=J;break}G=c[C>>2]|0;if((G|0)==(c[T>>2]|0)){T=c[(c[B>>2]|0)+40>>2]|0;b0[T&127](B)|0;l=S;w=B;continue}else{c[C>>2]=G+4;l=S;w=B;continue}}w=d[m]|0;if((w&1|0)==0){V=w>>>1}else{V=c[m+4>>2]|0}do{if((V|0)!=0){w=c[r>>2]|0;if((w-q|0)>=160){break}S=c[s>>2]|0;c[r>>2]=w+4;c[w>>2]=S}}while(0);s=fF(L,c[p>>2]|0,j,t)|0;c[k>>2]=s;c[k+4>>2]=K;ft(m,v,c[r>>2]|0,j);do{if(E){W=0}else{r=c[B+12>>2]|0;if((r|0)==(c[B+16>>2]|0)){X=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{X=c[r>>2]|0}if((X|0)!=-1){W=B;break}c[h>>2]=0;W=0}}while(0);h=(W|0)==0;do{if(N){F=772}else{B=c[M+12>>2]|0;if((B|0)==(c[M+16>>2]|0)){Y=b0[c[(c[M>>2]|0)+36>>2]&127](M)|0}else{Y=c[B>>2]|0}if((Y|0)==-1){c[f>>2]=0;F=772;break}if(!(h^(M|0)==0)){break}Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}}while(0);do{if((F|0)==772){if(h){break}Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Z=b|0;c[Z>>2]=W;dP(n);dP(m);i=e;return}function f0(b,e,f,h,j,k,l){b=b|0;e=e|0;f=f|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0;e=i;i=i+176|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=h;h=i;i=i+4|0;i=i+7>>3<<3;c[h>>2]=c[m>>2];m=e+128|0;n=e+136|0;o=e+144|0;p=e+160|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;f1(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=h|0;h=p|0;C=p+4|0;D=c[m>>2]|0;m=c[n>>2]|0;n=z;z=c[j>>2]|0;L975:while(1){do{if((z|0)==0){E=0}else{F=c[z+12>>2]|0;if((F|0)==(c[z+16>>2]|0)){G=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{G=c[F>>2]|0}if((G|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);H=(E|0)==0;F=c[f>>2]|0;do{if((F|0)==0){I=796}else{J=c[F+12>>2]|0;if((J|0)==(c[F+16>>2]|0)){K=b0[c[(c[F>>2]|0)+36>>2]&127](F)|0}else{K=c[J>>2]|0}if((K|0)==-1){c[f>>2]=0;I=796;break}else{J=(F|0)==0;if(H^J){L=F;M=J;break}else{N=n;O=F;P=J;break L975}}}}while(0);if((I|0)==796){I=0;if(H){N=n;O=0;P=1;break}else{L=0;M=1}}F=d[q]|0;J=(F&1|0)==0;if(((c[r>>2]|0)-n|0)==((J?F>>>1:c[C>>2]|0)|0)){if(J){Q=F>>>1;R=F>>>1}else{F=c[C>>2]|0;Q=F;R=F}dR(p,Q<<1,0);if((a[q]&1)==0){S=10}else{S=(c[h>>2]&-2)-1|0}dR(p,S,0);if((a[q]&1)==0){T=A}else{T=c[B>>2]|0}c[r>>2]=T+R;U=T}else{U=n}F=E+12|0;J=c[F>>2]|0;V=E+16|0;if((J|0)==(c[V>>2]|0)){W=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{W=c[J>>2]|0}if((f2(W,v,w,U,r,D,m,o,y,t,u,x)|0)!=0){N=U;O=L;P=M;break}J=c[F>>2]|0;if((J|0)==(c[V>>2]|0)){V=c[(c[E>>2]|0)+40>>2]|0;b0[V&127](E)|0;n=U;z=E;continue}else{c[F>>2]=J+4;n=U;z=E;continue}}z=d[o]|0;if((z&1|0)==0){X=z>>>1}else{X=c[o+4>>2]|0}do{if((X|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}U=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=U}}while(0);g[l>>2]=+fI(N,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(H){Y=0}else{t=c[E+12>>2]|0;if((t|0)==(c[E+16>>2]|0)){Z=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{Z=c[t>>2]|0}if((Z|0)!=-1){Y=E;break}c[j>>2]=0;Y=0}}while(0);j=(Y|0)==0;do{if(P){I=839}else{E=c[O+12>>2]|0;if((E|0)==(c[O+16>>2]|0)){_=b0[c[(c[O>>2]|0)+36>>2]&127](O)|0}else{_=c[E>>2]|0}if((_|0)==-1){c[f>>2]=0;I=839;break}if(!(j^(O|0)==0)){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);do{if((I|0)==839){if(j){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}function f1(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;g=i;i=i+40|0;h=g|0;j=g+16|0;k=g+32|0;ej(k,b);b=k|0;k=c[b>>2]|0;if((c[3922]|0)!=-1){c[j>>2]=15688;c[j+4>>2]=12;c[j+8>>2]=0;dW(15688,j,96)}j=(c[3923]|0)-1|0;l=c[k+8>>2]|0;do{if((c[k+12>>2]|0)-l>>2>>>0>j>>>0){m=c[l+(j<<2)>>2]|0;if((m|0)==0){break}n=m;o=c[(c[m>>2]|0)+48>>2]|0;ca[o&15](n,9744,9776,d)|0;n=c[b>>2]|0;if((c[3826]|0)!=-1){c[h>>2]=15304;c[h+4>>2]=12;c[h+8>>2]=0;dW(15304,h,96)}o=(c[3827]|0)-1|0;m=c[n+8>>2]|0;do{if((c[n+12>>2]|0)-m>>2>>>0>o>>>0){p=c[m+(o<<2)>>2]|0;if((p|0)==0){break}q=p;r=p;c[e>>2]=b0[c[(c[r>>2]|0)+12>>2]&127](q)|0;c[f>>2]=b0[c[(c[r>>2]|0)+16>>2]&127](q)|0;bZ[c[(c[p>>2]|0)+20>>2]&127](a,q);q=c[b>>2]|0;dz(q)|0;i=g;return}}while(0);o=bO(4)|0;kq(o);bl(o|0,8176,134)}}while(0);g=bO(4)|0;kq(g);bl(g|0,8176,134)}function f2(b,e,f,g,h,i,j,k,l,m,n,o){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;var p=0,q=0,r=0,s=0,t=0;if((b|0)==(i|0)){if((a[e]&1)==0){p=-1;return p|0}a[e]=0;i=c[h>>2]|0;c[h>>2]=i+1;a[i]=46;i=d[k]|0;if((i&1|0)==0){q=i>>>1}else{q=c[k+4>>2]|0}if((q|0)==0){p=0;return p|0}q=c[m>>2]|0;if((q-l|0)>=160){p=0;return p|0}i=c[n>>2]|0;c[m>>2]=q+4;c[q>>2]=i;p=0;return p|0}do{if((b|0)==(j|0)){i=d[k]|0;if((i&1|0)==0){r=i>>>1}else{r=c[k+4>>2]|0}if((r|0)==0){break}if((a[e]&1)==0){p=-1;return p|0}i=c[m>>2]|0;if((i-l|0)>=160){p=0;return p|0}q=c[n>>2]|0;c[m>>2]=i+4;c[i>>2]=q;c[n>>2]=0;p=0;return p|0}}while(0);r=o+128|0;j=o;while(1){if((j|0)==(r|0)){s=r;break}if((c[j>>2]|0)==(b|0)){s=j;break}else{j=j+4|0}}j=s-o|0;o=j>>2;if((j|0)>124){p=-1;return p|0}s=a[9744+o|0]|0;L1115:do{switch(o|0){case 25:case 24:{b=c[h>>2]|0;do{if((b|0)!=(g|0)){if((a[b-1|0]&95|0)==(a[f]&127|0)){break}else{p=-1}return p|0}}while(0);c[h>>2]=b+1;a[b]=s;p=0;return p|0};case 22:case 23:{a[f]=80;break};default:{r=a[f]|0;if((s&95|0)!=(r<<24>>24|0)){break L1115}a[f]=r|-128;if((a[e]&1)==0){break L1115}a[e]=0;r=d[k]|0;if((r&1|0)==0){t=r>>>1}else{t=c[k+4>>2]|0}if((t|0)==0){break L1115}r=c[m>>2]|0;if((r-l|0)>=160){break L1115}q=c[n>>2]|0;c[m>>2]=r+4;c[r>>2]=q}}}while(0);m=c[h>>2]|0;c[h>>2]=m+1;a[m]=s;if((j|0)>84){p=0;return p|0}c[n>>2]=(c[n>>2]|0)+1;p=0;return p|0}function f3(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0;e=i;i=i+176|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=e+128|0;n=e+136|0;o=e+144|0;p=e+160|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;f1(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=g|0;g=p|0;C=p+4|0;D=c[m>>2]|0;m=c[n>>2]|0;n=z;z=c[j>>2]|0;L1143:while(1){do{if((z|0)==0){E=0}else{F=c[z+12>>2]|0;if((F|0)==(c[z+16>>2]|0)){G=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{G=c[F>>2]|0}if((G|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);H=(E|0)==0;F=c[f>>2]|0;do{if((F|0)==0){I=929}else{J=c[F+12>>2]|0;if((J|0)==(c[F+16>>2]|0)){K=b0[c[(c[F>>2]|0)+36>>2]&127](F)|0}else{K=c[J>>2]|0}if((K|0)==-1){c[f>>2]=0;I=929;break}else{J=(F|0)==0;if(H^J){L=F;M=J;break}else{N=n;O=F;P=J;break L1143}}}}while(0);if((I|0)==929){I=0;if(H){N=n;O=0;P=1;break}else{L=0;M=1}}F=d[q]|0;J=(F&1|0)==0;if(((c[r>>2]|0)-n|0)==((J?F>>>1:c[C>>2]|0)|0)){if(J){Q=F>>>1;R=F>>>1}else{F=c[C>>2]|0;Q=F;R=F}dR(p,Q<<1,0);if((a[q]&1)==0){S=10}else{S=(c[g>>2]&-2)-1|0}dR(p,S,0);if((a[q]&1)==0){T=A}else{T=c[B>>2]|0}c[r>>2]=T+R;U=T}else{U=n}F=E+12|0;J=c[F>>2]|0;V=E+16|0;if((J|0)==(c[V>>2]|0)){W=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{W=c[J>>2]|0}if((f2(W,v,w,U,r,D,m,o,y,t,u,x)|0)!=0){N=U;O=L;P=M;break}J=c[F>>2]|0;if((J|0)==(c[V>>2]|0)){V=c[(c[E>>2]|0)+40>>2]|0;b0[V&127](E)|0;n=U;z=E;continue}else{c[F>>2]=J+4;n=U;z=E;continue}}z=d[o]|0;if((z&1|0)==0){X=z>>>1}else{X=c[o+4>>2]|0}do{if((X|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}U=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=U}}while(0);h[l>>3]=+fL(N,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(H){Y=0}else{t=c[E+12>>2]|0;if((t|0)==(c[E+16>>2]|0)){Z=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{Z=c[t>>2]|0}if((Z|0)!=-1){Y=E;break}c[j>>2]=0;Y=0}}while(0);j=(Y|0)==0;do{if(P){I=972}else{E=c[O+12>>2]|0;if((E|0)==(c[O+16>>2]|0)){_=b0[c[(c[O>>2]|0)+36>>2]&127](O)|0}else{_=c[E>>2]|0}if((_|0)==-1){c[f>>2]=0;I=972;break}if(!(j^(O|0)==0)){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);do{if((I|0)==972){if(j){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}function f4(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0;e=i;i=i+176|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[m>>2];m=e+128|0;n=e+136|0;o=e+144|0;p=e+160|0;q=p;r=i;i=i+4|0;i=i+7>>3<<3;s=i;i=i+160|0;t=i;i=i+4|0;i=i+7>>3<<3;u=i;i=i+4|0;i=i+7>>3<<3;v=i;i=i+1|0;i=i+7>>3<<3;w=i;i=i+1|0;i=i+7>>3<<3;x=e|0;f1(o,j,x,m,n);k$(q|0,0,12);j=p;dR(p,10,0);if((a[q]&1)==0){y=j+1|0;z=y;A=y;B=p+8|0}else{y=p+8|0;z=c[y>>2]|0;A=j+1|0;B=y}c[r>>2]=z;y=s|0;c[t>>2]=y;c[u>>2]=0;a[v]=1;a[w]=69;j=f|0;f=g|0;g=p|0;C=p+4|0;D=c[m>>2]|0;m=c[n>>2]|0;n=z;z=c[j>>2]|0;L1228:while(1){do{if((z|0)==0){E=0}else{F=c[z+12>>2]|0;if((F|0)==(c[z+16>>2]|0)){G=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{G=c[F>>2]|0}if((G|0)!=-1){E=z;break}c[j>>2]=0;E=0}}while(0);H=(E|0)==0;F=c[f>>2]|0;do{if((F|0)==0){I=996}else{J=c[F+12>>2]|0;if((J|0)==(c[F+16>>2]|0)){K=b0[c[(c[F>>2]|0)+36>>2]&127](F)|0}else{K=c[J>>2]|0}if((K|0)==-1){c[f>>2]=0;I=996;break}else{J=(F|0)==0;if(H^J){L=F;M=J;break}else{N=n;O=F;P=J;break L1228}}}}while(0);if((I|0)==996){I=0;if(H){N=n;O=0;P=1;break}else{L=0;M=1}}F=d[q]|0;J=(F&1|0)==0;if(((c[r>>2]|0)-n|0)==((J?F>>>1:c[C>>2]|0)|0)){if(J){Q=F>>>1;R=F>>>1}else{F=c[C>>2]|0;Q=F;R=F}dR(p,Q<<1,0);if((a[q]&1)==0){S=10}else{S=(c[g>>2]&-2)-1|0}dR(p,S,0);if((a[q]&1)==0){T=A}else{T=c[B>>2]|0}c[r>>2]=T+R;U=T}else{U=n}F=E+12|0;J=c[F>>2]|0;V=E+16|0;if((J|0)==(c[V>>2]|0)){W=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{W=c[J>>2]|0}if((f2(W,v,w,U,r,D,m,o,y,t,u,x)|0)!=0){N=U;O=L;P=M;break}J=c[F>>2]|0;if((J|0)==(c[V>>2]|0)){V=c[(c[E>>2]|0)+40>>2]|0;b0[V&127](E)|0;n=U;z=E;continue}else{c[F>>2]=J+4;n=U;z=E;continue}}z=d[o]|0;if((z&1|0)==0){X=z>>>1}else{X=c[o+4>>2]|0}do{if((X|0)!=0){if((a[v]&1)==0){break}z=c[t>>2]|0;if((z-s|0)>=160){break}U=c[u>>2]|0;c[t>>2]=z+4;c[z>>2]=U}}while(0);h[l>>3]=+fN(N,c[r>>2]|0,k);ft(o,y,c[t>>2]|0,k);do{if(H){Y=0}else{t=c[E+12>>2]|0;if((t|0)==(c[E+16>>2]|0)){Z=b0[c[(c[E>>2]|0)+36>>2]&127](E)|0}else{Z=c[t>>2]|0}if((Z|0)!=-1){Y=E;break}c[j>>2]=0;Y=0}}while(0);j=(Y|0)==0;do{if(P){I=1039}else{E=c[O+12>>2]|0;if((E|0)==(c[O+16>>2]|0)){_=b0[c[(c[O>>2]|0)+36>>2]&127](O)|0}else{_=c[E>>2]|0}if((_|0)==-1){c[f>>2]=0;I=1039;break}if(!(j^(O|0)==0)){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);do{if((I|0)==1039){if(j){break}$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}}while(0);c[k>>2]=c[k>>2]|2;$=b|0;c[$>>2]=Y;dP(p);dP(o);i=e;return}function f5(a){a=a|0;de(a|0);kS(a);return}function f6(a){a=a|0;de(a|0);return}function f7(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0;e=i;i=i+136|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[l>>2];l=e|0;m=e+16|0;n=e+120|0;o=i;i=i+4|0;i=i+7>>3<<3;p=i;i=i+12|0;i=i+7>>3<<3;q=i;i=i+4|0;i=i+7>>3<<3;r=i;i=i+160|0;s=i;i=i+4|0;i=i+7>>3<<3;t=i;i=i+4|0;i=i+7>>3<<3;k$(n|0,0,12);u=p;ej(o,h);h=o|0;o=c[h>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;v=c[o+8>>2]|0;do{if((c[o+12>>2]|0)-v>>2>>>0>l>>>0){w=c[v+(l<<2)>>2]|0;if((w|0)==0){break}x=w;y=m|0;z=c[(c[w>>2]|0)+48>>2]|0;ca[z&15](x,9744,9770,y)|0;x=c[h>>2]|0;dz(x)|0;k$(u|0,0,12);x=p;dR(p,10,0);if((a[u]&1)==0){z=x+1|0;A=z;B=z;C=p+8|0}else{z=p+8|0;A=c[z>>2]|0;B=x+1|0;C=z}c[q>>2]=A;z=r|0;c[s>>2]=z;c[t>>2]=0;x=f|0;w=g|0;D=p|0;E=p+4|0;F=A;G=c[x>>2]|0;L1323:while(1){do{if((G|0)==0){H=0}else{I=c[G+12>>2]|0;if((I|0)==(c[G+16>>2]|0)){J=b0[c[(c[G>>2]|0)+36>>2]&127](G)|0}else{J=c[I>>2]|0}if((J|0)!=-1){H=G;break}c[x>>2]=0;H=0}}while(0);I=(H|0)==0;K=c[w>>2]|0;do{if((K|0)==0){L=1073}else{M=c[K+12>>2]|0;if((M|0)==(c[K+16>>2]|0)){N=b0[c[(c[K>>2]|0)+36>>2]&127](K)|0}else{N=c[M>>2]|0}if((N|0)==-1){c[w>>2]=0;L=1073;break}else{if(I^(K|0)==0){break}else{O=F;break L1323}}}}while(0);if((L|0)==1073){L=0;if(I){O=F;break}}K=d[u]|0;M=(K&1|0)==0;if(((c[q>>2]|0)-F|0)==((M?K>>>1:c[E>>2]|0)|0)){if(M){P=K>>>1;Q=K>>>1}else{K=c[E>>2]|0;P=K;Q=K}dR(p,P<<1,0);if((a[u]&1)==0){R=10}else{R=(c[D>>2]&-2)-1|0}dR(p,R,0);if((a[u]&1)==0){S=B}else{S=c[C>>2]|0}c[q>>2]=S+Q;T=S}else{T=F}K=H+12|0;M=c[K>>2]|0;U=H+16|0;if((M|0)==(c[U>>2]|0)){V=b0[c[(c[H>>2]|0)+36>>2]&127](H)|0}else{V=c[M>>2]|0}if((fU(V,16,T,q,t,0,n,z,s,y)|0)!=0){O=T;break}M=c[K>>2]|0;if((M|0)==(c[U>>2]|0)){U=c[(c[H>>2]|0)+40>>2]|0;b0[U&127](H)|0;F=T;G=H;continue}else{c[K>>2]=M+4;F=T;G=H;continue}}a[O+3|0]=0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);G=fR(O,c[2958]|0,1320,(F=i,i=i+8|0,c[F>>2]=k,F)|0)|0;i=F;if((G|0)!=1){c[j>>2]=4}G=c[x>>2]|0;do{if((G|0)==0){W=0}else{F=c[G+12>>2]|0;if((F|0)==(c[G+16>>2]|0)){X=b0[c[(c[G>>2]|0)+36>>2]&127](G)|0}else{X=c[F>>2]|0}if((X|0)!=-1){W=G;break}c[x>>2]=0;W=0}}while(0);x=(W|0)==0;G=c[w>>2]|0;do{if((G|0)==0){L=1118}else{F=c[G+12>>2]|0;if((F|0)==(c[G+16>>2]|0)){Y=b0[c[(c[G>>2]|0)+36>>2]&127](G)|0}else{Y=c[F>>2]|0}if((Y|0)==-1){c[w>>2]=0;L=1118;break}if(!(x^(G|0)==0)){break}Z=b|0;c[Z>>2]=W;dP(p);dP(n);i=e;return}}while(0);do{if((L|0)==1118){if(x){break}Z=b|0;c[Z>>2]=W;dP(p);dP(n);i=e;return}}while(0);c[j>>2]=c[j>>2]|2;Z=b|0;c[Z>>2]=W;dP(p);dP(n);i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function f8(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+80|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+8|0;l=d+24|0;m=d+48|0;n=d+56|0;o=d+64|0;p=d+72|0;q=j|0;a[q]=a[2384]|0;a[q+1|0]=a[2385|0]|0;a[q+2|0]=a[2386|0]|0;a[q+3|0]=a[2387|0]|0;a[q+4|0]=a[2388|0]|0;a[q+5|0]=a[2389|0]|0;r=j+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=r}else{a[r]=43;u=j+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;u=v+1|0;L1410:do{switch(t&74|0){case 64:{a[u]=111;break};case 8:{if((t&16384|0)==0){a[u]=120;break L1410}else{a[u]=88;break L1410}break};default:{a[u]=100}}}while(0);u=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,12,c[2958]|0,q,(q=i,i=i+8|0,c[q>>2]=h,q)|0)|0;i=q;q=k+t|0;L1423:do{switch(c[s>>2]&176|0){case 32:{w=q;break};case 16:{h=a[u]|0;switch(h<<24>>24){case 45:case 43:{w=k+1|0;break L1423;break};default:{}}if(!((t|0)>1&h<<24>>24==48)){x=1147;break L1423}switch(a[k+1|0]|0){case 120:case 88:{break};default:{x=1147;break L1423}}w=k+2|0;break};default:{x=1147}}}while(0);if((x|0)==1147){w=u}x=l|0;ej(o,f);gc(u,w,q,x,m,n,o);dz(c[o>>2]|0)|0;c[p>>2]=c[e>>2];f9(b,p,x,c[m>>2]|0,c[n>>2]|0,f,g);i=d;return}function f9(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;k=i;i=i+16|0;l=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[l>>2];l=k|0;m=d|0;d=c[m>>2]|0;if((d|0)==0){c[b>>2]=0;i=k;return}n=g;g=e;o=n-g|0;p=h+12|0;h=c[p>>2]|0;q=(h|0)>(o|0)?h-o|0:0;o=f;h=o-g|0;do{if((h|0)>0){if((b1[c[(c[d>>2]|0)+48>>2]&63](d,e,h)|0)==(h|0)){break}c[m>>2]=0;c[b>>2]=0;i=k;return}}while(0);do{if((q|0)>0){d_(l,q,j);if((a[l]&1)==0){r=l+1|0}else{r=c[l+8>>2]|0}if((b1[c[(c[d>>2]|0)+48>>2]&63](d,r,q)|0)==(q|0)){dP(l);break}c[m>>2]=0;c[b>>2]=0;dP(l);i=k;return}}while(0);l=n-o|0;do{if((l|0)>0){if((b1[c[(c[d>>2]|0)+48>>2]&63](d,f,l)|0)==(l|0)){break}c[m>>2]=0;c[b>>2]=0;i=k;return}}while(0);c[p>>2]=0;c[b>>2]=d;i=k;return}function ga(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;j=i;i=i+48|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+16|0;m=j+24|0;n=j+32|0;if((c[f+4>>2]&1|0)==0){o=c[(c[d>>2]|0)+24>>2]|0;c[l>>2]=c[e>>2];b9[o&31](b,d,l,f,g,h&1);i=j;return}ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3828]|0)!=-1){c[k>>2]=15312;c[k+4>>2]=12;c[k+8>>2]=0;dW(15312,k,96)}k=(c[3829]|0)-1|0;g=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-g>>2>>>0>k>>>0){l=c[g+(k<<2)>>2]|0;if((l|0)==0){break}d=l;o=c[f>>2]|0;dz(o)|0;o=c[l>>2]|0;if(h){bZ[c[o+24>>2]&127](n,d)}else{bZ[c[o+28>>2]&127](n,d)}d=n;o=n;l=a[o]|0;if((l&1)==0){p=d+1|0;q=p;r=p;s=n+8|0}else{p=n+8|0;q=c[p>>2]|0;r=d+1|0;s=p}p=e|0;d=n+4|0;t=q;u=l;while(1){if((u&1)==0){v=r}else{v=c[s>>2]|0}l=u&255;if((t|0)==(v+((l&1|0)==0?l>>>1:c[d>>2]|0)|0)){break}l=a[t]|0;w=c[p>>2]|0;do{if((w|0)!=0){x=w+24|0;y=c[x>>2]|0;if((y|0)!=(c[w+28>>2]|0)){c[x>>2]=y+1;a[y]=l;break}if((b_[c[(c[w>>2]|0)+52>>2]&31](w,l&255)|0)!=-1){break}c[p>>2]=0}}while(0);t=t+1|0;u=a[o]|0}c[b>>2]=c[p>>2];dP(n);i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gb(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0;g=i;i=i+16|0;h=g|0;j=h;c[j>>2]=f;c[j+4>>2]=0;j=bB(d|0)|0;d=bC(a|0,b|0,e|0,h|0)|0;if((j|0)==0){i=g;return d|0}bB(j|0)|0;i=g;return d|0}function gc(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0;l=i;i=i+48|0;m=l|0;n=l+16|0;o=l+32|0;p=k|0;k=c[p>>2]|0;if((c[3924]|0)!=-1){c[n>>2]=15696;c[n+4>>2]=12;c[n+8>>2]=0;dW(15696,n,96)}n=(c[3925]|0)-1|0;q=c[k+8>>2]|0;if((c[k+12>>2]|0)-q>>2>>>0<=n>>>0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}k=c[q+(n<<2)>>2]|0;if((k|0)==0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}r=k;s=c[p>>2]|0;if((c[3828]|0)!=-1){c[m>>2]=15312;c[m+4>>2]=12;c[m+8>>2]=0;dW(15312,m,96)}m=(c[3829]|0)-1|0;p=c[s+8>>2]|0;if((c[s+12>>2]|0)-p>>2>>>0<=m>>>0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}s=c[p+(m<<2)>>2]|0;if((s|0)==0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}t=s;bZ[c[(c[s>>2]|0)+20>>2]&127](o,t);u=o;m=o;p=d[m]|0;if((p&1|0)==0){v=p>>>1}else{v=c[o+4>>2]|0}do{if((v|0)==0){p=c[(c[k>>2]|0)+32>>2]|0;ca[p&15](r,b,f,g)|0;c[j>>2]=g+(f-b)}else{c[j>>2]=g;p=a[b]|0;switch(p<<24>>24){case 45:case 43:{n=b_[c[(c[k>>2]|0)+28>>2]&31](r,p)|0;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=n;w=b+1|0;break};default:{w=b}}L1533:do{if((f-w|0)>1){if((a[w]|0)!=48){x=w;break}n=w+1|0;switch(a[n]|0){case 120:case 88:{break};default:{x=w;break L1533}}p=k;q=b_[c[(c[p>>2]|0)+28>>2]&31](r,48)|0;y=c[j>>2]|0;c[j>>2]=y+1;a[y]=q;q=b_[c[(c[p>>2]|0)+28>>2]&31](r,a[n]|0)|0;n=c[j>>2]|0;c[j>>2]=n+1;a[n]=q;x=w+2|0}else{x=w}}while(0);do{if((x|0)!=(f|0)){q=f-1|0;if(x>>>0<q>>>0){z=x;A=q}else{break}do{q=a[z]|0;a[z]=a[A]|0;a[A]=q;z=z+1|0;A=A-1|0;}while(z>>>0<A>>>0)}}while(0);q=b0[c[(c[s>>2]|0)+16>>2]&127](t)|0;if(x>>>0<f>>>0){n=u+1|0;p=k;y=o+4|0;B=o+8|0;C=0;D=0;E=x;while(1){F=(a[m]&1)==0;do{if((a[(F?n:c[B>>2]|0)+D|0]|0)==0){G=D;H=C}else{if((C|0)!=(a[(F?n:c[B>>2]|0)+D|0]|0)){G=D;H=C;break}I=c[j>>2]|0;c[j>>2]=I+1;a[I]=q;I=d[m]|0;G=(D>>>0<(((I&1|0)==0?I>>>1:c[y>>2]|0)-1|0)>>>0)+D|0;H=0}}while(0);F=b_[c[(c[p>>2]|0)+28>>2]&31](r,a[E]|0)|0;I=c[j>>2]|0;c[j>>2]=I+1;a[I]=F;F=E+1|0;if(F>>>0<f>>>0){C=H+1|0;D=G;E=F}else{break}}}E=g+(x-b)|0;D=c[j>>2]|0;if((E|0)==(D|0)){break}C=D-1|0;if(E>>>0<C>>>0){J=E;K=C}else{break}do{C=a[J]|0;a[J]=a[K]|0;a[K]=C;J=J+1|0;K=K-1|0;}while(J>>>0<K>>>0)}}while(0);if((e|0)==(f|0)){L=c[j>>2]|0;c[h>>2]=L;dP(o);i=l;return}else{L=g+(e-b)|0;c[h>>2]=L;dP(o);i=l;return}}function gd(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+112|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+32|0;n=d+80|0;o=d+88|0;p=d+96|0;q=d+104|0;c[k>>2]=37;c[k+4>>2]=0;r=k;k=r+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=k}else{a[k]=43;u=r+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;a[v+1|0]=108;u=v+2|0;L1574:do{switch(t&74|0){case 64:{a[u]=111;break};case 8:{if((t&16384|0)==0){a[u]=120;break L1574}else{a[u]=88;break L1574}break};default:{a[u]=100}}}while(0);u=l|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,22,c[2958]|0,r,(r=i,i=i+16|0,c[r>>2]=h,c[r+8>>2]=j,r)|0)|0;i=r;r=l+t|0;L1587:do{switch(c[s>>2]&176|0){case 16:{j=a[u]|0;switch(j<<24>>24){case 45:case 43:{w=l+1|0;break L1587;break};default:{}}if(!((t|0)>1&j<<24>>24==48)){x=1286;break L1587}switch(a[l+1|0]|0){case 120:case 88:{break};default:{x=1286;break L1587}}w=l+2|0;break};case 32:{w=r;break};default:{x=1286}}}while(0);if((x|0)==1286){w=u}x=m|0;ej(p,f);gc(u,w,r,x,n,o,p);dz(c[p>>2]|0)|0;c[q>>2]=c[e>>2];f9(b,q,x,c[n>>2]|0,c[o>>2]|0,f,g);i=d;return}function ge(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+80|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+8|0;l=d+24|0;m=d+48|0;n=d+56|0;o=d+64|0;p=d+72|0;q=j|0;a[q]=a[2384]|0;a[q+1|0]=a[2385|0]|0;a[q+2|0]=a[2386|0]|0;a[q+3|0]=a[2387|0]|0;a[q+4|0]=a[2388|0]|0;a[q+5|0]=a[2389|0]|0;r=j+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=r}else{a[r]=43;u=j+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;u=v+1|0;L1605:do{switch(t&74|0){case 64:{a[u]=111;break};case 8:{if((t&16384|0)==0){a[u]=120;break L1605}else{a[u]=88;break L1605}break};default:{a[u]=117}}}while(0);u=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,12,c[2958]|0,q,(q=i,i=i+8|0,c[q>>2]=h,q)|0)|0;i=q;q=k+t|0;L1618:do{switch(c[s>>2]&176|0){case 32:{w=q;break};case 16:{h=a[u]|0;switch(h<<24>>24){case 45:case 43:{w=k+1|0;break L1618;break};default:{}}if(!((t|0)>1&h<<24>>24==48)){x=1311;break L1618}switch(a[k+1|0]|0){case 120:case 88:{break};default:{x=1311;break L1618}}w=k+2|0;break};default:{x=1311}}}while(0);if((x|0)==1311){w=u}x=l|0;ej(o,f);gc(u,w,q,x,m,n,o);dz(c[o>>2]|0)|0;c[p>>2]=c[e>>2];f9(b,p,x,c[m>>2]|0,c[n>>2]|0,f,g);i=d;return}function gf(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+112|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+32|0;n=d+80|0;o=d+88|0;p=d+96|0;q=d+104|0;c[k>>2]=37;c[k+4>>2]=0;r=k;k=r+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=k}else{a[k]=43;u=r+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;a[v+1|0]=108;u=v+2|0;L1636:do{switch(t&74|0){case 8:{if((t&16384|0)==0){a[u]=120;break L1636}else{a[u]=88;break L1636}break};case 64:{a[u]=111;break};default:{a[u]=117}}}while(0);u=l|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,23,c[2958]|0,r,(r=i,i=i+16|0,c[r>>2]=h,c[r+8>>2]=j,r)|0)|0;i=r;r=l+t|0;L1649:do{switch(c[s>>2]&176|0){case 16:{j=a[u]|0;switch(j<<24>>24){case 45:case 43:{w=l+1|0;break L1649;break};default:{}}if(!((t|0)>1&j<<24>>24==48)){x=1336;break L1649}switch(a[l+1|0]|0){case 120:case 88:{break};default:{x=1336;break L1649}}w=l+2|0;break};case 32:{w=r;break};default:{x=1336}}}while(0);if((x|0)==1336){w=u}x=m|0;ej(p,f);gc(u,w,r,x,n,o,p);dz(c[p>>2]|0)|0;c[q>>2]=c[e>>2];f9(b,q,x,c[n>>2]|0,c[o>>2]|0,f,g);i=d;return}function gg(b,d,e,f,g,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;j=+j;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;d=i;i=i+152|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+40|0;n=d+48|0;o=d+112|0;p=d+120|0;q=d+128|0;r=d+136|0;s=d+144|0;c[k>>2]=37;c[k+4>>2]=0;t=k;k=t+1|0;u=f+4|0;v=c[u>>2]|0;if((v&2048|0)==0){w=k}else{a[k]=43;w=t+2|0}if((v&1024|0)==0){x=w}else{a[w]=35;x=w+1|0}w=v&260;k=v>>>14;L1667:do{if((w|0)==260){if((k&1|0)==0){a[x]=97;y=0;break}else{a[x]=65;y=0;break}}else{a[x]=46;v=x+2|0;a[x+1|0]=42;switch(w|0){case 4:{if((k&1|0)==0){a[v]=102;y=1;break L1667}else{a[v]=70;y=1;break L1667}break};case 256:{if((k&1|0)==0){a[v]=101;y=1;break L1667}else{a[v]=69;y=1;break L1667}break};default:{if((k&1|0)==0){a[v]=103;y=1;break L1667}else{a[v]=71;y=1;break L1667}}}}}while(0);k=l|0;c[m>>2]=k;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=c[2958]|0;if(y){w=gb(k,30,l,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;A=w}else{w=gb(k,30,l,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;A=w}do{if((A|0)>29){w=(a[16264]|0)==0;if(y){do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=l}else{do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);w=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=w}w=c[m>>2]|0;if((w|0)!=0){C=B;D=w;E=w;break}kX();w=c[m>>2]|0;C=B;D=w;E=w}else{C=A;D=0;E=c[m>>2]|0}}while(0);A=E+C|0;L1716:do{switch(c[u>>2]&176|0){case 16:{B=a[E]|0;switch(B<<24>>24){case 45:case 43:{F=E+1|0;break L1716;break};default:{}}if(!((C|0)>1&B<<24>>24==48)){G=1392;break L1716}switch(a[E+1|0]|0){case 120:case 88:{break};default:{G=1392;break L1716}}F=E+2|0;break};case 32:{F=A;break};default:{G=1392}}}while(0);if((G|0)==1392){F=E}do{if((E|0)==(k|0)){H=n|0;I=0;J=k}else{G=kJ(C<<1)|0;if((G|0)!=0){H=G;I=G;J=E;break}kX();H=0;I=0;J=c[m>>2]|0}}while(0);ej(q,f);gj(J,F,A,H,o,p,q);dz(c[q>>2]|0)|0;q=e|0;c[s>>2]=c[q>>2];f9(r,s,H,c[o>>2]|0,c[p>>2]|0,f,g);g=c[r>>2]|0;c[q>>2]=g;c[b>>2]=g;if((I|0)!=0){kK(I)}if((D|0)==0){i=d;return}kK(D);i=d;return}function gh(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;f=i;i=i+16|0;g=f|0;h=g;c[h>>2]=e;c[h+4>>2]=0;h=bB(b|0)|0;b=bQ(a|0,d|0,g|0)|0;if((h|0)==0){i=f;return b|0}bB(h|0)|0;i=f;return b|0}function gi(b,d,e,f,g,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;j=+j;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;d=i;i=i+152|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+40|0;n=d+48|0;o=d+112|0;p=d+120|0;q=d+128|0;r=d+136|0;s=d+144|0;c[k>>2]=37;c[k+4>>2]=0;t=k;k=t+1|0;u=f+4|0;v=c[u>>2]|0;if((v&2048|0)==0){w=k}else{a[k]=43;w=t+2|0}if((v&1024|0)==0){x=w}else{a[w]=35;x=w+1|0}w=v&260;k=v>>>14;L1754:do{if((w|0)==260){a[x]=76;v=x+1|0;if((k&1|0)==0){a[v]=97;y=0;break}else{a[v]=65;y=0;break}}else{a[x]=46;a[x+1|0]=42;a[x+2|0]=76;v=x+3|0;switch(w|0){case 4:{if((k&1|0)==0){a[v]=102;y=1;break L1754}else{a[v]=70;y=1;break L1754}break};case 256:{if((k&1|0)==0){a[v]=101;y=1;break L1754}else{a[v]=69;y=1;break L1754}break};default:{if((k&1|0)==0){a[v]=103;y=1;break L1754}else{a[v]=71;y=1;break L1754}}}}}while(0);k=l|0;c[m>>2]=k;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=c[2958]|0;if(y){w=gb(k,30,l,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;A=w}else{w=gb(k,30,l,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;A=w}do{if((A|0)>29){w=(a[16264]|0)==0;if(y){do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=l}else{do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);w=gh(m,c[2958]|0,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;B=w}w=c[m>>2]|0;if((w|0)!=0){C=B;D=w;E=w;break}kX();w=c[m>>2]|0;C=B;D=w;E=w}else{C=A;D=0;E=c[m>>2]|0}}while(0);A=E+C|0;L1803:do{switch(c[u>>2]&176|0){case 32:{F=A;break};case 16:{B=a[E]|0;switch(B<<24>>24){case 45:case 43:{F=E+1|0;break L1803;break};default:{}}if(!((C|0)>1&B<<24>>24==48)){G=1477;break L1803}switch(a[E+1|0]|0){case 120:case 88:{break};default:{G=1477;break L1803}}F=E+2|0;break};default:{G=1477}}}while(0);if((G|0)==1477){F=E}do{if((E|0)==(k|0)){H=n|0;I=0;J=k}else{G=kJ(C<<1)|0;if((G|0)!=0){H=G;I=G;J=E;break}kX();H=0;I=0;J=c[m>>2]|0}}while(0);ej(q,f);gj(J,F,A,H,o,p,q);dz(c[q>>2]|0)|0;q=e|0;c[s>>2]=c[q>>2];f9(r,s,H,c[o>>2]|0,c[p>>2]|0,f,g);g=c[r>>2]|0;c[q>>2]=g;c[b>>2]=g;if((I|0)!=0){kK(I)}if((D|0)==0){i=d;return}kK(D);i=d;return}function gj(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0;l=i;i=i+48|0;m=l|0;n=l+16|0;o=l+32|0;p=k|0;k=c[p>>2]|0;if((c[3924]|0)!=-1){c[n>>2]=15696;c[n+4>>2]=12;c[n+8>>2]=0;dW(15696,n,96)}n=(c[3925]|0)-1|0;q=c[k+8>>2]|0;if((c[k+12>>2]|0)-q>>2>>>0<=n>>>0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}k=c[q+(n<<2)>>2]|0;if((k|0)==0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}r=k;s=c[p>>2]|0;if((c[3828]|0)!=-1){c[m>>2]=15312;c[m+4>>2]=12;c[m+8>>2]=0;dW(15312,m,96)}m=(c[3829]|0)-1|0;p=c[s+8>>2]|0;if((c[s+12>>2]|0)-p>>2>>>0<=m>>>0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}s=c[p+(m<<2)>>2]|0;if((s|0)==0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}t=s;bZ[c[(c[s>>2]|0)+20>>2]&127](o,t);c[j>>2]=g;u=a[b]|0;switch(u<<24>>24){case 45:case 43:{m=b_[c[(c[k>>2]|0)+28>>2]&31](r,u)|0;u=c[j>>2]|0;c[j>>2]=u+1;a[u]=m;v=b+1|0;break};default:{v=b}}m=f;L1851:do{if((m-v|0)>1){if((a[v]|0)!=48){w=v;x=1532;break}u=v+1|0;switch(a[u]|0){case 120:case 88:{break};default:{w=v;x=1532;break L1851}}p=k;n=b_[c[(c[p>>2]|0)+28>>2]&31](r,48)|0;q=c[j>>2]|0;c[j>>2]=q+1;a[q]=n;n=v+2|0;q=b_[c[(c[p>>2]|0)+28>>2]&31](r,a[u]|0)|0;u=c[j>>2]|0;c[j>>2]=u+1;a[u]=q;q=n;while(1){if(q>>>0>=f>>>0){y=q;z=n;break L1851}u=a[q]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((a3(u<<24>>24|0,c[2958]|0)|0)==0){y=q;z=n;break}else{q=q+1|0}}}else{w=v;x=1532}}while(0);L1866:do{if((x|0)==1532){while(1){x=0;if(w>>>0>=f>>>0){y=w;z=v;break L1866}q=a[w]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((bH(q<<24>>24|0,c[2958]|0)|0)==0){y=w;z=v;break}else{w=w+1|0;x=1532}}}}while(0);x=o;w=o;v=d[w]|0;if((v&1|0)==0){A=v>>>1}else{A=c[o+4>>2]|0}do{if((A|0)==0){v=c[j>>2]|0;u=c[(c[k>>2]|0)+32>>2]|0;ca[u&15](r,z,y,v)|0;c[j>>2]=(c[j>>2]|0)+(y-z)}else{do{if((z|0)!=(y|0)){v=y-1|0;if(z>>>0<v>>>0){B=z;C=v}else{break}do{v=a[B]|0;a[B]=a[C]|0;a[C]=v;B=B+1|0;C=C-1|0;}while(B>>>0<C>>>0)}}while(0);q=b0[c[(c[s>>2]|0)+16>>2]&127](t)|0;if(z>>>0<y>>>0){v=x+1|0;u=o+4|0;n=o+8|0;p=k;D=0;E=0;F=z;while(1){G=(a[w]&1)==0;do{if((a[(G?v:c[n>>2]|0)+E|0]|0)>0){if((D|0)!=(a[(G?v:c[n>>2]|0)+E|0]|0)){H=E;I=D;break}J=c[j>>2]|0;c[j>>2]=J+1;a[J]=q;J=d[w]|0;H=(E>>>0<(((J&1|0)==0?J>>>1:c[u>>2]|0)-1|0)>>>0)+E|0;I=0}else{H=E;I=D}}while(0);G=b_[c[(c[p>>2]|0)+28>>2]&31](r,a[F]|0)|0;J=c[j>>2]|0;c[j>>2]=J+1;a[J]=G;G=F+1|0;if(G>>>0<y>>>0){D=I+1|0;E=H;F=G}else{break}}}F=g+(z-b)|0;E=c[j>>2]|0;if((F|0)==(E|0)){break}D=E-1|0;if(F>>>0<D>>>0){K=F;L=D}else{break}do{D=a[K]|0;a[K]=a[L]|0;a[L]=D;K=K+1|0;L=L-1|0;}while(K>>>0<L>>>0)}}while(0);L1905:do{if(y>>>0<f>>>0){L=k;K=y;while(1){z=a[K]|0;if(z<<24>>24==46){break}H=b_[c[(c[L>>2]|0)+28>>2]&31](r,z)|0;z=c[j>>2]|0;c[j>>2]=z+1;a[z]=H;H=K+1|0;if(H>>>0<f>>>0){K=H}else{M=H;break L1905}}L=b0[c[(c[s>>2]|0)+12>>2]&127](t)|0;H=c[j>>2]|0;c[j>>2]=H+1;a[H]=L;M=K+1|0}else{M=y}}while(0);ca[c[(c[k>>2]|0)+32>>2]&15](r,M,f,c[j>>2]|0)|0;r=(c[j>>2]|0)+(m-M)|0;c[j>>2]=r;if((e|0)==(f|0)){N=r;c[h>>2]=N;dP(o);i=l;return}N=g+(e-b)|0;c[h>>2]=N;dP(o);i=l;return}function gk(a){a=a|0;de(a|0);kS(a);return}function gl(a){a=a|0;de(a|0);return}function gm(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+144|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+8|0;l=d+24|0;m=d+112|0;n=d+120|0;o=d+128|0;p=d+136|0;q=j|0;a[q]=a[2384]|0;a[q+1|0]=a[2385|0]|0;a[q+2|0]=a[2386|0]|0;a[q+3|0]=a[2387|0]|0;a[q+4|0]=a[2388|0]|0;a[q+5|0]=a[2389|0]|0;r=j+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=r}else{a[r]=43;u=j+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;u=v+1|0;L1928:do{switch(t&74|0){case 64:{a[u]=111;break};case 8:{if((t&16384|0)==0){a[u]=120;break L1928}else{a[u]=88;break L1928}break};default:{a[u]=100}}}while(0);u=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,12,c[2958]|0,q,(q=i,i=i+8|0,c[q>>2]=h,q)|0)|0;i=q;q=k+t|0;L1941:do{switch(c[s>>2]&176|0){case 32:{w=q;break};case 16:{h=a[u]|0;switch(h<<24>>24){case 45:case 43:{w=k+1|0;break L1941;break};default:{}}if(!((t|0)>1&h<<24>>24==48)){x=1600;break L1941}switch(a[k+1|0]|0){case 120:case 88:{break};default:{x=1600;break L1941}}w=k+2|0;break};default:{x=1600}}}while(0);if((x|0)==1600){w=u}x=l|0;ej(o,f);gp(u,w,q,x,m,n,o);dz(c[o>>2]|0)|0;c[p>>2]=c[e>>2];gq(b,p,x,c[m>>2]|0,c[n>>2]|0,f,g);i=d;return}function gn(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;d=i;i=i+104|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+24|0;l=d+48|0;m=d+88|0;n=d+96|0;o=d+16|0;a[o]=a[2392]|0;a[o+1|0]=a[2393|0]|0;a[o+2|0]=a[2394|0]|0;a[o+3|0]=a[2395|0]|0;a[o+4|0]=a[2396|0]|0;a[o+5|0]=a[2397|0]|0;p=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);q=gb(p,20,c[2958]|0,o,(o=i,i=i+8|0,c[o>>2]=h,o)|0)|0;i=o;o=k+q|0;L1958:do{switch(c[f+4>>2]&176|0){case 16:{h=a[p]|0;switch(h<<24>>24){case 45:case 43:{r=k+1|0;break L1958;break};default:{}}if(!((q|0)>1&h<<24>>24==48)){s=1615;break L1958}switch(a[k+1|0]|0){case 120:case 88:{break};default:{s=1615;break L1958}}r=k+2|0;break};case 32:{r=o;break};default:{s=1615}}}while(0);if((s|0)==1615){r=p}ej(m,f);s=m|0;m=c[s>>2]|0;if((c[3924]|0)!=-1){c[j>>2]=15696;c[j+4>>2]=12;c[j+8>>2]=0;dW(15696,j,96)}j=(c[3925]|0)-1|0;h=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-h>>2>>>0>j>>>0){t=c[h+(j<<2)>>2]|0;if((t|0)==0){break}u=t;v=c[s>>2]|0;dz(v)|0;v=l|0;w=c[(c[t>>2]|0)+32>>2]|0;ca[w&15](u,p,o,v)|0;u=l+q|0;if((r|0)==(o|0)){x=u;y=e|0;z=c[y>>2]|0;A=n|0;c[A>>2]=z;f9(b,n,v,x,u,f,g);i=d;return}x=l+(r-k)|0;y=e|0;z=c[y>>2]|0;A=n|0;c[A>>2]=z;f9(b,n,v,x,u,f,g);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function go(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0;j=i;i=i+48|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+16|0;m=j+24|0;n=j+32|0;if((c[f+4>>2]&1|0)==0){o=c[(c[d>>2]|0)+24>>2]|0;c[l>>2]=c[e>>2];b9[o&31](b,d,l,f,g,h&1);i=j;return}ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3826]|0)!=-1){c[k>>2]=15304;c[k+4>>2]=12;c[k+8>>2]=0;dW(15304,k,96)}k=(c[3827]|0)-1|0;g=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-g>>2>>>0>k>>>0){l=c[g+(k<<2)>>2]|0;if((l|0)==0){break}d=l;o=c[f>>2]|0;dz(o)|0;o=c[l>>2]|0;if(h){bZ[c[o+24>>2]&127](n,d)}else{bZ[c[o+28>>2]&127](n,d)}d=n;o=a[d]|0;if((o&1)==0){l=n+4|0;p=l;q=l;r=n+8|0}else{l=n+8|0;p=c[l>>2]|0;q=n+4|0;r=l}l=e|0;s=p;t=o;while(1){if((t&1)==0){u=q}else{u=c[r>>2]|0}o=t&255;if((o&1|0)==0){v=o>>>1}else{v=c[q>>2]|0}if((s|0)==(u+(v<<2)|0)){break}o=c[s>>2]|0;w=c[l>>2]|0;do{if((w|0)!=0){x=w+24|0;y=c[x>>2]|0;if((y|0)==(c[w+28>>2]|0)){z=b_[c[(c[w>>2]|0)+52>>2]&31](w,o)|0}else{c[x>>2]=y+4;c[y>>2]=o;z=o}if((z|0)!=-1){break}c[l>>2]=0}}while(0);s=s+4|0;t=a[d]|0}c[b>>2]=c[l>>2];ee(n);i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gp(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0;l=i;i=i+48|0;m=l|0;n=l+16|0;o=l+32|0;p=k|0;k=c[p>>2]|0;if((c[3922]|0)!=-1){c[n>>2]=15688;c[n+4>>2]=12;c[n+8>>2]=0;dW(15688,n,96)}n=(c[3923]|0)-1|0;q=c[k+8>>2]|0;if((c[k+12>>2]|0)-q>>2>>>0<=n>>>0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}k=c[q+(n<<2)>>2]|0;if((k|0)==0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}r=k;s=c[p>>2]|0;if((c[3826]|0)!=-1){c[m>>2]=15304;c[m+4>>2]=12;c[m+8>>2]=0;dW(15304,m,96)}m=(c[3827]|0)-1|0;p=c[s+8>>2]|0;if((c[s+12>>2]|0)-p>>2>>>0<=m>>>0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}s=c[p+(m<<2)>>2]|0;if((s|0)==0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}t=s;bZ[c[(c[s>>2]|0)+20>>2]&127](o,t);u=o;m=o;p=d[m]|0;if((p&1|0)==0){v=p>>>1}else{v=c[o+4>>2]|0}do{if((v|0)==0){p=c[(c[k>>2]|0)+48>>2]|0;ca[p&15](r,b,f,g)|0;c[j>>2]=g+(f-b<<2)}else{c[j>>2]=g;p=a[b]|0;switch(p<<24>>24){case 45:case 43:{n=b_[c[(c[k>>2]|0)+44>>2]&31](r,p)|0;p=c[j>>2]|0;c[j>>2]=p+4;c[p>>2]=n;w=b+1|0;break};default:{w=b}}L2050:do{if((f-w|0)>1){if((a[w]|0)!=48){x=w;break}n=w+1|0;switch(a[n]|0){case 120:case 88:{break};default:{x=w;break L2050}}p=k;q=b_[c[(c[p>>2]|0)+44>>2]&31](r,48)|0;y=c[j>>2]|0;c[j>>2]=y+4;c[y>>2]=q;q=b_[c[(c[p>>2]|0)+44>>2]&31](r,a[n]|0)|0;n=c[j>>2]|0;c[j>>2]=n+4;c[n>>2]=q;x=w+2|0}else{x=w}}while(0);do{if((x|0)!=(f|0)){q=f-1|0;if(x>>>0<q>>>0){z=x;A=q}else{break}do{q=a[z]|0;a[z]=a[A]|0;a[A]=q;z=z+1|0;A=A-1|0;}while(z>>>0<A>>>0)}}while(0);q=b0[c[(c[s>>2]|0)+16>>2]&127](t)|0;if(x>>>0<f>>>0){n=u+1|0;p=k;y=o+4|0;B=o+8|0;C=0;D=0;E=x;while(1){F=(a[m]&1)==0;do{if((a[(F?n:c[B>>2]|0)+D|0]|0)==0){G=D;H=C}else{if((C|0)!=(a[(F?n:c[B>>2]|0)+D|0]|0)){G=D;H=C;break}I=c[j>>2]|0;c[j>>2]=I+4;c[I>>2]=q;I=d[m]|0;G=(D>>>0<(((I&1|0)==0?I>>>1:c[y>>2]|0)-1|0)>>>0)+D|0;H=0}}while(0);F=b_[c[(c[p>>2]|0)+44>>2]&31](r,a[E]|0)|0;I=c[j>>2]|0;c[j>>2]=I+4;c[I>>2]=F;F=E+1|0;if(F>>>0<f>>>0){C=H+1|0;D=G;E=F}else{break}}}E=g+(x-b<<2)|0;D=c[j>>2]|0;if((E|0)==(D|0)){break}C=D-4|0;if(E>>>0<C>>>0){J=E;K=C}else{break}do{C=c[J>>2]|0;c[J>>2]=c[K>>2];c[K>>2]=C;J=J+4|0;K=K-4|0;}while(J>>>0<K>>>0)}}while(0);if((e|0)==(f|0)){L=c[j>>2]|0;c[h>>2]=L;dP(o);i=l;return}else{L=g+(e-b<<2)|0;c[h>>2]=L;dP(o);i=l;return}}function gq(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;k=i;i=i+16|0;l=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[l>>2];l=k|0;m=d|0;d=c[m>>2]|0;if((d|0)==0){c[b>>2]=0;i=k;return}n=g;g=e;o=n-g>>2;p=h+12|0;h=c[p>>2]|0;q=(h|0)>(o|0)?h-o|0:0;o=f;h=o-g|0;g=h>>2;do{if((h|0)>0){if((b1[c[(c[d>>2]|0)+48>>2]&63](d,e,g)|0)==(g|0)){break}c[m>>2]=0;c[b>>2]=0;i=k;return}}while(0);do{if((q|0)>0){ey(l,q,j);if((a[l]&1)==0){r=l+4|0}else{r=c[l+8>>2]|0}if((b1[c[(c[d>>2]|0)+48>>2]&63](d,r,q)|0)==(q|0)){ee(l);break}c[m>>2]=0;c[b>>2]=0;ee(l);i=k;return}}while(0);l=n-o|0;o=l>>2;do{if((l|0)>0){if((b1[c[(c[d>>2]|0)+48>>2]&63](d,f,o)|0)==(o|0)){break}c[m>>2]=0;c[b>>2]=0;i=k;return}}while(0);c[p>>2]=0;c[b>>2]=d;i=k;return}function gr(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+232|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+32|0;n=d+200|0;o=d+208|0;p=d+216|0;q=d+224|0;c[k>>2]=37;c[k+4>>2]=0;r=k;k=r+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=k}else{a[k]=43;u=r+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;a[v+1|0]=108;u=v+2|0;L2119:do{switch(t&74|0){case 8:{if((t&16384|0)==0){a[u]=120;break L2119}else{a[u]=88;break L2119}break};case 64:{a[u]=111;break};default:{a[u]=100}}}while(0);u=l|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,22,c[2958]|0,r,(r=i,i=i+16|0,c[r>>2]=h,c[r+8>>2]=j,r)|0)|0;i=r;r=l+t|0;L2132:do{switch(c[s>>2]&176|0){case 32:{w=r;break};case 16:{j=a[u]|0;switch(j<<24>>24){case 45:case 43:{w=l+1|0;break L2132;break};default:{}}if(!((t|0)>1&j<<24>>24==48)){x=1760;break L2132}switch(a[l+1|0]|0){case 120:case 88:{break};default:{x=1760;break L2132}}w=l+2|0;break};default:{x=1760}}}while(0);if((x|0)==1760){w=u}x=m|0;ej(p,f);gp(u,w,r,x,n,o,p);dz(c[p>>2]|0)|0;c[q>>2]=c[e>>2];gq(b,q,x,c[n>>2]|0,c[o>>2]|0,f,g);i=d;return}function gs(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+144|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+8|0;l=d+24|0;m=d+112|0;n=d+120|0;o=d+128|0;p=d+136|0;q=j|0;a[q]=a[2384]|0;a[q+1|0]=a[2385|0]|0;a[q+2|0]=a[2386|0]|0;a[q+3|0]=a[2387|0]|0;a[q+4|0]=a[2388|0]|0;a[q+5|0]=a[2389|0]|0;r=j+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=r}else{a[r]=43;u=j+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;u=v+1|0;L2150:do{switch(t&74|0){case 64:{a[u]=111;break};case 8:{if((t&16384|0)==0){a[u]=120;break L2150}else{a[u]=88;break L2150}break};default:{a[u]=117}}}while(0);u=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,12,c[2958]|0,q,(q=i,i=i+8|0,c[q>>2]=h,q)|0)|0;i=q;q=k+t|0;L2163:do{switch(c[s>>2]&176|0){case 16:{h=a[u]|0;switch(h<<24>>24){case 45:case 43:{w=k+1|0;break L2163;break};default:{}}if(!((t|0)>1&h<<24>>24==48)){x=1785;break L2163}switch(a[k+1|0]|0){case 120:case 88:{break};default:{x=1785;break L2163}}w=k+2|0;break};case 32:{w=q;break};default:{x=1785}}}while(0);if((x|0)==1785){w=u}x=l|0;ej(o,f);gp(u,w,q,x,m,n,o);dz(c[o>>2]|0)|0;c[p>>2]=c[e>>2];gq(b,p,x,c[m>>2]|0,c[n>>2]|0,f,g);i=d;return}function gt(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;d=i;i=i+240|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+32|0;n=d+208|0;o=d+216|0;p=d+224|0;q=d+232|0;c[k>>2]=37;c[k+4>>2]=0;r=k;k=r+1|0;s=f+4|0;t=c[s>>2]|0;if((t&2048|0)==0){u=k}else{a[k]=43;u=r+2|0}if((t&512|0)==0){v=u}else{a[u]=35;v=u+1|0}a[v]=108;a[v+1|0]=108;u=v+2|0;L2181:do{switch(t&74|0){case 8:{if((t&16384|0)==0){a[u]=120;break L2181}else{a[u]=88;break L2181}break};case 64:{a[u]=111;break};default:{a[u]=117}}}while(0);u=l|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);t=gb(u,23,c[2958]|0,r,(r=i,i=i+16|0,c[r>>2]=h,c[r+8>>2]=j,r)|0)|0;i=r;r=l+t|0;L2194:do{switch(c[s>>2]&176|0){case 16:{j=a[u]|0;switch(j<<24>>24){case 45:case 43:{w=l+1|0;break L2194;break};default:{}}if(!((t|0)>1&j<<24>>24==48)){x=1810;break L2194}switch(a[l+1|0]|0){case 120:case 88:{break};default:{x=1810;break L2194}}w=l+2|0;break};case 32:{w=r;break};default:{x=1810}}}while(0);if((x|0)==1810){w=u}x=m|0;ej(p,f);gp(u,w,r,x,n,o,p);dz(c[p>>2]|0)|0;c[q>>2]=c[e>>2];gq(b,q,x,c[n>>2]|0,c[o>>2]|0,f,g);i=d;return}function gu(b,d,e,f,g,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;j=+j;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;d=i;i=i+320|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+40|0;n=d+48|0;o=d+280|0;p=d+288|0;q=d+296|0;r=d+304|0;s=d+312|0;c[k>>2]=37;c[k+4>>2]=0;t=k;k=t+1|0;u=f+4|0;v=c[u>>2]|0;if((v&2048|0)==0){w=k}else{a[k]=43;w=t+2|0}if((v&1024|0)==0){x=w}else{a[w]=35;x=w+1|0}w=v&260;k=v>>>14;L2212:do{if((w|0)==260){if((k&1|0)==0){a[x]=97;y=0;break}else{a[x]=65;y=0;break}}else{a[x]=46;v=x+2|0;a[x+1|0]=42;switch(w|0){case 256:{if((k&1|0)==0){a[v]=101;y=1;break L2212}else{a[v]=69;y=1;break L2212}break};case 4:{if((k&1|0)==0){a[v]=102;y=1;break L2212}else{a[v]=70;y=1;break L2212}break};default:{if((k&1|0)==0){a[v]=103;y=1;break L2212}else{a[v]=71;y=1;break L2212}}}}}while(0);k=l|0;c[m>>2]=k;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=c[2958]|0;if(y){w=gb(k,30,l,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;A=w}else{w=gb(k,30,l,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;A=w}do{if((A|0)>29){w=(a[16264]|0)==0;if(y){do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=l}else{do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);w=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=w}w=c[m>>2]|0;if((w|0)!=0){C=B;D=w;E=w;break}kX();w=c[m>>2]|0;C=B;D=w;E=w}else{C=A;D=0;E=c[m>>2]|0}}while(0);A=E+C|0;L2261:do{switch(c[u>>2]&176|0){case 32:{F=A;break};case 16:{B=a[E]|0;switch(B<<24>>24){case 45:case 43:{F=E+1|0;break L2261;break};default:{}}if(!((C|0)>1&B<<24>>24==48)){G=1866;break L2261}switch(a[E+1|0]|0){case 120:case 88:{break};default:{G=1866;break L2261}}F=E+2|0;break};default:{G=1866}}}while(0);if((G|0)==1866){F=E}do{if((E|0)==(k|0)){H=n|0;I=0;J=k}else{G=kJ(C<<3)|0;u=G;if((G|0)!=0){H=u;I=u;J=E;break}kX();H=u;I=u;J=c[m>>2]|0}}while(0);ej(q,f);gv(J,F,A,H,o,p,q);dz(c[q>>2]|0)|0;q=e|0;c[s>>2]=c[q>>2];gq(r,s,H,c[o>>2]|0,c[p>>2]|0,f,g);g=c[r>>2]|0;c[q>>2]=g;c[b>>2]=g;if((I|0)!=0){kK(I)}if((D|0)==0){i=d;return}kK(D);i=d;return}function gv(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0;l=i;i=i+48|0;m=l|0;n=l+16|0;o=l+32|0;p=k|0;k=c[p>>2]|0;if((c[3922]|0)!=-1){c[n>>2]=15688;c[n+4>>2]=12;c[n+8>>2]=0;dW(15688,n,96)}n=(c[3923]|0)-1|0;q=c[k+8>>2]|0;if((c[k+12>>2]|0)-q>>2>>>0<=n>>>0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}k=c[q+(n<<2)>>2]|0;if((k|0)==0){r=bO(4)|0;s=r;kq(s);bl(r|0,8176,134)}r=k;s=c[p>>2]|0;if((c[3826]|0)!=-1){c[m>>2]=15304;c[m+4>>2]=12;c[m+8>>2]=0;dW(15304,m,96)}m=(c[3827]|0)-1|0;p=c[s+8>>2]|0;if((c[s+12>>2]|0)-p>>2>>>0<=m>>>0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}s=c[p+(m<<2)>>2]|0;if((s|0)==0){t=bO(4)|0;u=t;kq(u);bl(t|0,8176,134)}t=s;bZ[c[(c[s>>2]|0)+20>>2]&127](o,t);c[j>>2]=g;u=a[b]|0;switch(u<<24>>24){case 45:case 43:{m=b_[c[(c[k>>2]|0)+44>>2]&31](r,u)|0;u=c[j>>2]|0;c[j>>2]=u+4;c[u>>2]=m;v=b+1|0;break};default:{v=b}}m=f;L2309:do{if((m-v|0)>1){if((a[v]|0)!=48){w=v;x=1921;break}u=v+1|0;switch(a[u]|0){case 120:case 88:{break};default:{w=v;x=1921;break L2309}}p=k;n=b_[c[(c[p>>2]|0)+44>>2]&31](r,48)|0;q=c[j>>2]|0;c[j>>2]=q+4;c[q>>2]=n;n=v+2|0;q=b_[c[(c[p>>2]|0)+44>>2]&31](r,a[u]|0)|0;u=c[j>>2]|0;c[j>>2]=u+4;c[u>>2]=q;q=n;while(1){if(q>>>0>=f>>>0){y=q;z=n;break L2309}u=a[q]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((a3(u<<24>>24|0,c[2958]|0)|0)==0){y=q;z=n;break}else{q=q+1|0}}}else{w=v;x=1921}}while(0);L2324:do{if((x|0)==1921){while(1){x=0;if(w>>>0>=f>>>0){y=w;z=v;break L2324}q=a[w]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((bH(q<<24>>24|0,c[2958]|0)|0)==0){y=w;z=v;break}else{w=w+1|0;x=1921}}}}while(0);x=o;w=o;v=d[w]|0;if((v&1|0)==0){A=v>>>1}else{A=c[o+4>>2]|0}do{if((A|0)==0){v=c[j>>2]|0;u=c[(c[k>>2]|0)+48>>2]|0;ca[u&15](r,z,y,v)|0;c[j>>2]=(c[j>>2]|0)+(y-z<<2)}else{do{if((z|0)!=(y|0)){v=y-1|0;if(z>>>0<v>>>0){B=z;C=v}else{break}do{v=a[B]|0;a[B]=a[C]|0;a[C]=v;B=B+1|0;C=C-1|0;}while(B>>>0<C>>>0)}}while(0);q=b0[c[(c[s>>2]|0)+16>>2]&127](t)|0;if(z>>>0<y>>>0){v=x+1|0;u=o+4|0;n=o+8|0;p=k;D=0;E=0;F=z;while(1){G=(a[w]&1)==0;do{if((a[(G?v:c[n>>2]|0)+E|0]|0)>0){if((D|0)!=(a[(G?v:c[n>>2]|0)+E|0]|0)){H=E;I=D;break}J=c[j>>2]|0;c[j>>2]=J+4;c[J>>2]=q;J=d[w]|0;H=(E>>>0<(((J&1|0)==0?J>>>1:c[u>>2]|0)-1|0)>>>0)+E|0;I=0}else{H=E;I=D}}while(0);G=b_[c[(c[p>>2]|0)+44>>2]&31](r,a[F]|0)|0;J=c[j>>2]|0;c[j>>2]=J+4;c[J>>2]=G;G=F+1|0;if(G>>>0<y>>>0){D=I+1|0;E=H;F=G}else{break}}}F=g+(z-b<<2)|0;E=c[j>>2]|0;if((F|0)==(E|0)){break}D=E-4|0;if(F>>>0<D>>>0){K=F;L=D}else{break}do{D=c[K>>2]|0;c[K>>2]=c[L>>2];c[L>>2]=D;K=K+4|0;L=L-4|0;}while(K>>>0<L>>>0)}}while(0);L2363:do{if(y>>>0<f>>>0){L=k;K=y;while(1){z=a[K]|0;if(z<<24>>24==46){break}H=b_[c[(c[L>>2]|0)+44>>2]&31](r,z)|0;z=c[j>>2]|0;c[j>>2]=z+4;c[z>>2]=H;H=K+1|0;if(H>>>0<f>>>0){K=H}else{M=H;break L2363}}L=b0[c[(c[s>>2]|0)+12>>2]&127](t)|0;H=c[j>>2]|0;c[j>>2]=H+4;c[H>>2]=L;M=K+1|0}else{M=y}}while(0);ca[c[(c[k>>2]|0)+48>>2]&15](r,M,f,c[j>>2]|0)|0;r=(c[j>>2]|0)+(m-M<<2)|0;c[j>>2]=r;if((e|0)==(f|0)){N=r;c[h>>2]=N;dP(o);i=l;return}N=g+(e-b<<2)|0;c[h>>2]=N;dP(o);i=l;return}function gw(b,d,e,f,g,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;j=+j;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;d=i;i=i+320|0;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;l=d+8|0;m=d+40|0;n=d+48|0;o=d+280|0;p=d+288|0;q=d+296|0;r=d+304|0;s=d+312|0;c[k>>2]=37;c[k+4>>2]=0;t=k;k=t+1|0;u=f+4|0;v=c[u>>2]|0;if((v&2048|0)==0){w=k}else{a[k]=43;w=t+2|0}if((v&1024|0)==0){x=w}else{a[w]=35;x=w+1|0}w=v&260;k=v>>>14;L2384:do{if((w|0)==260){a[x]=76;v=x+1|0;if((k&1|0)==0){a[v]=97;y=0;break}else{a[v]=65;y=0;break}}else{a[x]=46;a[x+1|0]=42;a[x+2|0]=76;v=x+3|0;switch(w|0){case 256:{if((k&1|0)==0){a[v]=101;y=1;break L2384}else{a[v]=69;y=1;break L2384}break};case 4:{if((k&1|0)==0){a[v]=102;y=1;break L2384}else{a[v]=70;y=1;break L2384}break};default:{if((k&1|0)==0){a[v]=103;y=1;break L2384}else{a[v]=71;y=1;break L2384}}}}}while(0);k=l|0;c[m>>2]=k;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=c[2958]|0;if(y){w=gb(k,30,l,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;A=w}else{w=gb(k,30,l,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;A=w}do{if((A|0)>29){w=(a[16264]|0)==0;if(y){do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);l=gh(m,c[2958]|0,t,(z=i,i=i+16|0,c[z>>2]=c[f+8>>2],h[z+8>>3]=j,z)|0)|0;i=z;B=l}else{do{if(w){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);w=gh(m,c[2958]|0,t,(z=i,i=i+8|0,h[z>>3]=j,z)|0)|0;i=z;B=w}w=c[m>>2]|0;if((w|0)!=0){C=B;D=w;E=w;break}kX();w=c[m>>2]|0;C=B;D=w;E=w}else{C=A;D=0;E=c[m>>2]|0}}while(0);A=E+C|0;L2433:do{switch(c[u>>2]&176|0){case 16:{B=a[E]|0;switch(B<<24>>24){case 45:case 43:{F=E+1|0;break L2433;break};default:{}}if(!((C|0)>1&B<<24>>24==48)){G=2018;break L2433}switch(a[E+1|0]|0){case 120:case 88:{break};default:{G=2018;break L2433}}F=E+2|0;break};case 32:{F=A;break};default:{G=2018}}}while(0);if((G|0)==2018){F=E}do{if((E|0)==(k|0)){H=n|0;I=0;J=k}else{G=kJ(C<<3)|0;u=G;if((G|0)!=0){H=u;I=u;J=E;break}kX();H=u;I=u;J=c[m>>2]|0}}while(0);ej(q,f);gv(J,F,A,H,o,p,q);dz(c[q>>2]|0)|0;q=e|0;c[s>>2]=c[q>>2];gq(r,s,H,c[o>>2]|0,c[p>>2]|0,f,g);g=c[r>>2]|0;c[q>>2]=g;c[b>>2]=g;if((I|0)!=0){kK(I)}if((D|0)==0){i=d;return}kK(D);i=d;return}function gx(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;d=i;i=i+216|0;j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=d|0;k=d+24|0;l=d+48|0;m=d+200|0;n=d+208|0;o=d+16|0;a[o]=a[2392]|0;a[o+1|0]=a[2393|0]|0;a[o+2|0]=a[2394|0]|0;a[o+3|0]=a[2395|0]|0;a[o+4|0]=a[2396|0]|0;a[o+5|0]=a[2397|0]|0;p=k|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);q=gb(p,20,c[2958]|0,o,(o=i,i=i+8|0,c[o>>2]=h,o)|0)|0;i=o;o=k+q|0;L2464:do{switch(c[f+4>>2]&176|0){case 16:{h=a[p]|0;switch(h<<24>>24){case 45:case 43:{r=k+1|0;break L2464;break};default:{}}if(!((q|0)>1&h<<24>>24==48)){s=2051;break L2464}switch(a[k+1|0]|0){case 120:case 88:{break};default:{s=2051;break L2464}}r=k+2|0;break};case 32:{r=o;break};default:{s=2051}}}while(0);if((s|0)==2051){r=p}ej(m,f);s=m|0;m=c[s>>2]|0;if((c[3922]|0)!=-1){c[j>>2]=15688;c[j+4>>2]=12;c[j+8>>2]=0;dW(15688,j,96)}j=(c[3923]|0)-1|0;h=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-h>>2>>>0>j>>>0){t=c[h+(j<<2)>>2]|0;if((t|0)==0){break}u=t;v=c[s>>2]|0;dz(v)|0;v=l|0;w=c[(c[t>>2]|0)+48>>2]|0;ca[w&15](u,p,o,v)|0;u=l+(q<<2)|0;if((r|0)==(o|0)){x=u;y=e|0;z=c[y>>2]|0;A=n|0;c[A>>2]=z;gq(b,n,v,x,u,f,g);i=d;return}x=l+(r-k<<2)|0;y=e|0;z=c[y>>2]|0;A=n|0;c[A>>2]=z;gq(b,n,v,x,u,f,g);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function gy(a){a=a|0;return 2}function gz(a){a=a|0;de(a|0);kS(a);return}function gA(a){a=a|0;de(a|0);return}function gB(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0;j=i;i=i+16|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;c[k>>2]=c[d>>2];c[l>>2]=c[e>>2];gD(a,b,k,l,f,g,h,2376,2384);i=j;return}function gC(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;k=i;i=i+16|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=k|0;m=k+8|0;n=d+8|0;o=b0[c[(c[n>>2]|0)+20>>2]&127](n)|0;c[l>>2]=c[e>>2];c[m>>2]=c[f>>2];f=o;e=a[o]|0;if((e&1)==0){p=f+1|0;q=f+1|0}else{f=c[o+8>>2]|0;p=f;q=f}f=e&255;if((f&1|0)==0){r=f>>>1}else{r=c[o+4>>2]|0}gD(b,d,l,m,g,h,j,q,p+r|0);i=k;return}function gD(d,e,f,g,h,j,k,l,m){d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0;n=i;i=i+48|0;o=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[o>>2];o=g;g=i;i=i+4|0;i=i+7>>3<<3;c[g>>2]=c[o>>2];o=n|0;p=n+16|0;q=n+24|0;r=n+32|0;s=n+40|0;ej(p,h);t=p|0;p=c[t>>2]|0;if((c[3924]|0)!=-1){c[o>>2]=15696;c[o+4>>2]=12;c[o+8>>2]=0;dW(15696,o,96)}o=(c[3925]|0)-1|0;u=c[p+8>>2]|0;do{if((c[p+12>>2]|0)-u>>2>>>0>o>>>0){v=c[u+(o<<2)>>2]|0;if((v|0)==0){break}w=v;x=c[t>>2]|0;dz(x)|0;c[j>>2]=0;x=f|0;L2506:do{if((l|0)==(m|0)){y=2141}else{z=g|0;A=v;B=v+8|0;C=v;D=e;E=r|0;F=s|0;G=q|0;H=l;I=0;L2508:while(1){J=I;while(1){if((J|0)!=0){y=2141;break L2506}K=c[x>>2]|0;do{if((K|0)==0){L=0}else{if((c[K+12>>2]|0)!=(c[K+16>>2]|0)){L=K;break}if((b0[c[(c[K>>2]|0)+36>>2]&127](K)|0)!=-1){L=K;break}c[x>>2]=0;L=0}}while(0);K=(L|0)==0;M=c[z>>2]|0;L2518:do{if((M|0)==0){y=2094}else{do{if((c[M+12>>2]|0)==(c[M+16>>2]|0)){if((b0[c[(c[M>>2]|0)+36>>2]&127](M)|0)!=-1){break}c[z>>2]=0;y=2094;break L2518}}while(0);if(K){N=M}else{y=2095;break L2508}}}while(0);if((y|0)==2094){y=0;if(K){y=2095;break L2508}else{N=0}}if((b1[c[(c[A>>2]|0)+36>>2]&63](w,a[H]|0,0)|0)<<24>>24==37){y=2098;break}M=a[H]|0;if(M<<24>>24>-1){O=c[B>>2]|0;if((b[O+(M<<24>>24<<1)>>1]&8192)!=0){P=H;y=2109;break}}Q=L+12|0;M=c[Q>>2]|0;R=L+16|0;if((M|0)==(c[R>>2]|0)){S=(b0[c[(c[L>>2]|0)+36>>2]&127](L)|0)&255}else{S=a[M]|0}M=b_[c[(c[C>>2]|0)+12>>2]&31](w,S)|0;if(M<<24>>24==(b_[c[(c[C>>2]|0)+12>>2]&31](w,a[H]|0)|0)<<24>>24){y=2136;break}c[j>>2]=4;J=4}L2536:do{if((y|0)==2136){y=0;J=c[Q>>2]|0;if((J|0)==(c[R>>2]|0)){M=c[(c[L>>2]|0)+40>>2]|0;b0[M&127](L)|0}else{c[Q>>2]=J+1}T=H+1|0}else if((y|0)==2098){y=0;J=H+1|0;if((J|0)==(m|0)){y=2099;break L2508}M=b1[c[(c[A>>2]|0)+36>>2]&63](w,a[J]|0,0)|0;switch(M<<24>>24){case 69:case 48:{U=H+2|0;if((U|0)==(m|0)){y=2102;break L2508}V=M;W=b1[c[(c[A>>2]|0)+36>>2]&63](w,a[U]|0,0)|0;X=U;break};default:{V=0;W=M;X=J}}J=c[(c[D>>2]|0)+36>>2]|0;c[E>>2]=L;c[F>>2]=N;b7[J&7](q,e,r,s,h,j,k,W,V);c[x>>2]=c[G>>2];T=X+1|0}else if((y|0)==2109){while(1){y=0;J=P+1|0;if((J|0)==(m|0)){Y=m;break}M=a[J]|0;if(M<<24>>24<=-1){Y=J;break}if((b[O+(M<<24>>24<<1)>>1]&8192)==0){Y=J;break}else{P=J;y=2109}}K=L;J=N;while(1){do{if((K|0)==0){Z=0}else{if((c[K+12>>2]|0)!=(c[K+16>>2]|0)){Z=K;break}if((b0[c[(c[K>>2]|0)+36>>2]&127](K)|0)!=-1){Z=K;break}c[x>>2]=0;Z=0}}while(0);M=(Z|0)==0;do{if((J|0)==0){y=2122}else{if((c[J+12>>2]|0)!=(c[J+16>>2]|0)){if(M){_=J;break}else{T=Y;break L2536}}if((b0[c[(c[J>>2]|0)+36>>2]&127](J)|0)==-1){c[z>>2]=0;y=2122;break}else{if(M^(J|0)==0){_=J;break}else{T=Y;break L2536}}}}while(0);if((y|0)==2122){y=0;if(M){T=Y;break L2536}else{_=0}}U=Z+12|0;$=c[U>>2]|0;aa=Z+16|0;if(($|0)==(c[aa>>2]|0)){ab=(b0[c[(c[Z>>2]|0)+36>>2]&127](Z)|0)&255}else{ab=a[$]|0}if(ab<<24>>24<=-1){T=Y;break L2536}if((b[(c[B>>2]|0)+(ab<<24>>24<<1)>>1]&8192)==0){T=Y;break L2536}$=c[U>>2]|0;if(($|0)==(c[aa>>2]|0)){aa=c[(c[Z>>2]|0)+40>>2]|0;b0[aa&127](Z)|0;K=Z;J=_;continue}else{c[U>>2]=$+1;K=Z;J=_;continue}}}}while(0);if((T|0)==(m|0)){y=2141;break L2506}H=T;I=c[j>>2]|0}if((y|0)==2099){c[j>>2]=4;ac=L;break}else if((y|0)==2102){c[j>>2]=4;ac=L;break}else if((y|0)==2095){c[j>>2]=4;ac=L;break}}}while(0);if((y|0)==2141){ac=c[x>>2]|0}w=f|0;do{if((ac|0)!=0){if((c[ac+12>>2]|0)!=(c[ac+16>>2]|0)){break}if((b0[c[(c[ac>>2]|0)+36>>2]&127](ac)|0)!=-1){break}c[w>>2]=0}}while(0);x=c[w>>2]|0;v=(x|0)==0;I=g|0;H=c[I>>2]|0;L2594:do{if((H|0)==0){y=2151}else{do{if((c[H+12>>2]|0)==(c[H+16>>2]|0)){if((b0[c[(c[H>>2]|0)+36>>2]&127](H)|0)!=-1){break}c[I>>2]=0;y=2151;break L2594}}while(0);if(!v){break}ad=d|0;c[ad>>2]=x;i=n;return}}while(0);do{if((y|0)==2151){if(v){break}ad=d|0;c[ad>>2]=x;i=n;return}}while(0);c[j>>2]=c[j>>2]|2;ad=d|0;c[ad>>2]=x;i=n;return}}while(0);n=bO(4)|0;kq(n);bl(n|0,8176,134)}function gE(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;j=i;i=i+32|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;m=j+24|0;ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;n=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-n>>2>>>0>l>>>0){o=c[n+(l<<2)>>2]|0;if((o|0)==0){break}p=o;o=c[f>>2]|0;dz(o)|0;o=c[e>>2]|0;q=b+8|0;r=b0[c[c[q>>2]>>2]&127](q)|0;c[k>>2]=o;o=(fq(d,k,r,r+168|0,p,g,0)|0)-r|0;if((o|0)>=168){s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}c[h+24>>2]=((o|0)/12|0|0)%7|0;s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gF(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;j=i;i=i+32|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;m=j+24|0;ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;n=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-n>>2>>>0>l>>>0){o=c[n+(l<<2)>>2]|0;if((o|0)==0){break}p=o;o=c[f>>2]|0;dz(o)|0;o=c[e>>2]|0;q=b+8|0;r=b0[c[(c[q>>2]|0)+4>>2]&127](q)|0;c[k>>2]=o;o=(fq(d,k,r,r+288|0,p,g,0)|0)-r|0;if((o|0)>=288){s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}c[h+16>>2]=((o|0)/12|0|0)%12|0;s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gG(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;b=i;i=i+32|0;j=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[j>>2];j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=b|0;k=b+8|0;l=b+24|0;ej(l,f);f=l|0;l=c[f>>2]|0;if((c[3924]|0)!=-1){c[k>>2]=15696;c[k+4>>2]=12;c[k+8>>2]=0;dW(15696,k,96)}k=(c[3925]|0)-1|0;m=c[l+8>>2]|0;do{if((c[l+12>>2]|0)-m>>2>>>0>k>>>0){n=c[m+(k<<2)>>2]|0;if((n|0)==0){break}o=n;n=c[f>>2]|0;dz(n)|0;c[j>>2]=c[e>>2];n=gL(d,j,g,o,4)|0;if((c[g>>2]&4|0)!=0){p=d|0;q=c[p>>2]|0;r=a|0;c[r>>2]=q;i=b;return}if((n|0)<69){s=n+2e3|0}else{s=(n-69|0)>>>0<31?n+1900|0:n}c[h+20>>2]=s-1900;p=d|0;q=c[p>>2]|0;r=a|0;c[r>>2]=q;i=b;return}}while(0);b=bO(4)|0;kq(b);bl(b|0,8176,134)}function gH(d,e,f,g,h){d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;d=i;j=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[j>>2];j=e|0;e=f|0;f=h+8|0;L2652:while(1){h=c[j>>2]|0;do{if((h|0)==0){k=0}else{if((c[h+12>>2]|0)!=(c[h+16>>2]|0)){k=h;break}if((b0[c[(c[h>>2]|0)+36>>2]&127](h)|0)==-1){c[j>>2]=0;k=0;break}else{k=c[j>>2]|0;break}}}while(0);h=(k|0)==0;l=c[e>>2]|0;L2661:do{if((l|0)==0){m=2207}else{do{if((c[l+12>>2]|0)==(c[l+16>>2]|0)){if((b0[c[(c[l>>2]|0)+36>>2]&127](l)|0)!=-1){break}c[e>>2]=0;m=2207;break L2661}}while(0);if(h){n=l;o=0}else{p=l;q=0;break L2652}}}while(0);if((m|0)==2207){m=0;if(h){p=0;q=1;break}else{n=0;o=1}}l=c[j>>2]|0;r=c[l+12>>2]|0;if((r|0)==(c[l+16>>2]|0)){s=(b0[c[(c[l>>2]|0)+36>>2]&127](l)|0)&255}else{s=a[r]|0}if(s<<24>>24<=-1){p=n;q=o;break}if((b[(c[f>>2]|0)+(s<<24>>24<<1)>>1]&8192)==0){p=n;q=o;break}r=c[j>>2]|0;l=r+12|0;t=c[l>>2]|0;if((t|0)==(c[r+16>>2]|0)){u=c[(c[r>>2]|0)+40>>2]|0;b0[u&127](r)|0;continue}else{c[l>>2]=t+1;continue}}o=c[j>>2]|0;do{if((o|0)==0){v=0}else{if((c[o+12>>2]|0)!=(c[o+16>>2]|0)){v=o;break}if((b0[c[(c[o>>2]|0)+36>>2]&127](o)|0)==-1){c[j>>2]=0;v=0;break}else{v=c[j>>2]|0;break}}}while(0);j=(v|0)==0;do{if(q){m=2226}else{if((c[p+12>>2]|0)!=(c[p+16>>2]|0)){if(!(j^(p|0)==0)){break}i=d;return}if((b0[c[(c[p>>2]|0)+36>>2]&127](p)|0)==-1){c[e>>2]=0;m=2226;break}if(!j){break}i=d;return}}while(0);do{if((m|0)==2226){if(j){break}i=d;return}}while(0);c[g>>2]=c[g>>2]|2;i=d;return}function gI(b,d,e,f,g,h,j,k,l){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0;l=i;i=i+328|0;m=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[m>>2];m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=l|0;n=l+8|0;o=l+16|0;p=l+24|0;q=l+32|0;r=l+40|0;s=l+48|0;t=l+56|0;u=l+64|0;v=l+72|0;w=l+80|0;x=l+88|0;y=l+96|0;z=l+112|0;A=l+120|0;B=l+128|0;C=l+136|0;D=l+144|0;E=l+152|0;F=l+160|0;G=l+168|0;H=l+176|0;I=l+184|0;J=l+192|0;K=l+200|0;L=l+208|0;M=l+216|0;N=l+224|0;O=l+232|0;P=l+240|0;Q=l+248|0;R=l+256|0;S=l+264|0;T=l+272|0;U=l+280|0;V=l+288|0;W=l+296|0;X=l+304|0;Y=l+312|0;Z=l+320|0;c[h>>2]=0;ej(z,g);_=z|0;z=c[_>>2]|0;if((c[3924]|0)!=-1){c[y>>2]=15696;c[y+4>>2]=12;c[y+8>>2]=0;dW(15696,y,96)}y=(c[3925]|0)-1|0;$=c[z+8>>2]|0;do{if((c[z+12>>2]|0)-$>>2>>>0>y>>>0){aa=c[$+(y<<2)>>2]|0;if((aa|0)==0){break}ab=aa;aa=c[_>>2]|0;dz(aa)|0;L2709:do{switch(k<<24>>24|0){case 72:{c[u>>2]=c[f>>2];aa=gL(e,u,h,ab,2)|0;ac=c[h>>2]|0;if((ac&4|0)==0&(aa|0)<24){c[j+8>>2]=aa;break L2709}else{c[h>>2]=ac|4;break L2709}break};case 97:case 65:{ac=c[f>>2]|0;aa=d+8|0;ad=b0[c[c[aa>>2]>>2]&127](aa)|0;c[x>>2]=ac;ac=(fq(e,x,ad,ad+168|0,ab,h,0)|0)-ad|0;if((ac|0)>=168){break L2709}c[j+24>>2]=((ac|0)/12|0|0)%7|0;break};case 109:{c[r>>2]=c[f>>2];ac=(gL(e,r,h,ab,2)|0)-1|0;ad=c[h>>2]|0;if((ad&4|0)==0&(ac|0)<12){c[j+16>>2]=ac;break L2709}else{c[h>>2]=ad|4;break L2709}break};case 121:{c[n>>2]=c[f>>2];ad=gL(e,n,h,ab,4)|0;if((c[h>>2]&4|0)!=0){break L2709}if((ad|0)<69){ae=ad+2e3|0}else{ae=(ad-69|0)>>>0<31?ad+1900|0:ad}c[j+20>>2]=ae-1900;break};case 98:case 66:case 104:{ad=c[f>>2]|0;ac=d+8|0;aa=b0[c[(c[ac>>2]|0)+4>>2]&127](ac)|0;c[w>>2]=ad;ad=(fq(e,w,aa,aa+288|0,ab,h,0)|0)-aa|0;if((ad|0)>=288){break L2709}c[j+16>>2]=((ad|0)/12|0|0)%12|0;break};case 73:{ad=j+8|0;c[t>>2]=c[f>>2];aa=gL(e,t,h,ab,2)|0;ac=c[h>>2]|0;do{if((ac&4|0)==0){if((aa-1|0)>>>0>=12){break}c[ad>>2]=aa;break L2709}}while(0);c[h>>2]=ac|4;break};case 106:{c[s>>2]=c[f>>2];aa=gL(e,s,h,ab,3)|0;ad=c[h>>2]|0;if((ad&4|0)==0&(aa|0)<366){c[j+28>>2]=aa;break L2709}else{c[h>>2]=ad|4;break L2709}break};case 68:{ad=e|0;c[E>>2]=c[ad>>2];c[F>>2]=c[f>>2];gD(D,d,E,F,g,h,j,2368,2376);c[ad>>2]=c[D>>2];break};case 84:{ad=e|0;c[S>>2]=c[ad>>2];c[T>>2]=c[f>>2];gD(R,d,S,T,g,h,j,2328,2336);c[ad>>2]=c[R>>2];break};case 119:{c[o>>2]=c[f>>2];ad=gL(e,o,h,ab,1)|0;aa=c[h>>2]|0;if((aa&4|0)==0&(ad|0)<7){c[j+24>>2]=ad;break L2709}else{c[h>>2]=aa|4;break L2709}break};case 70:{aa=e|0;c[H>>2]=c[aa>>2];c[I>>2]=c[f>>2];gD(G,d,H,I,g,h,j,2360,2368);c[aa>>2]=c[G>>2];break};case 89:{c[m>>2]=c[f>>2];aa=gL(e,m,h,ab,4)|0;if((c[h>>2]&4|0)!=0){break L2709}c[j+20>>2]=aa-1900;break};case 37:{c[Z>>2]=c[f>>2];gK(0,e,Z,h,ab);break};case 114:{aa=e|0;c[M>>2]=c[aa>>2];c[N>>2]=c[f>>2];gD(L,d,M,N,g,h,j,2344,2355);c[aa>>2]=c[L>>2];break};case 82:{aa=e|0;c[P>>2]=c[aa>>2];c[Q>>2]=c[f>>2];gD(O,d,P,Q,g,h,j,2336,2341);c[aa>>2]=c[O>>2];break};case 99:{aa=d+8|0;ad=b0[c[(c[aa>>2]|0)+12>>2]&127](aa)|0;aa=e|0;c[B>>2]=c[aa>>2];c[C>>2]=c[f>>2];af=ad;ag=a[ad]|0;if((ag&1)==0){ah=af+1|0;ai=af+1|0}else{af=c[ad+8>>2]|0;ah=af;ai=af}af=ag&255;if((af&1|0)==0){aj=af>>>1}else{aj=c[ad+4>>2]|0}gD(A,d,B,C,g,h,j,ai,ah+aj|0);c[aa>>2]=c[A>>2];break};case 83:{c[p>>2]=c[f>>2];aa=gL(e,p,h,ab,2)|0;ad=c[h>>2]|0;if((ad&4|0)==0&(aa|0)<61){c[j>>2]=aa;break L2709}else{c[h>>2]=ad|4;break L2709}break};case 77:{c[q>>2]=c[f>>2];ad=gL(e,q,h,ab,2)|0;aa=c[h>>2]|0;if((aa&4|0)==0&(ad|0)<60){c[j+4>>2]=ad;break L2709}else{c[h>>2]=aa|4;break L2709}break};case 110:case 116:{c[J>>2]=c[f>>2];gH(0,e,J,h,ab);break};case 112:{c[K>>2]=c[f>>2];gJ(d,j+8|0,e,K,h,ab);break};case 120:{aa=c[(c[d>>2]|0)+20>>2]|0;c[U>>2]=c[e>>2];c[V>>2]=c[f>>2];bX[aa&127](b,d,U,V,g,h,j);i=l;return};case 88:{aa=d+8|0;ad=b0[c[(c[aa>>2]|0)+24>>2]&127](aa)|0;aa=e|0;c[X>>2]=c[aa>>2];c[Y>>2]=c[f>>2];af=ad;ag=a[ad]|0;if((ag&1)==0){ak=af+1|0;al=af+1|0}else{af=c[ad+8>>2]|0;ak=af;al=af}af=ag&255;if((af&1|0)==0){am=af>>>1}else{am=c[ad+4>>2]|0}gD(W,d,X,Y,g,h,j,al,ak+am|0);c[aa>>2]=c[W>>2];break};case 100:case 101:{aa=j+12|0;c[v>>2]=c[f>>2];ad=gL(e,v,h,ab,2)|0;af=c[h>>2]|0;do{if((af&4|0)==0){if((ad-1|0)>>>0>=31){break}c[aa>>2]=ad;break L2709}}while(0);c[h>>2]=af|4;break};default:{c[h>>2]=c[h>>2]|4}}}while(0);c[b>>2]=c[e>>2];i=l;return}}while(0);l=bO(4)|0;kq(l);bl(l|0,8176,134)}function gJ(a,b,e,f,g,h){a=a|0;b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0;j=i;i=i+8|0;k=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[k>>2];k=j|0;l=a+8|0;a=b0[c[(c[l>>2]|0)+8>>2]&127](l)|0;l=d[a]|0;if((l&1|0)==0){m=l>>>1}else{m=c[a+4>>2]|0}l=d[a+12|0]|0;if((l&1|0)==0){n=l>>>1}else{n=c[a+16>>2]|0}if((m|0)==(-n|0)){c[g>>2]=c[g>>2]|4;i=j;return}c[k>>2]=c[f>>2];f=fq(e,k,a,a+24|0,h,g,0)|0;g=f-a|0;do{if((f|0)==(a|0)){if((c[b>>2]|0)!=12){break}c[b>>2]=0;i=j;return}}while(0);if((g|0)!=12){i=j;return}g=c[b>>2]|0;if((g|0)>=12){i=j;return}c[b>>2]=g+12;i=j;return}function gK(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,j=0,k=0,l=0,m=0,n=0,o=0;b=i;h=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[h>>2];h=d|0;d=c[h>>2]|0;do{if((d|0)==0){j=0}else{if((c[d+12>>2]|0)!=(c[d+16>>2]|0)){j=d;break}if((b0[c[(c[d>>2]|0)+36>>2]&127](d)|0)==-1){c[h>>2]=0;j=0;break}else{j=c[h>>2]|0;break}}}while(0);d=(j|0)==0;j=e|0;e=c[j>>2]|0;L2822:do{if((e|0)==0){k=2337}else{do{if((c[e+12>>2]|0)==(c[e+16>>2]|0)){if((b0[c[(c[e>>2]|0)+36>>2]&127](e)|0)!=-1){break}c[j>>2]=0;k=2337;break L2822}}while(0);if(d){l=e;m=0}else{k=2338}}}while(0);if((k|0)==2337){if(d){k=2338}else{l=0;m=1}}if((k|0)==2338){c[f>>2]=c[f>>2]|6;i=b;return}d=c[h>>2]|0;e=c[d+12>>2]|0;if((e|0)==(c[d+16>>2]|0)){n=(b0[c[(c[d>>2]|0)+36>>2]&127](d)|0)&255}else{n=a[e]|0}if((b1[c[(c[g>>2]|0)+36>>2]&63](g,n,0)|0)<<24>>24!=37){c[f>>2]=c[f>>2]|4;i=b;return}n=c[h>>2]|0;g=n+12|0;e=c[g>>2]|0;if((e|0)==(c[n+16>>2]|0)){d=c[(c[n>>2]|0)+40>>2]|0;b0[d&127](n)|0}else{c[g>>2]=e+1}e=c[h>>2]|0;do{if((e|0)==0){o=0}else{if((c[e+12>>2]|0)!=(c[e+16>>2]|0)){o=e;break}if((b0[c[(c[e>>2]|0)+36>>2]&127](e)|0)==-1){c[h>>2]=0;o=0;break}else{o=c[h>>2]|0;break}}}while(0);h=(o|0)==0;do{if(m){k=2357}else{if((c[l+12>>2]|0)!=(c[l+16>>2]|0)){if(!(h^(l|0)==0)){break}i=b;return}if((b0[c[(c[l>>2]|0)+36>>2]&127](l)|0)==-1){c[j>>2]=0;k=2357;break}if(!h){break}i=b;return}}while(0);do{if((k|0)==2357){if(h){break}i=b;return}}while(0);c[f>>2]=c[f>>2]|2;i=b;return}function gL(d,e,f,g,h){d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;j=i;k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=d|0;d=c[k>>2]|0;do{if((d|0)==0){l=0}else{if((c[d+12>>2]|0)!=(c[d+16>>2]|0)){l=d;break}if((b0[c[(c[d>>2]|0)+36>>2]&127](d)|0)==-1){c[k>>2]=0;l=0;break}else{l=c[k>>2]|0;break}}}while(0);d=(l|0)==0;l=e|0;e=c[l>>2]|0;L2876:do{if((e|0)==0){m=2377}else{do{if((c[e+12>>2]|0)==(c[e+16>>2]|0)){if((b0[c[(c[e>>2]|0)+36>>2]&127](e)|0)!=-1){break}c[l>>2]=0;m=2377;break L2876}}while(0);if(d){n=e}else{m=2378}}}while(0);if((m|0)==2377){if(d){m=2378}else{n=0}}if((m|0)==2378){c[f>>2]=c[f>>2]|6;o=0;i=j;return o|0}d=c[k>>2]|0;e=c[d+12>>2]|0;if((e|0)==(c[d+16>>2]|0)){p=(b0[c[(c[d>>2]|0)+36>>2]&127](d)|0)&255}else{p=a[e]|0}do{if(p<<24>>24>-1){e=g+8|0;if((b[(c[e>>2]|0)+(p<<24>>24<<1)>>1]&2048)==0){break}d=g;q=(b1[c[(c[d>>2]|0)+36>>2]&63](g,p,0)|0)<<24>>24;r=c[k>>2]|0;s=r+12|0;t=c[s>>2]|0;if((t|0)==(c[r+16>>2]|0)){u=c[(c[r>>2]|0)+40>>2]|0;b0[u&127](r)|0;v=q;w=h;x=n}else{c[s>>2]=t+1;v=q;w=h;x=n}while(1){y=v-48|0;q=w-1|0;t=c[k>>2]|0;do{if((t|0)==0){z=0}else{if((c[t+12>>2]|0)!=(c[t+16>>2]|0)){z=t;break}if((b0[c[(c[t>>2]|0)+36>>2]&127](t)|0)==-1){c[k>>2]=0;z=0;break}else{z=c[k>>2]|0;break}}}while(0);t=(z|0)==0;if((x|0)==0){A=z;B=0}else{do{if((c[x+12>>2]|0)==(c[x+16>>2]|0)){if((b0[c[(c[x>>2]|0)+36>>2]&127](x)|0)!=-1){C=x;break}c[l>>2]=0;C=0}else{C=x}}while(0);A=c[k>>2]|0;B=C}D=(B|0)==0;if(!((t^D)&(q|0)>0)){m=2407;break}s=c[A+12>>2]|0;if((s|0)==(c[A+16>>2]|0)){E=(b0[c[(c[A>>2]|0)+36>>2]&127](A)|0)&255}else{E=a[s]|0}if(E<<24>>24<=-1){o=y;m=2422;break}if((b[(c[e>>2]|0)+(E<<24>>24<<1)>>1]&2048)==0){o=y;m=2423;break}s=((b1[c[(c[d>>2]|0)+36>>2]&63](g,E,0)|0)<<24>>24)+(y*10|0)|0;r=c[k>>2]|0;u=r+12|0;F=c[u>>2]|0;if((F|0)==(c[r+16>>2]|0)){G=c[(c[r>>2]|0)+40>>2]|0;b0[G&127](r)|0;v=s;w=q;x=B;continue}else{c[u>>2]=F+1;v=s;w=q;x=B;continue}}if((m|0)==2407){do{if((A|0)==0){H=0}else{if((c[A+12>>2]|0)!=(c[A+16>>2]|0)){H=A;break}if((b0[c[(c[A>>2]|0)+36>>2]&127](A)|0)==-1){c[k>>2]=0;H=0;break}else{H=c[k>>2]|0;break}}}while(0);d=(H|0)==0;L2933:do{if(D){m=2417}else{do{if((c[B+12>>2]|0)==(c[B+16>>2]|0)){if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){break}c[l>>2]=0;m=2417;break L2933}}while(0);if(d){o=y}else{break}i=j;return o|0}}while(0);do{if((m|0)==2417){if(d){break}else{o=y}i=j;return o|0}}while(0);c[f>>2]=c[f>>2]|2;o=y;i=j;return o|0}else if((m|0)==2422){i=j;return o|0}else if((m|0)==2423){i=j;return o|0}}}while(0);c[f>>2]=c[f>>2]|4;o=0;i=j;return o|0}function gM(a){a=a|0;return 2}function gN(a){a=a|0;de(a|0);kS(a);return}function gO(a){a=a|0;de(a|0);return}function gP(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0;j=i;i=i+16|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;c[k>>2]=c[d>>2];c[l>>2]=c[e>>2];gR(a,b,k,l,f,g,h,2296,2328);i=j;return}function gQ(b,d,e,f,g,h,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;k=i;i=i+16|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=k|0;m=k+8|0;n=d+8|0;o=b0[c[(c[n>>2]|0)+20>>2]&127](n)|0;c[l>>2]=c[e>>2];c[m>>2]=c[f>>2];f=a[o]|0;if((f&1)==0){p=o+4|0;q=o+4|0}else{e=c[o+8>>2]|0;p=e;q=e}e=f&255;if((e&1|0)==0){r=e>>>1}else{r=c[o+4>>2]|0}gR(b,d,l,m,g,h,j,q,p+(r<<2)|0);i=k;return}function gR(a,b,d,e,f,g,h,j,k){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0;l=i;i=i+48|0;m=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[m>>2];m=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[m>>2];m=l|0;n=l+16|0;o=l+24|0;p=l+32|0;q=l+40|0;ej(n,f);r=n|0;n=c[r>>2]|0;if((c[3922]|0)!=-1){c[m>>2]=15688;c[m+4>>2]=12;c[m+8>>2]=0;dW(15688,m,96)}m=(c[3923]|0)-1|0;s=c[n+8>>2]|0;do{if((c[n+12>>2]|0)-s>>2>>>0>m>>>0){t=c[s+(m<<2)>>2]|0;if((t|0)==0){break}u=t;v=c[r>>2]|0;dz(v)|0;c[g>>2]=0;v=d|0;L2969:do{if((j|0)==(k|0)){w=2508}else{x=e|0;y=t;z=t;A=t;B=b;C=p|0;D=q|0;E=o|0;F=j;G=0;L2971:while(1){H=G;while(1){if((H|0)!=0){w=2508;break L2969}I=c[v>>2]|0;do{if((I|0)==0){J=0}else{K=c[I+12>>2]|0;if((K|0)==(c[I+16>>2]|0)){L=b0[c[(c[I>>2]|0)+36>>2]&127](I)|0}else{L=c[K>>2]|0}if((L|0)!=-1){J=I;break}c[v>>2]=0;J=0}}while(0);I=(J|0)==0;K=c[x>>2]|0;do{if((K|0)==0){w=2460}else{M=c[K+12>>2]|0;if((M|0)==(c[K+16>>2]|0)){N=b0[c[(c[K>>2]|0)+36>>2]&127](K)|0}else{N=c[M>>2]|0}if((N|0)==-1){c[x>>2]=0;w=2460;break}else{if(I^(K|0)==0){O=K;break}else{w=2462;break L2971}}}}while(0);if((w|0)==2460){w=0;if(I){w=2462;break L2971}else{O=0}}if((b1[c[(c[y>>2]|0)+52>>2]&63](u,c[F>>2]|0,0)|0)<<24>>24==37){w=2465;break}if(b1[c[(c[z>>2]|0)+12>>2]&63](u,8192,c[F>>2]|0)|0){P=F;w=2475;break}Q=J+12|0;K=c[Q>>2]|0;R=J+16|0;if((K|0)==(c[R>>2]|0)){S=b0[c[(c[J>>2]|0)+36>>2]&127](J)|0}else{S=c[K>>2]|0}K=b_[c[(c[A>>2]|0)+28>>2]&31](u,S)|0;if((K|0)==(b_[c[(c[A>>2]|0)+28>>2]&31](u,c[F>>2]|0)|0)){w=2503;break}c[g>>2]=4;H=4}L3003:do{if((w|0)==2503){w=0;H=c[Q>>2]|0;if((H|0)==(c[R>>2]|0)){K=c[(c[J>>2]|0)+40>>2]|0;b0[K&127](J)|0}else{c[Q>>2]=H+4}T=F+4|0}else if((w|0)==2465){w=0;H=F+4|0;if((H|0)==(k|0)){w=2466;break L2971}K=b1[c[(c[y>>2]|0)+52>>2]&63](u,c[H>>2]|0,0)|0;switch(K<<24>>24){case 69:case 48:{M=F+8|0;if((M|0)==(k|0)){w=2469;break L2971}U=K;V=b1[c[(c[y>>2]|0)+52>>2]&63](u,c[M>>2]|0,0)|0;W=M;break};default:{U=0;V=K;W=H}}H=c[(c[B>>2]|0)+36>>2]|0;c[C>>2]=J;c[D>>2]=O;b7[H&7](o,b,p,q,f,g,h,V,U);c[v>>2]=c[E>>2];T=W+4|0}else if((w|0)==2475){while(1){w=0;H=P+4|0;if((H|0)==(k|0)){X=k;break}if(b1[c[(c[z>>2]|0)+12>>2]&63](u,8192,c[H>>2]|0)|0){P=H;w=2475}else{X=H;break}}I=J;H=O;while(1){do{if((I|0)==0){Y=0}else{K=c[I+12>>2]|0;if((K|0)==(c[I+16>>2]|0)){Z=b0[c[(c[I>>2]|0)+36>>2]&127](I)|0}else{Z=c[K>>2]|0}if((Z|0)!=-1){Y=I;break}c[v>>2]=0;Y=0}}while(0);K=(Y|0)==0;do{if((H|0)==0){w=2490}else{M=c[H+12>>2]|0;if((M|0)==(c[H+16>>2]|0)){_=b0[c[(c[H>>2]|0)+36>>2]&127](H)|0}else{_=c[M>>2]|0}if((_|0)==-1){c[x>>2]=0;w=2490;break}else{if(K^(H|0)==0){$=H;break}else{T=X;break L3003}}}}while(0);if((w|0)==2490){w=0;if(K){T=X;break L3003}else{$=0}}M=Y+12|0;aa=c[M>>2]|0;ab=Y+16|0;if((aa|0)==(c[ab>>2]|0)){ac=b0[c[(c[Y>>2]|0)+36>>2]&127](Y)|0}else{ac=c[aa>>2]|0}if(!(b1[c[(c[z>>2]|0)+12>>2]&63](u,8192,ac)|0)){T=X;break L3003}aa=c[M>>2]|0;if((aa|0)==(c[ab>>2]|0)){ab=c[(c[Y>>2]|0)+40>>2]|0;b0[ab&127](Y)|0;I=Y;H=$;continue}else{c[M>>2]=aa+4;I=Y;H=$;continue}}}}while(0);if((T|0)==(k|0)){w=2508;break L2969}F=T;G=c[g>>2]|0}if((w|0)==2462){c[g>>2]=4;ad=J;break}else if((w|0)==2466){c[g>>2]=4;ad=J;break}else if((w|0)==2469){c[g>>2]=4;ad=J;break}}}while(0);if((w|0)==2508){ad=c[v>>2]|0}u=d|0;do{if((ad|0)!=0){t=c[ad+12>>2]|0;if((t|0)==(c[ad+16>>2]|0)){ae=b0[c[(c[ad>>2]|0)+36>>2]&127](ad)|0}else{ae=c[t>>2]|0}if((ae|0)!=-1){break}c[u>>2]=0}}while(0);v=c[u>>2]|0;t=(v|0)==0;G=e|0;F=c[G>>2]|0;do{if((F|0)==0){w=2521}else{z=c[F+12>>2]|0;if((z|0)==(c[F+16>>2]|0)){af=b0[c[(c[F>>2]|0)+36>>2]&127](F)|0}else{af=c[z>>2]|0}if((af|0)==-1){c[G>>2]=0;w=2521;break}if(!(t^(F|0)==0)){break}ag=a|0;c[ag>>2]=v;i=l;return}}while(0);do{if((w|0)==2521){if(t){break}ag=a|0;c[ag>>2]=v;i=l;return}}while(0);c[g>>2]=c[g>>2]|2;ag=a|0;c[ag>>2]=v;i=l;return}}while(0);l=bO(4)|0;kq(l);bl(l|0,8176,134)}function gS(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;j=i;i=i+32|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;m=j+24|0;ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;n=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-n>>2>>>0>l>>>0){o=c[n+(l<<2)>>2]|0;if((o|0)==0){break}p=o;o=c[f>>2]|0;dz(o)|0;o=c[e>>2]|0;q=b+8|0;r=b0[c[c[q>>2]>>2]&127](q)|0;c[k>>2]=o;o=(fS(d,k,r,r+168|0,p,g,0)|0)-r|0;if((o|0)>=168){s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}c[h+24>>2]=((o|0)/12|0|0)%7|0;s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gT(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;j=i;i=i+32|0;k=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[k>>2];k=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[k>>2];k=j|0;l=j+8|0;m=j+24|0;ej(m,f);f=m|0;m=c[f>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;n=c[m+8>>2]|0;do{if((c[m+12>>2]|0)-n>>2>>>0>l>>>0){o=c[n+(l<<2)>>2]|0;if((o|0)==0){break}p=o;o=c[f>>2]|0;dz(o)|0;o=c[e>>2]|0;q=b+8|0;r=b0[c[(c[q>>2]|0)+4>>2]&127](q)|0;c[k>>2]=o;o=(fS(d,k,r,r+288|0,p,g,0)|0)-r|0;if((o|0)>=288){s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}c[h+16>>2]=((o|0)/12|0|0)%12|0;s=d|0;t=c[s>>2]|0;u=a|0;c[u>>2]=t;i=j;return}}while(0);j=bO(4)|0;kq(j);bl(j|0,8176,134)}function gU(a,b,d,e,f,g,h){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;b=i;i=i+32|0;j=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[j>>2];j=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[j>>2];j=b|0;k=b+8|0;l=b+24|0;ej(l,f);f=l|0;l=c[f>>2]|0;if((c[3922]|0)!=-1){c[k>>2]=15688;c[k+4>>2]=12;c[k+8>>2]=0;dW(15688,k,96)}k=(c[3923]|0)-1|0;m=c[l+8>>2]|0;do{if((c[l+12>>2]|0)-m>>2>>>0>k>>>0){n=c[m+(k<<2)>>2]|0;if((n|0)==0){break}o=n;n=c[f>>2]|0;dz(n)|0;c[j>>2]=c[e>>2];n=hc(d,j,g,o,4)|0;if((c[g>>2]&4|0)!=0){p=d|0;q=c[p>>2]|0;r=a|0;c[r>>2]=q;i=b;return}if((n|0)<69){s=n+2e3|0}else{s=(n-69|0)>>>0<31?n+1900|0:n}c[h+20>>2]=s-1900;p=d|0;q=c[p>>2]|0;r=a|0;c[r>>2]=q;i=b;return}}while(0);b=bO(4)|0;kq(b);bl(b|0,8176,134)}function gV(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0;a=i;g=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[g>>2];g=b|0;b=d|0;d=f;L3127:while(1){h=c[g>>2]|0;do{if((h|0)==0){j=1}else{k=c[h+12>>2]|0;if((k|0)==(c[h+16>>2]|0)){l=b0[c[(c[h>>2]|0)+36>>2]&127](h)|0}else{l=c[k>>2]|0}if((l|0)==-1){c[g>>2]=0;j=1;break}else{j=(c[g>>2]|0)==0;break}}}while(0);h=c[b>>2]|0;do{if((h|0)==0){m=2581}else{k=c[h+12>>2]|0;if((k|0)==(c[h+16>>2]|0)){n=b0[c[(c[h>>2]|0)+36>>2]&127](h)|0}else{n=c[k>>2]|0}if((n|0)==-1){c[b>>2]=0;m=2581;break}else{k=(h|0)==0;if(j^k){o=h;p=k;break}else{q=h;r=k;break L3127}}}}while(0);if((m|0)==2581){m=0;if(j){q=0;r=1;break}else{o=0;p=1}}h=c[g>>2]|0;k=c[h+12>>2]|0;if((k|0)==(c[h+16>>2]|0)){s=b0[c[(c[h>>2]|0)+36>>2]&127](h)|0}else{s=c[k>>2]|0}if(!(b1[c[(c[d>>2]|0)+12>>2]&63](f,8192,s)|0)){q=o;r=p;break}k=c[g>>2]|0;h=k+12|0;t=c[h>>2]|0;if((t|0)==(c[k+16>>2]|0)){u=c[(c[k>>2]|0)+40>>2]|0;b0[u&127](k)|0;continue}else{c[h>>2]=t+4;continue}}p=c[g>>2]|0;do{if((p|0)==0){v=1}else{o=c[p+12>>2]|0;if((o|0)==(c[p+16>>2]|0)){w=b0[c[(c[p>>2]|0)+36>>2]&127](p)|0}else{w=c[o>>2]|0}if((w|0)==-1){c[g>>2]=0;v=1;break}else{v=(c[g>>2]|0)==0;break}}}while(0);do{if(r){m=2603}else{g=c[q+12>>2]|0;if((g|0)==(c[q+16>>2]|0)){x=b0[c[(c[q>>2]|0)+36>>2]&127](q)|0}else{x=c[g>>2]|0}if((x|0)==-1){c[b>>2]=0;m=2603;break}if(!(v^(q|0)==0)){break}i=a;return}}while(0);do{if((m|0)==2603){if(v){break}i=a;return}}while(0);c[e>>2]=c[e>>2]|2;i=a;return}function gW(b,d,e,f,g,h,j,k,l){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0;l=i;i=i+328|0;m=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[m>>2];m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=l|0;n=l+8|0;o=l+16|0;p=l+24|0;q=l+32|0;r=l+40|0;s=l+48|0;t=l+56|0;u=l+64|0;v=l+72|0;w=l+80|0;x=l+88|0;y=l+96|0;z=l+112|0;A=l+120|0;B=l+128|0;C=l+136|0;D=l+144|0;E=l+152|0;F=l+160|0;G=l+168|0;H=l+176|0;I=l+184|0;J=l+192|0;K=l+200|0;L=l+208|0;M=l+216|0;N=l+224|0;O=l+232|0;P=l+240|0;Q=l+248|0;R=l+256|0;S=l+264|0;T=l+272|0;U=l+280|0;V=l+288|0;W=l+296|0;X=l+304|0;Y=l+312|0;Z=l+320|0;c[h>>2]=0;ej(z,g);_=z|0;z=c[_>>2]|0;if((c[3922]|0)!=-1){c[y>>2]=15688;c[y+4>>2]=12;c[y+8>>2]=0;dW(15688,y,96)}y=(c[3923]|0)-1|0;$=c[z+8>>2]|0;do{if((c[z+12>>2]|0)-$>>2>>>0>y>>>0){aa=c[$+(y<<2)>>2]|0;if((aa|0)==0){break}ab=aa;aa=c[_>>2]|0;dz(aa)|0;L3192:do{switch(k<<24>>24|0){case 100:case 101:{aa=j+12|0;c[v>>2]=c[f>>2];ac=hc(e,v,h,ab,2)|0;ad=c[h>>2]|0;do{if((ad&4|0)==0){if((ac-1|0)>>>0>=31){break}c[aa>>2]=ac;break L3192}}while(0);c[h>>2]=ad|4;break};case 110:case 116:{c[J>>2]=c[f>>2];gV(0,e,J,h,ab);break};case 88:{ac=d+8|0;aa=b0[c[(c[ac>>2]|0)+24>>2]&127](ac)|0;ac=e|0;c[X>>2]=c[ac>>2];c[Y>>2]=c[f>>2];ae=a[aa]|0;if((ae&1)==0){af=aa+4|0;ag=aa+4|0}else{ah=c[aa+8>>2]|0;af=ah;ag=ah}ah=ae&255;if((ah&1|0)==0){ai=ah>>>1}else{ai=c[aa+4>>2]|0}gR(W,d,X,Y,g,h,j,ag,af+(ai<<2)|0);c[ac>>2]=c[W>>2];break};case 98:case 66:case 104:{ac=c[f>>2]|0;aa=d+8|0;ah=b0[c[(c[aa>>2]|0)+4>>2]&127](aa)|0;c[w>>2]=ac;ac=(fS(e,w,ah,ah+288|0,ab,h,0)|0)-ah|0;if((ac|0)>=288){break L3192}c[j+16>>2]=((ac|0)/12|0|0)%12|0;break};case 112:{c[K>>2]=c[f>>2];gX(d,j+8|0,e,K,h,ab);break};case 114:{ac=e|0;c[M>>2]=c[ac>>2];c[N>>2]=c[f>>2];gR(L,d,M,N,g,h,j,2216,2260);c[ac>>2]=c[L>>2];break};case 37:{c[Z>>2]=c[f>>2];gY(0,e,Z,h,ab);break};case 68:{ac=e|0;c[E>>2]=c[ac>>2];c[F>>2]=c[f>>2];gR(D,d,E,F,g,h,j,2264,2296);c[ac>>2]=c[D>>2];break};case 89:{c[m>>2]=c[f>>2];ac=hc(e,m,h,ab,4)|0;if((c[h>>2]&4|0)!=0){break L3192}c[j+20>>2]=ac-1900;break};case 82:{ac=e|0;c[P>>2]=c[ac>>2];c[Q>>2]=c[f>>2];gR(O,d,P,Q,g,h,j,2192,2212);c[ac>>2]=c[O>>2];break};case 83:{c[p>>2]=c[f>>2];ac=hc(e,p,h,ab,2)|0;ah=c[h>>2]|0;if((ah&4|0)==0&(ac|0)<61){c[j>>2]=ac;break L3192}else{c[h>>2]=ah|4;break L3192}break};case 106:{c[s>>2]=c[f>>2];ah=hc(e,s,h,ab,3)|0;ac=c[h>>2]|0;if((ac&4|0)==0&(ah|0)<366){c[j+28>>2]=ah;break L3192}else{c[h>>2]=ac|4;break L3192}break};case 97:case 65:{ac=c[f>>2]|0;ah=d+8|0;aa=b0[c[c[ah>>2]>>2]&127](ah)|0;c[x>>2]=ac;ac=(fS(e,x,aa,aa+168|0,ab,h,0)|0)-aa|0;if((ac|0)>=168){break L3192}c[j+24>>2]=((ac|0)/12|0|0)%7|0;break};case 99:{ac=d+8|0;aa=b0[c[(c[ac>>2]|0)+12>>2]&127](ac)|0;ac=e|0;c[B>>2]=c[ac>>2];c[C>>2]=c[f>>2];ah=a[aa]|0;if((ah&1)==0){aj=aa+4|0;ak=aa+4|0}else{ae=c[aa+8>>2]|0;aj=ae;ak=ae}ae=ah&255;if((ae&1|0)==0){al=ae>>>1}else{al=c[aa+4>>2]|0}gR(A,d,B,C,g,h,j,ak,aj+(al<<2)|0);c[ac>>2]=c[A>>2];break};case 84:{ac=e|0;c[S>>2]=c[ac>>2];c[T>>2]=c[f>>2];gR(R,d,S,T,g,h,j,2160,2192);c[ac>>2]=c[R>>2];break};case 70:{ac=e|0;c[H>>2]=c[ac>>2];c[I>>2]=c[f>>2];gR(G,d,H,I,g,h,j,2128,2160);c[ac>>2]=c[G>>2];break};case 72:{c[u>>2]=c[f>>2];ac=hc(e,u,h,ab,2)|0;aa=c[h>>2]|0;if((aa&4|0)==0&(ac|0)<24){c[j+8>>2]=ac;break L3192}else{c[h>>2]=aa|4;break L3192}break};case 121:{c[n>>2]=c[f>>2];aa=hc(e,n,h,ab,4)|0;if((c[h>>2]&4|0)!=0){break L3192}if((aa|0)<69){am=aa+2e3|0}else{am=(aa-69|0)>>>0<31?aa+1900|0:aa}c[j+20>>2]=am-1900;break};case 109:{c[r>>2]=c[f>>2];aa=(hc(e,r,h,ab,2)|0)-1|0;ac=c[h>>2]|0;if((ac&4|0)==0&(aa|0)<12){c[j+16>>2]=aa;break L3192}else{c[h>>2]=ac|4;break L3192}break};case 73:{ac=j+8|0;c[t>>2]=c[f>>2];aa=hc(e,t,h,ab,2)|0;ae=c[h>>2]|0;do{if((ae&4|0)==0){if((aa-1|0)>>>0>=12){break}c[ac>>2]=aa;break L3192}}while(0);c[h>>2]=ae|4;break};case 77:{c[q>>2]=c[f>>2];aa=hc(e,q,h,ab,2)|0;ac=c[h>>2]|0;if((ac&4|0)==0&(aa|0)<60){c[j+4>>2]=aa;break L3192}else{c[h>>2]=ac|4;break L3192}break};case 119:{c[o>>2]=c[f>>2];ac=hc(e,o,h,ab,1)|0;aa=c[h>>2]|0;if((aa&4|0)==0&(ac|0)<7){c[j+24>>2]=ac;break L3192}else{c[h>>2]=aa|4;break L3192}break};case 120:{aa=c[(c[d>>2]|0)+20>>2]|0;c[U>>2]=c[e>>2];c[V>>2]=c[f>>2];bX[aa&127](b,d,U,V,g,h,j);i=l;return};default:{c[h>>2]=c[h>>2]|4}}}while(0);c[b>>2]=c[e>>2];i=l;return}}while(0);l=bO(4)|0;kq(l);bl(l|0,8176,134)}function gX(a,b,e,f,g,h){a=a|0;b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0;j=i;i=i+8|0;k=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[k>>2];k=j|0;l=a+8|0;a=b0[c[(c[l>>2]|0)+8>>2]&127](l)|0;l=d[a]|0;if((l&1|0)==0){m=l>>>1}else{m=c[a+4>>2]|0}l=d[a+12|0]|0;if((l&1|0)==0){n=l>>>1}else{n=c[a+16>>2]|0}if((m|0)==(-n|0)){c[g>>2]=c[g>>2]|4;i=j;return}c[k>>2]=c[f>>2];f=fS(e,k,a,a+24|0,h,g,0)|0;g=f-a|0;do{if((f|0)==(a|0)){if((c[b>>2]|0)!=12){break}c[b>>2]=0;i=j;return}}while(0);if((g|0)!=12){i=j;return}g=c[b>>2]|0;if((g|0)>=12){i=j;return}c[b>>2]=g+12;i=j;return}
function gY(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0;a=i;g=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[g>>2];g=b|0;b=c[g>>2]|0;do{if((b|0)==0){h=1}else{j=c[b+12>>2]|0;if((j|0)==(c[b+16>>2]|0)){k=b0[c[(c[b>>2]|0)+36>>2]&127](b)|0}else{k=c[j>>2]|0}if((k|0)==-1){c[g>>2]=0;h=1;break}else{h=(c[g>>2]|0)==0;break}}}while(0);k=d|0;d=c[k>>2]|0;do{if((d|0)==0){l=2716}else{b=c[d+12>>2]|0;if((b|0)==(c[d+16>>2]|0)){m=b0[c[(c[d>>2]|0)+36>>2]&127](d)|0}else{m=c[b>>2]|0}if((m|0)==-1){c[k>>2]=0;l=2716;break}else{b=(d|0)==0;if(h^b){n=d;o=b;break}else{l=2718;break}}}}while(0);if((l|0)==2716){if(h){l=2718}else{n=0;o=1}}if((l|0)==2718){c[e>>2]=c[e>>2]|6;i=a;return}h=c[g>>2]|0;d=c[h+12>>2]|0;if((d|0)==(c[h+16>>2]|0)){p=b0[c[(c[h>>2]|0)+36>>2]&127](h)|0}else{p=c[d>>2]|0}if((b1[c[(c[f>>2]|0)+52>>2]&63](f,p,0)|0)<<24>>24!=37){c[e>>2]=c[e>>2]|4;i=a;return}p=c[g>>2]|0;f=p+12|0;d=c[f>>2]|0;if((d|0)==(c[p+16>>2]|0)){h=c[(c[p>>2]|0)+40>>2]|0;b0[h&127](p)|0}else{c[f>>2]=d+4}d=c[g>>2]|0;do{if((d|0)==0){q=1}else{f=c[d+12>>2]|0;if((f|0)==(c[d+16>>2]|0)){r=b0[c[(c[d>>2]|0)+36>>2]&127](d)|0}else{r=c[f>>2]|0}if((r|0)==-1){c[g>>2]=0;q=1;break}else{q=(c[g>>2]|0)==0;break}}}while(0);do{if(o){l=2740}else{g=c[n+12>>2]|0;if((g|0)==(c[n+16>>2]|0)){s=b0[c[(c[n>>2]|0)+36>>2]&127](n)|0}else{s=c[g>>2]|0}if((s|0)==-1){c[k>>2]=0;l=2740;break}if(!(q^(n|0)==0)){break}i=a;return}}while(0);do{if((l|0)==2740){if(q){break}i=a;return}}while(0);c[e>>2]=c[e>>2]|2;i=a;return}function gZ(a){a=a|0;return 127}function g_(a){a=a|0;return 127}function g$(a){a=a|0;return 0}function g0(a){a=a|0;return 127}function g1(a){a=a|0;return 127}function g2(a){a=a|0;return 0}function g3(a){a=a|0;return 2147483647}function g4(a){a=a|0;return 2147483647}function g5(a){a=a|0;return 0}function g6(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function g7(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function g8(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function g9(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function ha(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function hb(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function hc(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0;g=i;h=b;b=i;i=i+4|0;i=i+7>>3<<3;c[b>>2]=c[h>>2];h=a|0;a=c[h>>2]|0;do{if((a|0)==0){j=1}else{k=c[a+12>>2]|0;if((k|0)==(c[a+16>>2]|0)){l=b0[c[(c[a>>2]|0)+36>>2]&127](a)|0}else{l=c[k>>2]|0}if((l|0)==-1){c[h>>2]=0;j=1;break}else{j=(c[h>>2]|0)==0;break}}}while(0);l=b|0;b=c[l>>2]|0;do{if((b|0)==0){m=29}else{a=c[b+12>>2]|0;if((a|0)==(c[b+16>>2]|0)){n=b0[c[(c[b>>2]|0)+36>>2]&127](b)|0}else{n=c[a>>2]|0}if((n|0)==-1){c[l>>2]=0;m=29;break}else{if(j^(b|0)==0){o=b;break}else{m=31;break}}}}while(0);if((m|0)==29){if(j){m=31}else{o=0}}if((m|0)==31){c[d>>2]=c[d>>2]|6;p=0;i=g;return p|0}j=c[h>>2]|0;b=c[j+12>>2]|0;if((b|0)==(c[j+16>>2]|0)){q=b0[c[(c[j>>2]|0)+36>>2]&127](j)|0}else{q=c[b>>2]|0}b=e;if(!(b1[c[(c[b>>2]|0)+12>>2]&63](e,2048,q)|0)){c[d>>2]=c[d>>2]|4;p=0;i=g;return p|0}j=e;n=(b1[c[(c[j>>2]|0)+52>>2]&63](e,q,0)|0)<<24>>24;q=c[h>>2]|0;a=q+12|0;k=c[a>>2]|0;if((k|0)==(c[q+16>>2]|0)){r=c[(c[q>>2]|0)+40>>2]|0;b0[r&127](q)|0;s=n;t=f;u=o}else{c[a>>2]=k+4;s=n;t=f;u=o}while(1){v=s-48|0;o=t-1|0;f=c[h>>2]|0;do{if((f|0)==0){w=0}else{n=c[f+12>>2]|0;if((n|0)==(c[f+16>>2]|0)){x=b0[c[(c[f>>2]|0)+36>>2]&127](f)|0}else{x=c[n>>2]|0}if((x|0)==-1){c[h>>2]=0;w=0;break}else{w=c[h>>2]|0;break}}}while(0);f=(w|0)==0;if((u|0)==0){y=w;z=0}else{n=c[u+12>>2]|0;if((n|0)==(c[u+16>>2]|0)){A=b0[c[(c[u>>2]|0)+36>>2]&127](u)|0}else{A=c[n>>2]|0}if((A|0)==-1){c[l>>2]=0;B=0}else{B=u}y=c[h>>2]|0;z=B}C=(z|0)==0;if(!((f^C)&(o|0)>0)){break}f=c[y+12>>2]|0;if((f|0)==(c[y+16>>2]|0)){D=b0[c[(c[y>>2]|0)+36>>2]&127](y)|0}else{D=c[f>>2]|0}if(!(b1[c[(c[b>>2]|0)+12>>2]&63](e,2048,D)|0)){p=v;m=81;break}f=((b1[c[(c[j>>2]|0)+52>>2]&63](e,D,0)|0)<<24>>24)+(v*10|0)|0;n=c[h>>2]|0;k=n+12|0;a=c[k>>2]|0;if((a|0)==(c[n+16>>2]|0)){q=c[(c[n>>2]|0)+40>>2]|0;b0[q&127](n)|0;s=f;t=o;u=z;continue}else{c[k>>2]=a+4;s=f;t=o;u=z;continue}}if((m|0)==81){i=g;return p|0}do{if((y|0)==0){E=1}else{u=c[y+12>>2]|0;if((u|0)==(c[y+16>>2]|0)){F=b0[c[(c[y>>2]|0)+36>>2]&127](y)|0}else{F=c[u>>2]|0}if((F|0)==-1){c[h>>2]=0;E=1;break}else{E=(c[h>>2]|0)==0;break}}}while(0);do{if(C){m=75}else{h=c[z+12>>2]|0;if((h|0)==(c[z+16>>2]|0)){G=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{G=c[h>>2]|0}if((G|0)==-1){c[l>>2]=0;m=75;break}if(E^(z|0)==0){p=v}else{break}i=g;return p|0}}while(0);do{if((m|0)==75){if(E){break}else{p=v}i=g;return p|0}}while(0);c[d>>2]=c[d>>2]|2;p=v;i=g;return p|0}function hd(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;g=i;i=i+112|0;f=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[f>>2];f=g|0;l=g+8|0;m=l|0;n=f|0;a[n]=37;o=f+1|0;a[o]=j;p=f+2|0;a[p]=k;a[f+3|0]=0;if(k<<24>>24!=0){a[o]=k;a[p]=j}j=bk(m|0,100,n|0,h|0,c[d+8>>2]|0)|0;d=l+j|0;l=c[e>>2]|0;if((j|0)==0){q=l;r=b|0;c[r>>2]=q;i=g;return}else{s=l;t=m}while(1){m=a[t]|0;if((s|0)==0){u=0}else{l=s+24|0;j=c[l>>2]|0;if((j|0)==(c[s+28>>2]|0)){v=b_[c[(c[s>>2]|0)+52>>2]&31](s,m&255)|0}else{c[l>>2]=j+1;a[j]=m;v=m&255}u=(v|0)==-1?0:s}m=t+1|0;if((m|0)==(d|0)){q=u;break}else{s=u;t=m}}r=b|0;c[r>>2]=q;i=g;return}function he(a,b,d,e,f,g,h,j){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;f=i;i=i+408|0;e=d;d=i;i=i+4|0;i=i+7>>3<<3;c[d>>2]=c[e>>2];e=f|0;k=f+400|0;l=e|0;c[k>>2]=e+400;hC(b+8|0,l,k,g,h,j);j=c[k>>2]|0;k=c[d>>2]|0;if((l|0)==(j|0)){m=k;n=a|0;c[n>>2]=m;i=f;return}else{o=k;p=l}while(1){l=c[p>>2]|0;if((o|0)==0){q=0}else{k=o+24|0;d=c[k>>2]|0;if((d|0)==(c[o+28>>2]|0)){r=b_[c[(c[o>>2]|0)+52>>2]&31](o,l)|0}else{c[k>>2]=d+4;c[d>>2]=l;r=l}q=(r|0)==-1?0:o}l=p+4|0;if((l|0)==(j|0)){m=q;break}else{o=q;p=l}}n=a|0;c[n>>2]=m;i=f;return}function hf(a){a=a|0;de(a|0);kS(a);return}function hg(a){a=a|0;de(a|0);return}function hh(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hi(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hj(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hk(a,b){a=a|0;b=b|0;d_(a,1,45);return}function hl(a){a=a|0;de(a|0);kS(a);return}function hm(a){a=a|0;de(a|0);return}function hn(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function ho(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hp(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hq(a,b){a=a|0;b=b|0;d_(a,1,45);return}function hr(a){a=a|0;de(a|0);kS(a);return}function hs(a){a=a|0;de(a|0);return}function ht(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hu(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hv(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hw(a,b){a=a|0;b=b|0;ey(a,1,45);return}function hx(a){a=a|0;de(a|0);kS(a);return}function hy(b){b=b|0;var d=0,e=0,f=0,g=0;d=b;e=b+8|0;f=c[e>>2]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((f|0)==(c[2958]|0)){g=b|0;de(g);kS(d);return}a7(c[e>>2]|0);g=b|0;de(g);kS(d);return}function hz(b){b=b|0;var d=0,e=0,f=0;d=b+8|0;e=c[d>>2]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((e|0)==(c[2958]|0)){f=b|0;de(f);return}a7(c[d>>2]|0);f=b|0;de(f);return}function hA(b){b=b|0;var d=0,e=0,f=0,g=0;d=b;e=b+8|0;f=c[e>>2]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((f|0)==(c[2958]|0)){g=b|0;de(g);kS(d);return}a7(c[e>>2]|0);g=b|0;de(g);kS(d);return}function hB(b){b=b|0;var d=0,e=0,f=0;d=b+8|0;e=c[d>>2]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((e|0)==(c[2958]|0)){f=b|0;de(f);return}a7(c[d>>2]|0);f=b|0;de(f);return}function hC(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0;j=i;i=i+120|0;k=j|0;l=j+112|0;m=i;i=i+4|0;i=i+7>>3<<3;n=j+8|0;o=k|0;a[o]=37;p=k+1|0;a[p]=g;q=k+2|0;a[q]=h;a[k+3|0]=0;if(h<<24>>24!=0){a[p]=h;a[q]=g}g=b|0;bk(n|0,100,o|0,f|0,c[g>>2]|0)|0;c[l>>2]=0;c[l+4>>2]=0;c[m>>2]=n;n=(c[e>>2]|0)-d>>2;f=bB(c[g>>2]|0)|0;g=ke(d,m,n,l)|0;if((f|0)!=0){bB(f|0)|0}if((g|0)==-1){hV(904)}else{c[e>>2]=d+(g<<2);i=j;return}}function hD(a){a=a|0;return 2147483647}function hE(a){a=a|0;return 2147483647}function hF(a){a=a|0;return 0}function hG(a){a=a|0;return}function hH(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function hI(b,c){b=b|0;c=c|0;c=b;C=67109634;a[c]=C&255;C=C>>8;a[c+1|0]=C&255;C=C>>8;a[c+2|0]=C&255;C=C>>8;a[c+3|0]=C&255;return}function hJ(a){a=a|0;de(a|0);return}function hK(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hL(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hM(a,b){a=a|0;b=b|0;k$(a|0,0,12);return}function hN(a,b){a=a|0;b=b|0;ey(a,1,45);return}function hO(a){a=a|0;de(a|0);kS(a);return}function hP(a){a=a|0;de(a|0);return}function hQ(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0;d=i;i=i+280|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=d|0;m=d+16|0;n=d+120|0;o=d+128|0;p=d+136|0;q=d+144|0;r=d+152|0;s=d+160|0;t=d+176|0;u=n|0;c[u>>2]=m;v=n+4|0;c[v>>2]=166;w=m+100|0;ej(p,h);m=p|0;x=c[m>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;y=c[x+8>>2]|0;do{if((c[x+12>>2]|0)-y>>2>>>0>l>>>0){z=c[y+(l<<2)>>2]|0;if((z|0)==0){break}A=z;a[q]=0;B=f|0;c[r>>2]=c[B>>2];do{if(hR(e,r,g,p,c[h+4>>2]|0,j,q,A,n,o,w)|0){C=s|0;D=c[(c[z>>2]|0)+32>>2]|0;ca[D&15](A,2112,2122,C)|0;D=t|0;E=c[o>>2]|0;F=c[u>>2]|0;G=E-F|0;do{if((G|0)>98){H=kJ(G+2|0)|0;if((H|0)!=0){I=H;J=H;break}kX();I=0;J=0}else{I=D;J=0}}while(0);if((a[q]&1)==0){K=I}else{a[I]=45;K=I+1|0}if(F>>>0<E>>>0){G=s+10|0;H=s;L=K;M=F;while(1){N=C;while(1){if((N|0)==(G|0)){O=G;break}if((a[N]|0)==(a[M]|0)){O=N;break}else{N=N+1|0}}a[L]=a[2112+(O-H)|0]|0;N=M+1|0;P=L+1|0;if(N>>>0<(c[o>>2]|0)>>>0){L=P;M=N}else{Q=P;break}}}else{Q=K}a[Q]=0;M=bD(D|0,1408,(L=i,i=i+8|0,c[L>>2]=k,L)|0)|0;i=L;if((M|0)==1){if((J|0)==0){break}kK(J);break}M=bO(8)|0;dE(M,1368);bl(M|0,8192,22)}}while(0);A=e|0;z=c[A>>2]|0;do{if((z|0)==0){R=0}else{if((c[z+12>>2]|0)!=(c[z+16>>2]|0)){R=z;break}if((b0[c[(c[z>>2]|0)+36>>2]&127](z)|0)!=-1){R=z;break}c[A>>2]=0;R=0}}while(0);A=(R|0)==0;z=c[B>>2]|0;do{if((z|0)==0){S=244}else{if((c[z+12>>2]|0)!=(c[z+16>>2]|0)){if(A){break}else{S=246;break}}if((b0[c[(c[z>>2]|0)+36>>2]&127](z)|0)==-1){c[B>>2]=0;S=244;break}else{if(A^(z|0)==0){break}else{S=246;break}}}}while(0);if((S|0)==244){if(A){S=246}}if((S|0)==246){c[j>>2]=c[j>>2]|2}c[b>>2]=R;z=c[m>>2]|0;dz(z)|0;z=c[u>>2]|0;c[u>>2]=0;if((z|0)==0){i=d;return}bY[c[v>>2]&511](z);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function hR(e,f,g,h,j,k,l,m,n,o,p){e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;p=p|0;var q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0,az=0,aA=0,aB=0,aC=0,aD=0,aE=0,aF=0,aG=0,aH=0,aI=0,aJ=0,aK=0,aL=0,aM=0,aN=0,aO=0,aP=0,aQ=0,aR=0,aS=0,aT=0,aU=0,aV=0,aW=0,aX=0,aY=0,aZ=0,a_=0,a$=0,a0=0,a1=0,a2=0,a3=0,a4=0,a5=0,a6=0,a7=0,a8=0,a9=0,ba=0,bb=0,bc=0,bd=0,be=0,bf=0,bg=0,bh=0,bi=0,bj=0,bk=0,bl=0,bm=0,bn=0,bo=0,bp=0,bq=0,br=0,bs=0,bt=0,bu=0,bv=0,bw=0,bx=0,by=0,bz=0,bA=0,bB=0,bC=0,bD=0,bE=0;q=i;i=i+440|0;r=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[r>>2];r=q|0;s=q+400|0;t=q+408|0;u=q+416|0;v=q+424|0;w=v;x=i;i=i+12|0;i=i+7>>3<<3;y=i;i=i+12|0;i=i+7>>3<<3;z=i;i=i+12|0;i=i+7>>3<<3;A=i;i=i+12|0;i=i+7>>3<<3;B=i;i=i+4|0;i=i+7>>3<<3;C=i;i=i+4|0;i=i+7>>3<<3;D=r|0;k$(w|0,0,12);E=x;F=y;G=z;H=A;k$(E|0,0,12);k$(F|0,0,12);k$(G|0,0,12);k$(H|0,0,12);hX(g,h,s,t,u,v,x,y,z,B);h=n|0;c[o>>2]=c[h>>2];g=e|0;e=f|0;f=m+8|0;m=z+1|0;I=z+4|0;J=z+8|0;K=y+1|0;L=y+4|0;M=y+8|0;N=(j&512|0)!=0;j=x+1|0;O=x+4|0;P=x+8|0;Q=A+1|0;R=A+4|0;S=A+8|0;T=s+3|0;U=n+4|0;n=v+4|0;V=p;p=166;W=D;X=D;D=r+400|0;r=0;Y=0;L286:while(1){Z=c[g>>2]|0;do{if((Z|0)==0){_=0}else{if((c[Z+12>>2]|0)!=(c[Z+16>>2]|0)){_=Z;break}if((b0[c[(c[Z>>2]|0)+36>>2]&127](Z)|0)==-1){c[g>>2]=0;_=0;break}else{_=c[g>>2]|0;break}}}while(0);Z=(_|0)==0;$=c[e>>2]|0;do{if(($|0)==0){aa=271}else{if((c[$+12>>2]|0)!=(c[$+16>>2]|0)){if(Z){ab=$;break}else{ac=p;ad=W;ae=X;af=r;aa=523;break L286}}if((b0[c[(c[$>>2]|0)+36>>2]&127]($)|0)==-1){c[e>>2]=0;aa=271;break}else{if(Z){ab=$;break}else{ac=p;ad=W;ae=X;af=r;aa=523;break L286}}}}while(0);if((aa|0)==271){aa=0;if(Z){ac=p;ad=W;ae=X;af=r;aa=523;break}else{ab=0}}L308:do{switch(a[s+Y|0]|0){case 1:{if((Y|0)==3){ac=p;ad=W;ae=X;af=r;aa=523;break L286}$=c[g>>2]|0;ag=c[$+12>>2]|0;if((ag|0)==(c[$+16>>2]|0)){ah=(b0[c[(c[$>>2]|0)+36>>2]&127]($)|0)&255}else{ah=a[ag]|0}if(ah<<24>>24<=-1){aa=296;break L286}if((b[(c[f>>2]|0)+(ah<<24>>24<<1)>>1]&8192)==0){aa=296;break L286}ag=c[g>>2]|0;$=ag+12|0;ai=c[$>>2]|0;if((ai|0)==(c[ag+16>>2]|0)){aj=(b0[c[(c[ag>>2]|0)+40>>2]&127](ag)|0)&255}else{c[$>>2]=ai+1;aj=a[ai]|0}dU(A,aj);aa=297;break};case 3:{ai=a[F]|0;$=ai&255;ag=($&1|0)==0?$>>>1:c[L>>2]|0;$=a[G]|0;ak=$&255;al=(ak&1|0)==0?ak>>>1:c[I>>2]|0;if((ag|0)==(-al|0)){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}ak=(ag|0)==0;ag=c[g>>2]|0;as=c[ag+12>>2]|0;at=c[ag+16>>2]|0;au=(as|0)==(at|0);if(!(ak|(al|0)==0)){if(au){al=(b0[c[(c[ag>>2]|0)+36>>2]&127](ag)|0)&255;av=c[g>>2]|0;aw=al;ax=a[F]|0;ay=av;az=c[av+12>>2]|0;aA=c[av+16>>2]|0}else{aw=a[as]|0;ax=ai;ay=ag;az=as;aA=at}at=ay+12|0;av=(az|0)==(aA|0);if(aw<<24>>24==(a[(ax&1)==0?K:c[M>>2]|0]|0)){if(av){al=c[(c[ay>>2]|0)+40>>2]|0;b0[al&127](ay)|0}else{c[at>>2]=az+1}at=d[F]|0;am=((at&1|0)==0?at>>>1:c[L>>2]|0)>>>0>1?y:r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}if(av){aB=(b0[c[(c[ay>>2]|0)+36>>2]&127](ay)|0)&255}else{aB=a[az]|0}if(aB<<24>>24!=(a[(a[G]&1)==0?m:c[J>>2]|0]|0)){aa=363;break L286}av=c[g>>2]|0;at=av+12|0;al=c[at>>2]|0;if((al|0)==(c[av+16>>2]|0)){aC=c[(c[av>>2]|0)+40>>2]|0;b0[aC&127](av)|0}else{c[at>>2]=al+1}a[l]=1;al=d[G]|0;am=((al&1|0)==0?al>>>1:c[I>>2]|0)>>>0>1?z:r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}if(ak){if(au){ak=(b0[c[(c[ag>>2]|0)+36>>2]&127](ag)|0)&255;aD=ak;aE=a[G]|0}else{aD=a[as]|0;aE=$}if(aD<<24>>24!=(a[(aE&1)==0?m:c[J>>2]|0]|0)){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}$=c[g>>2]|0;ak=$+12|0;al=c[ak>>2]|0;if((al|0)==(c[$+16>>2]|0)){at=c[(c[$>>2]|0)+40>>2]|0;b0[at&127]($)|0}else{c[ak>>2]=al+1}a[l]=1;al=d[G]|0;am=((al&1|0)==0?al>>>1:c[I>>2]|0)>>>0>1?z:r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}if(au){au=(b0[c[(c[ag>>2]|0)+36>>2]&127](ag)|0)&255;aF=au;aG=a[F]|0}else{aF=a[as]|0;aG=ai}if(aF<<24>>24!=(a[(aG&1)==0?K:c[M>>2]|0]|0)){a[l]=1;am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}ai=c[g>>2]|0;as=ai+12|0;au=c[as>>2]|0;if((au|0)==(c[ai+16>>2]|0)){ag=c[(c[ai>>2]|0)+40>>2]|0;b0[ag&127](ai)|0}else{c[as>>2]=au+1}au=d[F]|0;am=((au&1|0)==0?au>>>1:c[L>>2]|0)>>>0>1?y:r;an=D;ao=X;ap=W;aq=p;ar=V;break};case 0:{aa=297;break};case 2:{if(!((r|0)!=0|Y>>>0<2)){if((Y|0)==2){aH=(a[T]|0)!=0}else{aH=0}if(!(N|aH)){am=0;an=D;ao=X;ap=W;aq=p;ar=V;break L308}}au=a[E]|0;as=(au&1)==0?j:c[P>>2]|0;L382:do{if((Y|0)==0){aI=as}else{if((d[s+(Y-1)|0]|0)>=2){aI=as;break}ai=au&255;ag=as+((ai&1|0)==0?ai>>>1:c[O>>2]|0)|0;ai=as;while(1){if((ai|0)==(ag|0)){aJ=ag;break}al=a[ai]|0;if(al<<24>>24<=-1){aJ=ai;break}if((b[(c[f>>2]|0)+(al<<24>>24<<1)>>1]&8192)==0){aJ=ai;break}else{ai=ai+1|0}}ai=aJ-as|0;ag=a[H]|0;al=ag&255;ak=(al&1|0)==0?al>>>1:c[R>>2]|0;if(ai>>>0>ak>>>0){aI=as;break}al=(ag&1)==0?Q:c[S>>2]|0;ag=al+ak|0;if((aJ|0)==(as|0)){aI=as;break}$=as;at=al+(ak-ai)|0;while(1){if((a[at]|0)!=(a[$]|0)){aI=as;break L382}ai=at+1|0;if((ai|0)==(ag|0)){aI=aJ;break}else{$=$+1|0;at=ai}}}}while(0);at=au&255;L396:do{if((aI|0)==(as+((at&1|0)==0?at>>>1:c[O>>2]|0)|0)){aK=aI}else{$=ab;ag=aI;while(1){ai=c[g>>2]|0;do{if((ai|0)==0){aL=0}else{if((c[ai+12>>2]|0)!=(c[ai+16>>2]|0)){aL=ai;break}if((b0[c[(c[ai>>2]|0)+36>>2]&127](ai)|0)==-1){c[g>>2]=0;aL=0;break}else{aL=c[g>>2]|0;break}}}while(0);ai=(aL|0)==0;do{if(($|0)==0){aa=392}else{if((c[$+12>>2]|0)!=(c[$+16>>2]|0)){if(ai){aM=$;break}else{aK=ag;break L396}}if((b0[c[(c[$>>2]|0)+36>>2]&127]($)|0)==-1){c[e>>2]=0;aa=392;break}else{if(ai){aM=$;break}else{aK=ag;break L396}}}}while(0);if((aa|0)==392){aa=0;if(ai){aK=ag;break L396}else{aM=0}}ak=c[g>>2]|0;al=c[ak+12>>2]|0;if((al|0)==(c[ak+16>>2]|0)){aN=(b0[c[(c[ak>>2]|0)+36>>2]&127](ak)|0)&255}else{aN=a[al]|0}if(aN<<24>>24!=(a[ag]|0)){aK=ag;break L396}al=c[g>>2]|0;ak=al+12|0;av=c[ak>>2]|0;if((av|0)==(c[al+16>>2]|0)){aC=c[(c[al>>2]|0)+40>>2]|0;b0[aC&127](al)|0}else{c[ak>>2]=av+1}av=ag+1|0;ak=a[E]|0;al=ak&255;if((av|0)==(((ak&1)==0?j:c[P>>2]|0)+((al&1|0)==0?al>>>1:c[O>>2]|0)|0)){aK=av;break}else{$=aM;ag=av}}}}while(0);if(!N){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L308}at=a[E]|0;as=at&255;if((aK|0)==(((at&1)==0?j:c[P>>2]|0)+((as&1|0)==0?as>>>1:c[O>>2]|0)|0)){am=r;an=D;ao=X;ap=W;aq=p;ar=V}else{aa=405;break L286}break};case 4:{as=0;at=D;au=X;ag=W;$=p;av=V;L431:while(1){al=c[g>>2]|0;do{if((al|0)==0){aO=0}else{if((c[al+12>>2]|0)!=(c[al+16>>2]|0)){aO=al;break}if((b0[c[(c[al>>2]|0)+36>>2]&127](al)|0)==-1){c[g>>2]=0;aO=0;break}else{aO=c[g>>2]|0;break}}}while(0);al=(aO|0)==0;ak=c[e>>2]|0;do{if((ak|0)==0){aa=418}else{if((c[ak+12>>2]|0)!=(c[ak+16>>2]|0)){if(al){break}else{break L431}}if((b0[c[(c[ak>>2]|0)+36>>2]&127](ak)|0)==-1){c[e>>2]=0;aa=418;break}else{if(al){break}else{break L431}}}}while(0);if((aa|0)==418){aa=0;if(al){break}}ak=c[g>>2]|0;aC=c[ak+12>>2]|0;if((aC|0)==(c[ak+16>>2]|0)){aP=(b0[c[(c[ak>>2]|0)+36>>2]&127](ak)|0)&255}else{aP=a[aC]|0}do{if(aP<<24>>24>-1){if((b[(c[f>>2]|0)+(aP<<24>>24<<1)>>1]&2048)==0){aa=437;break}aC=c[o>>2]|0;if((aC|0)==(av|0)){ak=(c[U>>2]|0)!=166;aQ=c[h>>2]|0;aR=av-aQ|0;aS=aR>>>0<2147483647?aR<<1:-1;aT=kL(ak?aQ:0,aS)|0;if((aT|0)==0){kX()}do{if(ak){c[h>>2]=aT;aU=aT}else{aQ=c[h>>2]|0;c[h>>2]=aT;if((aQ|0)==0){aU=aT;break}bY[c[U>>2]&511](aQ);aU=c[h>>2]|0}}while(0);c[U>>2]=82;aT=aU+aR|0;c[o>>2]=aT;aV=(c[h>>2]|0)+aS|0;aW=aT}else{aV=av;aW=aC}c[o>>2]=aW+1;a[aW]=aP;aX=as+1|0;aY=at;aZ=au;a_=ag;a$=$;a0=aV}else{aa=437}}while(0);if((aa|0)==437){aa=0;al=d[w]|0;if((((al&1|0)==0?al>>>1:c[n>>2]|0)|0)==0|(as|0)==0){break}if(aP<<24>>24!=(a[u]|0)){break}if((au|0)==(at|0)){al=au-ag|0;aT=al>>>0<2147483647?al<<1:-1;if(($|0)==166){a1=0}else{a1=ag}ak=kL(a1,aT)|0;ai=ak;if((ak|0)==0){kX()}a2=ai+(aT>>>2<<2)|0;a3=ai+(al>>2<<2)|0;a4=ai;a5=82}else{a2=at;a3=au;a4=ag;a5=$}c[a3>>2]=as;aX=0;aY=a2;aZ=a3+4|0;a_=a4;a$=a5;a0=av}ai=c[g>>2]|0;al=ai+12|0;aT=c[al>>2]|0;if((aT|0)==(c[ai+16>>2]|0)){ak=c[(c[ai>>2]|0)+40>>2]|0;b0[ak&127](ai)|0;as=aX;at=aY;au=aZ;ag=a_;$=a$;av=a0;continue}else{c[al>>2]=aT+1;as=aX;at=aY;au=aZ;ag=a_;$=a$;av=a0;continue}}if((ag|0)==(au|0)|(as|0)==0){a6=at;a7=au;a8=ag;a9=$}else{if((au|0)==(at|0)){aT=au-ag|0;al=aT>>>0<2147483647?aT<<1:-1;if(($|0)==166){ba=0}else{ba=ag}ai=kL(ba,al)|0;ak=ai;if((ai|0)==0){kX()}bb=ak+(al>>>2<<2)|0;bc=ak+(aT>>2<<2)|0;bd=ak;be=82}else{bb=at;bc=au;bd=ag;be=$}c[bc>>2]=as;a6=bb;a7=bc+4|0;a8=bd;a9=be}if((c[B>>2]|0)>0){ak=c[g>>2]|0;do{if((ak|0)==0){bf=0}else{if((c[ak+12>>2]|0)!=(c[ak+16>>2]|0)){bf=ak;break}if((b0[c[(c[ak>>2]|0)+36>>2]&127](ak)|0)==-1){c[g>>2]=0;bf=0;break}else{bf=c[g>>2]|0;break}}}while(0);ak=(bf|0)==0;as=c[e>>2]|0;do{if((as|0)==0){aa=470}else{if((c[as+12>>2]|0)!=(c[as+16>>2]|0)){if(ak){bg=as;break}else{aa=477;break L286}}if((b0[c[(c[as>>2]|0)+36>>2]&127](as)|0)==-1){c[e>>2]=0;aa=470;break}else{if(ak){bg=as;break}else{aa=477;break L286}}}}while(0);if((aa|0)==470){aa=0;if(ak){aa=477;break L286}else{bg=0}}as=c[g>>2]|0;$=c[as+12>>2]|0;if(($|0)==(c[as+16>>2]|0)){bh=(b0[c[(c[as>>2]|0)+36>>2]&127](as)|0)&255}else{bh=a[$]|0}if(bh<<24>>24!=(a[t]|0)){aa=477;break L286}$=c[g>>2]|0;as=$+12|0;ag=c[as>>2]|0;if((ag|0)==(c[$+16>>2]|0)){au=c[(c[$>>2]|0)+40>>2]|0;b0[au&127]($)|0;bi=av;bj=bg}else{c[as>>2]=ag+1;bi=av;bj=bg}while(1){ag=c[g>>2]|0;do{if((ag|0)==0){bk=0}else{if((c[ag+12>>2]|0)!=(c[ag+16>>2]|0)){bk=ag;break}if((b0[c[(c[ag>>2]|0)+36>>2]&127](ag)|0)==-1){c[g>>2]=0;bk=0;break}else{bk=c[g>>2]|0;break}}}while(0);ag=(bk|0)==0;do{if((bj|0)==0){aa=493}else{if((c[bj+12>>2]|0)!=(c[bj+16>>2]|0)){if(ag){bl=bj;break}else{aa=501;break L286}}if((b0[c[(c[bj>>2]|0)+36>>2]&127](bj)|0)==-1){c[e>>2]=0;aa=493;break}else{if(ag){bl=bj;break}else{aa=501;break L286}}}}while(0);if((aa|0)==493){aa=0;if(ag){aa=501;break L286}else{bl=0}}as=c[g>>2]|0;$=c[as+12>>2]|0;if(($|0)==(c[as+16>>2]|0)){bm=(b0[c[(c[as>>2]|0)+36>>2]&127](as)|0)&255}else{bm=a[$]|0}if(bm<<24>>24<=-1){aa=501;break L286}if((b[(c[f>>2]|0)+(bm<<24>>24<<1)>>1]&2048)==0){aa=501;break L286}$=c[o>>2]|0;if(($|0)==(bi|0)){as=(c[U>>2]|0)!=166;au=c[h>>2]|0;at=bi-au|0;aT=at>>>0<2147483647?at<<1:-1;al=kL(as?au:0,aT)|0;if((al|0)==0){kX()}do{if(as){c[h>>2]=al;bn=al}else{au=c[h>>2]|0;c[h>>2]=al;if((au|0)==0){bn=al;break}bY[c[U>>2]&511](au);bn=c[h>>2]|0}}while(0);c[U>>2]=82;al=bn+at|0;c[o>>2]=al;bo=(c[h>>2]|0)+aT|0;bp=al}else{bo=bi;bp=$}al=c[g>>2]|0;as=c[al+12>>2]|0;if((as|0)==(c[al+16>>2]|0)){ag=(b0[c[(c[al>>2]|0)+36>>2]&127](al)|0)&255;bq=ag;br=c[o>>2]|0}else{bq=a[as]|0;br=bp}c[o>>2]=br+1;a[br]=bq;as=(c[B>>2]|0)-1|0;c[B>>2]=as;ag=c[g>>2]|0;al=ag+12|0;au=c[al>>2]|0;if((au|0)==(c[ag+16>>2]|0)){ai=c[(c[ag>>2]|0)+40>>2]|0;b0[ai&127](ag)|0}else{c[al>>2]=au+1}if((as|0)>0){bi=bo;bj=bl}else{bs=bo;break}}}else{bs=av}if((c[o>>2]|0)==(c[h>>2]|0)){aa=521;break L286}else{am=r;an=a6;ao=a7;ap=a8;aq=a9;ar=bs}break};default:{am=r;an=D;ao=X;ap=W;aq=p;ar=V}}}while(0);L585:do{if((aa|0)==297){aa=0;if((Y|0)==3){ac=p;ad=W;ae=X;af=r;aa=523;break L286}else{bt=ab}while(1){Z=c[g>>2]|0;do{if((Z|0)==0){bu=0}else{if((c[Z+12>>2]|0)!=(c[Z+16>>2]|0)){bu=Z;break}if((b0[c[(c[Z>>2]|0)+36>>2]&127](Z)|0)==-1){c[g>>2]=0;bu=0;break}else{bu=c[g>>2]|0;break}}}while(0);Z=(bu|0)==0;do{if((bt|0)==0){aa=310}else{if((c[bt+12>>2]|0)!=(c[bt+16>>2]|0)){if(Z){bv=bt;break}else{am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L585}}if((b0[c[(c[bt>>2]|0)+36>>2]&127](bt)|0)==-1){c[e>>2]=0;aa=310;break}else{if(Z){bv=bt;break}else{am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L585}}}}while(0);if((aa|0)==310){aa=0;if(Z){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L585}else{bv=0}}$=c[g>>2]|0;aT=c[$+12>>2]|0;if((aT|0)==(c[$+16>>2]|0)){bw=(b0[c[(c[$>>2]|0)+36>>2]&127]($)|0)&255}else{bw=a[aT]|0}if(bw<<24>>24<=-1){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L585}if((b[(c[f>>2]|0)+(bw<<24>>24<<1)>>1]&8192)==0){am=r;an=D;ao=X;ap=W;aq=p;ar=V;break L585}aT=c[g>>2]|0;$=aT+12|0;at=c[$>>2]|0;if((at|0)==(c[aT+16>>2]|0)){bx=(b0[c[(c[aT>>2]|0)+40>>2]&127](aT)|0)&255}else{c[$>>2]=at+1;bx=a[at]|0}dU(A,bx);bt=bv}}}while(0);av=Y+1|0;if(av>>>0<4){V=ar;p=aq;W=ap;X=ao;D=an;r=am;Y=av}else{ac=aq;ad=ap;ae=ao;af=am;aa=523;break}}L622:do{if((aa|0)==296){c[k>>2]=c[k>>2]|4;by=0;bz=W;bA=p}else if((aa|0)==363){c[k>>2]=c[k>>2]|4;by=0;bz=W;bA=p}else if((aa|0)==405){c[k>>2]=c[k>>2]|4;by=0;bz=W;bA=p}else if((aa|0)==477){c[k>>2]=c[k>>2]|4;by=0;bz=a8;bA=a9}else if((aa|0)==501){c[k>>2]=c[k>>2]|4;by=0;bz=a8;bA=a9}else if((aa|0)==521){c[k>>2]=c[k>>2]|4;by=0;bz=a8;bA=a9}else if((aa|0)==523){L630:do{if((af|0)!=0){am=af;ao=af+1|0;ap=af+8|0;aq=af+4|0;Y=1;L632:while(1){r=d[am]|0;if((r&1|0)==0){bB=r>>>1}else{bB=c[aq>>2]|0}if(Y>>>0>=bB>>>0){break L630}r=c[g>>2]|0;do{if((r|0)==0){bC=0}else{if((c[r+12>>2]|0)!=(c[r+16>>2]|0)){bC=r;break}if((b0[c[(c[r>>2]|0)+36>>2]&127](r)|0)==-1){c[g>>2]=0;bC=0;break}else{bC=c[g>>2]|0;break}}}while(0);r=(bC|0)==0;Z=c[e>>2]|0;do{if((Z|0)==0){aa=541}else{if((c[Z+12>>2]|0)!=(c[Z+16>>2]|0)){if(r){break}else{break L632}}if((b0[c[(c[Z>>2]|0)+36>>2]&127](Z)|0)==-1){c[e>>2]=0;aa=541;break}else{if(r){break}else{break L632}}}}while(0);if((aa|0)==541){aa=0;if(r){break}}Z=c[g>>2]|0;an=c[Z+12>>2]|0;if((an|0)==(c[Z+16>>2]|0)){bD=(b0[c[(c[Z>>2]|0)+36>>2]&127](Z)|0)&255}else{bD=a[an]|0}if((a[am]&1)==0){bE=ao}else{bE=c[ap>>2]|0}if(bD<<24>>24!=(a[bE+Y|0]|0)){break}an=Y+1|0;Z=c[g>>2]|0;D=Z+12|0;X=c[D>>2]|0;if((X|0)==(c[Z+16>>2]|0)){ar=c[(c[Z>>2]|0)+40>>2]|0;b0[ar&127](Z)|0;Y=an;continue}else{c[D>>2]=X+1;Y=an;continue}}c[k>>2]=c[k>>2]|4;by=0;bz=ad;bA=ac;break L622}}while(0);if((ad|0)==(ae|0)){by=1;bz=ae;bA=ac;break}c[C>>2]=0;ft(v,ad,ae,C);if((c[C>>2]|0)==0){by=1;bz=ad;bA=ac;break}c[k>>2]=c[k>>2]|4;by=0;bz=ad;bA=ac}}while(0);dP(A);dP(z);dP(y);dP(x);dP(v);if((bz|0)==0){i=q;return by|0}bY[bA&511](bz);i=q;return by|0}function hS(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0;f=b;g=d;h=a[f]|0;i=h&255;if((i&1|0)==0){j=i>>>1}else{j=c[b+4>>2]|0}if((h&1)==0){k=10;l=h}else{h=c[b>>2]|0;k=(h&-2)-1|0;l=h&255}h=e-g|0;if((e|0)==(d|0)){return b|0}if((k-j|0)>>>0<h>>>0){d1(b,k,j+h-k|0,j,j,0,0);m=a[f]|0}else{m=l}if((m&1)==0){n=b+1|0}else{n=c[b+8>>2]|0}m=e+(j-g)|0;g=d;d=n+j|0;while(1){a[d]=a[g]|0;l=g+1|0;if((l|0)==(e|0)){break}else{g=l;d=d+1|0}}a[n+m|0]=0;m=j+h|0;if((a[f]&1)==0){a[f]=m<<1&255;return b|0}else{c[b+4>>2]=m;return b|0}return 0}function hT(a){a=a|0;de(a|0);kS(a);return}function hU(a){a=a|0;de(a|0);return}function hV(a){a=a|0;var b=0;b=bO(8)|0;dE(b,a);bl(b|0,8192,22)}function hW(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0;d=i;i=i+160|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=d|0;m=d+16|0;n=d+120|0;o=d+128|0;p=d+136|0;q=d+144|0;r=d+152|0;s=n|0;c[s>>2]=m;t=n+4|0;c[t>>2]=166;u=m+100|0;ej(p,h);m=p|0;v=c[m>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;w=c[v+8>>2]|0;do{if((c[v+12>>2]|0)-w>>2>>>0>l>>>0){x=c[w+(l<<2)>>2]|0;if((x|0)==0){break}y=x;a[q]=0;z=f|0;A=c[z>>2]|0;c[r>>2]=A;if(hR(e,r,g,p,c[h+4>>2]|0,j,q,y,n,o,u)|0){B=k;if((a[B]&1)==0){a[k+1|0]=0;a[B]=0}else{a[c[k+8>>2]|0]=0;c[k+4>>2]=0}B=x;if((a[q]&1)!=0){dU(k,b_[c[(c[B>>2]|0)+28>>2]&31](y,45)|0)}x=b_[c[(c[B>>2]|0)+28>>2]&31](y,48)|0;y=c[o>>2]|0;B=y-1|0;C=c[s>>2]|0;while(1){if(C>>>0>=B>>>0){break}if((a[C]|0)==x<<24>>24){C=C+1|0}else{break}}hS(k,C,y)|0}x=e|0;B=c[x>>2]|0;do{if((B|0)==0){D=0}else{if((c[B+12>>2]|0)!=(c[B+16>>2]|0)){D=B;break}if((b0[c[(c[B>>2]|0)+36>>2]&127](B)|0)!=-1){D=B;break}c[x>>2]=0;D=0}}while(0);x=(D|0)==0;do{if((A|0)==0){E=621}else{if((c[A+12>>2]|0)!=(c[A+16>>2]|0)){if(x){break}else{E=623;break}}if((b0[c[(c[A>>2]|0)+36>>2]&127](A)|0)==-1){c[z>>2]=0;E=621;break}else{if(x^(A|0)==0){break}else{E=623;break}}}}while(0);if((E|0)==621){if(x){E=623}}if((E|0)==623){c[j>>2]=c[j>>2]|2}c[b>>2]=D;A=c[m>>2]|0;dz(A)|0;A=c[s>>2]|0;c[s>>2]=0;if((A|0)==0){i=d;return}bY[c[t>>2]&511](A);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function hX(b,d,e,f,g,h,j,k,l,m){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0;n=i;i=i+56|0;o=n|0;p=n+16|0;q=n+32|0;r=n+40|0;s=r;t=i;i=i+12|0;i=i+7>>3<<3;u=t;v=i;i=i+12|0;i=i+7>>3<<3;w=v;x=i;i=i+12|0;i=i+7>>3<<3;y=x;z=i;i=i+4|0;i=i+7>>3<<3;A=i;i=i+12|0;i=i+7>>3<<3;B=A;D=i;i=i+12|0;i=i+7>>3<<3;E=D;F=i;i=i+12|0;i=i+7>>3<<3;G=F;H=i;i=i+12|0;i=i+7>>3<<3;I=H;if(b){b=c[d>>2]|0;if((c[4044]|0)!=-1){c[p>>2]=16176;c[p+4>>2]=12;c[p+8>>2]=0;dW(16176,p,96)}p=(c[4045]|0)-1|0;J=c[b+8>>2]|0;if((c[b+12>>2]|0)-J>>2>>>0<=p>>>0){K=bO(4)|0;L=K;kq(L);bl(K|0,8176,134)}b=c[J+(p<<2)>>2]|0;if((b|0)==0){K=bO(4)|0;L=K;kq(L);bl(K|0,8176,134)}K=b;bZ[c[(c[b>>2]|0)+44>>2]&127](q,K);L=e;C=c[q>>2]|0;a[L]=C&255;C=C>>8;a[L+1|0]=C&255;C=C>>8;a[L+2|0]=C&255;C=C>>8;a[L+3|0]=C&255;L=b;bZ[c[(c[L>>2]|0)+32>>2]&127](r,K);q=l;if((a[q]&1)==0){a[l+1|0]=0;a[q]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[q>>2]=c[s>>2];c[q+4>>2]=c[s+4>>2];c[q+8>>2]=c[s+8>>2];k$(s|0,0,12);dP(r);bZ[c[(c[L>>2]|0)+28>>2]&127](t,K);r=k;if((a[r]&1)==0){a[k+1|0]=0;a[r]=0}else{a[c[k+8>>2]|0]=0;c[k+4>>2]=0}d$(k,0);c[r>>2]=c[u>>2];c[r+4>>2]=c[u+4>>2];c[r+8>>2]=c[u+8>>2];k$(u|0,0,12);dP(t);t=b;a[f]=b0[c[(c[t>>2]|0)+12>>2]&127](K)|0;a[g]=b0[c[(c[t>>2]|0)+16>>2]&127](K)|0;bZ[c[(c[L>>2]|0)+20>>2]&127](v,K);t=h;if((a[t]&1)==0){a[h+1|0]=0;a[t]=0}else{a[c[h+8>>2]|0]=0;c[h+4>>2]=0}d$(h,0);c[t>>2]=c[w>>2];c[t+4>>2]=c[w+4>>2];c[t+8>>2]=c[w+8>>2];k$(w|0,0,12);dP(v);bZ[c[(c[L>>2]|0)+24>>2]&127](x,K);L=j;if((a[L]&1)==0){a[j+1|0]=0;a[L]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[L>>2]=c[y>>2];c[L+4>>2]=c[y+4>>2];c[L+8>>2]=c[y+8>>2];k$(y|0,0,12);dP(x);M=b0[c[(c[b>>2]|0)+36>>2]&127](K)|0;c[m>>2]=M;i=n;return}else{K=c[d>>2]|0;if((c[4046]|0)!=-1){c[o>>2]=16184;c[o+4>>2]=12;c[o+8>>2]=0;dW(16184,o,96)}o=(c[4047]|0)-1|0;d=c[K+8>>2]|0;if((c[K+12>>2]|0)-d>>2>>>0<=o>>>0){N=bO(4)|0;O=N;kq(O);bl(N|0,8176,134)}K=c[d+(o<<2)>>2]|0;if((K|0)==0){N=bO(4)|0;O=N;kq(O);bl(N|0,8176,134)}N=K;bZ[c[(c[K>>2]|0)+44>>2]&127](z,N);O=e;C=c[z>>2]|0;a[O]=C&255;C=C>>8;a[O+1|0]=C&255;C=C>>8;a[O+2|0]=C&255;C=C>>8;a[O+3|0]=C&255;O=K;bZ[c[(c[O>>2]|0)+32>>2]&127](A,N);z=l;if((a[z]&1)==0){a[l+1|0]=0;a[z]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[z>>2]=c[B>>2];c[z+4>>2]=c[B+4>>2];c[z+8>>2]=c[B+8>>2];k$(B|0,0,12);dP(A);bZ[c[(c[O>>2]|0)+28>>2]&127](D,N);A=k;if((a[A]&1)==0){a[k+1|0]=0;a[A]=0}else{a[c[k+8>>2]|0]=0;c[k+4>>2]=0}d$(k,0);c[A>>2]=c[E>>2];c[A+4>>2]=c[E+4>>2];c[A+8>>2]=c[E+8>>2];k$(E|0,0,12);dP(D);D=K;a[f]=b0[c[(c[D>>2]|0)+12>>2]&127](N)|0;a[g]=b0[c[(c[D>>2]|0)+16>>2]&127](N)|0;bZ[c[(c[O>>2]|0)+20>>2]&127](F,N);D=h;if((a[D]&1)==0){a[h+1|0]=0;a[D]=0}else{a[c[h+8>>2]|0]=0;c[h+4>>2]=0}d$(h,0);c[D>>2]=c[G>>2];c[D+4>>2]=c[G+4>>2];c[D+8>>2]=c[G+8>>2];k$(G|0,0,12);dP(F);bZ[c[(c[O>>2]|0)+24>>2]&127](H,N);O=j;if((a[O]&1)==0){a[j+1|0]=0;a[O]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[O>>2]=c[I>>2];c[O+4>>2]=c[I+4>>2];c[O+8>>2]=c[I+8>>2];k$(I|0,0,12);dP(H);M=b0[c[(c[K>>2]|0)+36>>2]&127](N)|0;c[m>>2]=M;i=n;return}}function hY(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;d=i;i=i+600|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=d|0;m=d+16|0;n=d+416|0;o=d+424|0;p=d+432|0;q=d+440|0;r=d+448|0;s=d+456|0;t=d+496|0;u=n|0;c[u>>2]=m;v=n+4|0;c[v>>2]=166;w=m+400|0;ej(p,h);m=p|0;x=c[m>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;y=c[x+8>>2]|0;do{if((c[x+12>>2]|0)-y>>2>>>0>l>>>0){z=c[y+(l<<2)>>2]|0;if((z|0)==0){break}A=z;a[q]=0;B=f|0;c[r>>2]=c[B>>2];do{if(hZ(e,r,g,p,c[h+4>>2]|0,j,q,A,n,o,w)|0){C=s|0;D=c[(c[z>>2]|0)+48>>2]|0;ca[D&15](A,2096,2106,C)|0;D=t|0;E=c[o>>2]|0;F=c[u>>2]|0;G=E-F|0;do{if((G|0)>392){H=kJ((G>>2)+2|0)|0;if((H|0)!=0){I=H;J=H;break}kX();I=0;J=0}else{I=D;J=0}}while(0);if((a[q]&1)==0){K=I}else{a[I]=45;K=I+1|0}if(F>>>0<E>>>0){G=s+40|0;H=s;L=K;M=F;while(1){N=C;while(1){if((N|0)==(G|0)){O=G;break}if((c[N>>2]|0)==(c[M>>2]|0)){O=N;break}else{N=N+4|0}}a[L]=a[2096+(O-H>>2)|0]|0;N=M+4|0;P=L+1|0;if(N>>>0<(c[o>>2]|0)>>>0){L=P;M=N}else{Q=P;break}}}else{Q=K}a[Q]=0;M=bD(D|0,1408,(L=i,i=i+8|0,c[L>>2]=k,L)|0)|0;i=L;if((M|0)==1){if((J|0)==0){break}kK(J);break}M=bO(8)|0;dE(M,1368);bl(M|0,8192,22)}}while(0);A=e|0;z=c[A>>2]|0;do{if((z|0)==0){R=0}else{M=c[z+12>>2]|0;if((M|0)==(c[z+16>>2]|0)){S=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{S=c[M>>2]|0}if((S|0)!=-1){R=z;break}c[A>>2]=0;R=0}}while(0);A=(R|0)==0;z=c[B>>2]|0;do{if((z|0)==0){T=739}else{M=c[z+12>>2]|0;if((M|0)==(c[z+16>>2]|0)){U=b0[c[(c[z>>2]|0)+36>>2]&127](z)|0}else{U=c[M>>2]|0}if((U|0)==-1){c[B>>2]=0;T=739;break}else{if(A^(z|0)==0){break}else{T=741;break}}}}while(0);if((T|0)==739){if(A){T=741}}if((T|0)==741){c[j>>2]=c[j>>2]|2}c[b>>2]=R;z=c[m>>2]|0;dz(z)|0;z=c[u>>2]|0;c[u>>2]=0;if((z|0)==0){i=d;return}bY[c[v>>2]&511](z);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function hZ(b,e,f,g,h,j,k,l,m,n,o){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;var p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0,az=0,aA=0,aB=0,aC=0,aD=0,aE=0,aF=0,aG=0,aH=0,aI=0,aJ=0,aK=0,aL=0,aM=0,aN=0,aO=0,aP=0,aQ=0,aR=0,aS=0,aT=0,aU=0,aV=0,aW=0,aX=0,aY=0,aZ=0,a_=0,a$=0,a0=0,a1=0,a2=0,a3=0,a4=0,a5=0,a6=0,a7=0,a8=0,a9=0,ba=0,bb=0,bc=0,bd=0,be=0,bf=0,bg=0,bh=0,bi=0,bj=0,bk=0,bl=0,bm=0,bn=0,bo=0,bp=0,bq=0,br=0,bs=0,bt=0,bu=0,bv=0,bw=0,bx=0,by=0,bz=0,bA=0,bB=0,bC=0,bD=0;p=i;i=i+448|0;q=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[q>>2];q=p|0;r=p+8|0;s=p+408|0;t=p+416|0;u=p+424|0;v=p+432|0;w=v;x=i;i=i+12|0;i=i+7>>3<<3;y=i;i=i+12|0;i=i+7>>3<<3;z=i;i=i+12|0;i=i+7>>3<<3;A=i;i=i+12|0;i=i+7>>3<<3;B=i;i=i+4|0;i=i+7>>3<<3;C=i;i=i+4|0;i=i+7>>3<<3;c[q>>2]=o;o=r|0;k$(w|0,0,12);D=x;E=y;F=z;G=A;k$(D|0,0,12);k$(E|0,0,12);k$(F|0,0,12);k$(G|0,0,12);h2(f,g,s,t,u,v,x,y,z,B);g=m|0;c[n>>2]=c[g>>2];f=b|0;b=e|0;e=l;H=z+4|0;I=z+8|0;J=y+4|0;K=y+8|0;L=(h&512|0)!=0;h=x+4|0;M=x+8|0;N=A+4|0;O=A+8|0;P=s+3|0;Q=v+4|0;R=166;S=o;T=o;o=r+400|0;r=0;U=0;L890:while(1){V=c[f>>2]|0;do{if((V|0)==0){W=1}else{X=c[V+12>>2]|0;if((X|0)==(c[V+16>>2]|0)){Y=b0[c[(c[V>>2]|0)+36>>2]&127](V)|0}else{Y=c[X>>2]|0}if((Y|0)==-1){c[f>>2]=0;W=1;break}else{W=(c[f>>2]|0)==0;break}}}while(0);V=c[b>>2]|0;do{if((V|0)==0){Z=767}else{X=c[V+12>>2]|0;if((X|0)==(c[V+16>>2]|0)){_=b0[c[(c[V>>2]|0)+36>>2]&127](V)|0}else{_=c[X>>2]|0}if((_|0)==-1){c[b>>2]=0;Z=767;break}else{if(W^(V|0)==0){$=V;break}else{aa=R;ab=S;ac=T;ad=r;Z=1007;break L890}}}}while(0);if((Z|0)==767){Z=0;if(W){aa=R;ab=S;ac=T;ad=r;Z=1007;break}else{$=0}}L914:do{switch(a[s+U|0]|0){case 2:{if(!((r|0)!=0|U>>>0<2)){if((U|0)==2){ae=(a[P]|0)!=0}else{ae=0}if(!(L|ae)){af=0;ag=o;ah=T;ai=S;aj=R;break L914}}V=a[D]|0;X=(V&1)==0?h:c[M>>2]|0;L922:do{if((U|0)==0){ak=X;al=V;am=$}else{if((d[s+(U-1)|0]|0)<2){an=X;ao=V}else{ak=X;al=V;am=$;break}while(1){ap=ao&255;if((an|0)==(((ao&1)==0?h:c[M>>2]|0)+(((ap&1|0)==0?ap>>>1:c[h>>2]|0)<<2)|0)){aq=ao;break}if(!(b1[c[(c[e>>2]|0)+12>>2]&63](l,8192,c[an>>2]|0)|0)){Z=868;break}an=an+4|0;ao=a[D]|0}if((Z|0)==868){Z=0;aq=a[D]|0}ap=(aq&1)==0;ar=an-(ap?h:c[M>>2]|0)>>2;as=a[G]|0;at=as&255;au=(at&1|0)==0;L932:do{if(ar>>>0<=(au?at>>>1:c[N>>2]|0)>>>0){av=(as&1)==0;aw=(av?N:c[O>>2]|0)+((au?at>>>1:c[N>>2]|0)-ar<<2)|0;ax=(av?N:c[O>>2]|0)+((au?at>>>1:c[N>>2]|0)<<2)|0;if((aw|0)==(ax|0)){ak=an;al=aq;am=$;break L922}else{ay=aw;az=ap?h:c[M>>2]|0}while(1){if((c[ay>>2]|0)!=(c[az>>2]|0)){break L932}aw=ay+4|0;if((aw|0)==(ax|0)){ak=an;al=aq;am=$;break L922}ay=aw;az=az+4|0}}}while(0);ak=ap?h:c[M>>2]|0;al=aq;am=$}}while(0);L939:while(1){V=al&255;if((ak|0)==(((al&1)==0?h:c[M>>2]|0)+(((V&1|0)==0?V>>>1:c[h>>2]|0)<<2)|0)){break}V=c[f>>2]|0;do{if((V|0)==0){aA=1}else{X=c[V+12>>2]|0;if((X|0)==(c[V+16>>2]|0)){aB=b0[c[(c[V>>2]|0)+36>>2]&127](V)|0}else{aB=c[X>>2]|0}if((aB|0)==-1){c[f>>2]=0;aA=1;break}else{aA=(c[f>>2]|0)==0;break}}}while(0);do{if((am|0)==0){Z=889}else{V=c[am+12>>2]|0;if((V|0)==(c[am+16>>2]|0)){aC=b0[c[(c[am>>2]|0)+36>>2]&127](am)|0}else{aC=c[V>>2]|0}if((aC|0)==-1){c[b>>2]=0;Z=889;break}else{if(aA^(am|0)==0){aD=am;break}else{break L939}}}}while(0);if((Z|0)==889){Z=0;if(aA){break}else{aD=0}}V=c[f>>2]|0;ap=c[V+12>>2]|0;if((ap|0)==(c[V+16>>2]|0)){aE=b0[c[(c[V>>2]|0)+36>>2]&127](V)|0}else{aE=c[ap>>2]|0}if((aE|0)!=(c[ak>>2]|0)){break}ap=c[f>>2]|0;V=ap+12|0;X=c[V>>2]|0;if((X|0)==(c[ap+16>>2]|0)){at=c[(c[ap>>2]|0)+40>>2]|0;b0[at&127](ap)|0}else{c[V>>2]=X+4}ak=ak+4|0;al=a[D]|0;am=aD}if(!L){af=r;ag=o;ah=T;ai=S;aj=R;break L914}X=a[D]|0;V=X&255;if((ak|0)==(((X&1)==0?h:c[M>>2]|0)+(((V&1|0)==0?V>>>1:c[h>>2]|0)<<2)|0)){af=r;ag=o;ah=T;ai=S;aj=R}else{Z=901;break L890}break};case 4:{V=0;X=o;ap=T;at=S;au=R;L975:while(1){ar=c[f>>2]|0;do{if((ar|0)==0){aF=1}else{as=c[ar+12>>2]|0;if((as|0)==(c[ar+16>>2]|0)){aG=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{aG=c[as>>2]|0}if((aG|0)==-1){c[f>>2]=0;aF=1;break}else{aF=(c[f>>2]|0)==0;break}}}while(0);ar=c[b>>2]|0;do{if((ar|0)==0){Z=915}else{as=c[ar+12>>2]|0;if((as|0)==(c[ar+16>>2]|0)){aH=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{aH=c[as>>2]|0}if((aH|0)==-1){c[b>>2]=0;Z=915;break}else{if(aF^(ar|0)==0){break}else{break L975}}}}while(0);if((Z|0)==915){Z=0;if(aF){break}}ar=c[f>>2]|0;as=c[ar+12>>2]|0;if((as|0)==(c[ar+16>>2]|0)){aI=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{aI=c[as>>2]|0}if(b1[c[(c[e>>2]|0)+12>>2]&63](l,2048,aI)|0){as=c[n>>2]|0;if((as|0)==(c[q>>2]|0)){h3(m,n,q);aJ=c[n>>2]|0}else{aJ=as}c[n>>2]=aJ+4;c[aJ>>2]=aI;aK=V+1|0;aL=X;aM=ap;aN=at;aO=au}else{as=d[w]|0;if((((as&1|0)==0?as>>>1:c[Q>>2]|0)|0)==0|(V|0)==0){break}if((aI|0)!=(c[u>>2]|0)){break}if((ap|0)==(X|0)){as=(au|0)!=166;ar=ap-at|0;ax=ar>>>0<2147483647?ar<<1:-1;if(as){aP=at}else{aP=0}as=kL(aP,ax)|0;aw=as;if((as|0)==0){kX()}aQ=aw+(ax>>>2<<2)|0;aR=aw+(ar>>2<<2)|0;aS=aw;aT=82}else{aQ=X;aR=ap;aS=at;aT=au}c[aR>>2]=V;aK=0;aL=aQ;aM=aR+4|0;aN=aS;aO=aT}aw=c[f>>2]|0;ar=aw+12|0;ax=c[ar>>2]|0;if((ax|0)==(c[aw+16>>2]|0)){as=c[(c[aw>>2]|0)+40>>2]|0;b0[as&127](aw)|0;V=aK;X=aL;ap=aM;at=aN;au=aO;continue}else{c[ar>>2]=ax+4;V=aK;X=aL;ap=aM;at=aN;au=aO;continue}}if((at|0)==(ap|0)|(V|0)==0){aU=X;aV=ap;aW=at;aX=au}else{if((ap|0)==(X|0)){ax=(au|0)!=166;ar=ap-at|0;aw=ar>>>0<2147483647?ar<<1:-1;if(ax){aY=at}else{aY=0}ax=kL(aY,aw)|0;as=ax;if((ax|0)==0){kX()}aZ=as+(aw>>>2<<2)|0;a_=as+(ar>>2<<2)|0;a$=as;a0=82}else{aZ=X;a_=ap;a$=at;a0=au}c[a_>>2]=V;aU=aZ;aV=a_+4|0;aW=a$;aX=a0}as=c[B>>2]|0;if((as|0)>0){ar=c[f>>2]|0;do{if((ar|0)==0){a1=1}else{aw=c[ar+12>>2]|0;if((aw|0)==(c[ar+16>>2]|0)){a2=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{a2=c[aw>>2]|0}if((a2|0)==-1){c[f>>2]=0;a1=1;break}else{a1=(c[f>>2]|0)==0;break}}}while(0);ar=c[b>>2]|0;do{if((ar|0)==0){Z=964}else{V=c[ar+12>>2]|0;if((V|0)==(c[ar+16>>2]|0)){a3=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{a3=c[V>>2]|0}if((a3|0)==-1){c[b>>2]=0;Z=964;break}else{if(a1^(ar|0)==0){a4=ar;break}else{Z=970;break L890}}}}while(0);if((Z|0)==964){Z=0;if(a1){Z=970;break L890}else{a4=0}}ar=c[f>>2]|0;V=c[ar+12>>2]|0;if((V|0)==(c[ar+16>>2]|0)){a5=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{a5=c[V>>2]|0}if((a5|0)!=(c[t>>2]|0)){Z=970;break L890}V=c[f>>2]|0;ar=V+12|0;au=c[ar>>2]|0;if((au|0)==(c[V+16>>2]|0)){at=c[(c[V>>2]|0)+40>>2]|0;b0[at&127](V)|0;a6=a4;a7=as}else{c[ar>>2]=au+4;a6=a4;a7=as}while(1){au=c[f>>2]|0;do{if((au|0)==0){a8=1}else{ar=c[au+12>>2]|0;if((ar|0)==(c[au+16>>2]|0)){a9=b0[c[(c[au>>2]|0)+36>>2]&127](au)|0}else{a9=c[ar>>2]|0}if((a9|0)==-1){c[f>>2]=0;a8=1;break}else{a8=(c[f>>2]|0)==0;break}}}while(0);do{if((a6|0)==0){Z=987}else{au=c[a6+12>>2]|0;if((au|0)==(c[a6+16>>2]|0)){ba=b0[c[(c[a6>>2]|0)+36>>2]&127](a6)|0}else{ba=c[au>>2]|0}if((ba|0)==-1){c[b>>2]=0;Z=987;break}else{if(a8^(a6|0)==0){bb=a6;break}else{Z=994;break L890}}}}while(0);if((Z|0)==987){Z=0;if(a8){Z=994;break L890}else{bb=0}}au=c[f>>2]|0;ar=c[au+12>>2]|0;if((ar|0)==(c[au+16>>2]|0)){bc=b0[c[(c[au>>2]|0)+36>>2]&127](au)|0}else{bc=c[ar>>2]|0}if(!(b1[c[(c[e>>2]|0)+12>>2]&63](l,2048,bc)|0)){Z=994;break L890}if((c[n>>2]|0)==(c[q>>2]|0)){h3(m,n,q)}ar=c[f>>2]|0;au=c[ar+12>>2]|0;if((au|0)==(c[ar+16>>2]|0)){bd=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{bd=c[au>>2]|0}au=c[n>>2]|0;c[n>>2]=au+4;c[au>>2]=bd;au=a7-1|0;c[B>>2]=au;ar=c[f>>2]|0;V=ar+12|0;at=c[V>>2]|0;if((at|0)==(c[ar+16>>2]|0)){ap=c[(c[ar>>2]|0)+40>>2]|0;b0[ap&127](ar)|0}else{c[V>>2]=at+4}if((au|0)>0){a6=bb;a7=au}else{break}}}if((c[n>>2]|0)==(c[g>>2]|0)){Z=1005;break L890}else{af=r;ag=aU;ah=aV;ai=aW;aj=aX}break};case 1:{if((U|0)==3){aa=R;ab=S;ac=T;ad=r;Z=1007;break L890}as=c[f>>2]|0;au=c[as+12>>2]|0;if((au|0)==(c[as+16>>2]|0)){be=b0[c[(c[as>>2]|0)+36>>2]&127](as)|0}else{be=c[au>>2]|0}if(!(b1[c[(c[e>>2]|0)+12>>2]&63](l,8192,be)|0)){Z=791;break L890}au=c[f>>2]|0;as=au+12|0;at=c[as>>2]|0;if((at|0)==(c[au+16>>2]|0)){bf=b0[c[(c[au>>2]|0)+40>>2]&127](au)|0}else{c[as>>2]=at+4;bf=c[at>>2]|0}eh(A,bf);Z=792;break};case 0:{Z=792;break};case 3:{at=a[E]|0;as=at&255;au=(as&1|0)==0;V=a[F]|0;ar=V&255;ap=(ar&1|0)==0;if(((au?as>>>1:c[J>>2]|0)|0)==(-(ap?ar>>>1:c[H>>2]|0)|0)){af=r;ag=o;ah=T;ai=S;aj=R;break L914}do{if(((au?as>>>1:c[J>>2]|0)|0)!=0){if(((ap?ar>>>1:c[H>>2]|0)|0)==0){break}X=c[f>>2]|0;aw=c[X+12>>2]|0;if((aw|0)==(c[X+16>>2]|0)){ax=b0[c[(c[X>>2]|0)+36>>2]&127](X)|0;bg=ax;bh=a[E]|0}else{bg=c[aw>>2]|0;bh=at}aw=c[f>>2]|0;ax=aw+12|0;X=c[ax>>2]|0;av=(X|0)==(c[aw+16>>2]|0);if((bg|0)==(c[((bh&1)==0?J:c[K>>2]|0)>>2]|0)){if(av){bi=c[(c[aw>>2]|0)+40>>2]|0;b0[bi&127](aw)|0}else{c[ax>>2]=X+4}ax=d[E]|0;af=((ax&1|0)==0?ax>>>1:c[J>>2]|0)>>>0>1?y:r;ag=o;ah=T;ai=S;aj=R;break L914}if(av){bj=b0[c[(c[aw>>2]|0)+36>>2]&127](aw)|0}else{bj=c[X>>2]|0}if((bj|0)!=(c[((a[F]&1)==0?H:c[I>>2]|0)>>2]|0)){Z=857;break L890}X=c[f>>2]|0;aw=X+12|0;av=c[aw>>2]|0;if((av|0)==(c[X+16>>2]|0)){ax=c[(c[X>>2]|0)+40>>2]|0;b0[ax&127](X)|0}else{c[aw>>2]=av+4}a[k]=1;av=d[F]|0;af=((av&1|0)==0?av>>>1:c[H>>2]|0)>>>0>1?z:r;ag=o;ah=T;ai=S;aj=R;break L914}}while(0);ar=c[f>>2]|0;ap=c[ar+12>>2]|0;av=(ap|0)==(c[ar+16>>2]|0);if(((au?as>>>1:c[J>>2]|0)|0)==0){if(av){aw=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0;bk=aw;bl=a[F]|0}else{bk=c[ap>>2]|0;bl=V}if((bk|0)!=(c[((bl&1)==0?H:c[I>>2]|0)>>2]|0)){af=r;ag=o;ah=T;ai=S;aj=R;break L914}aw=c[f>>2]|0;X=aw+12|0;ax=c[X>>2]|0;if((ax|0)==(c[aw+16>>2]|0)){bi=c[(c[aw>>2]|0)+40>>2]|0;b0[bi&127](aw)|0}else{c[X>>2]=ax+4}a[k]=1;ax=d[F]|0;af=((ax&1|0)==0?ax>>>1:c[H>>2]|0)>>>0>1?z:r;ag=o;ah=T;ai=S;aj=R;break L914}if(av){av=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0;bm=av;bn=a[E]|0}else{bm=c[ap>>2]|0;bn=at}if((bm|0)!=(c[((bn&1)==0?J:c[K>>2]|0)>>2]|0)){a[k]=1;af=r;ag=o;ah=T;ai=S;aj=R;break L914}ap=c[f>>2]|0;av=ap+12|0;ar=c[av>>2]|0;if((ar|0)==(c[ap+16>>2]|0)){ax=c[(c[ap>>2]|0)+40>>2]|0;b0[ax&127](ap)|0}else{c[av>>2]=ar+4}ar=d[E]|0;af=((ar&1|0)==0?ar>>>1:c[J>>2]|0)>>>0>1?y:r;ag=o;ah=T;ai=S;aj=R;break};default:{af=r;ag=o;ah=T;ai=S;aj=R}}}while(0);L1183:do{if((Z|0)==792){Z=0;if((U|0)==3){aa=R;ab=S;ac=T;ad=r;Z=1007;break L890}else{bo=$}while(1){ar=c[f>>2]|0;do{if((ar|0)==0){bp=1}else{av=c[ar+12>>2]|0;if((av|0)==(c[ar+16>>2]|0)){bq=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{bq=c[av>>2]|0}if((bq|0)==-1){c[f>>2]=0;bp=1;break}else{bp=(c[f>>2]|0)==0;break}}}while(0);do{if((bo|0)==0){Z=806}else{ar=c[bo+12>>2]|0;if((ar|0)==(c[bo+16>>2]|0)){br=b0[c[(c[bo>>2]|0)+36>>2]&127](bo)|0}else{br=c[ar>>2]|0}if((br|0)==-1){c[b>>2]=0;Z=806;break}else{if(bp^(bo|0)==0){bs=bo;break}else{af=r;ag=o;ah=T;ai=S;aj=R;break L1183}}}}while(0);if((Z|0)==806){Z=0;if(bp){af=r;ag=o;ah=T;ai=S;aj=R;break L1183}else{bs=0}}ar=c[f>>2]|0;av=c[ar+12>>2]|0;if((av|0)==(c[ar+16>>2]|0)){bt=b0[c[(c[ar>>2]|0)+36>>2]&127](ar)|0}else{bt=c[av>>2]|0}if(!(b1[c[(c[e>>2]|0)+12>>2]&63](l,8192,bt)|0)){af=r;ag=o;ah=T;ai=S;aj=R;break L1183}av=c[f>>2]|0;ar=av+12|0;ap=c[ar>>2]|0;if((ap|0)==(c[av+16>>2]|0)){bu=b0[c[(c[av>>2]|0)+40>>2]&127](av)|0}else{c[ar>>2]=ap+4;bu=c[ap>>2]|0}eh(A,bu);bo=bs}}}while(0);at=U+1|0;if(at>>>0<4){R=aj;S=ai;T=ah;o=ag;r=af;U=at}else{aa=aj;ab=ai;ac=ah;ad=af;Z=1007;break}}L1220:do{if((Z|0)==857){c[j>>2]=c[j>>2]|4;bv=0;bw=S;bx=R}else if((Z|0)==901){c[j>>2]=c[j>>2]|4;bv=0;bw=S;bx=R}else if((Z|0)==970){c[j>>2]=c[j>>2]|4;bv=0;bw=aW;bx=aX}else if((Z|0)==994){c[j>>2]=c[j>>2]|4;bv=0;bw=aW;bx=aX}else if((Z|0)==1005){c[j>>2]=c[j>>2]|4;bv=0;bw=aW;bx=aX}else if((Z|0)==1007){L1227:do{if((ad|0)!=0){af=ad;ah=ad+4|0;ai=ad+8|0;aj=1;L1229:while(1){U=d[af]|0;if((U&1|0)==0){by=U>>>1}else{by=c[ah>>2]|0}if(aj>>>0>=by>>>0){break L1227}U=c[f>>2]|0;do{if((U|0)==0){bz=1}else{r=c[U+12>>2]|0;if((r|0)==(c[U+16>>2]|0)){bA=b0[c[(c[U>>2]|0)+36>>2]&127](U)|0}else{bA=c[r>>2]|0}if((bA|0)==-1){c[f>>2]=0;bz=1;break}else{bz=(c[f>>2]|0)==0;break}}}while(0);U=c[b>>2]|0;do{if((U|0)==0){Z=1026}else{r=c[U+12>>2]|0;if((r|0)==(c[U+16>>2]|0)){bB=b0[c[(c[U>>2]|0)+36>>2]&127](U)|0}else{bB=c[r>>2]|0}if((bB|0)==-1){c[b>>2]=0;Z=1026;break}else{if(bz^(U|0)==0){break}else{break L1229}}}}while(0);if((Z|0)==1026){Z=0;if(bz){break}}U=c[f>>2]|0;r=c[U+12>>2]|0;if((r|0)==(c[U+16>>2]|0)){bC=b0[c[(c[U>>2]|0)+36>>2]&127](U)|0}else{bC=c[r>>2]|0}if((a[af]&1)==0){bD=ah}else{bD=c[ai>>2]|0}if((bC|0)!=(c[bD+(aj<<2)>>2]|0)){break}r=aj+1|0;U=c[f>>2]|0;ag=U+12|0;o=c[ag>>2]|0;if((o|0)==(c[U+16>>2]|0)){T=c[(c[U>>2]|0)+40>>2]|0;b0[T&127](U)|0;aj=r;continue}else{c[ag>>2]=o+4;aj=r;continue}}c[j>>2]=c[j>>2]|4;bv=0;bw=ab;bx=aa;break L1220}}while(0);if((ab|0)==(ac|0)){bv=1;bw=ac;bx=aa;break}c[C>>2]=0;ft(v,ab,ac,C);if((c[C>>2]|0)==0){bv=1;bw=ab;bx=aa;break}c[j>>2]=c[j>>2]|4;bv=0;bw=ab;bx=aa}else if((Z|0)==791){c[j>>2]=c[j>>2]|4;bv=0;bw=S;bx=R}}while(0);ee(A);ee(z);ee(y);ee(x);dP(v);if((bw|0)==0){i=p;return bv|0}bY[bx&511](bw);i=p;return bv|0}function h_(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0;f=b;g=d;h=a[f]|0;i=h&255;if((i&1|0)==0){j=i>>>1}else{j=c[b+4>>2]|0}if((h&1)==0){k=1;l=h}else{h=c[b>>2]|0;k=(h&-2)-1|0;l=h&255}h=e-g>>2;if((h|0)==0){return b|0}if((k-j|0)>>>0<h>>>0){eB(b,k,j+h-k|0,j,j,0,0);m=a[f]|0}else{m=l}if((m&1)==0){n=b+4|0}else{n=c[b+8>>2]|0}m=n+(j<<2)|0;if((d|0)==(e|0)){o=m}else{l=j+((e-4+(-g|0)|0)>>>2)+1|0;g=d;d=m;while(1){c[d>>2]=c[g>>2];m=g+4|0;if((m|0)==(e|0)){break}else{g=m;d=d+4|0}}o=n+(l<<2)|0}c[o>>2]=0;o=j+h|0;if((a[f]&1)==0){a[f]=o<<1&255;return b|0}else{c[b+4>>2]=o;return b|0}return 0}function h$(a){a=a|0;de(a|0);kS(a);return}function h0(a){a=a|0;de(a|0);return}function h1(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;d=i;i=i+456|0;l=e;e=i;i=i+4|0;i=i+7>>3<<3;c[e>>2]=c[l>>2];l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=d|0;m=d+16|0;n=d+416|0;o=d+424|0;p=d+432|0;q=d+440|0;r=d+448|0;s=n|0;c[s>>2]=m;t=n+4|0;c[t>>2]=166;u=m+400|0;ej(p,h);m=p|0;v=c[m>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;w=c[v+8>>2]|0;do{if((c[v+12>>2]|0)-w>>2>>>0>l>>>0){x=c[w+(l<<2)>>2]|0;if((x|0)==0){break}y=x;a[q]=0;z=f|0;A=c[z>>2]|0;c[r>>2]=A;if(hZ(e,r,g,p,c[h+4>>2]|0,j,q,y,n,o,u)|0){B=k;if((a[B]&1)==0){c[k+4>>2]=0;a[B]=0}else{c[c[k+8>>2]>>2]=0;c[k+4>>2]=0}B=x;if((a[q]&1)!=0){eh(k,b_[c[(c[B>>2]|0)+44>>2]&31](y,45)|0)}x=b_[c[(c[B>>2]|0)+44>>2]&31](y,48)|0;y=c[o>>2]|0;B=y-4|0;C=c[s>>2]|0;while(1){if(C>>>0>=B>>>0){break}if((c[C>>2]|0)==(x|0)){C=C+4|0}else{break}}h_(k,C,y)|0}x=e|0;B=c[x>>2]|0;do{if((B|0)==0){D=0}else{E=c[B+12>>2]|0;if((E|0)==(c[B+16>>2]|0)){F=b0[c[(c[B>>2]|0)+36>>2]&127](B)|0}else{F=c[E>>2]|0}if((F|0)!=-1){D=B;break}c[x>>2]=0;D=0}}while(0);x=(D|0)==0;do{if((A|0)==0){G=1105}else{B=c[A+12>>2]|0;if((B|0)==(c[A+16>>2]|0)){H=b0[c[(c[A>>2]|0)+36>>2]&127](A)|0}else{H=c[B>>2]|0}if((H|0)==-1){c[z>>2]=0;G=1105;break}else{if(x^(A|0)==0){break}else{G=1107;break}}}}while(0);if((G|0)==1105){if(x){G=1107}}if((G|0)==1107){c[j>>2]=c[j>>2]|2}c[b>>2]=D;A=c[m>>2]|0;dz(A)|0;A=c[s>>2]|0;c[s>>2]=0;if((A|0)==0){i=d;return}bY[c[t>>2]&511](A);i=d;return}}while(0);d=bO(4)|0;kq(d);bl(d|0,8176,134)}function h2(b,d,e,f,g,h,j,k,l,m){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0;n=i;i=i+56|0;o=n|0;p=n+16|0;q=n+32|0;r=n+40|0;s=r;t=i;i=i+12|0;i=i+7>>3<<3;u=t;v=i;i=i+12|0;i=i+7>>3<<3;w=v;x=i;i=i+12|0;i=i+7>>3<<3;y=x;z=i;i=i+4|0;i=i+7>>3<<3;A=i;i=i+12|0;i=i+7>>3<<3;B=A;D=i;i=i+12|0;i=i+7>>3<<3;E=D;F=i;i=i+12|0;i=i+7>>3<<3;G=F;H=i;i=i+12|0;i=i+7>>3<<3;I=H;if(b){b=c[d>>2]|0;if((c[4040]|0)!=-1){c[p>>2]=16160;c[p+4>>2]=12;c[p+8>>2]=0;dW(16160,p,96)}p=(c[4041]|0)-1|0;J=c[b+8>>2]|0;if((c[b+12>>2]|0)-J>>2>>>0<=p>>>0){K=bO(4)|0;L=K;kq(L);bl(K|0,8176,134)}b=c[J+(p<<2)>>2]|0;if((b|0)==0){K=bO(4)|0;L=K;kq(L);bl(K|0,8176,134)}K=b;bZ[c[(c[b>>2]|0)+44>>2]&127](q,K);L=e;C=c[q>>2]|0;a[L]=C&255;C=C>>8;a[L+1|0]=C&255;C=C>>8;a[L+2|0]=C&255;C=C>>8;a[L+3|0]=C&255;L=b;bZ[c[(c[L>>2]|0)+32>>2]&127](r,K);q=l;if((a[q]&1)==0){c[l+4>>2]=0;a[q]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[q>>2]=c[s>>2];c[q+4>>2]=c[s+4>>2];c[q+8>>2]=c[s+8>>2];k$(s|0,0,12);ee(r);bZ[c[(c[L>>2]|0)+28>>2]&127](t,K);r=k;if((a[r]&1)==0){c[k+4>>2]=0;a[r]=0}else{c[c[k+8>>2]>>2]=0;c[k+4>>2]=0}ez(k,0);c[r>>2]=c[u>>2];c[r+4>>2]=c[u+4>>2];c[r+8>>2]=c[u+8>>2];k$(u|0,0,12);ee(t);t=b;c[f>>2]=b0[c[(c[t>>2]|0)+12>>2]&127](K)|0;c[g>>2]=b0[c[(c[t>>2]|0)+16>>2]&127](K)|0;bZ[c[(c[b>>2]|0)+20>>2]&127](v,K);b=h;if((a[b]&1)==0){a[h+1|0]=0;a[b]=0}else{a[c[h+8>>2]|0]=0;c[h+4>>2]=0}d$(h,0);c[b>>2]=c[w>>2];c[b+4>>2]=c[w+4>>2];c[b+8>>2]=c[w+8>>2];k$(w|0,0,12);dP(v);bZ[c[(c[L>>2]|0)+24>>2]&127](x,K);L=j;if((a[L]&1)==0){c[j+4>>2]=0;a[L]=0}else{c[c[j+8>>2]>>2]=0;c[j+4>>2]=0}ez(j,0);c[L>>2]=c[y>>2];c[L+4>>2]=c[y+4>>2];c[L+8>>2]=c[y+8>>2];k$(y|0,0,12);ee(x);M=b0[c[(c[t>>2]|0)+36>>2]&127](K)|0;c[m>>2]=M;i=n;return}else{K=c[d>>2]|0;if((c[4042]|0)!=-1){c[o>>2]=16168;c[o+4>>2]=12;c[o+8>>2]=0;dW(16168,o,96)}o=(c[4043]|0)-1|0;d=c[K+8>>2]|0;if((c[K+12>>2]|0)-d>>2>>>0<=o>>>0){N=bO(4)|0;O=N;kq(O);bl(N|0,8176,134)}K=c[d+(o<<2)>>2]|0;if((K|0)==0){N=bO(4)|0;O=N;kq(O);bl(N|0,8176,134)}N=K;bZ[c[(c[K>>2]|0)+44>>2]&127](z,N);O=e;C=c[z>>2]|0;a[O]=C&255;C=C>>8;a[O+1|0]=C&255;C=C>>8;a[O+2|0]=C&255;C=C>>8;a[O+3|0]=C&255;O=K;bZ[c[(c[O>>2]|0)+32>>2]&127](A,N);z=l;if((a[z]&1)==0){c[l+4>>2]=0;a[z]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[z>>2]=c[B>>2];c[z+4>>2]=c[B+4>>2];c[z+8>>2]=c[B+8>>2];k$(B|0,0,12);ee(A);bZ[c[(c[O>>2]|0)+28>>2]&127](D,N);A=k;if((a[A]&1)==0){c[k+4>>2]=0;a[A]=0}else{c[c[k+8>>2]>>2]=0;c[k+4>>2]=0}ez(k,0);c[A>>2]=c[E>>2];c[A+4>>2]=c[E+4>>2];c[A+8>>2]=c[E+8>>2];k$(E|0,0,12);ee(D);D=K;c[f>>2]=b0[c[(c[D>>2]|0)+12>>2]&127](N)|0;c[g>>2]=b0[c[(c[D>>2]|0)+16>>2]&127](N)|0;bZ[c[(c[K>>2]|0)+20>>2]&127](F,N);K=h;if((a[K]&1)==0){a[h+1|0]=0;a[K]=0}else{a[c[h+8>>2]|0]=0;c[h+4>>2]=0}d$(h,0);c[K>>2]=c[G>>2];c[K+4>>2]=c[G+4>>2];c[K+8>>2]=c[G+8>>2];k$(G|0,0,12);dP(F);bZ[c[(c[O>>2]|0)+24>>2]&127](H,N);O=j;if((a[O]&1)==0){c[j+4>>2]=0;a[O]=0}else{c[c[j+8>>2]>>2]=0;c[j+4>>2]=0}ez(j,0);c[O>>2]=c[I>>2];c[O+4>>2]=c[I+4>>2];c[O+8>>2]=c[I+8>>2];k$(I|0,0,12);ee(H);M=b0[c[(c[D>>2]|0)+36>>2]&127](N)|0;c[m>>2]=M;i=n;return}}function h3(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0;e=a+4|0;f=(c[e>>2]|0)!=166;g=a|0;a=c[g>>2]|0;h=a;i=(c[d>>2]|0)-h|0;j=i>>>0<2147483647?i<<1:-1;i=(c[b>>2]|0)-h>>2;if(f){k=a}else{k=0}a=kL(k,j)|0;k=a;if((a|0)==0){kX()}do{if(f){c[g>>2]=k;l=k}else{a=c[g>>2]|0;c[g>>2]=k;if((a|0)==0){l=k;break}bY[c[e>>2]&511](a);l=c[g>>2]|0}}while(0);c[e>>2]=82;c[b>>2]=l+(i<<2);c[d>>2]=(c[g>>2]|0)+(j>>>2<<2);return}function h4(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=+l;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0;e=i;i=i+280|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=e|0;n=e+120|0;o=e+232|0;p=e+240|0;q=e+248|0;r=e+256|0;s=e+264|0;t=s;u=i;i=i+12|0;i=i+7>>3<<3;v=u;w=i;i=i+12|0;i=i+7>>3<<3;x=w;y=i;i=i+4|0;i=i+7>>3<<3;z=i;i=i+100|0;i=i+7>>3<<3;A=i;i=i+4|0;i=i+7>>3<<3;B=i;i=i+4|0;i=i+7>>3<<3;C=i;i=i+4|0;i=i+7>>3<<3;D=e+16|0;c[n>>2]=D;E=e+128|0;F=aZ(D|0,100,1360,(D=i,i=i+8|0,h[D>>3]=l,D)|0)|0;i=D;do{if(F>>>0>99){do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);G=gh(n,c[2958]|0,1360,(D=i,i=i+8|0,h[D>>3]=l,D)|0)|0;i=D;H=c[n>>2]|0;if((H|0)==0){kX();I=c[n>>2]|0}else{I=H}H=kJ(G)|0;if((H|0)!=0){J=H;K=G;L=I;M=H;break}kX();J=0;K=G;L=I;M=0}else{J=E;K=F;L=0;M=0}}while(0);ej(o,j);F=o|0;E=c[F>>2]|0;if((c[3924]|0)!=-1){c[m>>2]=15696;c[m+4>>2]=12;c[m+8>>2]=0;dW(15696,m,96)}m=(c[3925]|0)-1|0;I=c[E+8>>2]|0;do{if((c[E+12>>2]|0)-I>>2>>>0>m>>>0){D=c[I+(m<<2)>>2]|0;if((D|0)==0){break}G=D;H=c[n>>2]|0;N=H+K|0;O=c[(c[D>>2]|0)+32>>2]|0;ca[O&15](G,H,N,J)|0;if((K|0)==0){P=0}else{P=(a[c[n>>2]|0]|0)==45}k$(t|0,0,12);k$(v|0,0,12);k$(x|0,0,12);h5(g,P,o,p,q,r,s,u,w,y);N=z|0;H=c[y>>2]|0;if((K|0)>(H|0)){O=d[x]|0;if((O&1|0)==0){Q=O>>>1}else{Q=c[w+4>>2]|0}O=d[v]|0;if((O&1|0)==0){R=O>>>1}else{R=c[u+4>>2]|0}S=(K-H<<1|1)+Q+R|0}else{O=d[x]|0;if((O&1|0)==0){T=O>>>1}else{T=c[w+4>>2]|0}O=d[v]|0;if((O&1|0)==0){U=O>>>1}else{U=c[u+4>>2]|0}S=T+2+U|0}O=S+H|0;do{if(O>>>0>100){D=kJ(O)|0;if((D|0)!=0){V=D;W=D;break}kX();V=0;W=0}else{V=N;W=0}}while(0);h6(V,A,B,c[j+4>>2]|0,J,J+K|0,G,P,p,a[q]|0,a[r]|0,s,u,w,H);c[C>>2]=c[f>>2];f9(b,C,V,c[A>>2]|0,c[B>>2]|0,j,k);if((W|0)!=0){kK(W)}dP(w);dP(u);dP(s);N=c[F>>2]|0;dz(N)|0;if((M|0)!=0){kK(M)}if((L|0)==0){i=e;return}kK(L);i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function h5(b,d,e,f,g,h,j,k,l,m){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;n=i;i=i+40|0;o=n|0;p=n+16|0;q=n+32|0;r=q;s=i;i=i+12|0;i=i+7>>3<<3;t=s;u=i;i=i+4|0;i=i+7>>3<<3;v=u;w=i;i=i+12|0;i=i+7>>3<<3;x=w;y=i;i=i+12|0;i=i+7>>3<<3;z=y;A=i;i=i+12|0;i=i+7>>3<<3;B=A;D=i;i=i+4|0;i=i+7>>3<<3;E=D;F=i;i=i+12|0;i=i+7>>3<<3;G=F;H=i;i=i+4|0;i=i+7>>3<<3;I=H;J=i;i=i+12|0;i=i+7>>3<<3;K=J;L=i;i=i+12|0;i=i+7>>3<<3;M=L;N=i;i=i+12|0;i=i+7>>3<<3;O=N;P=c[e>>2]|0;if(b){if((c[4044]|0)!=-1){c[p>>2]=16176;c[p+4>>2]=12;c[p+8>>2]=0;dW(16176,p,96)}p=(c[4045]|0)-1|0;b=c[P+8>>2]|0;if((c[P+12>>2]|0)-b>>2>>>0<=p>>>0){Q=bO(4)|0;R=Q;kq(R);bl(Q|0,8176,134)}e=c[b+(p<<2)>>2]|0;if((e|0)==0){Q=bO(4)|0;R=Q;kq(R);bl(Q|0,8176,134)}Q=e;R=c[e>>2]|0;if(d){bZ[c[R+44>>2]&127](r,Q);r=f;C=c[q>>2]|0;a[r]=C&255;C=C>>8;a[r+1|0]=C&255;C=C>>8;a[r+2|0]=C&255;C=C>>8;a[r+3|0]=C&255;bZ[c[(c[e>>2]|0)+32>>2]&127](s,Q);r=l;if((a[r]&1)==0){a[l+1|0]=0;a[r]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[r>>2]=c[t>>2];c[r+4>>2]=c[t+4>>2];c[r+8>>2]=c[t+8>>2];k$(t|0,0,12);dP(s)}else{bZ[c[R+40>>2]&127](v,Q);v=f;C=c[u>>2]|0;a[v]=C&255;C=C>>8;a[v+1|0]=C&255;C=C>>8;a[v+2|0]=C&255;C=C>>8;a[v+3|0]=C&255;bZ[c[(c[e>>2]|0)+28>>2]&127](w,Q);v=l;if((a[v]&1)==0){a[l+1|0]=0;a[v]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[v>>2]=c[x>>2];c[v+4>>2]=c[x+4>>2];c[v+8>>2]=c[x+8>>2];k$(x|0,0,12);dP(w)}w=e;a[g]=b0[c[(c[w>>2]|0)+12>>2]&127](Q)|0;a[h]=b0[c[(c[w>>2]|0)+16>>2]&127](Q)|0;w=e;bZ[c[(c[w>>2]|0)+20>>2]&127](y,Q);x=j;if((a[x]&1)==0){a[j+1|0]=0;a[x]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[x>>2]=c[z>>2];c[x+4>>2]=c[z+4>>2];c[x+8>>2]=c[z+8>>2];k$(z|0,0,12);dP(y);bZ[c[(c[w>>2]|0)+24>>2]&127](A,Q);w=k;if((a[w]&1)==0){a[k+1|0]=0;a[w]=0}else{a[c[k+8>>2]|0]=0;c[k+4>>2]=0}d$(k,0);c[w>>2]=c[B>>2];c[w+4>>2]=c[B+4>>2];c[w+8>>2]=c[B+8>>2];k$(B|0,0,12);dP(A);S=b0[c[(c[e>>2]|0)+36>>2]&127](Q)|0;c[m>>2]=S;i=n;return}else{if((c[4046]|0)!=-1){c[o>>2]=16184;c[o+4>>2]=12;c[o+8>>2]=0;dW(16184,o,96)}o=(c[4047]|0)-1|0;Q=c[P+8>>2]|0;if((c[P+12>>2]|0)-Q>>2>>>0<=o>>>0){T=bO(4)|0;U=T;kq(U);bl(T|0,8176,134)}P=c[Q+(o<<2)>>2]|0;if((P|0)==0){T=bO(4)|0;U=T;kq(U);bl(T|0,8176,134)}T=P;U=c[P>>2]|0;if(d){bZ[c[U+44>>2]&127](E,T);E=f;C=c[D>>2]|0;a[E]=C&255;C=C>>8;a[E+1|0]=C&255;C=C>>8;a[E+2|0]=C&255;C=C>>8;a[E+3|0]=C&255;bZ[c[(c[P>>2]|0)+32>>2]&127](F,T);E=l;if((a[E]&1)==0){a[l+1|0]=0;a[E]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[E>>2]=c[G>>2];c[E+4>>2]=c[G+4>>2];c[E+8>>2]=c[G+8>>2];k$(G|0,0,12);dP(F)}else{bZ[c[U+40>>2]&127](I,T);I=f;C=c[H>>2]|0;a[I]=C&255;C=C>>8;a[I+1|0]=C&255;C=C>>8;a[I+2|0]=C&255;C=C>>8;a[I+3|0]=C&255;bZ[c[(c[P>>2]|0)+28>>2]&127](J,T);I=l;if((a[I]&1)==0){a[l+1|0]=0;a[I]=0}else{a[c[l+8>>2]|0]=0;c[l+4>>2]=0}d$(l,0);c[I>>2]=c[K>>2];c[I+4>>2]=c[K+4>>2];c[I+8>>2]=c[K+8>>2];k$(K|0,0,12);dP(J)}J=P;a[g]=b0[c[(c[J>>2]|0)+12>>2]&127](T)|0;a[h]=b0[c[(c[J>>2]|0)+16>>2]&127](T)|0;J=P;bZ[c[(c[J>>2]|0)+20>>2]&127](L,T);h=j;if((a[h]&1)==0){a[j+1|0]=0;a[h]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[h>>2]=c[M>>2];c[h+4>>2]=c[M+4>>2];c[h+8>>2]=c[M+8>>2];k$(M|0,0,12);dP(L);bZ[c[(c[J>>2]|0)+24>>2]&127](N,T);J=k;if((a[J]&1)==0){a[k+1|0]=0;a[J]=0}else{a[c[k+8>>2]|0]=0;c[k+4>>2]=0}d$(k,0);c[J>>2]=c[O>>2];c[J+4>>2]=c[O+4>>2];c[J+8>>2]=c[O+8>>2];k$(O|0,0,12);dP(N);S=b0[c[(c[P>>2]|0)+36>>2]&127](T)|0;c[m>>2]=S;i=n;return}}function h6(d,e,f,g,h,i,j,k,l,m,n,o,p,q,r){d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;p=p|0;q=q|0;r=r|0;var s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0;c[f>>2]=d;s=j;t=q;u=q+1|0;v=q+8|0;w=q+4|0;q=p;x=(g&512|0)==0;y=p+1|0;z=p+4|0;A=p+8|0;p=j+8|0;B=(r|0)>0;C=o;D=o+1|0;E=o+8|0;F=o+4|0;o=-r|0;G=h;h=0;while(1){L1580:do{switch(a[l+h|0]|0){case 0:{c[e>>2]=c[f>>2];H=G;break};case 1:{c[e>>2]=c[f>>2];I=b_[c[(c[s>>2]|0)+28>>2]&31](j,32)|0;J=c[f>>2]|0;c[f>>2]=J+1;a[J]=I;H=G;break};case 3:{I=a[t]|0;J=I&255;if((J&1|0)==0){K=J>>>1}else{K=c[w>>2]|0}if((K|0)==0){H=G;break L1580}if((I&1)==0){L=u}else{L=c[v>>2]|0}I=a[L]|0;J=c[f>>2]|0;c[f>>2]=J+1;a[J]=I;H=G;break};case 2:{I=a[q]|0;J=I&255;M=(J&1|0)==0;if(M){N=J>>>1}else{N=c[z>>2]|0}if((N|0)==0|x){H=G;break L1580}if((I&1)==0){O=y;P=y}else{I=c[A>>2]|0;O=I;P=I}if(M){Q=J>>>1}else{Q=c[z>>2]|0}J=O+Q|0;M=c[f>>2]|0;if((P|0)==(J|0)){R=M}else{I=P;S=M;while(1){a[S]=a[I]|0;M=I+1|0;T=S+1|0;if((M|0)==(J|0)){R=T;break}else{I=M;S=T}}}c[f>>2]=R;H=G;break};case 4:{S=c[f>>2]|0;I=k?G+1|0:G;J=I;while(1){if(J>>>0>=i>>>0){break}T=a[J]|0;if(T<<24>>24<=-1){break}if((b[(c[p>>2]|0)+(T<<24>>24<<1)>>1]&2048)==0){break}else{J=J+1|0}}T=J;if(B){if(J>>>0>I>>>0){M=I+(-T|0)|0;T=M>>>0<o>>>0?o:M;M=T+r|0;U=J;V=r;W=S;while(1){X=U-1|0;Y=a[X]|0;c[f>>2]=W+1;a[W]=Y;Y=V-1|0;Z=(Y|0)>0;if(!(X>>>0>I>>>0&Z)){break}U=X;V=Y;W=c[f>>2]|0}W=J+T|0;if(Z){_=M;$=W;aa=1354}else{ab=0;ac=M;ad=W}}else{_=r;$=J;aa=1354}if((aa|0)==1354){aa=0;ab=b_[c[(c[s>>2]|0)+28>>2]&31](j,48)|0;ac=_;ad=$}W=c[f>>2]|0;c[f>>2]=W+1;if((ac|0)>0){V=ac;U=W;while(1){a[U]=ab;Y=V-1|0;X=c[f>>2]|0;c[f>>2]=X+1;if((Y|0)>0){V=Y;U=X}else{ae=X;break}}}else{ae=W}a[ae]=m;af=ad}else{af=J}if((af|0)==(I|0)){U=b_[c[(c[s>>2]|0)+28>>2]&31](j,48)|0;V=c[f>>2]|0;c[f>>2]=V+1;a[V]=U}else{U=a[C]|0;V=U&255;if((V&1|0)==0){ag=V>>>1}else{ag=c[F>>2]|0}if((ag|0)==0){ah=af;ai=0;aj=0;ak=-1}else{if((U&1)==0){al=D}else{al=c[E>>2]|0}ah=af;ai=0;aj=0;ak=a[al]|0}while(1){do{if((ai|0)==(ak|0)){U=c[f>>2]|0;c[f>>2]=U+1;a[U]=n;U=aj+1|0;V=a[C]|0;M=V&255;if((M&1|0)==0){am=M>>>1}else{am=c[F>>2]|0}if(U>>>0>=am>>>0){an=ak;ao=U;ap=0;break}M=(V&1)==0;if(M){aq=D}else{aq=c[E>>2]|0}if((a[aq+U|0]|0)==127){an=-1;ao=U;ap=0;break}if(M){ar=D}else{ar=c[E>>2]|0}an=a[ar+U|0]|0;ao=U;ap=0}else{an=ak;ao=aj;ap=ai}}while(0);U=ah-1|0;M=a[U]|0;V=c[f>>2]|0;c[f>>2]=V+1;a[V]=M;if((U|0)==(I|0)){break}else{ah=U;ai=ap+1|0;aj=ao;ak=an}}}J=c[f>>2]|0;if((S|0)==(J|0)){H=I;break L1580}W=J-1|0;if(S>>>0<W>>>0){as=S;at=W}else{H=I;break L1580}while(1){W=a[as]|0;a[as]=a[at]|0;a[at]=W;W=as+1|0;J=at-1|0;if(W>>>0<J>>>0){as=W;at=J}else{H=I;break}}break};default:{H=G}}}while(0);I=h+1|0;if(I>>>0<4){G=H;h=I}else{break}}h=a[t]|0;t=h&255;H=(t&1|0)==0;if(H){au=t>>>1}else{au=c[w>>2]|0}if(au>>>0>1){if((h&1)==0){av=u;aw=u}else{u=c[v>>2]|0;av=u;aw=u}if(H){ax=t>>>1}else{ax=c[w>>2]|0}w=av+ax|0;ax=c[f>>2]|0;av=aw+1|0;if((av|0)==(w|0)){ay=ax}else{aw=ax;ax=av;while(1){a[aw]=a[ax]|0;av=aw+1|0;t=ax+1|0;if((t|0)==(w|0)){ay=av;break}else{aw=av;ax=t}}}c[f>>2]=ay}switch(g&176|0){case 16:{return};case 32:{c[e>>2]=c[f>>2];return};default:{c[e>>2]=d;return}}}function h7(a){a=a|0;de(a|0);kS(a);return}function h8(a){a=a|0;de(a|0);return}function h9(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0;e=i;i=i+64|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=e|0;m=e+16|0;n=e+24|0;o=e+32|0;p=e+40|0;q=e+48|0;r=q;s=i;i=i+12|0;i=i+7>>3<<3;t=s;u=i;i=i+12|0;i=i+7>>3<<3;v=u;w=i;i=i+4|0;i=i+7>>3<<3;x=i;i=i+100|0;i=i+7>>3<<3;y=i;i=i+4|0;i=i+7>>3<<3;z=i;i=i+4|0;i=i+7>>3<<3;A=i;i=i+4|0;i=i+7>>3<<3;ej(m,h);B=m|0;C=c[B>>2]|0;if((c[3924]|0)!=-1){c[l>>2]=15696;c[l+4>>2]=12;c[l+8>>2]=0;dW(15696,l,96)}l=(c[3925]|0)-1|0;D=c[C+8>>2]|0;do{if((c[C+12>>2]|0)-D>>2>>>0>l>>>0){E=c[D+(l<<2)>>2]|0;if((E|0)==0){break}F=E;G=k;H=k;I=a[H]|0;J=I&255;if((J&1|0)==0){K=J>>>1}else{K=c[k+4>>2]|0}if((K|0)==0){L=0}else{if((I&1)==0){M=G+1|0}else{M=c[k+8>>2]|0}I=a[M]|0;L=I<<24>>24==(b_[c[(c[E>>2]|0)+28>>2]&31](F,45)|0)<<24>>24}k$(r|0,0,12);k$(t|0,0,12);k$(v|0,0,12);h5(g,L,m,n,o,p,q,s,u,w);E=x|0;I=a[H]|0;J=I&255;N=(J&1|0)==0;if(N){O=J>>>1}else{O=c[k+4>>2]|0}P=c[w>>2]|0;if((O|0)>(P|0)){if(N){Q=J>>>1}else{Q=c[k+4>>2]|0}J=d[v]|0;if((J&1|0)==0){R=J>>>1}else{R=c[u+4>>2]|0}J=d[t]|0;if((J&1|0)==0){S=J>>>1}else{S=c[s+4>>2]|0}T=(Q-P<<1|1)+R+S|0}else{J=d[v]|0;if((J&1|0)==0){U=J>>>1}else{U=c[u+4>>2]|0}J=d[t]|0;if((J&1|0)==0){V=J>>>1}else{V=c[s+4>>2]|0}T=U+2+V|0}J=T+P|0;do{if(J>>>0>100){N=kJ(J)|0;if((N|0)!=0){W=N;X=N;Y=I;break}kX();W=0;X=0;Y=a[H]|0}else{W=E;X=0;Y=I}}while(0);if((Y&1)==0){Z=G+1|0;_=G+1|0}else{I=c[k+8>>2]|0;Z=I;_=I}I=Y&255;if((I&1|0)==0){$=I>>>1}else{$=c[k+4>>2]|0}h6(W,y,z,c[h+4>>2]|0,_,Z+$|0,F,L,n,a[o]|0,a[p]|0,q,s,u,P);c[A>>2]=c[f>>2];f9(b,A,W,c[y>>2]|0,c[z>>2]|0,h,j);if((X|0)==0){dP(u);dP(s);dP(q);aa=c[B>>2]|0;ab=aa|0;ac=dz(ab)|0;i=e;return}kK(X);dP(u);dP(s);dP(q);aa=c[B>>2]|0;ab=aa|0;ac=dz(ab)|0;i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function ia(b,e,f,g,j,k,l){b=b|0;e=e|0;f=f|0;g=g|0;j=j|0;k=k|0;l=+l;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0;e=i;i=i+576|0;m=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[m>>2];m=e|0;n=e+120|0;o=e+528|0;p=e+536|0;q=e+544|0;r=e+552|0;s=e+560|0;t=s;u=i;i=i+12|0;i=i+7>>3<<3;v=u;w=i;i=i+12|0;i=i+7>>3<<3;x=w;y=i;i=i+4|0;i=i+7>>3<<3;z=i;i=i+400|0;A=i;i=i+4|0;i=i+7>>3<<3;B=i;i=i+4|0;i=i+7>>3<<3;C=i;i=i+4|0;i=i+7>>3<<3;D=e+16|0;c[n>>2]=D;E=e+128|0;F=aZ(D|0,100,1360,(D=i,i=i+8|0,h[D>>3]=l,D)|0)|0;i=D;do{if(F>>>0>99){do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);G=gh(n,c[2958]|0,1360,(D=i,i=i+8|0,h[D>>3]=l,D)|0)|0;i=D;H=c[n>>2]|0;if((H|0)==0){kX();I=c[n>>2]|0}else{I=H}H=kJ(G<<2)|0;J=H;if((H|0)!=0){K=J;L=G;M=I;N=J;break}kX();K=J;L=G;M=I;N=J}else{K=E;L=F;M=0;N=0}}while(0);ej(o,j);F=o|0;E=c[F>>2]|0;if((c[3922]|0)!=-1){c[m>>2]=15688;c[m+4>>2]=12;c[m+8>>2]=0;dW(15688,m,96)}m=(c[3923]|0)-1|0;I=c[E+8>>2]|0;do{if((c[E+12>>2]|0)-I>>2>>>0>m>>>0){D=c[I+(m<<2)>>2]|0;if((D|0)==0){break}J=D;G=c[n>>2]|0;H=G+L|0;O=c[(c[D>>2]|0)+48>>2]|0;ca[O&15](J,G,H,K)|0;if((L|0)==0){P=0}else{P=(a[c[n>>2]|0]|0)==45}k$(t|0,0,12);k$(v|0,0,12);k$(x|0,0,12);ib(g,P,o,p,q,r,s,u,w,y);H=z|0;G=c[y>>2]|0;if((L|0)>(G|0)){O=d[x]|0;if((O&1|0)==0){Q=O>>>1}else{Q=c[w+4>>2]|0}O=d[v]|0;if((O&1|0)==0){R=O>>>1}else{R=c[u+4>>2]|0}S=(L-G<<1|1)+Q+R|0}else{O=d[x]|0;if((O&1|0)==0){T=O>>>1}else{T=c[w+4>>2]|0}O=d[v]|0;if((O&1|0)==0){U=O>>>1}else{U=c[u+4>>2]|0}S=T+2+U|0}O=S+G|0;do{if(O>>>0>100){D=kJ(O<<2)|0;V=D;if((D|0)!=0){W=V;X=V;break}kX();W=V;X=V}else{W=H;X=0}}while(0);ic(W,A,B,c[j+4>>2]|0,K,K+(L<<2)|0,J,P,p,c[q>>2]|0,c[r>>2]|0,s,u,w,G);c[C>>2]=c[f>>2];gq(b,C,W,c[A>>2]|0,c[B>>2]|0,j,k);if((X|0)!=0){kK(X)}ee(w);ee(u);dP(s);H=c[F>>2]|0;dz(H)|0;if((N|0)!=0){kK(N)}if((M|0)==0){i=e;return}kK(M);i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function ib(b,d,e,f,g,h,j,k,l,m){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;l=l|0;m=m|0;var n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0;n=i;i=i+40|0;o=n|0;p=n+16|0;q=n+32|0;r=q;s=i;i=i+12|0;i=i+7>>3<<3;t=s;u=i;i=i+4|0;i=i+7>>3<<3;v=u;w=i;i=i+12|0;i=i+7>>3<<3;x=w;y=i;i=i+12|0;i=i+7>>3<<3;z=y;A=i;i=i+12|0;i=i+7>>3<<3;B=A;D=i;i=i+4|0;i=i+7>>3<<3;E=D;F=i;i=i+12|0;i=i+7>>3<<3;G=F;H=i;i=i+4|0;i=i+7>>3<<3;I=H;J=i;i=i+12|0;i=i+7>>3<<3;K=J;L=i;i=i+12|0;i=i+7>>3<<3;M=L;N=i;i=i+12|0;i=i+7>>3<<3;O=N;P=c[e>>2]|0;if(b){if((c[4040]|0)!=-1){c[p>>2]=16160;c[p+4>>2]=12;c[p+8>>2]=0;dW(16160,p,96)}p=(c[4041]|0)-1|0;b=c[P+8>>2]|0;if((c[P+12>>2]|0)-b>>2>>>0<=p>>>0){Q=bO(4)|0;R=Q;kq(R);bl(Q|0,8176,134)}e=c[b+(p<<2)>>2]|0;if((e|0)==0){Q=bO(4)|0;R=Q;kq(R);bl(Q|0,8176,134)}Q=e;R=c[e>>2]|0;if(d){bZ[c[R+44>>2]&127](r,Q);r=f;C=c[q>>2]|0;a[r]=C&255;C=C>>8;a[r+1|0]=C&255;C=C>>8;a[r+2|0]=C&255;C=C>>8;a[r+3|0]=C&255;bZ[c[(c[e>>2]|0)+32>>2]&127](s,Q);r=l;if((a[r]&1)==0){c[l+4>>2]=0;a[r]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[r>>2]=c[t>>2];c[r+4>>2]=c[t+4>>2];c[r+8>>2]=c[t+8>>2];k$(t|0,0,12);ee(s)}else{bZ[c[R+40>>2]&127](v,Q);v=f;C=c[u>>2]|0;a[v]=C&255;C=C>>8;a[v+1|0]=C&255;C=C>>8;a[v+2|0]=C&255;C=C>>8;a[v+3|0]=C&255;bZ[c[(c[e>>2]|0)+28>>2]&127](w,Q);v=l;if((a[v]&1)==0){c[l+4>>2]=0;a[v]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[v>>2]=c[x>>2];c[v+4>>2]=c[x+4>>2];c[v+8>>2]=c[x+8>>2];k$(x|0,0,12);ee(w)}w=e;c[g>>2]=b0[c[(c[w>>2]|0)+12>>2]&127](Q)|0;c[h>>2]=b0[c[(c[w>>2]|0)+16>>2]&127](Q)|0;bZ[c[(c[e>>2]|0)+20>>2]&127](y,Q);x=j;if((a[x]&1)==0){a[j+1|0]=0;a[x]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[x>>2]=c[z>>2];c[x+4>>2]=c[z+4>>2];c[x+8>>2]=c[z+8>>2];k$(z|0,0,12);dP(y);bZ[c[(c[e>>2]|0)+24>>2]&127](A,Q);e=k;if((a[e]&1)==0){c[k+4>>2]=0;a[e]=0}else{c[c[k+8>>2]>>2]=0;c[k+4>>2]=0}ez(k,0);c[e>>2]=c[B>>2];c[e+4>>2]=c[B+4>>2];c[e+8>>2]=c[B+8>>2];k$(B|0,0,12);ee(A);S=b0[c[(c[w>>2]|0)+36>>2]&127](Q)|0;c[m>>2]=S;i=n;return}else{if((c[4042]|0)!=-1){c[o>>2]=16168;c[o+4>>2]=12;c[o+8>>2]=0;dW(16168,o,96)}o=(c[4043]|0)-1|0;Q=c[P+8>>2]|0;if((c[P+12>>2]|0)-Q>>2>>>0<=o>>>0){T=bO(4)|0;U=T;kq(U);bl(T|0,8176,134)}P=c[Q+(o<<2)>>2]|0;if((P|0)==0){T=bO(4)|0;U=T;kq(U);bl(T|0,8176,134)}T=P;U=c[P>>2]|0;if(d){bZ[c[U+44>>2]&127](E,T);E=f;C=c[D>>2]|0;a[E]=C&255;C=C>>8;a[E+1|0]=C&255;C=C>>8;a[E+2|0]=C&255;C=C>>8;a[E+3|0]=C&255;bZ[c[(c[P>>2]|0)+32>>2]&127](F,T);E=l;if((a[E]&1)==0){c[l+4>>2]=0;a[E]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[E>>2]=c[G>>2];c[E+4>>2]=c[G+4>>2];c[E+8>>2]=c[G+8>>2];k$(G|0,0,12);ee(F)}else{bZ[c[U+40>>2]&127](I,T);I=f;C=c[H>>2]|0;a[I]=C&255;C=C>>8;a[I+1|0]=C&255;C=C>>8;a[I+2|0]=C&255;C=C>>8;a[I+3|0]=C&255;bZ[c[(c[P>>2]|0)+28>>2]&127](J,T);I=l;if((a[I]&1)==0){c[l+4>>2]=0;a[I]=0}else{c[c[l+8>>2]>>2]=0;c[l+4>>2]=0}ez(l,0);c[I>>2]=c[K>>2];c[I+4>>2]=c[K+4>>2];c[I+8>>2]=c[K+8>>2];k$(K|0,0,12);ee(J)}J=P;c[g>>2]=b0[c[(c[J>>2]|0)+12>>2]&127](T)|0;c[h>>2]=b0[c[(c[J>>2]|0)+16>>2]&127](T)|0;bZ[c[(c[P>>2]|0)+20>>2]&127](L,T);h=j;if((a[h]&1)==0){a[j+1|0]=0;a[h]=0}else{a[c[j+8>>2]|0]=0;c[j+4>>2]=0}d$(j,0);c[h>>2]=c[M>>2];c[h+4>>2]=c[M+4>>2];c[h+8>>2]=c[M+8>>2];k$(M|0,0,12);dP(L);bZ[c[(c[P>>2]|0)+24>>2]&127](N,T);P=k;if((a[P]&1)==0){c[k+4>>2]=0;a[P]=0}else{c[c[k+8>>2]>>2]=0;c[k+4>>2]=0}ez(k,0);c[P>>2]=c[O>>2];c[P+4>>2]=c[O+4>>2];c[P+8>>2]=c[O+8>>2];k$(O|0,0,12);ee(N);S=b0[c[(c[J>>2]|0)+36>>2]&127](T)|0;c[m>>2]=S;i=n;return}}function ic(b,d,e,f,g,h,i,j,k,l,m,n,o,p,q){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;m=m|0;n=n|0;o=o|0;p=p|0;q=q|0;var r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0;c[e>>2]=b;r=i;s=p;t=p+4|0;u=p+8|0;p=o;v=(f&512|0)==0;w=o+4|0;x=o+8|0;o=i;y=(q|0)>0;z=n;A=n+1|0;B=n+8|0;C=n+4|0;n=g;g=0;while(1){L1901:do{switch(a[k+g|0]|0){case 0:{c[d>>2]=c[e>>2];D=n;break};case 1:{c[d>>2]=c[e>>2];E=b_[c[(c[r>>2]|0)+44>>2]&31](i,32)|0;F=c[e>>2]|0;c[e>>2]=F+4;c[F>>2]=E;D=n;break};case 3:{E=a[s]|0;F=E&255;if((F&1|0)==0){G=F>>>1}else{G=c[t>>2]|0}if((G|0)==0){D=n;break L1901}if((E&1)==0){H=t}else{H=c[u>>2]|0}E=c[H>>2]|0;F=c[e>>2]|0;c[e>>2]=F+4;c[F>>2]=E;D=n;break};case 2:{E=a[p]|0;F=E&255;I=(F&1|0)==0;if(I){J=F>>>1}else{J=c[w>>2]|0}if((J|0)==0|v){D=n;break L1901}if((E&1)==0){K=w;L=w;M=w}else{E=c[x>>2]|0;K=E;L=E;M=E}if(I){N=F>>>1}else{N=c[w>>2]|0}F=K+(N<<2)|0;I=c[e>>2]|0;if((L|0)==(F|0)){O=I}else{E=(K+(N-1<<2)+(-M|0)|0)>>>2;P=L;Q=I;while(1){c[Q>>2]=c[P>>2];R=P+4|0;if((R|0)==(F|0)){break}P=R;Q=Q+4|0}O=I+(E+1<<2)|0}c[e>>2]=O;D=n;break};case 4:{Q=c[e>>2]|0;P=j?n+4|0:n;F=P;while(1){if(F>>>0>=h>>>0){break}if(b1[c[(c[o>>2]|0)+12>>2]&63](i,2048,c[F>>2]|0)|0){F=F+4|0}else{break}}if(y){if(F>>>0>P>>>0){E=F;I=q;do{E=E-4|0;R=c[E>>2]|0;S=c[e>>2]|0;c[e>>2]=S+4;c[S>>2]=R;I=I-1|0;T=(I|0)>0;}while(E>>>0>P>>>0&T);if(T){U=I;V=E;W=1630}else{X=0;Y=I;Z=E}}else{U=q;V=F;W=1630}if((W|0)==1630){W=0;X=b_[c[(c[r>>2]|0)+44>>2]&31](i,48)|0;Y=U;Z=V}R=c[e>>2]|0;c[e>>2]=R+4;if((Y|0)>0){S=Y;_=R;while(1){c[_>>2]=X;$=S-1|0;aa=c[e>>2]|0;c[e>>2]=aa+4;if(($|0)>0){S=$;_=aa}else{ab=aa;break}}}else{ab=R}c[ab>>2]=l;ac=Z}else{ac=F}if((ac|0)==(P|0)){_=b_[c[(c[r>>2]|0)+44>>2]&31](i,48)|0;S=c[e>>2]|0;c[e>>2]=S+4;c[S>>2]=_}else{_=a[z]|0;S=_&255;if((S&1|0)==0){ad=S>>>1}else{ad=c[C>>2]|0}if((ad|0)==0){ae=ac;af=0;ag=0;ah=-1}else{if((_&1)==0){ai=A}else{ai=c[B>>2]|0}ae=ac;af=0;ag=0;ah=a[ai]|0}while(1){do{if((af|0)==(ah|0)){_=c[e>>2]|0;c[e>>2]=_+4;c[_>>2]=m;_=ag+1|0;S=a[z]|0;E=S&255;if((E&1|0)==0){aj=E>>>1}else{aj=c[C>>2]|0}if(_>>>0>=aj>>>0){ak=ah;al=_;am=0;break}E=(S&1)==0;if(E){an=A}else{an=c[B>>2]|0}if((a[an+_|0]|0)==127){ak=-1;al=_;am=0;break}if(E){ao=A}else{ao=c[B>>2]|0}ak=a[ao+_|0]|0;al=_;am=0}else{ak=ah;al=ag;am=af}}while(0);_=ae-4|0;E=c[_>>2]|0;S=c[e>>2]|0;c[e>>2]=S+4;c[S>>2]=E;if((_|0)==(P|0)){break}else{ae=_;af=am+1|0;ag=al;ah=ak}}}F=c[e>>2]|0;if((Q|0)==(F|0)){D=P;break L1901}R=F-4|0;if(Q>>>0<R>>>0){ap=Q;aq=R}else{D=P;break L1901}while(1){R=c[ap>>2]|0;c[ap>>2]=c[aq>>2];c[aq>>2]=R;R=ap+4|0;F=aq-4|0;if(R>>>0<F>>>0){ap=R;aq=F}else{D=P;break}}break};default:{D=n}}}while(0);P=g+1|0;if(P>>>0<4){n=D;g=P}else{break}}g=a[s]|0;s=g&255;D=(s&1|0)==0;if(D){ar=s>>>1}else{ar=c[t>>2]|0}if(ar>>>0>1){if((g&1)==0){as=t;at=t;au=t}else{g=c[u>>2]|0;as=g;at=g;au=g}if(D){av=s>>>1}else{av=c[t>>2]|0}t=as+(av<<2)|0;s=c[e>>2]|0;D=at+4|0;if((D|0)==(t|0)){aw=s}else{at=((as+(av-2<<2)+(-au|0)|0)>>>2)+1|0;au=s;av=D;while(1){c[au>>2]=c[av>>2];D=av+4|0;if((D|0)==(t|0)){break}else{au=au+4|0;av=D}}aw=s+(at<<2)|0}c[e>>2]=aw}switch(f&176|0){case 32:{c[d>>2]=c[e>>2];return};case 16:{return};default:{c[d>>2]=b;return}}}function id(a){a=a|0;de(a|0);kS(a);return}function ie(a){a=a|0;de(a|0);return}function ig(b,d,e){b=b|0;d=d|0;e=e|0;var f=0;if((a[d]&1)==0){f=d+1|0}else{f=c[d+8>>2]|0}d=a8(f|0,200)|0;return d>>>(((d|0)!=-1|0)>>>0)|0}function ih(b,e,f,g,h,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0;e=i;i=i+64|0;l=f;f=i;i=i+4|0;i=i+7>>3<<3;c[f>>2]=c[l>>2];l=e|0;m=e+16|0;n=e+24|0;o=e+32|0;p=e+40|0;q=e+48|0;r=q;s=i;i=i+12|0;i=i+7>>3<<3;t=s;u=i;i=i+12|0;i=i+7>>3<<3;v=u;w=i;i=i+4|0;i=i+7>>3<<3;x=i;i=i+400|0;y=i;i=i+4|0;i=i+7>>3<<3;z=i;i=i+4|0;i=i+7>>3<<3;A=i;i=i+4|0;i=i+7>>3<<3;ej(m,h);B=m|0;C=c[B>>2]|0;if((c[3922]|0)!=-1){c[l>>2]=15688;c[l+4>>2]=12;c[l+8>>2]=0;dW(15688,l,96)}l=(c[3923]|0)-1|0;D=c[C+8>>2]|0;do{if((c[C+12>>2]|0)-D>>2>>>0>l>>>0){E=c[D+(l<<2)>>2]|0;if((E|0)==0){break}F=E;G=k;H=a[G]|0;I=H&255;if((I&1|0)==0){J=I>>>1}else{J=c[k+4>>2]|0}if((J|0)==0){K=0}else{if((H&1)==0){L=k+4|0}else{L=c[k+8>>2]|0}H=c[L>>2]|0;K=(H|0)==(b_[c[(c[E>>2]|0)+44>>2]&31](F,45)|0)}k$(r|0,0,12);k$(t|0,0,12);k$(v|0,0,12);ib(g,K,m,n,o,p,q,s,u,w);E=x|0;H=a[G]|0;I=H&255;M=(I&1|0)==0;if(M){N=I>>>1}else{N=c[k+4>>2]|0}O=c[w>>2]|0;if((N|0)>(O|0)){if(M){P=I>>>1}else{P=c[k+4>>2]|0}I=d[v]|0;if((I&1|0)==0){Q=I>>>1}else{Q=c[u+4>>2]|0}I=d[t]|0;if((I&1|0)==0){R=I>>>1}else{R=c[s+4>>2]|0}S=(P-O<<1|1)+Q+R|0}else{I=d[v]|0;if((I&1|0)==0){T=I>>>1}else{T=c[u+4>>2]|0}I=d[t]|0;if((I&1|0)==0){U=I>>>1}else{U=c[s+4>>2]|0}S=T+2+U|0}I=S+O|0;do{if(I>>>0>100){M=kJ(I<<2)|0;V=M;if((M|0)!=0){W=V;X=V;Y=H;break}kX();W=V;X=V;Y=a[G]|0}else{W=E;X=0;Y=H}}while(0);if((Y&1)==0){Z=k+4|0;_=k+4|0}else{H=c[k+8>>2]|0;Z=H;_=H}H=Y&255;if((H&1|0)==0){$=H>>>1}else{$=c[k+4>>2]|0}ic(W,y,z,c[h+4>>2]|0,_,Z+($<<2)|0,F,K,n,c[o>>2]|0,c[p>>2]|0,q,s,u,O);c[A>>2]=c[f>>2];gq(b,A,W,c[y>>2]|0,c[z>>2]|0,h,j);if((X|0)==0){ee(u);ee(s);dP(q);aa=c[B>>2]|0;ab=aa|0;ac=dz(ab)|0;i=e;return}kK(X);ee(u);ee(s);dP(q);aa=c[B>>2]|0;ab=aa|0;ac=dz(ab)|0;i=e;return}}while(0);e=bO(4)|0;kq(e);bl(e|0,8176,134)}function ii(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;d=i;i=i+16|0;j=d|0;k=j;k$(k|0,0,12);l=b;m=h;n=a[h]|0;if((n&1)==0){o=m+1|0;p=m+1|0}else{m=c[h+8>>2]|0;o=m;p=m}m=n&255;if((m&1|0)==0){q=m>>>1}else{q=c[h+4>>2]|0}h=o+q|0;do{if(p>>>0<h>>>0){q=p;do{dU(j,a[q]|0);q=q+1|0;}while(q>>>0<h>>>0);q=(e|0)==-1?-1:e<<1;if((a[k]&1)==0){r=q;s=1762;break}t=c[j+8>>2]|0;u=q}else{r=(e|0)==-1?-1:e<<1;s=1762}}while(0);if((s|0)==1762){t=j+1|0;u=r}r=bM(u|0,f|0,g|0,t|0)|0;k$(l|0,0,12);l=k0(r|0)|0;t=r+l|0;if((l|0)>0){v=r}else{dP(j);i=d;return}do{dU(b,a[v]|0);v=v+1|0;}while(v>>>0<t>>>0);dP(j);i=d;return}function ij(a,b){a=a|0;b=b|0;a1(((b|0)==-1?-1:b<<1)|0)|0;return}function ik(a){a=a|0;de(a|0);kS(a);return}function il(a){a=a|0;de(a|0);return}function im(b,d,e){b=b|0;d=d|0;e=e|0;var f=0;if((a[d]&1)==0){f=d+1|0}else{f=c[d+8>>2]|0}d=a8(f|0,200)|0;return d>>>(((d|0)!=-1|0)>>>0)|0}function io(a,b){a=a|0;b=b|0;a1(((b|0)==-1?-1:b<<1)|0)|0;return}function ip(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;d=i;i=i+224|0;j=d|0;k=d+8|0;l=d+40|0;m=d+48|0;n=d+56|0;o=d+64|0;p=d+192|0;q=d+200|0;r=d+208|0;s=r;t=i;i=i+8|0;u=i;i=i+8|0;k$(s|0,0,12);v=b;w=t|0;c[t+4>>2]=0;c[t>>2]=4104;x=a[h]|0;if((x&1)==0){y=h+4|0;z=h+4|0}else{A=c[h+8>>2]|0;y=A;z=A}A=x&255;if((A&1|0)==0){B=A>>>1}else{B=c[h+4>>2]|0}h=y+(B<<2)|0;L2135:do{if(z>>>0<h>>>0){B=t;y=k|0;A=k+32|0;x=z;C=4104;while(1){c[m>>2]=x;D=(b6[c[C+12>>2]&31](w,j,x,h,m,y,A,l)|0)==2;E=c[m>>2]|0;if(D|(E|0)==(x|0)){break}if(y>>>0<(c[l>>2]|0)>>>0){D=y;do{dU(r,a[D]|0);D=D+1|0;}while(D>>>0<(c[l>>2]|0)>>>0);F=c[m>>2]|0}else{F=E}if(F>>>0>=h>>>0){break L2135}x=F;C=c[B>>2]|0}B=bO(8)|0;dE(B,904);bl(B|0,8192,22)}}while(0);de(t|0);if((a[s]&1)==0){G=r+1|0}else{G=c[r+8>>2]|0}s=bM(((e|0)==-1?-1:e<<1)|0,f|0,g|0,G|0)|0;k$(v|0,0,12);v=u|0;c[u+4>>2]=0;c[u>>2]=4048;G=k0(s|0)|0;g=s+G|0;if((G|0)<1){H=u|0;de(H);dP(r);i=d;return}G=u;f=g;e=o|0;t=o+128|0;o=s;s=4048;while(1){c[q>>2]=o;F=(b6[c[s+16>>2]&31](v,n,o,(f-o|0)>32?o+32|0:g,q,e,t,p)|0)==2;h=c[q>>2]|0;if(F|(h|0)==(o|0)){break}if(e>>>0<(c[p>>2]|0)>>>0){F=e;do{eh(b,c[F>>2]|0);F=F+4|0;}while(F>>>0<(c[p>>2]|0)>>>0);I=c[q>>2]|0}else{I=h}if(I>>>0>=g>>>0){J=1829;break}o=I;s=c[G>>2]|0}if((J|0)==1829){H=u|0;de(H);dP(r);i=d;return}d=bO(8)|0;dE(d,904);bl(d|0,8192,22)}function iq(b){b=b|0;var d=0,e=0,f=0;c[b>>2]=3568;d=b+8|0;e=c[d>>2]|0;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);if((e|0)==(c[2958]|0)){f=b|0;de(f);return}a7(c[d>>2]|0);f=b|0;de(f);return}function ir(a){a=a|0;a=bO(8)|0;dA(a,1352);c[a>>2]=2504;bl(a|0,8208,32)}function is(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0;e=i;i=i+448|0;f=e|0;g=e+16|0;h=e+32|0;j=e+48|0;k=e+64|0;l=e+80|0;m=e+96|0;n=e+112|0;o=e+128|0;p=e+144|0;q=e+160|0;r=e+176|0;s=e+192|0;t=e+208|0;u=e+224|0;v=e+240|0;w=e+256|0;x=e+272|0;y=e+288|0;z=e+304|0;A=e+320|0;B=e+336|0;C=e+352|0;D=e+368|0;E=e+384|0;F=e+400|0;G=e+416|0;H=e+432|0;c[b+4>>2]=d-1;c[b>>2]=3824;d=b+8|0;I=b+12|0;a[b+136|0]=1;J=b+24|0;K=J;c[I>>2]=K;c[d>>2]=K;c[b+16>>2]=J+112;J=28;L=K;do{if((L|0)==0){M=0}else{c[L>>2]=0;M=c[I>>2]|0}L=M+4|0;c[I>>2]=L;J=J-1|0;}while((J|0)!=0);dZ(b+144|0,1344,1);J=c[d>>2]|0;d=c[I>>2]|0;if((J|0)!=(d|0)){c[I>>2]=d+(~((d-4+(-J|0)|0)>>>2)<<2)}c[3617]=0;c[3616]=3528;if((c[3844]|0)!=-1){c[H>>2]=15376;c[H+4>>2]=12;c[H+8>>2]=0;dW(15376,H,96)}iJ(b,14464,(c[3845]|0)-1|0);c[3615]=0;c[3614]=3488;if((c[3842]|0)!=-1){c[G>>2]=15368;c[G+4>>2]=12;c[G+8>>2]=0;dW(15368,G,96)}iJ(b,14456,(c[3843]|0)-1|0);c[3667]=0;c[3666]=3936;c[3668]=0;a[14676]=0;c[3668]=c[(a6()|0)>>2];if((c[3924]|0)!=-1){c[F>>2]=15696;c[F+4>>2]=12;c[F+8>>2]=0;dW(15696,F,96)}iJ(b,14664,(c[3925]|0)-1|0);c[3665]=0;c[3664]=3856;if((c[3922]|0)!=-1){c[E>>2]=15688;c[E+4>>2]=12;c[E+8>>2]=0;dW(15688,E,96)}iJ(b,14656,(c[3923]|0)-1|0);c[3619]=0;c[3618]=3624;if((c[3848]|0)!=-1){c[D>>2]=15392;c[D+4>>2]=12;c[D+8>>2]=0;dW(15392,D,96)}iJ(b,14472,(c[3849]|0)-1|0);c[521]=0;c[520]=3568;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);c[522]=c[2958];if((c[3846]|0)!=-1){c[C>>2]=15384;c[C+4>>2]=12;c[C+8>>2]=0;dW(15384,C,96)}iJ(b,2080,(c[3847]|0)-1|0);c[3621]=0;c[3620]=3680;if((c[3850]|0)!=-1){c[B>>2]=15400;c[B+4>>2]=12;c[B+8>>2]=0;dW(15400,B,96)}iJ(b,14480,(c[3851]|0)-1|0);c[3623]=0;c[3622]=3736;if((c[3852]|0)!=-1){c[A>>2]=15408;c[A+4>>2]=12;c[A+8>>2]=0;dW(15408,A,96)}iJ(b,14488,(c[3853]|0)-1|0);c[3597]=0;c[3596]=3032;a[14392]=46;a[14393]=44;k$(14396,0,12);if((c[3828]|0)!=-1){c[z>>2]=15312;c[z+4>>2]=12;c[z+8>>2]=0;dW(15312,z,96)}iJ(b,14384,(c[3829]|0)-1|0);c[513]=0;c[512]=2984;c[514]=46;c[515]=44;k$(2064,0,12);if((c[3826]|0)!=-1){c[y>>2]=15304;c[y+4>>2]=12;c[y+8>>2]=0;dW(15304,y,96)}iJ(b,2048,(c[3827]|0)-1|0);c[3613]=0;c[3612]=3416;if((c[3840]|0)!=-1){c[x>>2]=15360;c[x+4>>2]=12;c[x+8>>2]=0;dW(15360,x,96)}iJ(b,14448,(c[3841]|0)-1|0);c[3611]=0;c[3610]=3344;if((c[3838]|0)!=-1){c[w>>2]=15352;c[w+4>>2]=12;c[w+8>>2]=0;dW(15352,w,96)}iJ(b,14440,(c[3839]|0)-1|0);c[3609]=0;c[3608]=3280;if((c[3836]|0)!=-1){c[v>>2]=15344;c[v+4>>2]=12;c[v+8>>2]=0;dW(15344,v,96)}iJ(b,14432,(c[3837]|0)-1|0);c[3607]=0;c[3606]=3216;if((c[3834]|0)!=-1){c[u>>2]=15336;c[u+4>>2]=12;c[u+8>>2]=0;dW(15336,u,96)}iJ(b,14424,(c[3835]|0)-1|0);c[3677]=0;c[3676]=4912;if((c[4046]|0)!=-1){c[t>>2]=16184;c[t+4>>2]=12;c[t+8>>2]=0;dW(16184,t,96)}iJ(b,14704,(c[4047]|0)-1|0);c[3675]=0;c[3674]=4848;if((c[4044]|0)!=-1){c[s>>2]=16176;c[s+4>>2]=12;c[s+8>>2]=0;dW(16176,s,96)}iJ(b,14696,(c[4045]|0)-1|0);c[3673]=0;c[3672]=4784;if((c[4042]|0)!=-1){c[r>>2]=16168;c[r+4>>2]=12;c[r+8>>2]=0;dW(16168,r,96)}iJ(b,14688,(c[4043]|0)-1|0);c[3671]=0;c[3670]=4720;if((c[4040]|0)!=-1){c[q>>2]=16160;c[q+4>>2]=12;c[q+8>>2]=0;dW(16160,q,96)}iJ(b,14680,(c[4041]|0)-1|0);c[3595]=0;c[3594]=2688;if((c[3816]|0)!=-1){c[p>>2]=15264;c[p+4>>2]=12;c[p+8>>2]=0;dW(15264,p,96)}iJ(b,14376,(c[3817]|0)-1|0);c[3593]=0;c[3592]=2648;if((c[3814]|0)!=-1){c[o>>2]=15256;c[o+4>>2]=12;c[o+8>>2]=0;dW(15256,o,96)}iJ(b,14368,(c[3815]|0)-1|0);c[3591]=0;c[3590]=2608;if((c[3812]|0)!=-1){c[n>>2]=15248;c[n+4>>2]=12;c[n+8>>2]=0;dW(15248,n,96)}iJ(b,14360,(c[3813]|0)-1|0);c[3589]=0;c[3588]=2568;if((c[3810]|0)!=-1){c[m>>2]=15240;c[m+4>>2]=12;c[m+8>>2]=0;dW(15240,m,96)}iJ(b,14352,(c[3811]|0)-1|0);c[509]=0;c[508]=2888;c[510]=2936;if((c[3824]|0)!=-1){c[l>>2]=15296;c[l+4>>2]=12;c[l+8>>2]=0;dW(15296,l,96)}iJ(b,2032,(c[3825]|0)-1|0);c[505]=0;c[504]=2792;c[506]=2840;if((c[3822]|0)!=-1){c[k>>2]=15288;c[k+4>>2]=12;c[k+8>>2]=0;dW(15288,k,96)}iJ(b,2016,(c[3823]|0)-1|0);c[501]=0;c[500]=3792;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);c[502]=c[2958];c[500]=2760;if((c[3820]|0)!=-1){c[j>>2]=15280;c[j+4>>2]=12;c[j+8>>2]=0;dW(15280,j,96)}iJ(b,2e3,(c[3821]|0)-1|0);c[497]=0;c[496]=3792;do{if((a[16264]|0)==0){if((bc(16264)|0)==0){break}c[2958]=aS(1,1344,0)|0}}while(0);c[498]=c[2958];c[496]=2728;if((c[3818]|0)!=-1){c[h>>2]=15272;c[h+4>>2]=12;c[h+8>>2]=0;dW(15272,h,96)}iJ(b,1984,(c[3819]|0)-1|0);c[3605]=0;c[3604]=3120;if((c[3832]|0)!=-1){c[g>>2]=15328;c[g+4>>2]=12;c[g+8>>2]=0;dW(15328,g,96)}iJ(b,14416,(c[3833]|0)-1|0);c[3603]=0;c[3602]=3080;if((c[3830]|0)!=-1){c[f>>2]=15320;c[f+4>>2]=12;c[f+8>>2]=0;dW(15320,f,96)}iJ(b,14408,(c[3831]|0)-1|0);i=e;return}function it(a,b){a=a|0;b=b|0;return b|0}function iu(a,b,d,e,f,g,h,i){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;c[f>>2]=d;c[i>>2]=g;return 3}function iv(a,b,d,e,f,g,h,i){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;c[f>>2]=d;c[i>>2]=g;return 3}function iw(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;c[f>>2]=d;return 3}function ix(a){a=a|0;return 1}function iy(a){a=a|0;return 1}function iz(a){a=a|0;return 1}function iA(a,b){a=a|0;b=b|0;return b<<24>>24|0}function iB(a,b,c){a=a|0;b=b|0;c=c|0;return(b>>>0<128?b&255:c)|0}function iC(a,b,c){a=a|0;b=b|0;c=c|0;return(b<<24>>24>-1?b:c)|0}function iD(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;b=d-c|0;return(b>>>0<e>>>0?b:e)|0}function iE(a){a=a|0;c[a+4>>2]=(I=c[3854]|0,c[3854]=I+1,I)+1;return}function iF(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0;if((d|0)==(e|0)){g=d;return g|0}else{h=d;i=f}while(1){c[i>>2]=a[h]|0;f=h+1|0;if((f|0)==(e|0)){g=e;break}else{h=f;i=i+4|0}}return g|0}function iG(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0;if((d|0)==(e|0)){h=d;return h|0}b=((e-4+(-d|0)|0)>>>2)+1|0;i=d;j=g;while(1){g=c[i>>2]|0;a[j]=g>>>0<128?g&255:f;g=i+4|0;if((g|0)==(e|0)){break}else{i=g;j=j+1|0}}h=d+(b<<2)|0;return h|0}function iH(b,c,d,e){b=b|0;c=c|0;d=d|0;e=e|0;var f=0,g=0,h=0;if((c|0)==(d|0)){f=c;return f|0}else{g=c;h=e}while(1){a[h]=a[g]|0;e=g+1|0;if((e|0)==(d|0)){f=d;break}else{g=e;h=h+1|0}}return f|0}function iI(b,c,d,e,f){b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0;if((c|0)==(d|0)){g=c;return g|0}else{h=c;i=f}while(1){f=a[h]|0;a[i]=f<<24>>24>-1?f:e;f=h+1|0;if((f|0)==(d|0)){g=d;break}else{h=f;i=i+1|0}}return g|0}function iJ(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0;dg(b|0);e=a+8|0;f=a+12|0;a=c[f>>2]|0;g=e|0;h=c[g>>2]|0;i=a-h>>2;do{if(i>>>0>d>>>0){j=h}else{k=d+1|0;if(i>>>0<k>>>0){j5(e,k-i|0);j=c[g>>2]|0;break}if(i>>>0<=k>>>0){j=h;break}l=h+(k<<2)|0;if((l|0)==(a|0)){j=h;break}c[f>>2]=a+(~((a-4+(-l|0)|0)>>>2)<<2);j=h}}while(0);h=c[j+(d<<2)>>2]|0;if((h|0)==0){m=j;n=m+(d<<2)|0;c[n>>2]=b;return}dz(h|0)|0;m=c[g>>2]|0;n=m+(d<<2)|0;c[n>>2]=b;return}function iK(a){a=a|0;iL(a);kS(a);return}function iL(b){b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0;c[b>>2]=3824;d=b+12|0;e=c[d>>2]|0;f=b+8|0;g=c[f>>2]|0;if((e|0)!=(g|0)){h=0;i=g;g=e;while(1){e=c[i+(h<<2)>>2]|0;if((e|0)==0){j=g;k=i}else{l=e|0;dz(l)|0;j=c[d>>2]|0;k=c[f>>2]|0}l=h+1|0;if(l>>>0<j-k>>2>>>0){h=l;i=k;g=j}else{break}}}dP(b+144|0);j=c[f>>2]|0;if((j|0)==0){m=b|0;de(m);return}f=c[d>>2]|0;if((j|0)!=(f|0)){c[d>>2]=f+(~((f-4+(-j|0)|0)>>>2)<<2)}if((j|0)==(b+24|0)){a[b+136|0]=0;m=b|0;de(m);return}else{kS(j);m=b|0;de(m);return}}function iM(){var b=0,d=0;if((a[16248]|0)!=0){b=c[2950]|0;return b|0}if((bc(16248)|0)==0){b=c[2950]|0;return b|0}do{if((a[16256]|0)==0){if((bc(16256)|0)==0){break}is(14496,1);c[2954]=14496;c[2952]=11816}}while(0);d=c[c[2952]>>2]|0;c[2956]=d;dg(d|0);c[2950]=11824;b=c[2950]|0;return b|0}function iN(a,b){a=a|0;b=b|0;var d=0;d=c[b>>2]|0;c[a>>2]=d;dg(d|0);return}function iO(a){a=a|0;dz(c[a>>2]|0)|0;return}function iP(a){a=a|0;de(a|0);kS(a);return}function iQ(a){a=a|0;if((a|0)==0){return}bY[c[(c[a>>2]|0)+4>>2]&511](a);return}function iR(a){a=a|0;de(a|0);kS(a);return}function iS(b){b=b|0;var d=0;c[b>>2]=3936;d=c[b+8>>2]|0;do{if((d|0)!=0){if((a[b+12|0]&1)==0){break}kT(d)}}while(0);de(b|0);kS(b);return}function iT(b){b=b|0;var d=0;c[b>>2]=3936;d=c[b+8>>2]|0;do{if((d|0)!=0){if((a[b+12|0]&1)==0){break}kT(d)}}while(0);de(b|0);return}function iU(a){a=a|0;de(a|0);kS(a);return}function iV(a){a=a|0;iq(a);kS(a);return}function iW(a){a=a|0;var b=0;b=c[(iM()|0)>>2]|0;c[a>>2]=b;dg(b|0);return}function iX(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0;d=i;i=i+16|0;e=d|0;f=c[a>>2]|0;a=b|0;if((c[a>>2]|0)!=-1){c[e>>2]=b;c[e+4>>2]=12;c[e+8>>2]=0;dW(a,e,96)}e=(c[b+4>>2]|0)-1|0;b=c[f+8>>2]|0;if((c[f+12>>2]|0)-b>>2>>>0<=e>>>0){g=bO(4)|0;h=g;kq(h);bl(g|0,8176,134);return 0}f=c[b+(e<<2)>>2]|0;if((f|0)==0){g=bO(4)|0;h=g;kq(h);bl(g|0,8176,134);return 0}else{i=d;return f|0}return 0}function iY(a,d,e){a=a|0;d=d|0;e=e|0;var f=0;if(e>>>0>=128){f=0;return f|0}f=(b[(c[(a6()|0)>>2]|0)+(e<<1)>>1]&d)<<16>>16!=0;return f|0}function iZ(a,d,e,f){a=a|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0;if((d|0)==(e|0)){g=d;return g|0}else{h=d;i=f}while(1){f=c[h>>2]|0;if(f>>>0<128){j=b[(c[(a6()|0)>>2]|0)+(f<<1)>>1]|0}else{j=0}b[i>>1]=j;f=h+4|0;if((f|0)==(e|0)){g=e;break}else{h=f;i=i+2|0}}return g|0}function i_(a,d,e,f){a=a|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0;if((e|0)==(f|0)){g=e;return g|0}else{h=e}while(1){e=c[h>>2]|0;if(e>>>0<128){if((b[(c[(a6()|0)>>2]|0)+(e<<1)>>1]&d)<<16>>16!=0){g=h;i=2094;break}}e=h+4|0;if((e|0)==(f|0)){g=f;i=2096;break}else{h=e}}if((i|0)==2094){return g|0}else if((i|0)==2096){return g|0}return 0}function i$(a,d,e,f){a=a|0;d=d|0;e=e|0;f=f|0;var g=0,h=0;a=e;while(1){if((a|0)==(f|0)){g=f;h=2104;break}e=c[a>>2]|0;if(e>>>0>=128){g=a;h=2106;break}if((b[(c[(a6()|0)>>2]|0)+(e<<1)>>1]&d)<<16>>16==0){g=a;h=2105;break}else{a=a+4|0}}if((h|0)==2104){return g|0}else if((h|0)==2105){return g|0}else if((h|0)==2106){return g|0}return 0}function i0(a,b){a=a|0;b=b|0;var d=0;if(b>>>0>=128){d=b;return d|0}d=c[(c[(bR()|0)>>2]|0)+(b<<2)>>2]|0;return d|0}function i1(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0;if((b|0)==(d|0)){e=b;return e|0}else{f=b}while(1){b=c[f>>2]|0;if(b>>>0<128){g=c[(c[(bR()|0)>>2]|0)+(b<<2)>>2]|0}else{g=b}c[f>>2]=g;b=f+4|0;if((b|0)==(d|0)){e=d;break}else{f=b}}return e|0}function i2(a,b){a=a|0;b=b|0;var d=0;if(b>>>0>=128){d=b;return d|0}d=c[(c[(bS()|0)>>2]|0)+(b<<2)>>2]|0;return d|0}function i3(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0;if((b|0)==(d|0)){e=b;return e|0}else{f=b}while(1){b=c[f>>2]|0;if(b>>>0<128){g=c[(c[(bS()|0)>>2]|0)+(b<<2)>>2]|0}else{g=b}c[f>>2]=g;b=f+4|0;if((b|0)==(d|0)){e=d;break}else{f=b}}return e|0}function i4(a,b){a=a|0;b=b|0;var d=0;if(b<<24>>24<=-1){d=b;return d|0}d=c[(c[(bR()|0)>>2]|0)+((b&255)<<2)>>2]&255;return d|0}function i5(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;if((d|0)==(e|0)){f=d;return f|0}else{g=d}while(1){d=a[g]|0;if(d<<24>>24>-1){h=c[(c[(bR()|0)>>2]|0)+(d<<24>>24<<2)>>2]&255}else{h=d}a[g]=h;d=g+1|0;if((d|0)==(e|0)){f=e;break}else{g=d}}return f|0}function i6(a,b){a=a|0;b=b|0;var d=0;if(b<<24>>24<=-1){d=b;return d|0}d=c[(c[(bS()|0)>>2]|0)+(b<<24>>24<<2)>>2]&255;return d|0}function i7(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;if((d|0)==(e|0)){f=d;return f|0}else{g=d}while(1){d=a[g]|0;if(d<<24>>24>-1){h=c[(c[(bS()|0)>>2]|0)+(d<<24>>24<<2)>>2]&255}else{h=d}a[g]=h;d=g+1|0;if((d|0)==(e|0)){f=e;break}else{g=d}}return f|0}function i8(a){a=a|0;return 0}function i9(a){a=a|0;de(a|0);kS(a);return}function ja(a,b,d,e,f,g,h,j){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0;b=i;i=i+16|0;a=b|0;k=b+8|0;c[a>>2]=d;c[k>>2]=g;l=jl(d,e,a,g,h,k,1114111,0)|0;c[f>>2]=d+((c[a>>2]|0)-d>>1<<1);c[j>>2]=g+((c[k>>2]|0)-g);i=b;return l|0}function jb(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0;l=i;i=i+8|0;m=l|0;n=m;o=i;i=i+1|0;i=i+7>>3<<3;p=e;while(1){if((p|0)==(f|0)){q=f;break}if((c[p>>2]|0)==0){q=p;break}else{p=p+4|0}}c[k>>2]=h;c[g>>2]=e;L2555:do{if((e|0)==(f|0)|(h|0)==(j|0)){r=e}else{p=d;s=j;t=b+8|0;u=o|0;v=h;w=e;x=q;L2557:while(1){y=c[p+4>>2]|0;c[m>>2]=c[p>>2];c[m+4>>2]=y;y=bB(c[t>>2]|0)|0;z=ks(v,g,x-w>>2,s-v|0,d)|0;if((y|0)!=0){bB(y|0)|0}switch(z|0){case-1:{A=2188;break L2557;break};case 0:{B=1;A=2224;break L2557;break};default:{}}y=(c[k>>2]|0)+z|0;c[k>>2]=y;if((y|0)==(j|0)){A=2221;break}if((x|0)==(f|0)){C=f;D=y;E=c[g>>2]|0}else{y=bB(c[t>>2]|0)|0;z=kg(u,0,d)|0;if((y|0)!=0){bB(y|0)|0}if((z|0)==-1){B=2;A=2226;break}y=c[k>>2]|0;if(z>>>0>(s-y|0)>>>0){B=1;A=2227;break}L2574:do{if((z|0)!=0){F=z;G=u;H=y;while(1){I=a[G]|0;c[k>>2]=H+1;a[H]=I;I=F-1|0;if((I|0)==0){break L2574}F=I;G=G+1|0;H=c[k>>2]|0}}}while(0);y=(c[g>>2]|0)+4|0;c[g>>2]=y;z=y;while(1){if((z|0)==(f|0)){J=f;break}if((c[z>>2]|0)==0){J=z;break}else{z=z+4|0}}C=J;D=c[k>>2]|0;E=y}if((E|0)==(f|0)|(D|0)==(j|0)){r=E;break L2555}else{v=D;w=E;x=C}}if((A|0)==2188){c[k>>2]=v;L2586:do{if((w|0)==(c[g>>2]|0)){K=w}else{x=w;u=v;while(1){s=c[x>>2]|0;p=bB(c[t>>2]|0)|0;z=kg(u,s,n)|0;if((p|0)!=0){bB(p|0)|0}if((z|0)==-1){K=x;break L2586}p=(c[k>>2]|0)+z|0;c[k>>2]=p;z=x+4|0;if((z|0)==(c[g>>2]|0)){K=z;break}else{x=z;u=p}}}}while(0);c[g>>2]=K;B=2;i=l;return B|0}else if((A|0)==2221){r=c[g>>2]|0;break}else if((A|0)==2224){i=l;return B|0}else if((A|0)==2226){i=l;return B|0}else if((A|0)==2227){i=l;return B|0}}}while(0);B=(r|0)!=(f|0)|0;i=l;return B|0}function jc(b,d,e,f,g,h,j,k){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0;l=i;i=i+8|0;m=l|0;n=m;o=e;while(1){if((o|0)==(f|0)){p=f;break}if((a[o]|0)==0){p=o;break}else{o=o+1|0}}c[k>>2]=h;c[g>>2]=e;L2607:do{if((e|0)==(f|0)|(h|0)==(j|0)){q=e}else{o=d;r=j;s=b+8|0;t=h;u=e;v=p;L2609:while(1){w=c[o+4>>2]|0;c[m>>2]=c[o>>2];c[m+4>>2]=w;x=v;w=bB(c[s>>2]|0)|0;y=kd(t,g,x-u|0,r-t>>2,d)|0;if((w|0)!=0){bB(w|0)|0}switch(y|0){case-1:{z=2243;break L2609;break};case 0:{A=2;z=2278;break L2609;break};default:{}}w=(c[k>>2]|0)+(y<<2)|0;c[k>>2]=w;if((w|0)==(j|0)){z=2275;break}y=c[g>>2]|0;if((v|0)==(f|0)){B=f;C=w;D=y}else{E=bB(c[s>>2]|0)|0;F=kc(w,y,1,d)|0;if((E|0)!=0){bB(E|0)|0}if((F|0)!=0){A=2;z=2282;break}c[k>>2]=(c[k>>2]|0)+4;F=(c[g>>2]|0)+1|0;c[g>>2]=F;E=F;while(1){if((E|0)==(f|0)){G=f;break}if((a[E]|0)==0){G=E;break}else{E=E+1|0}}B=G;C=c[k>>2]|0;D=F}if((D|0)==(f|0)|(C|0)==(j|0)){q=D;break L2607}else{t=C;u=D;v=B}}if((z|0)==2243){c[k>>2]=t;L2631:do{if((u|0)==(c[g>>2]|0)){H=u}else{v=t;r=u;L2632:while(1){o=bB(c[s>>2]|0)|0;E=kc(v,r,x-r|0,n)|0;if((o|0)!=0){bB(o|0)|0}switch(E|0){case 0:{I=r+1|0;break};case-1:{z=2254;break L2632;break};case-2:{z=2255;break L2632;break};default:{I=r+E|0}}E=(c[k>>2]|0)+4|0;c[k>>2]=E;if((I|0)==(c[g>>2]|0)){H=I;break L2631}else{v=E;r=I}}if((z|0)==2254){c[g>>2]=r;A=2;i=l;return A|0}else if((z|0)==2255){c[g>>2]=r;A=1;i=l;return A|0}}}while(0);c[g>>2]=H;A=(H|0)!=(f|0)|0;i=l;return A|0}else if((z|0)==2275){q=c[g>>2]|0;break}else if((z|0)==2278){i=l;return A|0}else if((z|0)==2282){i=l;return A|0}}}while(0);A=(q|0)!=(f|0)|0;i=l;return A|0}function jd(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,j=0,k=0,l=0,m=0,n=0;h=i;i=i+8|0;c[g>>2]=e;e=h|0;j=bB(c[b+8>>2]|0)|0;b=kg(e,0,d)|0;if((j|0)!=0){bB(j|0)|0}switch(b|0){case-1:case 0:{k=2;i=h;return k|0};default:{}}j=b-1|0;b=c[g>>2]|0;if(j>>>0>(f-b|0)>>>0){k=1;i=h;return k|0}if((j|0)==0){k=0;i=h;return k|0}else{l=j;m=e;n=b}while(1){b=a[m]|0;c[g>>2]=n+1;a[n]=b;b=l-1|0;if((b|0)==0){k=0;break}l=b;m=m+1|0;n=c[g>>2]|0}i=h;return k|0}function je(a){a=a|0;var b=0,d=0,e=0,f=0,g=0;b=a+8|0;a=bB(c[b>>2]|0)|0;d=kf(0,0,1)|0;if((a|0)!=0){bB(a|0)|0}if((d|0)!=0){e=-1;return e|0}d=c[b>>2]|0;if((d|0)==0){e=1;return e|0}e=bB(d|0)|0;d=bd()|0;if((e|0)==0){f=(d|0)==1;g=f&1;return g|0}bB(e|0)|0;f=(d|0)==1;g=f&1;return g|0}function jf(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0;if((f|0)==0|(d|0)==(e|0)){g=0;return g|0}h=e;i=a+8|0;a=d;d=0;j=0;L2693:while(1){k=bB(c[i>>2]|0)|0;l=kb(a,h-a|0,b)|0;if((k|0)!=0){bB(k|0)|0}switch(l|0){case 0:{m=1;n=a+1|0;break};case-1:case-2:{g=d;o=2343;break L2693;break};default:{m=l;n=a+l|0}}l=m+d|0;k=j+1|0;if(k>>>0>=f>>>0|(n|0)==(e|0)){g=l;o=2345;break}else{a=n;d=l;j=k}}if((o|0)==2345){return g|0}else if((o|0)==2343){return g|0}return 0}function jg(a){a=a|0;var b=0,d=0,e=0;b=c[a+8>>2]|0;do{if((b|0)==0){d=1}else{a=bB(b|0)|0;e=bd()|0;if((a|0)==0){d=e;break}bB(a|0)|0;d=e}}while(0);return d|0}function jh(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;c[f>>2]=d;return 3}function ji(a){a=a|0;return 0}function jj(a){a=a|0;return 0}function jk(a){a=a|0;return 4}function jl(d,f,g,h,i,j,k,l){d=d|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0;c[g>>2]=d;c[j>>2]=h;do{if((l&2|0)!=0){if((i-h|0)<3){m=1;return m|0}else{c[j>>2]=h+1;a[h]=-17;d=c[j>>2]|0;c[j>>2]=d+1;a[d]=-69;d=c[j>>2]|0;c[j>>2]=d+1;a[d]=-65;break}}}while(0);h=f;l=c[g>>2]|0;if(l>>>0>=f>>>0){m=0;return m|0}d=i;i=l;L2727:while(1){l=b[i>>1]|0;n=l&65535;if(n>>>0>k>>>0){m=2;o=2399;break}do{if((l&65535)<128){p=c[j>>2]|0;if((d-p|0)<1){m=1;o=2388;break L2727}c[j>>2]=p+1;a[p]=l&255}else{if((l&65535)<2048){p=c[j>>2]|0;if((d-p|0)<2){m=1;o=2389;break L2727}c[j>>2]=p+1;a[p]=(n>>>6|192)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n&63|128)&255;break}if((l&65535)<55296){p=c[j>>2]|0;if((d-p|0)<3){m=1;o=2397;break L2727}c[j>>2]=p+1;a[p]=(n>>>12|224)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n>>>6&63|128)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n&63|128)&255;break}if((l&65535)>=56320){if((l&65535)<57344){m=2;o=2393;break L2727}p=c[j>>2]|0;if((d-p|0)<3){m=1;o=2394;break L2727}c[j>>2]=p+1;a[p]=(n>>>12|224)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n>>>6&63|128)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n&63|128)&255;break}if((h-i|0)<4){m=1;o=2391;break L2727}p=i+2|0;q=e[p>>1]|0;if((q&64512|0)!=56320){m=2;o=2392;break L2727}if((d-(c[j>>2]|0)|0)<4){m=1;o=2398;break L2727}r=n&960;if(((r<<10)+65536|n<<10&64512|q&1023)>>>0>k>>>0){m=2;o=2400;break L2727}c[g>>2]=p;p=(r>>>6)+1|0;r=c[j>>2]|0;c[j>>2]=r+1;a[r]=(p>>>2|240)&255;r=c[j>>2]|0;c[j>>2]=r+1;a[r]=(n>>>2&15|p<<4&48|128)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(n<<4&48|q>>>6&15|128)&255;p=c[j>>2]|0;c[j>>2]=p+1;a[p]=(q&63|128)&255}}while(0);n=(c[g>>2]|0)+2|0;c[g>>2]=n;if(n>>>0<f>>>0){i=n}else{m=0;o=2395;break}}if((o|0)==2389){return m|0}else if((o|0)==2393){return m|0}else if((o|0)==2394){return m|0}else if((o|0)==2400){return m|0}else if((o|0)==2388){return m|0}else if((o|0)==2391){return m|0}else if((o|0)==2392){return m|0}else if((o|0)==2395){return m|0}else if((o|0)==2397){return m|0}else if((o|0)==2398){return m|0}else if((o|0)==2399){return m|0}return 0}function jm(e,f,g,h,i,j,k,l){e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;l=l|0;var m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0;c[g>>2]=e;c[j>>2]=h;h=c[g>>2]|0;do{if((l&4|0)==0){m=h}else{if((f-h|0)<=2){m=h;break}if((a[h]|0)!=-17){m=h;break}if((a[h+1|0]|0)!=-69){m=h;break}if((a[h+2|0]|0)!=-65){m=h;break}e=h+3|0;c[g>>2]=e;m=e}}while(0);L2772:do{if(m>>>0<f>>>0){h=f;l=i;e=c[j>>2]|0;n=m;L2774:while(1){if(e>>>0>=i>>>0){o=n;break L2772}p=a[n]|0;q=p&255;if(q>>>0>k>>>0){r=2;s=2444;break}do{if(p<<24>>24>-1){b[e>>1]=p&255;c[g>>2]=(c[g>>2]|0)+1}else{if((p&255)<194){r=2;s=2453;break L2774}if((p&255)<224){if((h-n|0)<2){r=1;s=2454;break L2774}t=d[n+1|0]|0;if((t&192|0)!=128){r=2;s=2455;break L2774}u=t&63|q<<6&1984;if(u>>>0>k>>>0){r=2;s=2456;break L2774}b[e>>1]=u&65535;c[g>>2]=(c[g>>2]|0)+2;break}if((p&255)<240){if((h-n|0)<3){r=1;s=2457;break L2774}u=a[n+1|0]|0;switch(q|0){case 224:{if((u&-32)<<24>>24!=-96){r=2;s=2450;break L2774}break};case 237:{if((u&-32)<<24>>24!=-128){r=2;s=2452;break L2774}break};default:{if((u&-64)<<24>>24!=-128){r=2;s=2462;break L2774}}}t=d[n+2|0]|0;if((t&192|0)!=128){r=2;s=2443;break L2774}v=(u&255)<<6&4032|q<<12|t&63;if((v&65535)>>>0>k>>>0){r=2;s=2442;break L2774}b[e>>1]=v&65535;c[g>>2]=(c[g>>2]|0)+3;break}if((p&255)>=245){r=2;s=2447;break L2774}if((h-n|0)<4){r=1;s=2458;break L2774}v=a[n+1|0]|0;switch(q|0){case 240:{if((v+112&255)>=48){r=2;s=2448;break L2774}break};case 244:{if((v&-16)<<24>>24!=-128){r=2;s=2459;break L2774}break};default:{if((v&-64)<<24>>24!=-128){r=2;s=2460;break L2774}}}t=d[n+2|0]|0;if((t&192|0)!=128){r=2;s=2461;break L2774}u=d[n+3|0]|0;if((u&192|0)!=128){r=2;s=2449;break L2774}if((l-e|0)<4){r=1;s=2445;break L2774}w=q&7;x=v&255;v=t<<6;y=u&63;if((x<<12&258048|w<<18|v&4032|y)>>>0>k>>>0){r=2;s=2446;break L2774}b[e>>1]=(x<<2&60|t>>>4&3|((x>>>4&3|w<<2)<<6)+16320|55296)&65535;w=(c[j>>2]|0)+2|0;c[j>>2]=w;b[w>>1]=(y|v&960|56320)&65535;c[g>>2]=(c[g>>2]|0)+4}}while(0);q=(c[j>>2]|0)+2|0;c[j>>2]=q;p=c[g>>2]|0;if(p>>>0<f>>>0){e=q;n=p}else{o=p;break L2772}}if((s|0)==2442){return r|0}else if((s|0)==2443){return r|0}else if((s|0)==2444){return r|0}else if((s|0)==2448){return r|0}else if((s|0)==2449){return r|0}else if((s|0)==2450){return r|0}else if((s|0)==2445){return r|0}else if((s|0)==2446){return r|0}else if((s|0)==2447){return r|0}else if((s|0)==2456){return r|0}else if((s|0)==2457){return r|0}else if((s|0)==2458){return r|0}else if((s|0)==2459){return r|0}else if((s|0)==2460){return r|0}else if((s|0)==2461){return r|0}else if((s|0)==2462){return r|0}else if((s|0)==2452){return r|0}else if((s|0)==2453){return r|0}else if((s|0)==2454){return r|0}else if((s|0)==2455){return r|0}}else{o=m}}while(0);r=o>>>0<f>>>0|0;return r|0}function jn(b,c,e,f,g){b=b|0;c=c|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0;do{if((g&4|0)==0){h=b}else{if((c-b|0)<=2){h=b;break}if((a[b]|0)!=-17){h=b;break}if((a[b+1|0]|0)!=-69){h=b;break}h=(a[b+2|0]|0)==-65?b+3|0:b}}while(0);L2841:do{if(h>>>0<c>>>0&(e|0)!=0){g=c;i=0;j=h;L2843:while(1){k=a[j]|0;l=k&255;if(l>>>0>f>>>0){m=j;break L2841}do{if(k<<24>>24>-1){n=j+1|0;o=i}else{if((k&255)<194){m=j;break L2841}if((k&255)<224){if((g-j|0)<2){m=j;break L2841}p=d[j+1|0]|0;if((p&192|0)!=128){m=j;break L2841}if((p&63|l<<6&1984)>>>0>f>>>0){m=j;break L2841}n=j+2|0;o=i;break}if((k&255)<240){q=j;if((g-q|0)<3){m=j;break L2841}p=a[j+1|0]|0;switch(l|0){case 224:{if((p&-32)<<24>>24!=-96){r=2483;break L2843}break};case 237:{if((p&-32)<<24>>24!=-128){r=2485;break L2843}break};default:{if((p&-64)<<24>>24!=-128){r=2487;break L2843}}}s=d[j+2|0]|0;if((s&192|0)!=128){m=j;break L2841}if(((p&255)<<6&4032|l<<12&61440|s&63)>>>0>f>>>0){m=j;break L2841}n=j+3|0;o=i;break}if((k&255)>=245){m=j;break L2841}t=j;if((g-t|0)<4){m=j;break L2841}if((e-i|0)>>>0<2){m=j;break L2841}s=a[j+1|0]|0;switch(l|0){case 240:{if((s+112&255)>=48){r=2496;break L2843}break};case 244:{if((s&-16)<<24>>24!=-128){r=2498;break L2843}break};default:{if((s&-64)<<24>>24!=-128){r=2500;break L2843}}}p=d[j+2|0]|0;if((p&192|0)!=128){m=j;break L2841}u=d[j+3|0]|0;if((u&192|0)!=128){m=j;break L2841}if(((s&255)<<12&258048|l<<18&1835008|p<<6&4032|u&63)>>>0>f>>>0){m=j;break L2841}n=j+4|0;o=i+1|0}}while(0);l=o+1|0;if(n>>>0<c>>>0&l>>>0<e>>>0){i=l;j=n}else{m=n;break L2841}}if((r|0)==2500){v=t-b|0;return v|0}else if((r|0)==2496){v=t-b|0;return v|0}else if((r|0)==2498){v=t-b|0;return v|0}else if((r|0)==2483){v=q-b|0;return v|0}else if((r|0)==2485){v=q-b|0;return v|0}else if((r|0)==2487){v=q-b|0;return v|0}}else{m=h}}while(0);v=m-b|0;return v|0}function jo(a,b,d,e,f,g,h,j){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0;b=i;i=i+16|0;a=b|0;k=b+8|0;c[a>>2]=d;c[k>>2]=g;l=jm(d,e,a,g,h,k,1114111,0)|0;c[f>>2]=d+((c[a>>2]|0)-d);c[j>>2]=g+((c[k>>2]|0)-g>>1<<1);i=b;return l|0}function jp(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;return jn(c,d,e,1114111,0)|0}function jq(a){a=a|0;de(a|0);kS(a);return}function jr(a,b,d,e,f,g,h,j){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0;b=i;i=i+16|0;a=b|0;k=b+8|0;c[a>>2]=d;c[k>>2]=g;l=jw(d,e,a,g,h,k,1114111,0)|0;c[f>>2]=d+((c[a>>2]|0)-d>>2<<2);c[j>>2]=g+((c[k>>2]|0)-g);i=b;return l|0}function js(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;c[f>>2]=d;return 3}function jt(a){a=a|0;return 0}function ju(a){a=a|0;return 0}function jv(a){a=a|0;return 4}function jw(b,d,e,f,g,h,i,j){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;var k=0,l=0,m=0,n=0;c[e>>2]=b;c[h>>2]=f;do{if((j&2|0)!=0){if((g-f|0)<3){k=1;return k|0}else{c[h>>2]=f+1;a[f]=-17;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=-69;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=-65;break}}}while(0);f=c[e>>2]|0;if(f>>>0>=d>>>0){k=0;return k|0}j=g;g=f;L2912:while(1){f=c[g>>2]|0;if((f&-2048|0)==55296|f>>>0>i>>>0){k=2;l=2548;break}do{if(f>>>0<128){b=c[h>>2]|0;if((j-b|0)<1){k=1;l=2544;break L2912}c[h>>2]=b+1;a[b]=f&255}else{if(f>>>0<2048){b=c[h>>2]|0;if((j-b|0)<2){k=1;l=2547;break L2912}c[h>>2]=b+1;a[b]=(f>>>6|192)&255;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=(f&63|128)&255;break}b=c[h>>2]|0;m=j-b|0;if(f>>>0<65536){if((m|0)<3){k=1;l=2549;break L2912}c[h>>2]=b+1;a[b]=(f>>>12|224)&255;n=c[h>>2]|0;c[h>>2]=n+1;a[n]=(f>>>6&63|128)&255;n=c[h>>2]|0;c[h>>2]=n+1;a[n]=(f&63|128)&255;break}else{if((m|0)<4){k=1;l=2542;break L2912}c[h>>2]=b+1;a[b]=(f>>>18|240)&255;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=(f>>>12&63|128)&255;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=(f>>>6&63|128)&255;b=c[h>>2]|0;c[h>>2]=b+1;a[b]=(f&63|128)&255;break}}}while(0);f=(c[e>>2]|0)+4|0;c[e>>2]=f;if(f>>>0<d>>>0){g=f}else{k=0;l=2543;break}}if((l|0)==2543){return k|0}else if((l|0)==2542){return k|0}else if((l|0)==2544){return k|0}else if((l|0)==2548){return k|0}else if((l|0)==2549){return k|0}else if((l|0)==2547){return k|0}return 0}function jx(b,e,f,g,h,i,j,k){b=b|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0;var l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;c[f>>2]=b;c[i>>2]=g;g=c[f>>2]|0;do{if((k&4|0)==0){l=g}else{if((e-g|0)<=2){l=g;break}if((a[g]|0)!=-17){l=g;break}if((a[g+1|0]|0)!=-69){l=g;break}if((a[g+2|0]|0)!=-65){l=g;break}b=g+3|0;c[f>>2]=b;l=b}}while(0);L2944:do{if(l>>>0<e>>>0){g=e;k=c[i>>2]|0;b=l;L2946:while(1){if(k>>>0>=h>>>0){m=b;break L2944}n=a[b]|0;o=n&255;do{if(n<<24>>24>-1){if(o>>>0>j>>>0){p=2;q=2609;break L2946}c[k>>2]=o;c[f>>2]=(c[f>>2]|0)+1}else{if((n&255)<194){p=2;q=2602;break L2946}if((n&255)<224){if((g-b|0)<2){p=1;q=2605;break L2946}r=d[b+1|0]|0;if((r&192|0)!=128){p=2;q=2598;break L2946}s=r&63|o<<6&1984;if(s>>>0>j>>>0){p=2;q=2604;break L2946}c[k>>2]=s;c[f>>2]=(c[f>>2]|0)+2;break}if((n&255)<240){if((g-b|0)<3){p=1;q=2606;break L2946}s=a[b+1|0]|0;switch(o|0){case 224:{if((s&-32)<<24>>24!=-96){p=2;q=2591;break L2946}break};case 237:{if((s&-32)<<24>>24!=-128){p=2;q=2593;break L2946}break};default:{if((s&-64)<<24>>24!=-128){p=2;q=2603;break L2946}}}r=d[b+2|0]|0;if((r&192|0)!=128){p=2;q=2594;break L2946}t=(s&255)<<6&4032|o<<12&61440|r&63;if(t>>>0>j>>>0){p=2;q=2592;break L2946}c[k>>2]=t;c[f>>2]=(c[f>>2]|0)+3;break}if((n&255)>=245){p=2;q=2607;break L2946}if((g-b|0)<4){p=1;q=2608;break L2946}t=a[b+1|0]|0;switch(o|0){case 240:{if((t+112&255)>=48){p=2;q=2590;break L2946}break};case 244:{if((t&-16)<<24>>24!=-128){p=2;q=2596;break L2946}break};default:{if((t&-64)<<24>>24!=-128){p=2;q=2597;break L2946}}}r=d[b+2|0]|0;if((r&192|0)!=128){p=2;q=2599;break L2946}s=d[b+3|0]|0;if((s&192|0)!=128){p=2;q=2601;break L2946}u=(t&255)<<12&258048|o<<18&1835008|r<<6&4032|s&63;if(u>>>0>j>>>0){p=2;q=2600;break L2946}c[k>>2]=u;c[f>>2]=(c[f>>2]|0)+4}}while(0);o=(c[i>>2]|0)+4|0;c[i>>2]=o;n=c[f>>2]|0;if(n>>>0<e>>>0){k=o;b=n}else{m=n;break L2944}}if((q|0)==2597){return p|0}else if((q|0)==2598){return p|0}else if((q|0)==2599){return p|0}else if((q|0)==2600){return p|0}else if((q|0)==2593){return p|0}else if((q|0)==2594){return p|0}else if((q|0)==2596){return p|0}else if((q|0)==2605){return p|0}else if((q|0)==2606){return p|0}else if((q|0)==2607){return p|0}else if((q|0)==2608){return p|0}else if((q|0)==2609){return p|0}else if((q|0)==2590){return p|0}else if((q|0)==2591){return p|0}else if((q|0)==2592){return p|0}else if((q|0)==2601){return p|0}else if((q|0)==2602){return p|0}else if((q|0)==2603){return p|0}else if((q|0)==2604){return p|0}}else{m=l}}while(0);p=m>>>0<e>>>0|0;return p|0}function jy(b,c,e,f,g){b=b|0;c=c|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0;do{if((g&4|0)==0){h=b}else{if((c-b|0)<=2){h=b;break}if((a[b]|0)!=-17){h=b;break}if((a[b+1|0]|0)!=-69){h=b;break}h=(a[b+2|0]|0)==-65?b+3|0:b}}while(0);L3011:do{if(h>>>0<c>>>0&(e|0)!=0){g=c;i=1;j=h;L3013:while(1){k=a[j]|0;l=k&255;do{if(k<<24>>24>-1){if(l>>>0>f>>>0){m=j;break L3011}n=j+1|0}else{if((k&255)<194){m=j;break L3011}if((k&255)<224){if((g-j|0)<2){m=j;break L3011}o=d[j+1|0]|0;if((o&192|0)!=128){m=j;break L3011}if((o&63|l<<6&1984)>>>0>f>>>0){m=j;break L3011}n=j+2|0;break}if((k&255)<240){p=j;if((g-p|0)<3){m=j;break L3011}o=a[j+1|0]|0;switch(l|0){case 224:{if((o&-32)<<24>>24!=-96){q=2630;break L3013}break};case 237:{if((o&-32)<<24>>24!=-128){q=2632;break L3013}break};default:{if((o&-64)<<24>>24!=-128){q=2634;break L3013}}}r=d[j+2|0]|0;if((r&192|0)!=128){m=j;break L3011}if(((o&255)<<6&4032|l<<12&61440|r&63)>>>0>f>>>0){m=j;break L3011}n=j+3|0;break}if((k&255)>=245){m=j;break L3011}s=j;if((g-s|0)<4){m=j;break L3011}r=a[j+1|0]|0;switch(l|0){case 240:{if((r+112&255)>=48){q=2642;break L3013}break};case 244:{if((r&-16)<<24>>24!=-128){q=2644;break L3013}break};default:{if((r&-64)<<24>>24!=-128){q=2646;break L3013}}}o=d[j+2|0]|0;if((o&192|0)!=128){m=j;break L3011}t=d[j+3|0]|0;if((t&192|0)!=128){m=j;break L3011}if(((r&255)<<12&258048|l<<18&1835008|o<<6&4032|t&63)>>>0>f>>>0){m=j;break L3011}n=j+4|0}}while(0);if(!(n>>>0<c>>>0&i>>>0<e>>>0)){m=n;break L3011}i=i+1|0;j=n}if((q|0)==2644){u=s-b|0;return u|0}else if((q|0)==2642){u=s-b|0;return u|0}else if((q|0)==2630){u=p-b|0;return u|0}else if((q|0)==2632){u=p-b|0;return u|0}else if((q|0)==2634){u=p-b|0;return u|0}else if((q|0)==2646){u=s-b|0;return u|0}}else{m=h}}while(0);u=m-b|0;return u|0}function jz(b){b=b|0;return a[b+8|0]|0}function jA(a){a=a|0;return c[a+8>>2]|0}function jB(b){b=b|0;return a[b+9|0]|0}function jC(a){a=a|0;return c[a+12>>2]|0}function jD(a,b,d,e,f,g,h,j){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;j=j|0;var k=0,l=0;b=i;i=i+16|0;a=b|0;k=b+8|0;c[a>>2]=d;c[k>>2]=g;l=jx(d,e,a,g,h,k,1114111,0)|0;c[f>>2]=d+((c[a>>2]|0)-d);c[j>>2]=g+((c[k>>2]|0)-g>>2<<2);i=b;return l|0}function jE(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;return jy(c,d,e,1114111,0)|0}function jF(a){a=a|0;de(a|0);kS(a);return}function jG(a){a=a|0;de(a|0);kS(a);return}function jH(a){a=a|0;c[a>>2]=3032;dP(a+12|0);de(a|0);kS(a);return}function jI(a){a=a|0;c[a>>2]=3032;dP(a+12|0);de(a|0);return}function jJ(a){a=a|0;c[a>>2]=2984;dP(a+16|0);de(a|0);kS(a);return}function jK(a){a=a|0;c[a>>2]=2984;dP(a+16|0);de(a|0);return}function jL(a,b){a=a|0;b=b|0;dY(a,b+12|0);return}function jM(a,b){a=a|0;b=b|0;dY(a,b+16|0);return}function jN(a,b){a=a|0;b=b|0;dZ(a,1288,4);return}function jO(a,b){a=a|0;b=b|0;ex(a,1264,km(1264)|0);return}function jP(a,b){a=a|0;b=b|0;dZ(a,1256,5);return}function jQ(a,b){a=a|0;b=b|0;ex(a,1232,km(1232)|0);return}function jR(b){b=b|0;var d=0;if((a[16352]|0)!=0){d=c[3702]|0;return d|0}if((bc(16352)|0)==0){d=c[3702]|0;return d|0}do{if((a[16232]|0)==0){if((bc(16232)|0)==0){break}k$(11344,0,168);a$(266,0,u|0)|0}}while(0);dQ(11344,1536)|0;dQ(11356,1528)|0;dQ(11368,1520)|0;dQ(11380,1504)|0;dQ(11392,1488)|0;dQ(11404,1480)|0;dQ(11416,1464)|0;dQ(11428,1456)|0;dQ(11440,1448)|0;dQ(11452,1440)|0;dQ(11464,1432)|0;dQ(11476,1400)|0;dQ(11488,1392)|0;dQ(11500,1384)|0;c[3702]=11344;d=c[3702]|0;return d|0}function jS(b){b=b|0;var d=0;if((a[16296]|0)!=0){d=c[3680]|0;return d|0}if((bc(16296)|0)==0){d=c[3680]|0;return d|0}do{if((a[16208]|0)==0){if((bc(16208)|0)==0){break}k$(10600,0,168);a$(148,0,u|0)|0}}while(0);ef(10600,1912)|0;ef(10612,1880)|0;ef(10624,1848)|0;ef(10636,1808)|0;ef(10648,1768)|0;ef(10660,1736)|0;ef(10672,1696)|0;ef(10684,1680)|0;ef(10696,1624)|0;ef(10708,1608)|0;ef(10720,1592)|0;ef(10732,1576)|0;ef(10744,1560)|0;ef(10756,1544)|0;c[3680]=10600;d=c[3680]|0;return d|0}function jT(b){b=b|0;var d=0;if((a[16344]|0)!=0){d=c[3700]|0;return d|0}if((bc(16344)|0)==0){d=c[3700]|0;return d|0}do{if((a[16224]|0)==0){if((bc(16224)|0)==0){break}k$(11056,0,288);a$(170,0,u|0)|0}}while(0);dQ(11056,288)|0;dQ(11068,272)|0;dQ(11080,264)|0;dQ(11092,256)|0;dQ(11104,248)|0;dQ(11116,240)|0;dQ(11128,232)|0;dQ(11140,224)|0;dQ(11152,168)|0;dQ(11164,160)|0;dQ(11176,144)|0;dQ(11188,128)|0;dQ(11200,120)|0;dQ(11212,112)|0;dQ(11224,104)|0;dQ(11236,96)|0;dQ(11248,248)|0;dQ(11260,88)|0;dQ(11272,80)|0;dQ(11284,1976)|0;dQ(11296,1968)|0;dQ(11308,1960)|0;dQ(11320,1952)|0;dQ(11332,1944)|0;c[3700]=11056;d=c[3700]|0;return d|0}function jU(b){b=b|0;var d=0;if((a[16288]|0)!=0){d=c[3678]|0;return d|0}if((bc(16288)|0)==0){d=c[3678]|0;return d|0}do{if((a[16200]|0)==0){if((bc(16200)|0)==0){break}k$(10312,0,288);a$(122,0,u|0)|0}}while(0);ef(10312,824)|0;ef(10324,784)|0;ef(10336,760)|0;ef(10348,736)|0;ef(10360,424)|0;ef(10372,712)|0;ef(10384,688)|0;ef(10396,656)|0;ef(10408,616)|0;ef(10420,584)|0;ef(10432,544)|0;ef(10444,504)|0;ef(10456,488)|0;ef(10468,472)|0;ef(10480,456)|0;ef(10492,440)|0;ef(10504,424)|0;ef(10516,408)|0;ef(10528,392)|0;ef(10540,376)|0;ef(10552,344)|0;ef(10564,328)|0;ef(10576,312)|0;ef(10588,296)|0;c[3678]=10312;d=c[3678]|0;return d|0}function jV(b){b=b|0;var d=0;if((a[16360]|0)!=0){d=c[3704]|0;return d|0}if((bc(16360)|0)==0){d=c[3704]|0;return d|0}do{if((a[16240]|0)==0){if((bc(16240)|0)==0){break}k$(11512,0,288);a$(120,0,u|0)|0}}while(0);dQ(11512,864)|0;dQ(11524,856)|0;c[3704]=11512;d=c[3704]|0;return d|0}function jW(b){b=b|0;var d=0;if((a[16304]|0)!=0){d=c[3682]|0;return d|0}if((bc(16304)|0)==0){d=c[3682]|0;return d|0}do{if((a[16216]|0)==0){if((bc(16216)|0)==0){break}k$(10768,0,288);a$(240,0,u|0)|0}}while(0);ef(10768,888)|0;ef(10780,872)|0;c[3682]=10768;d=c[3682]|0;return d|0}function jX(b){b=b|0;if((a[16368]|0)!=0){return 14824}if((bc(16368)|0)==0){return 14824}dZ(14824,1216,8);a$(258,14824,u|0)|0;return 14824}function jY(b){b=b|0;if((a[16312]|0)!=0){return 14736}if((bc(16312)|0)==0){return 14736}ex(14736,1176,km(1176)|0);a$(192,14736,u|0)|0;return 14736}function jZ(b){b=b|0;if((a[16392]|0)!=0){return 14872}if((bc(16392)|0)==0){return 14872}dZ(14872,1144,8);a$(258,14872,u|0)|0;return 14872}function j_(b){b=b|0;if((a[16336]|0)!=0){return 14784}if((bc(16336)|0)==0){return 14784}ex(14784,1104,km(1104)|0);a$(192,14784,u|0)|0;return 14784}function j$(b){b=b|0;if((a[16384]|0)!=0){return 14856}if((bc(16384)|0)==0){return 14856}dZ(14856,1080,20);a$(258,14856,u|0)|0;return 14856}function j0(b){b=b|0;if((a[16328]|0)!=0){return 14768}if((bc(16328)|0)==0){return 14768}ex(14768,992,km(992)|0);a$(192,14768,u|0)|0;return 14768}function j1(b){b=b|0;if((a[16376]|0)!=0){return 14840}if((bc(16376)|0)==0){return 14840}dZ(14840,976,11);a$(258,14840,u|0)|0;return 14840}function j2(b){b=b|0;if((a[16320]|0)!=0){return 14752}if((bc(16320)|0)==0){return 14752}ex(14752,928,km(928)|0);a$(192,14752,u|0)|0;return 14752}function j3(a){a=a|0;var b=0,d=0,e=0,f=0;b=a+4|0;d=(c[a>>2]|0)+(c[b+4>>2]|0)|0;a=d;e=c[b>>2]|0;if((e&1|0)==0){f=e;bY[f&511](a);return}else{f=c[(c[d>>2]|0)+(e-1)>>2]|0;bY[f&511](a);return}}function j4(a){a=a|0;ee(11044);ee(11032);ee(11020);ee(11008);ee(10996);ee(10984);ee(10972);ee(10960);ee(10948);ee(10936);ee(10924);ee(10912);ee(10900);ee(10888);ee(10876);ee(10864);ee(10852);ee(10840);ee(10828);ee(10816);ee(10804);ee(10792);ee(10780);ee(10768);return}function j5(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0;e=b+8|0;f=b+4|0;g=c[f>>2]|0;h=c[e>>2]|0;i=g;if(h-i>>2>>>0>=d>>>0){j=d;k=g;do{if((k|0)==0){l=0}else{c[k>>2]=0;l=c[f>>2]|0}k=l+4|0;c[f>>2]=k;j=j-1|0;}while((j|0)!=0);return}j=b+16|0;k=b|0;l=c[k>>2]|0;g=i-l>>2;i=g+d|0;if(i>>>0>1073741823){ir(0)}m=h-l|0;do{if(m>>2>>>0>536870910){n=1073741823;o=2919}else{l=m>>1;h=l>>>0<i>>>0?i:l;if((h|0)==0){p=0;q=0;break}l=b+128|0;if(!((a[l]&1)==0&h>>>0<29)){n=h;o=2919;break}a[l]=1;p=j;q=h}}while(0);if((o|0)==2919){p=kO(n<<2)|0;q=n}n=d;d=p+(g<<2)|0;do{if((d|0)==0){r=0}else{c[d>>2]=0;r=d}d=r+4|0;n=n-1|0;}while((n|0)!=0);n=p+(q<<2)|0;q=c[k>>2]|0;r=(c[f>>2]|0)-q|0;o=p+(g-(r>>2)<<2)|0;g=o;p=q;kY(g|0,p|0,r)|0;c[k>>2]=o;c[f>>2]=d;c[e>>2]=n;if((q|0)==0){return}if((q|0)==(j|0)){a[b+128|0]=0;return}else{kS(p);return}}function j6(a){a=a|0;dP(11788);dP(11776);dP(11764);dP(11752);dP(11740);dP(11728);dP(11716);dP(11704);dP(11692);dP(11680);dP(11668);dP(11656);dP(11644);dP(11632);dP(11620);dP(11608);dP(11596);dP(11584);dP(11572);dP(11560);dP(11548);dP(11536);dP(11524);dP(11512);return}function j7(a){a=a|0;ee(10588);ee(10576);ee(10564);ee(10552);ee(10540);ee(10528);ee(10516);ee(10504);ee(10492);ee(10480);ee(10468);ee(10456);ee(10444);ee(10432);ee(10420);ee(10408);ee(10396);ee(10384);ee(10372);ee(10360);ee(10348);ee(10336);ee(10324);ee(10312);return}function j8(a){a=a|0;dP(11332);dP(11320);dP(11308);dP(11296);dP(11284);dP(11272);dP(11260);dP(11248);dP(11236);dP(11224);dP(11212);dP(11200);dP(11188);dP(11176);dP(11164);dP(11152);dP(11140);dP(11128);dP(11116);dP(11104);dP(11092);dP(11080);dP(11068);dP(11056);return}function j9(a){a=a|0;ee(10756);ee(10744);ee(10732);ee(10720);ee(10708);ee(10696);ee(10684);ee(10672);ee(10660);ee(10648);ee(10636);ee(10624);ee(10612);ee(10600);return}function ka(a){a=a|0;dP(11500);dP(11488);dP(11476);dP(11464);dP(11452);dP(11440);dP(11428);dP(11416);dP(11404);dP(11392);dP(11380);dP(11368);dP(11356);dP(11344);return}function kb(a,b,c){a=a|0;b=b|0;c=c|0;return kc(0,a,b,(c|0)!=0?c:9832)|0}function kc(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,t=0,u=0,v=0,w=0;g=i;i=i+8|0;h=g|0;c[h>>2]=b;j=((f|0)==0?9824:f)|0;f=c[j>>2]|0;L7:do{if((d|0)==0){if((f|0)==0){k=0}else{break}i=g;return k|0}else{if((b|0)==0){l=h;c[h>>2]=l;m=l}else{m=b}if((e|0)==0){k=-2;i=g;return k|0}do{if((f|0)==0){l=a[d]|0;n=l&255;if(l<<24>>24>-1){c[m>>2]=n;k=l<<24>>24!=0|0;i=g;return k|0}else{l=n-194|0;if(l>>>0>50){break L7}o=d+1|0;p=c[s+(l<<2)>>2]|0;q=e-1|0;break}}else{o=d;p=f;q=e}}while(0);L25:do{if((q|0)==0){r=p}else{l=a[o]|0;n=(l&255)>>>3;if((n-16|n+(p>>26))>>>0>7){break L7}else{t=o;u=p;v=q;w=l}while(1){t=t+1|0;u=(w&255)-128|u<<6;v=v-1|0;if((u|0)>=0){break}if((v|0)==0){r=u;break L25}w=a[t]|0;if(((w&255)-128|0)>>>0>63){break L7}}c[j>>2]=0;c[m>>2]=u;k=e-v|0;i=g;return k|0}}while(0);c[j>>2]=r;k=-2;i=g;return k|0}}while(0);c[j>>2]=0;c[(bw()|0)>>2]=138;k=-1;i=g;return k|0}function kd(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0;g=i;i=i+1032|0;h=g|0;j=g+1024|0;k=c[b>>2]|0;c[j>>2]=k;l=(a|0)!=0;m=l?e:256;e=l?a:h|0;L38:do{if((k|0)==0|(m|0)==0){n=0;o=d;p=m;q=e;r=k}else{a=h|0;s=m;t=d;u=0;v=e;w=k;while(1){x=t>>>2;y=x>>>0>=s>>>0;if(!(y|t>>>0>131)){n=u;o=t;p=s;q=v;r=w;break L38}z=y?s:x;A=t-z|0;x=ke(v,j,z,f)|0;if((x|0)==-1){break}if((v|0)==(a|0)){B=a;C=s}else{B=v+(x<<2)|0;C=s-x|0}z=x+u|0;x=c[j>>2]|0;if((x|0)==0|(C|0)==0){n=z;o=A;p=C;q=B;r=x;break L38}else{s=C;t=A;u=z;v=B;w=x}}n=-1;o=A;p=0;q=v;r=c[j>>2]|0}}while(0);L49:do{if((r|0)==0){D=n}else{if((p|0)==0|(o|0)==0){D=n;break}else{E=p;F=o;G=n;H=q;I=r}while(1){J=kc(H,I,F,f)|0;if((J+2|0)>>>0<3){break}A=(c[j>>2]|0)+J|0;c[j>>2]=A;B=E-1|0;C=G+1|0;if((B|0)==0|(F|0)==(J|0)){D=C;break L49}else{E=B;F=F-J|0;G=C;H=H+4|0;I=A}}switch(J|0){case 0:{c[j>>2]=0;D=G;break L49;break};case-1:{D=-1;break L49;break};default:{c[f>>2]=0;D=G;break L49}}}}while(0);if(!l){i=g;return D|0}c[b>>2]=c[j>>2];i=g;return D|0}function ke(b,e,f,g){b=b|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0;h=c[e>>2]|0;do{if((g|0)==0){i=56}else{j=g|0;k=c[j>>2]|0;if((k|0)==0){i=56;break}if((b|0)==0){l=k;m=h;n=f;i=67;break}c[j>>2]=0;o=k;p=h;q=b;r=f;i=87}}while(0);if((i|0)==56){if((b|0)==0){t=h;u=f;i=58}else{v=h;w=b;x=f;i=57}}L70:while(1){if((i|0)==87){i=0;h=d[p]|0;g=h>>>3;if((g-16|g+(o>>26))>>>0>7){i=88;break}g=p+1|0;y=h-128|o<<6;do{if((y|0)<0){h=(d[g]|0)-128|0;if(h>>>0>63){i=91;break L70}k=p+2|0;z=h|y<<6;if((z|0)>=0){A=z;B=k;break}h=(d[k]|0)-128|0;if(h>>>0>63){i=94;break L70}A=h|z<<6;B=p+3|0}else{A=y;B=g}}while(0);c[q>>2]=A;v=B;w=q+4|0;x=r-1|0;i=57;continue}else if((i|0)==58){i=0;g=a[t]|0;do{if(((g&255)-1|0)>>>0<127){if((t&3|0)!=0){C=t;D=u;E=g;break}h=c[t>>2]|0;if(((h-16843009|h)&-2139062144|0)==0){F=u;G=t}else{C=t;D=u;E=h&255;break}do{G=G+4|0;F=F-4|0;H=c[G>>2]|0;}while(((H-16843009|H)&-2139062144|0)==0);C=G;D=F;E=H&255}else{C=t;D=u;E=g}}while(0);g=E&255;if((g-1|0)>>>0<127){t=C+1|0;u=D-1|0;i=58;continue}h=g-194|0;if(h>>>0>50){I=D;J=b;K=C;i=98;break}l=c[s+(h<<2)>>2]|0;m=C+1|0;n=D;i=67;continue}else if((i|0)==57){i=0;if((x|0)==0){L=f;i=106;break}else{M=x;N=w;O=v}while(1){h=a[O]|0;do{if(((h&255)-1|0)>>>0<127){if((O&3|0)==0&M>>>0>3){P=M;Q=N;R=O}else{S=O;T=N;U=M;V=h;break}while(1){W=c[R>>2]|0;if(((W-16843009|W)&-2139062144|0)!=0){i=81;break}c[Q>>2]=W&255;c[Q+4>>2]=d[R+1|0]|0;c[Q+8>>2]=d[R+2|0]|0;X=R+4|0;Y=Q+16|0;c[Q+12>>2]=d[R+3|0]|0;Z=P-4|0;if(Z>>>0>3){P=Z;Q=Y;R=X}else{i=82;break}}if((i|0)==81){i=0;S=R;T=Q;U=P;V=W&255;break}else if((i|0)==82){i=0;S=X;T=Y;U=Z;V=a[X]|0;break}}else{S=O;T=N;U=M;V=h}}while(0);_=V&255;if((_-1|0)>>>0>=127){break}c[T>>2]=_;h=U-1|0;if((h|0)==0){L=f;i=105;break L70}else{M=h;N=T+4|0;O=S+1|0}}h=_-194|0;if(h>>>0>50){I=U;J=T;K=S;i=98;break}o=c[s+(h<<2)>>2]|0;p=S+1|0;q=T;r=U;i=87;continue}else if((i|0)==67){i=0;h=(d[m]|0)>>>3;if((h-16|h+(l>>26))>>>0>7){i=68;break}h=m+1|0;do{if((l&33554432|0)==0){$=h}else{if(((d[h]|0)-128|0)>>>0>63){i=71;break L70}g=m+2|0;if((l&524288|0)==0){$=g;break}if(((d[g]|0)-128|0)>>>0>63){i=74;break L70}$=m+3|0}}while(0);t=$;u=n-1|0;i=58;continue}}if((i|0)==71){aa=l;ab=m-1|0;ac=b;ad=n;i=97}else if((i|0)==74){aa=l;ab=m-1|0;ac=b;ad=n;i=97}else if((i|0)==68){aa=l;ab=m-1|0;ac=b;ad=n;i=97}else if((i|0)==88){aa=o;ab=p-1|0;ac=q;ad=r;i=97}else if((i|0)==91){aa=y;ab=p-1|0;ac=q;ad=r;i=97}else if((i|0)==94){aa=z;ab=p-1|0;ac=q;ad=r;i=97}else if((i|0)==105){return L|0}else if((i|0)==106){return L|0}if((i|0)==97){if((aa|0)==0){I=ad;J=ac;K=ab;i=98}else{ae=ac;af=ab}}do{if((i|0)==98){if((a[K]|0)!=0){ae=J;af=K;break}if((J|0)!=0){c[J>>2]=0;c[e>>2]=0}L=f-I|0;return L|0}}while(0);c[(bw()|0)>>2]=138;if((ae|0)==0){L=-1;return L|0}c[e>>2]=af;L=-1;return L|0}function kf(b,e,f){b=b|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0;g=i;i=i+8|0;h=g|0;c[h>>2]=b;if((e|0)==0){j=0;i=g;return j|0}do{if((f|0)!=0){if((b|0)==0){k=h;c[h>>2]=k;l=k}else{l=b}k=a[e]|0;m=k&255;if(k<<24>>24>-1){c[l>>2]=m;j=k<<24>>24!=0|0;i=g;return j|0}k=m-194|0;if(k>>>0>50){break}m=e+1|0;n=c[s+(k<<2)>>2]|0;if(f>>>0<4){if((n&-2147483648>>>(((f*6|0)-6|0)>>>0)|0)!=0){break}}k=d[m]|0;m=k>>>3;if((m-16|m+(n>>26))>>>0>7){break}m=k-128|n<<6;if((m|0)>=0){c[l>>2]=m;j=2;i=g;return j|0}n=(d[e+2|0]|0)-128|0;if(n>>>0>63){break}k=n|m<<6;if((k|0)>=0){c[l>>2]=k;j=3;i=g;return j|0}m=(d[e+3|0]|0)-128|0;if(m>>>0>63){break}c[l>>2]=m|k<<6;j=4;i=g;return j|0}}while(0);c[(bw()|0)>>2]=138;j=-1;i=g;return j|0}function kg(b,d,e){b=b|0;d=d|0;e=e|0;var f=0;if((b|0)==0){f=1;return f|0}if(d>>>0<128){a[b]=d&255;f=1;return f|0}if(d>>>0<2048){a[b]=(d>>>6|192)&255;a[b+1|0]=(d&63|128)&255;f=2;return f|0}if(d>>>0<55296|(d-57344|0)>>>0<8192){a[b]=(d>>>12|224)&255;a[b+1|0]=(d>>>6&63|128)&255;a[b+2|0]=(d&63|128)&255;f=3;return f|0}if((d-65536|0)>>>0<1048576){a[b]=(d>>>18|240)&255;a[b+1|0]=(d>>>12&63|128)&255;a[b+2|0]=(d>>>6&63|128)&255;a[b+3|0]=(d&63|128)&255;f=4;return f|0}else{c[(bw()|0)>>2]=138;f=-1;return f|0}return 0}function kh(a){a=a|0;return}function ki(a){a=a|0;return}function kj(a){a=a|0;return 1328|0}function kk(a){a=a|0;return}function kl(a){a=a|0;return}function km(a){a=a|0;var b=0;b=a;while(1){if((c[b>>2]|0)==0){break}else{b=b+4|0}}return b-a>>2|0}function kn(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0;if((d|0)==0){return a|0}else{e=b;f=d;g=a}while(1){d=f-1|0;c[g>>2]=c[e>>2];if((d|0)==0){break}else{e=e+4|0;f=d;g=g+4|0}}return a|0}function ko(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0,i=0;e=(d|0)==0;if(a-b>>2>>>0<d>>>0){if(e){return a|0}else{f=d}do{f=f-1|0;c[a+(f<<2)>>2]=c[b+(f<<2)>>2];}while((f|0)!=0);return a|0}else{if(e){return a|0}else{g=b;h=d;i=a}while(1){d=h-1|0;c[i>>2]=c[g>>2];if((d|0)==0){break}else{g=g+4|0;h=d;i=i+4|0}}return a|0}return 0}function kp(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0;if((d|0)==0){return a|0}else{e=d;f=a}while(1){d=e-1|0;c[f>>2]=b;if((d|0)==0){break}else{e=d;f=f+4|0}}return a|0}function kq(a){a=a|0;c[a>>2]=2440;return}function kr(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0;if((c[d+8>>2]|0)!=(b|0)){return}b=d+16|0;g=c[b>>2]|0;if((g|0)==0){c[b>>2]=e;c[d+24>>2]=f;c[d+36>>2]=1;return}if((g|0)!=(e|0)){e=d+36|0;c[e>>2]=(c[e>>2]|0)+1;c[d+24>>2]=2;a[d+54|0]=1;return}e=d+24|0;if((c[e>>2]|0)!=2){return}c[e>>2]=f;return}function ks(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;f=i;i=i+264|0;g=f|0;h=f+256|0;j=c[b>>2]|0;c[h>>2]=j;k=(a|0)!=0;l=k?e:256;e=k?a:g|0;L243:do{if((j|0)==0|(l|0)==0){m=0;n=d;o=l;p=e;q=j}else{a=g|0;r=l;s=d;t=0;u=e;v=j;while(1){w=s>>>0>=r>>>0;if(!(w|s>>>0>32)){m=t;n=s;o=r;p=u;q=v;break L243}x=w?r:s;y=s-x|0;w=kt(u,h,x,0)|0;if((w|0)==-1){break}if((u|0)==(a|0)){z=a;A=r}else{z=u+w|0;A=r-w|0}x=w+t|0;w=c[h>>2]|0;if((w|0)==0|(A|0)==0){m=x;n=y;o=A;p=z;q=w;break L243}else{r=A;s=y;t=x;u=z;v=w}}m=-1;n=y;o=0;p=u;q=c[h>>2]|0}}while(0);L254:do{if((q|0)==0){B=m}else{if((o|0)==0|(n|0)==0){B=m;break}else{C=o;D=n;E=m;F=p;G=q}while(1){H=kg(F,c[G>>2]|0,0)|0;if((H+1|0)>>>0<2){break}y=(c[h>>2]|0)+4|0;c[h>>2]=y;z=D-1|0;A=E+1|0;if((C|0)==(H|0)|(z|0)==0){B=A;break L254}else{C=C-H|0;D=z;E=A;F=F+H|0;G=y}}if((H|0)!=0){B=-1;break}c[h>>2]=0;B=E}}while(0);if(!k){i=f;return B|0}c[b>>2]=c[h>>2];i=f;return B|0}function kt(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;f=i;i=i+8|0;g=f|0;if((b|0)==0){h=c[d>>2]|0;j=g|0;k=c[h>>2]|0;if((k|0)==0){l=0;i=f;return l|0}else{m=0;n=h;o=k}while(1){if(o>>>0>127){k=kg(j,o,0)|0;if((k|0)==-1){l=-1;p=240;break}else{q=k}}else{q=1}k=q+m|0;h=n+4|0;r=c[h>>2]|0;if((r|0)==0){l=k;p=241;break}else{m=k;n=h;o=r}}if((p|0)==240){i=f;return l|0}else if((p|0)==241){i=f;return l|0}}L280:do{if(e>>>0>3){o=e;n=b;m=c[d>>2]|0;while(1){q=c[m>>2]|0;if((q|0)==0){s=o;t=n;break L280}if(q>>>0>127){j=kg(n,q,0)|0;if((j|0)==-1){l=-1;break}u=n+j|0;v=o-j|0;w=m}else{a[n]=q&255;u=n+1|0;v=o-1|0;w=c[d>>2]|0}q=w+4|0;c[d>>2]=q;if(v>>>0>3){o=v;n=u;m=q}else{s=v;t=u;break L280}}i=f;return l|0}else{s=e;t=b}}while(0);L292:do{if((s|0)==0){x=0}else{b=g|0;u=s;v=t;w=c[d>>2]|0;while(1){m=c[w>>2]|0;if((m|0)==0){p=236;break}if(m>>>0>127){n=kg(b,m,0)|0;if((n|0)==-1){l=-1;p=243;break}if(n>>>0>u>>>0){p=232;break}o=c[w>>2]|0;kg(v,o,0)|0;y=v+n|0;z=u-n|0;A=w}else{a[v]=m&255;y=v+1|0;z=u-1|0;A=c[d>>2]|0}m=A+4|0;c[d>>2]=m;if((z|0)==0){x=0;break L292}else{u=z;v=y;w=m}}if((p|0)==232){l=e-u|0;i=f;return l|0}else if((p|0)==236){a[v]=0;x=u;break}else if((p|0)==243){i=f;return l|0}}}while(0);c[d>>2]=0;l=e-x|0;i=f;return l|0}function ku(a){a=a|0;kS(a);return}function kv(a){a=a|0;kh(a|0);return}function kw(a){a=a|0;kh(a|0);kS(a);return}function kx(a){a=a|0;kh(a|0);kS(a);return}function ky(a){a=a|0;kh(a|0);kS(a);return}function kz(a,b,d){a=a|0;b=b|0;d=d|0;var e=0,f=0,g=0,h=0;e=i;i=i+56|0;f=e|0;if((a|0)==(b|0)){g=1;i=e;return g|0}if((b|0)==0){g=0;i=e;return g|0}h=kC(b,9704,9688,-1)|0;b=h;if((h|0)==0){g=0;i=e;return g|0}k$(f|0,0,56);c[f>>2]=b;c[f+8>>2]=a;c[f+12>>2]=-1;c[f+48>>2]=1;cb[c[(c[h>>2]|0)+28>>2]&15](b,f,c[d>>2]|0,1);if((c[f+24>>2]|0)!=1){g=0;i=e;return g|0}c[d>>2]=c[f+16>>2];g=1;i=e;return g|0}function kA(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0;if((b|0)!=(c[d+8>>2]|0)){g=c[b+8>>2]|0;cb[c[(c[g>>2]|0)+28>>2]&15](g,d,e,f);return}g=d+16|0;b=c[g>>2]|0;if((b|0)==0){c[g>>2]=e;c[d+24>>2]=f;c[d+36>>2]=1;return}if((b|0)!=(e|0)){e=d+36|0;c[e>>2]=(c[e>>2]|0)+1;c[d+24>>2]=2;a[d+54|0]=1;return}e=d+24|0;if((c[e>>2]|0)!=2){return}c[e>>2]=f;return}function kB(b,d,e,f){b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0,m=0;if((b|0)==(c[d+8>>2]|0)){g=d+16|0;h=c[g>>2]|0;if((h|0)==0){c[g>>2]=e;c[d+24>>2]=f;c[d+36>>2]=1;return}if((h|0)!=(e|0)){h=d+36|0;c[h>>2]=(c[h>>2]|0)+1;c[d+24>>2]=2;a[d+54|0]=1;return}h=d+24|0;if((c[h>>2]|0)!=2){return}c[h>>2]=f;return}h=c[b+12>>2]|0;g=b+16+(h<<3)|0;i=c[b+20>>2]|0;j=i>>8;if((i&1|0)==0){k=j}else{k=c[(c[e>>2]|0)+j>>2]|0}j=c[b+16>>2]|0;cb[c[(c[j>>2]|0)+28>>2]&15](j,d,e+k|0,(i&2|0)!=0?f:2);if((h|0)<=1){return}h=d+54|0;i=e;k=b+24|0;while(1){b=c[k+4>>2]|0;j=b>>8;if((b&1|0)==0){l=j}else{l=c[(c[i>>2]|0)+j>>2]|0}j=c[k>>2]|0;cb[c[(c[j>>2]|0)+28>>2]&15](j,d,e+l|0,(b&2|0)!=0?f:2);if((a[h]&1)!=0){m=297;break}b=k+8|0;if(b>>>0<g>>>0){k=b}else{m=298;break}}if((m|0)==297){return}else if((m|0)==298){return}}function kC(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,j=0,k=0,l=0,m=0,n=0,o=0;f=i;i=i+56|0;g=f|0;h=c[a>>2]|0;j=a+(c[h-8>>2]|0)|0;k=c[h-4>>2]|0;h=k;c[g>>2]=d;c[g+4>>2]=a;c[g+8>>2]=b;c[g+12>>2]=e;e=g+16|0;b=g+20|0;a=g+24|0;l=g+28|0;m=g+32|0;n=g+40|0;k$(e|0,0,39);if((k|0)==(d|0)){c[g+48>>2]=1;b9[c[(c[k>>2]|0)+20>>2]&31](h,g,j,j,1,0);i=f;return((c[a>>2]|0)==1?j:0)|0}bW[c[(c[k>>2]|0)+24>>2]&7](h,g,j,1,0);switch(c[g+36>>2]|0){case 1:{do{if((c[a>>2]|0)!=1){if((c[n>>2]|0)!=0){o=0;i=f;return o|0}if((c[l>>2]|0)!=1){o=0;i=f;return o|0}if((c[m>>2]|0)==1){break}else{o=0}i=f;return o|0}}while(0);o=c[e>>2]|0;i=f;return o|0};case 0:{if((c[n>>2]|0)!=1){o=0;i=f;return o|0}if((c[l>>2]|0)!=1){o=0;i=f;return o|0}o=(c[m>>2]|0)==1?c[b>>2]|0:0;i=f;return o|0};default:{o=0;i=f;return o|0}}return 0}function kD(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;if((c[d+8>>2]|0)==(b|0)){if((c[d+4>>2]|0)!=(e|0)){return}g=d+28|0;if((c[g>>2]|0)==1){return}c[g>>2]=f;return}if((c[d>>2]|0)!=(b|0)){return}do{if((c[d+16>>2]|0)!=(e|0)){b=d+20|0;if((c[b>>2]|0)==(e|0)){break}c[d+32>>2]=f;c[b>>2]=e;b=d+40|0;c[b>>2]=(c[b>>2]|0)+1;do{if((c[d+36>>2]|0)==1){if((c[d+24>>2]|0)!=2){break}a[d+54|0]=1}}while(0);c[d+44>>2]=4;return}}while(0);if((f|0)!=1){return}c[d+32>>2]=1;return}function kE(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var i=0;if((c[d+8>>2]|0)!=(b|0)){return}a[d+53|0]=1;if((c[d+4>>2]|0)!=(f|0)){return}a[d+52|0]=1;f=d+16|0;b=c[f>>2]|0;if((b|0)==0){c[f>>2]=e;c[d+24>>2]=g;c[d+36>>2]=1;if(!((c[d+48>>2]|0)==1&(g|0)==1)){return}a[d+54|0]=1;return}if((b|0)!=(e|0)){e=d+36|0;c[e>>2]=(c[e>>2]|0)+1;a[d+54|0]=1;return}e=d+24|0;b=c[e>>2]|0;if((b|0)==2){c[e>>2]=g;i=g}else{i=b}if(!((c[d+48>>2]|0)==1&(i|0)==1)){return}a[d+54|0]=1;return}function kF(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0;h=b|0;if((h|0)==(c[d+8>>2]|0)){if((c[d+4>>2]|0)!=(e|0)){return}i=d+28|0;if((c[i>>2]|0)==1){return}c[i>>2]=f;return}if((h|0)==(c[d>>2]|0)){do{if((c[d+16>>2]|0)!=(e|0)){h=d+20|0;if((c[h>>2]|0)==(e|0)){break}c[d+32>>2]=f;i=d+44|0;if((c[i>>2]|0)==4){return}j=c[b+12>>2]|0;k=b+16+(j<<3)|0;L474:do{if((j|0)>0){l=d+52|0;m=d+53|0;n=d+54|0;o=b+8|0;p=d+24|0;q=e;r=0;s=b+16|0;t=0;L476:while(1){a[l]=0;a[m]=0;u=c[s+4>>2]|0;v=u>>8;if((u&1|0)==0){w=v}else{w=c[(c[q>>2]|0)+v>>2]|0}v=c[s>>2]|0;b9[c[(c[v>>2]|0)+20>>2]&31](v,d,e,e+w|0,2-(u>>>1&1)|0,g);if((a[n]&1)!=0){x=t;y=r;break}do{if((a[m]&1)==0){z=t;A=r}else{if((a[l]&1)==0){if((c[o>>2]&1|0)==0){x=1;y=r;break L476}else{z=1;A=r;break}}if((c[p>>2]|0)==1){B=385;break L474}if((c[o>>2]&2|0)==0){B=385;break L474}else{z=1;A=1}}}while(0);u=s+8|0;if(u>>>0<k>>>0){r=A;s=u;t=z}else{x=z;y=A;break}}if(y){C=x;B=384}else{D=x;B=381}}else{D=0;B=381}}while(0);do{if((B|0)==381){c[h>>2]=e;k=d+40|0;c[k>>2]=(c[k>>2]|0)+1;if((c[d+36>>2]|0)!=1){C=D;B=384;break}if((c[d+24>>2]|0)!=2){C=D;B=384;break}a[d+54|0]=1;if(D){B=385}else{B=386}}}while(0);if((B|0)==384){if(C){B=385}else{B=386}}if((B|0)==385){c[i>>2]=3;return}else if((B|0)==386){c[i>>2]=4;return}}}while(0);if((f|0)!=1){return}c[d+32>>2]=1;return}C=c[b+12>>2]|0;D=b+16+(C<<3)|0;x=c[b+20>>2]|0;y=x>>8;if((x&1|0)==0){E=y}else{E=c[(c[e>>2]|0)+y>>2]|0}y=c[b+16>>2]|0;bW[c[(c[y>>2]|0)+24>>2]&7](y,d,e+E|0,(x&2|0)!=0?f:2,g);x=b+24|0;if((C|0)<=1){return}C=c[b+8>>2]|0;do{if((C&2|0)==0){b=d+36|0;if((c[b>>2]|0)==1){break}if((C&1|0)==0){E=d+54|0;y=e;A=x;while(1){if((a[E]&1)!=0){B=425;break}if((c[b>>2]|0)==1){B=426;break}z=c[A+4>>2]|0;w=z>>8;if((z&1|0)==0){F=w}else{F=c[(c[y>>2]|0)+w>>2]|0}w=c[A>>2]|0;bW[c[(c[w>>2]|0)+24>>2]&7](w,d,e+F|0,(z&2|0)!=0?f:2,g);z=A+8|0;if(z>>>0<D>>>0){A=z}else{B=412;break}}if((B|0)==412){return}else if((B|0)==425){return}else if((B|0)==426){return}}A=d+24|0;y=d+54|0;E=e;i=x;while(1){if((a[y]&1)!=0){B=422;break}if((c[b>>2]|0)==1){if((c[A>>2]|0)==1){B=423;break}}z=c[i+4>>2]|0;w=z>>8;if((z&1|0)==0){G=w}else{G=c[(c[E>>2]|0)+w>>2]|0}w=c[i>>2]|0;bW[c[(c[w>>2]|0)+24>>2]&7](w,d,e+G|0,(z&2|0)!=0?f:2,g);z=i+8|0;if(z>>>0<D>>>0){i=z}else{B=424;break}}if((B|0)==422){return}else if((B|0)==423){return}else if((B|0)==424){return}}}while(0);G=d+54|0;F=e;C=x;while(1){if((a[G]&1)!=0){B=421;break}x=c[C+4>>2]|0;i=x>>8;if((x&1|0)==0){H=i}else{H=c[(c[F>>2]|0)+i>>2]|0}i=c[C>>2]|0;bW[c[(c[i>>2]|0)+24>>2]&7](i,d,e+H|0,(x&2|0)!=0?f:2,g);x=C+8|0;if(x>>>0<D>>>0){C=x}else{B=420;break}}if((B|0)==420){return}else if((B|0)==421){return}}function kG(b,d,e,f,g){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;var h=0,i=0,j=0,k=0,l=0,m=0,n=0;h=b|0;if((h|0)==(c[d+8>>2]|0)){if((c[d+4>>2]|0)!=(e|0)){return}i=d+28|0;if((c[i>>2]|0)==1){return}c[i>>2]=f;return}if((h|0)!=(c[d>>2]|0)){h=c[b+8>>2]|0;bW[c[(c[h>>2]|0)+24>>2]&7](h,d,e,f,g);return}do{if((c[d+16>>2]|0)!=(e|0)){h=d+20|0;if((c[h>>2]|0)==(e|0)){break}c[d+32>>2]=f;i=d+44|0;if((c[i>>2]|0)==4){return}j=d+52|0;a[j]=0;k=d+53|0;a[k]=0;l=c[b+8>>2]|0;b9[c[(c[l>>2]|0)+20>>2]&31](l,d,e,e,1,g);if((a[k]&1)==0){m=0;n=441}else{if((a[j]&1)==0){m=1;n=441}}L576:do{if((n|0)==441){c[h>>2]=e;j=d+40|0;c[j>>2]=(c[j>>2]|0)+1;do{if((c[d+36>>2]|0)==1){if((c[d+24>>2]|0)!=2){n=444;break}a[d+54|0]=1;if(m){break L576}}else{n=444}}while(0);if((n|0)==444){if(m){break}}c[i>>2]=4;return}}while(0);c[i>>2]=3;return}}while(0);if((f|0)!=1){return}c[d+32>>2]=1;return}function kH(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0;if((b|0)!=(c[d+8>>2]|0)){i=d+52|0;j=a[i]&1;k=d+53|0;l=a[k]&1;m=c[b+12>>2]|0;n=b+16+(m<<3)|0;a[i]=0;a[k]=0;o=c[b+20>>2]|0;p=o>>8;if((o&1|0)==0){q=p}else{q=c[(c[f>>2]|0)+p>>2]|0}p=c[b+16>>2]|0;b9[c[(c[p>>2]|0)+20>>2]&31](p,d,e,f+q|0,(o&2|0)!=0?g:2,h);L598:do{if((m|0)>1){o=d+24|0;q=b+8|0;p=d+54|0;r=f;s=b+24|0;do{if((a[p]&1)!=0){break L598}do{if((a[i]&1)==0){if((a[k]&1)==0){break}if((c[q>>2]&1|0)==0){break L598}}else{if((c[o>>2]|0)==1){break L598}if((c[q>>2]&2|0)==0){break L598}}}while(0);a[i]=0;a[k]=0;t=c[s+4>>2]|0;u=t>>8;if((t&1|0)==0){v=u}else{v=c[(c[r>>2]|0)+u>>2]|0}u=c[s>>2]|0;b9[c[(c[u>>2]|0)+20>>2]&31](u,d,e,f+v|0,(t&2|0)!=0?g:2,h);s=s+8|0;}while(s>>>0<n>>>0)}}while(0);a[i]=j;a[k]=l;return}a[d+53|0]=1;if((c[d+4>>2]|0)!=(f|0)){return}a[d+52|0]=1;f=d+16|0;l=c[f>>2]|0;if((l|0)==0){c[f>>2]=e;c[d+24>>2]=g;c[d+36>>2]=1;if(!((c[d+48>>2]|0)==1&(g|0)==1)){return}a[d+54|0]=1;return}if((l|0)!=(e|0)){e=d+36|0;c[e>>2]=(c[e>>2]|0)+1;a[d+54|0]=1;return}e=d+24|0;l=c[e>>2]|0;if((l|0)==2){c[e>>2]=g;w=g}else{w=l}if(!((c[d+48>>2]|0)==1&(w|0)==1)){return}a[d+54|0]=1;return}function kI(b,d,e,f,g,h){b=b|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;var i=0,j=0;if((b|0)!=(c[d+8>>2]|0)){i=c[b+8>>2]|0;b9[c[(c[i>>2]|0)+20>>2]&31](i,d,e,f,g,h);return}a[d+53|0]=1;if((c[d+4>>2]|0)!=(f|0)){return}a[d+52|0]=1;f=d+16|0;h=c[f>>2]|0;if((h|0)==0){c[f>>2]=e;c[d+24>>2]=g;c[d+36>>2]=1;if(!((c[d+48>>2]|0)==1&(g|0)==1)){return}a[d+54|0]=1;return}if((h|0)!=(e|0)){e=d+36|0;c[e>>2]=(c[e>>2]|0)+1;a[d+54|0]=1;return}e=d+24|0;h=c[e>>2]|0;if((h|0)==2){c[e>>2]=g;j=g}else{j=h}if(!((c[d+48>>2]|0)==1&(j|0)==1)){return}a[d+54|0]=1;return}function kJ(a){a=a|0;var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0,P=0,Q=0,R=0,S=0,T=0,U=0,V=0,W=0,X=0,Y=0,Z=0,_=0,$=0,aa=0,ab=0,ac=0,ad=0,ae=0,af=0,ag=0,ah=0,ai=0,aj=0,ak=0,al=0,am=0,an=0,ao=0,ap=0,aq=0,ar=0,as=0,at=0,au=0,av=0,aw=0,ax=0,ay=0,az=0,aA=0,aB=0,aC=0,aD=0,aE=0,aF=0,aG=0;do{if(a>>>0<245){if(a>>>0<11){b=16}else{b=a+11&-8}d=b>>>3;e=c[2460]|0;f=e>>>(d>>>0);if((f&3|0)!=0){g=(f&1^1)+d|0;h=g<<1;i=9880+(h<<2)|0;j=9880+(h+2<<2)|0;h=c[j>>2]|0;k=h+8|0;l=c[k>>2]|0;do{if((i|0)==(l|0)){c[2460]=e&~(1<<g)}else{if(l>>>0<(c[2464]|0)>>>0){bG();return 0}m=l+12|0;if((c[m>>2]|0)==(h|0)){c[m>>2]=i;c[j>>2]=l;break}else{bG();return 0}}}while(0);l=g<<3;c[h+4>>2]=l|3;j=h+(l|4)|0;c[j>>2]=c[j>>2]|1;n=k;return n|0}if(b>>>0<=(c[2462]|0)>>>0){o=b;break}if((f|0)!=0){j=2<<d;l=f<<d&(j|-j);j=(l&-l)-1|0;l=j>>>12&16;i=j>>>(l>>>0);j=i>>>5&8;m=i>>>(j>>>0);i=m>>>2&4;p=m>>>(i>>>0);m=p>>>1&2;q=p>>>(m>>>0);p=q>>>1&1;r=(j|l|i|m|p)+(q>>>(p>>>0))|0;p=r<<1;q=9880+(p<<2)|0;m=9880+(p+2<<2)|0;p=c[m>>2]|0;i=p+8|0;l=c[i>>2]|0;do{if((q|0)==(l|0)){c[2460]=e&~(1<<r)}else{if(l>>>0<(c[2464]|0)>>>0){bG();return 0}j=l+12|0;if((c[j>>2]|0)==(p|0)){c[j>>2]=q;c[m>>2]=l;break}else{bG();return 0}}}while(0);l=r<<3;m=l-b|0;c[p+4>>2]=b|3;q=p;e=q+b|0;c[q+(b|4)>>2]=m|1;c[q+l>>2]=m;l=c[2462]|0;if((l|0)!=0){q=c[2465]|0;d=l>>>3;l=d<<1;f=9880+(l<<2)|0;k=c[2460]|0;h=1<<d;do{if((k&h|0)==0){c[2460]=k|h;s=f;t=9880+(l+2<<2)|0}else{d=9880+(l+2<<2)|0;g=c[d>>2]|0;if(g>>>0>=(c[2464]|0)>>>0){s=g;t=d;break}bG();return 0}}while(0);c[t>>2]=q;c[s+12>>2]=q;c[q+8>>2]=s;c[q+12>>2]=f}c[2462]=m;c[2465]=e;n=i;return n|0}l=c[2461]|0;if((l|0)==0){o=b;break}h=(l&-l)-1|0;l=h>>>12&16;k=h>>>(l>>>0);h=k>>>5&8;p=k>>>(h>>>0);k=p>>>2&4;r=p>>>(k>>>0);p=r>>>1&2;d=r>>>(p>>>0);r=d>>>1&1;g=c[10144+((h|l|k|p|r)+(d>>>(r>>>0))<<2)>>2]|0;r=g;d=g;p=(c[g+4>>2]&-8)-b|0;while(1){g=c[r+16>>2]|0;if((g|0)==0){k=c[r+20>>2]|0;if((k|0)==0){break}else{u=k}}else{u=g}g=(c[u+4>>2]&-8)-b|0;k=g>>>0<p>>>0;r=u;d=k?u:d;p=k?g:p}r=d;i=c[2464]|0;if(r>>>0<i>>>0){bG();return 0}e=r+b|0;m=e;if(r>>>0>=e>>>0){bG();return 0}e=c[d+24>>2]|0;f=c[d+12>>2]|0;do{if((f|0)==(d|0)){q=d+20|0;g=c[q>>2]|0;if((g|0)==0){k=d+16|0;l=c[k>>2]|0;if((l|0)==0){v=0;break}else{w=l;x=k}}else{w=g;x=q}while(1){q=w+20|0;g=c[q>>2]|0;if((g|0)!=0){w=g;x=q;continue}q=w+16|0;g=c[q>>2]|0;if((g|0)==0){break}else{w=g;x=q}}if(x>>>0<i>>>0){bG();return 0}else{c[x>>2]=0;v=w;break}}else{q=c[d+8>>2]|0;if(q>>>0<i>>>0){bG();return 0}g=q+12|0;if((c[g>>2]|0)!=(d|0)){bG();return 0}k=f+8|0;if((c[k>>2]|0)==(d|0)){c[g>>2]=f;c[k>>2]=q;v=f;break}else{bG();return 0}}}while(0);L740:do{if((e|0)!=0){f=d+28|0;i=10144+(c[f>>2]<<2)|0;do{if((d|0)==(c[i>>2]|0)){c[i>>2]=v;if((v|0)!=0){break}c[2461]=c[2461]&~(1<<c[f>>2]);break L740}else{if(e>>>0<(c[2464]|0)>>>0){bG();return 0}q=e+16|0;if((c[q>>2]|0)==(d|0)){c[q>>2]=v}else{c[e+20>>2]=v}if((v|0)==0){break L740}}}while(0);if(v>>>0<(c[2464]|0)>>>0){bG();return 0}c[v+24>>2]=e;f=c[d+16>>2]|0;do{if((f|0)!=0){if(f>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[v+16>>2]=f;c[f+24>>2]=v;break}}}while(0);f=c[d+20>>2]|0;if((f|0)==0){break}if(f>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[v+20>>2]=f;c[f+24>>2]=v;break}}}while(0);if(p>>>0<16){e=p+b|0;c[d+4>>2]=e|3;f=r+(e+4)|0;c[f>>2]=c[f>>2]|1}else{c[d+4>>2]=b|3;c[r+(b|4)>>2]=p|1;c[r+(p+b)>>2]=p;f=c[2462]|0;if((f|0)!=0){e=c[2465]|0;i=f>>>3;f=i<<1;q=9880+(f<<2)|0;k=c[2460]|0;g=1<<i;do{if((k&g|0)==0){c[2460]=k|g;y=q;z=9880+(f+2<<2)|0}else{i=9880+(f+2<<2)|0;l=c[i>>2]|0;if(l>>>0>=(c[2464]|0)>>>0){y=l;z=i;break}bG();return 0}}while(0);c[z>>2]=e;c[y+12>>2]=e;c[e+8>>2]=y;c[e+12>>2]=q}c[2462]=p;c[2465]=m}f=d+8|0;if((f|0)==0){o=b;break}else{n=f}return n|0}else{if(a>>>0>4294967231){o=-1;break}f=a+11|0;g=f&-8;k=c[2461]|0;if((k|0)==0){o=g;break}r=-g|0;i=f>>>8;do{if((i|0)==0){A=0}else{if(g>>>0>16777215){A=31;break}f=(i+1048320|0)>>>16&8;l=i<<f;h=(l+520192|0)>>>16&4;j=l<<h;l=(j+245760|0)>>>16&2;B=14-(h|f|l)+(j<<l>>>15)|0;A=g>>>((B+7|0)>>>0)&1|B<<1}}while(0);i=c[10144+(A<<2)>>2]|0;L788:do{if((i|0)==0){C=0;D=r;E=0}else{if((A|0)==31){F=0}else{F=25-(A>>>1)|0}d=0;m=r;p=i;q=g<<F;e=0;while(1){B=c[p+4>>2]&-8;l=B-g|0;if(l>>>0<m>>>0){if((B|0)==(g|0)){C=p;D=l;E=p;break L788}else{G=p;H=l}}else{G=d;H=m}l=c[p+20>>2]|0;B=c[p+16+(q>>>31<<2)>>2]|0;j=(l|0)==0|(l|0)==(B|0)?e:l;if((B|0)==0){C=G;D=H;E=j;break}else{d=G;m=H;p=B;q=q<<1;e=j}}}}while(0);if((E|0)==0&(C|0)==0){i=2<<A;r=k&(i|-i);if((r|0)==0){o=g;break}i=(r&-r)-1|0;r=i>>>12&16;e=i>>>(r>>>0);i=e>>>5&8;q=e>>>(i>>>0);e=q>>>2&4;p=q>>>(e>>>0);q=p>>>1&2;m=p>>>(q>>>0);p=m>>>1&1;I=c[10144+((i|r|e|q|p)+(m>>>(p>>>0))<<2)>>2]|0}else{I=E}if((I|0)==0){J=D;K=C}else{p=I;m=D;q=C;while(1){e=(c[p+4>>2]&-8)-g|0;r=e>>>0<m>>>0;i=r?e:m;e=r?p:q;r=c[p+16>>2]|0;if((r|0)!=0){p=r;m=i;q=e;continue}r=c[p+20>>2]|0;if((r|0)==0){J=i;K=e;break}else{p=r;m=i;q=e}}}if((K|0)==0){o=g;break}if(J>>>0>=((c[2462]|0)-g|0)>>>0){o=g;break}q=K;m=c[2464]|0;if(q>>>0<m>>>0){bG();return 0}p=q+g|0;k=p;if(q>>>0>=p>>>0){bG();return 0}e=c[K+24>>2]|0;i=c[K+12>>2]|0;do{if((i|0)==(K|0)){r=K+20|0;d=c[r>>2]|0;if((d|0)==0){j=K+16|0;B=c[j>>2]|0;if((B|0)==0){L=0;break}else{M=B;N=j}}else{M=d;N=r}while(1){r=M+20|0;d=c[r>>2]|0;if((d|0)!=0){M=d;N=r;continue}r=M+16|0;d=c[r>>2]|0;if((d|0)==0){break}else{M=d;N=r}}if(N>>>0<m>>>0){bG();return 0}else{c[N>>2]=0;L=M;break}}else{r=c[K+8>>2]|0;if(r>>>0<m>>>0){bG();return 0}d=r+12|0;if((c[d>>2]|0)!=(K|0)){bG();return 0}j=i+8|0;if((c[j>>2]|0)==(K|0)){c[d>>2]=i;c[j>>2]=r;L=i;break}else{bG();return 0}}}while(0);L838:do{if((e|0)!=0){i=K+28|0;m=10144+(c[i>>2]<<2)|0;do{if((K|0)==(c[m>>2]|0)){c[m>>2]=L;if((L|0)!=0){break}c[2461]=c[2461]&~(1<<c[i>>2]);break L838}else{if(e>>>0<(c[2464]|0)>>>0){bG();return 0}r=e+16|0;if((c[r>>2]|0)==(K|0)){c[r>>2]=L}else{c[e+20>>2]=L}if((L|0)==0){break L838}}}while(0);if(L>>>0<(c[2464]|0)>>>0){bG();return 0}c[L+24>>2]=e;i=c[K+16>>2]|0;do{if((i|0)!=0){if(i>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[L+16>>2]=i;c[i+24>>2]=L;break}}}while(0);i=c[K+20>>2]|0;if((i|0)==0){break}if(i>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[L+20>>2]=i;c[i+24>>2]=L;break}}}while(0);do{if(J>>>0<16){e=J+g|0;c[K+4>>2]=e|3;i=q+(e+4)|0;c[i>>2]=c[i>>2]|1}else{c[K+4>>2]=g|3;c[q+(g|4)>>2]=J|1;c[q+(J+g)>>2]=J;i=J>>>3;if(J>>>0<256){e=i<<1;m=9880+(e<<2)|0;r=c[2460]|0;j=1<<i;do{if((r&j|0)==0){c[2460]=r|j;O=m;P=9880+(e+2<<2)|0}else{i=9880+(e+2<<2)|0;d=c[i>>2]|0;if(d>>>0>=(c[2464]|0)>>>0){O=d;P=i;break}bG();return 0}}while(0);c[P>>2]=k;c[O+12>>2]=k;c[q+(g+8)>>2]=O;c[q+(g+12)>>2]=m;break}e=p;j=J>>>8;do{if((j|0)==0){Q=0}else{if(J>>>0>16777215){Q=31;break}r=(j+1048320|0)>>>16&8;i=j<<r;d=(i+520192|0)>>>16&4;B=i<<d;i=(B+245760|0)>>>16&2;l=14-(d|r|i)+(B<<i>>>15)|0;Q=J>>>((l+7|0)>>>0)&1|l<<1}}while(0);j=10144+(Q<<2)|0;c[q+(g+28)>>2]=Q;c[q+(g+20)>>2]=0;c[q+(g+16)>>2]=0;m=c[2461]|0;l=1<<Q;if((m&l|0)==0){c[2461]=m|l;c[j>>2]=e;c[q+(g+24)>>2]=j;c[q+(g+12)>>2]=e;c[q+(g+8)>>2]=e;break}if((Q|0)==31){R=0}else{R=25-(Q>>>1)|0}l=J<<R;m=c[j>>2]|0;while(1){if((c[m+4>>2]&-8|0)==(J|0)){break}S=m+16+(l>>>31<<2)|0;j=c[S>>2]|0;if((j|0)==0){T=661;break}else{l=l<<1;m=j}}if((T|0)==661){if(S>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[S>>2]=e;c[q+(g+24)>>2]=m;c[q+(g+12)>>2]=e;c[q+(g+8)>>2]=e;break}}l=m+8|0;j=c[l>>2]|0;i=c[2464]|0;if(m>>>0<i>>>0){bG();return 0}if(j>>>0<i>>>0){bG();return 0}else{c[j+12>>2]=e;c[l>>2]=e;c[q+(g+8)>>2]=j;c[q+(g+12)>>2]=m;c[q+(g+24)>>2]=0;break}}}while(0);q=K+8|0;if((q|0)==0){o=g;break}else{n=q}return n|0}}while(0);K=c[2462]|0;if(o>>>0<=K>>>0){S=K-o|0;J=c[2465]|0;if(S>>>0>15){R=J;c[2465]=R+o;c[2462]=S;c[R+(o+4)>>2]=S|1;c[R+K>>2]=S;c[J+4>>2]=o|3}else{c[2462]=0;c[2465]=0;c[J+4>>2]=K|3;S=J+(K+4)|0;c[S>>2]=c[S>>2]|1}n=J+8|0;return n|0}J=c[2463]|0;if(o>>>0<J>>>0){S=J-o|0;c[2463]=S;J=c[2466]|0;K=J;c[2466]=K+o;c[K+(o+4)>>2]=S|1;c[J+4>>2]=o|3;n=J+8|0;return n|0}do{if((c[2450]|0)==0){J=bE(8)|0;if((J-1&J|0)==0){c[2452]=J;c[2451]=J;c[2453]=-1;c[2454]=-1;c[2455]=0;c[2571]=0;c[2450]=(bV(0)|0)&-16^1431655768;break}else{bG();return 0}}}while(0);J=o+48|0;S=c[2452]|0;K=o+47|0;R=S+K|0;Q=-S|0;S=R&Q;if(S>>>0<=o>>>0){n=0;return n|0}O=c[2570]|0;do{if((O|0)!=0){P=c[2568]|0;L=P+S|0;if(L>>>0<=P>>>0|L>>>0>O>>>0){n=0}else{break}return n|0}}while(0);L930:do{if((c[2571]&4|0)==0){O=c[2466]|0;L932:do{if((O|0)==0){T=691}else{L=O;P=10288;while(1){U=P|0;M=c[U>>2]|0;if(M>>>0<=L>>>0){V=P+4|0;if((M+(c[V>>2]|0)|0)>>>0>L>>>0){break}}M=c[P+8>>2]|0;if((M|0)==0){T=691;break L932}else{P=M}}if((P|0)==0){T=691;break}L=R-(c[2463]|0)&Q;if(L>>>0>=2147483647){W=0;break}m=bv(L|0)|0;e=(m|0)==((c[U>>2]|0)+(c[V>>2]|0)|0);X=e?m:-1;Y=e?L:0;Z=m;_=L;T=700}}while(0);do{if((T|0)==691){O=bv(0)|0;if((O|0)==-1){W=0;break}g=O;L=c[2451]|0;m=L-1|0;if((m&g|0)==0){$=S}else{$=S-g+(m+g&-L)|0}L=c[2568]|0;g=L+$|0;if(!($>>>0>o>>>0&$>>>0<2147483647)){W=0;break}m=c[2570]|0;if((m|0)!=0){if(g>>>0<=L>>>0|g>>>0>m>>>0){W=0;break}}m=bv($|0)|0;g=(m|0)==(O|0);X=g?O:-1;Y=g?$:0;Z=m;_=$;T=700}}while(0);L952:do{if((T|0)==700){m=-_|0;if((X|0)!=-1){aa=Y;ab=X;T=711;break L930}do{if((Z|0)!=-1&_>>>0<2147483647&_>>>0<J>>>0){g=c[2452]|0;O=K-_+g&-g;if(O>>>0>=2147483647){ac=_;break}if((bv(O|0)|0)==-1){bv(m|0)|0;W=Y;break L952}else{ac=O+_|0;break}}else{ac=_}}while(0);if((Z|0)==-1){W=Y}else{aa=ac;ab=Z;T=711;break L930}}}while(0);c[2571]=c[2571]|4;ad=W;T=708}else{ad=0;T=708}}while(0);do{if((T|0)==708){if(S>>>0>=2147483647){break}W=bv(S|0)|0;Z=bv(0)|0;if(!((Z|0)!=-1&(W|0)!=-1&W>>>0<Z>>>0)){break}ac=Z-W|0;Z=ac>>>0>(o+40|0)>>>0;Y=Z?W:-1;if((Y|0)!=-1){aa=Z?ac:ad;ab=Y;T=711}}}while(0);do{if((T|0)==711){ad=(c[2568]|0)+aa|0;c[2568]=ad;if(ad>>>0>(c[2569]|0)>>>0){c[2569]=ad}ad=c[2466]|0;L972:do{if((ad|0)==0){S=c[2464]|0;if((S|0)==0|ab>>>0<S>>>0){c[2464]=ab}c[2572]=ab;c[2573]=aa;c[2575]=0;c[2469]=c[2450];c[2468]=-1;S=0;do{Y=S<<1;ac=9880+(Y<<2)|0;c[9880+(Y+3<<2)>>2]=ac;c[9880+(Y+2<<2)>>2]=ac;S=S+1|0;}while(S>>>0<32);S=ab+8|0;if((S&7|0)==0){ae=0}else{ae=-S&7}S=aa-40-ae|0;c[2466]=ab+ae;c[2463]=S;c[ab+(ae+4)>>2]=S|1;c[ab+(aa-36)>>2]=40;c[2467]=c[2454]}else{S=10288;while(1){af=c[S>>2]|0;ag=S+4|0;ah=c[ag>>2]|0;if((ab|0)==(af+ah|0)){T=723;break}ac=c[S+8>>2]|0;if((ac|0)==0){break}else{S=ac}}do{if((T|0)==723){if((c[S+12>>2]&8|0)!=0){break}ac=ad;if(!(ac>>>0>=af>>>0&ac>>>0<ab>>>0)){break}c[ag>>2]=ah+aa;ac=c[2466]|0;Y=(c[2463]|0)+aa|0;Z=ac;W=ac+8|0;if((W&7|0)==0){ai=0}else{ai=-W&7}W=Y-ai|0;c[2466]=Z+ai;c[2463]=W;c[Z+(ai+4)>>2]=W|1;c[Z+(Y+4)>>2]=40;c[2467]=c[2454];break L972}}while(0);if(ab>>>0<(c[2464]|0)>>>0){c[2464]=ab}S=ab+aa|0;Y=10288;while(1){aj=Y|0;if((c[aj>>2]|0)==(S|0)){T=733;break}Z=c[Y+8>>2]|0;if((Z|0)==0){break}else{Y=Z}}do{if((T|0)==733){if((c[Y+12>>2]&8|0)!=0){break}c[aj>>2]=ab;S=Y+4|0;c[S>>2]=(c[S>>2]|0)+aa;S=ab+8|0;if((S&7|0)==0){ak=0}else{ak=-S&7}S=ab+(aa+8)|0;if((S&7|0)==0){al=0}else{al=-S&7}S=ab+(al+aa)|0;Z=S;W=ak+o|0;ac=ab+W|0;_=ac;K=S-(ab+ak)-o|0;c[ab+(ak+4)>>2]=o|3;do{if((Z|0)==(c[2466]|0)){J=(c[2463]|0)+K|0;c[2463]=J;c[2466]=_;c[ab+(W+4)>>2]=J|1}else{if((Z|0)==(c[2465]|0)){J=(c[2462]|0)+K|0;c[2462]=J;c[2465]=_;c[ab+(W+4)>>2]=J|1;c[ab+(J+W)>>2]=J;break}J=aa+4|0;X=c[ab+(J+al)>>2]|0;if((X&3|0)==1){$=X&-8;V=X>>>3;L1017:do{if(X>>>0<256){U=c[ab+((al|8)+aa)>>2]|0;Q=c[ab+(aa+12+al)>>2]|0;R=9880+(V<<1<<2)|0;do{if((U|0)!=(R|0)){if(U>>>0<(c[2464]|0)>>>0){bG();return 0}if((c[U+12>>2]|0)==(Z|0)){break}bG();return 0}}while(0);if((Q|0)==(U|0)){c[2460]=c[2460]&~(1<<V);break}do{if((Q|0)==(R|0)){am=Q+8|0}else{if(Q>>>0<(c[2464]|0)>>>0){bG();return 0}m=Q+8|0;if((c[m>>2]|0)==(Z|0)){am=m;break}bG();return 0}}while(0);c[U+12>>2]=Q;c[am>>2]=U}else{R=S;m=c[ab+((al|24)+aa)>>2]|0;P=c[ab+(aa+12+al)>>2]|0;do{if((P|0)==(R|0)){O=al|16;g=ab+(J+O)|0;L=c[g>>2]|0;if((L|0)==0){e=ab+(O+aa)|0;O=c[e>>2]|0;if((O|0)==0){an=0;break}else{ao=O;ap=e}}else{ao=L;ap=g}while(1){g=ao+20|0;L=c[g>>2]|0;if((L|0)!=0){ao=L;ap=g;continue}g=ao+16|0;L=c[g>>2]|0;if((L|0)==0){break}else{ao=L;ap=g}}if(ap>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[ap>>2]=0;an=ao;break}}else{g=c[ab+((al|8)+aa)>>2]|0;if(g>>>0<(c[2464]|0)>>>0){bG();return 0}L=g+12|0;if((c[L>>2]|0)!=(R|0)){bG();return 0}e=P+8|0;if((c[e>>2]|0)==(R|0)){c[L>>2]=P;c[e>>2]=g;an=P;break}else{bG();return 0}}}while(0);if((m|0)==0){break}P=ab+(aa+28+al)|0;U=10144+(c[P>>2]<<2)|0;do{if((R|0)==(c[U>>2]|0)){c[U>>2]=an;if((an|0)!=0){break}c[2461]=c[2461]&~(1<<c[P>>2]);break L1017}else{if(m>>>0<(c[2464]|0)>>>0){bG();return 0}Q=m+16|0;if((c[Q>>2]|0)==(R|0)){c[Q>>2]=an}else{c[m+20>>2]=an}if((an|0)==0){break L1017}}}while(0);if(an>>>0<(c[2464]|0)>>>0){bG();return 0}c[an+24>>2]=m;R=al|16;P=c[ab+(R+aa)>>2]|0;do{if((P|0)!=0){if(P>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[an+16>>2]=P;c[P+24>>2]=an;break}}}while(0);P=c[ab+(J+R)>>2]|0;if((P|0)==0){break}if(P>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[an+20>>2]=P;c[P+24>>2]=an;break}}}while(0);aq=ab+(($|al)+aa)|0;ar=$+K|0}else{aq=Z;ar=K}J=aq+4|0;c[J>>2]=c[J>>2]&-2;c[ab+(W+4)>>2]=ar|1;c[ab+(ar+W)>>2]=ar;J=ar>>>3;if(ar>>>0<256){V=J<<1;X=9880+(V<<2)|0;P=c[2460]|0;m=1<<J;do{if((P&m|0)==0){c[2460]=P|m;as=X;at=9880+(V+2<<2)|0}else{J=9880+(V+2<<2)|0;U=c[J>>2]|0;if(U>>>0>=(c[2464]|0)>>>0){as=U;at=J;break}bG();return 0}}while(0);c[at>>2]=_;c[as+12>>2]=_;c[ab+(W+8)>>2]=as;c[ab+(W+12)>>2]=X;break}V=ac;m=ar>>>8;do{if((m|0)==0){au=0}else{if(ar>>>0>16777215){au=31;break}P=(m+1048320|0)>>>16&8;$=m<<P;J=($+520192|0)>>>16&4;U=$<<J;$=(U+245760|0)>>>16&2;Q=14-(J|P|$)+(U<<$>>>15)|0;au=ar>>>((Q+7|0)>>>0)&1|Q<<1}}while(0);m=10144+(au<<2)|0;c[ab+(W+28)>>2]=au;c[ab+(W+20)>>2]=0;c[ab+(W+16)>>2]=0;X=c[2461]|0;Q=1<<au;if((X&Q|0)==0){c[2461]=X|Q;c[m>>2]=V;c[ab+(W+24)>>2]=m;c[ab+(W+12)>>2]=V;c[ab+(W+8)>>2]=V;break}if((au|0)==31){av=0}else{av=25-(au>>>1)|0}Q=ar<<av;X=c[m>>2]|0;while(1){if((c[X+4>>2]&-8|0)==(ar|0)){break}aw=X+16+(Q>>>31<<2)|0;m=c[aw>>2]|0;if((m|0)==0){T=806;break}else{Q=Q<<1;X=m}}if((T|0)==806){if(aw>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[aw>>2]=V;c[ab+(W+24)>>2]=X;c[ab+(W+12)>>2]=V;c[ab+(W+8)>>2]=V;break}}Q=X+8|0;m=c[Q>>2]|0;$=c[2464]|0;if(X>>>0<$>>>0){bG();return 0}if(m>>>0<$>>>0){bG();return 0}else{c[m+12>>2]=V;c[Q>>2]=V;c[ab+(W+8)>>2]=m;c[ab+(W+12)>>2]=X;c[ab+(W+24)>>2]=0;break}}}while(0);n=ab+(ak|8)|0;return n|0}}while(0);Y=ad;W=10288;while(1){ax=c[W>>2]|0;if(ax>>>0<=Y>>>0){ay=c[W+4>>2]|0;az=ax+ay|0;if(az>>>0>Y>>>0){break}}W=c[W+8>>2]|0}W=ax+(ay-39)|0;if((W&7|0)==0){aA=0}else{aA=-W&7}W=ax+(ay-47+aA)|0;ac=W>>>0<(ad+16|0)>>>0?Y:W;W=ac+8|0;_=ab+8|0;if((_&7|0)==0){aB=0}else{aB=-_&7}_=aa-40-aB|0;c[2466]=ab+aB;c[2463]=_;c[ab+(aB+4)>>2]=_|1;c[ab+(aa-36)>>2]=40;c[2467]=c[2454];c[ac+4>>2]=27;c[W>>2]=c[2572];c[W+4>>2]=c[10292>>2];c[W+8>>2]=c[10296>>2];c[W+12>>2]=c[10300>>2];c[2572]=ab;c[2573]=aa;c[2575]=0;c[2574]=W;W=ac+28|0;c[W>>2]=7;if((ac+32|0)>>>0<az>>>0){_=W;while(1){W=_+4|0;c[W>>2]=7;if((_+8|0)>>>0<az>>>0){_=W}else{break}}}if((ac|0)==(Y|0)){break}_=ac-ad|0;W=Y+(_+4)|0;c[W>>2]=c[W>>2]&-2;c[ad+4>>2]=_|1;c[Y+_>>2]=_;W=_>>>3;if(_>>>0<256){K=W<<1;Z=9880+(K<<2)|0;S=c[2460]|0;m=1<<W;do{if((S&m|0)==0){c[2460]=S|m;aC=Z;aD=9880+(K+2<<2)|0}else{W=9880+(K+2<<2)|0;Q=c[W>>2]|0;if(Q>>>0>=(c[2464]|0)>>>0){aC=Q;aD=W;break}bG();return 0}}while(0);c[aD>>2]=ad;c[aC+12>>2]=ad;c[ad+8>>2]=aC;c[ad+12>>2]=Z;break}K=ad;m=_>>>8;do{if((m|0)==0){aE=0}else{if(_>>>0>16777215){aE=31;break}S=(m+1048320|0)>>>16&8;Y=m<<S;ac=(Y+520192|0)>>>16&4;W=Y<<ac;Y=(W+245760|0)>>>16&2;Q=14-(ac|S|Y)+(W<<Y>>>15)|0;aE=_>>>((Q+7|0)>>>0)&1|Q<<1}}while(0);m=10144+(aE<<2)|0;c[ad+28>>2]=aE;c[ad+20>>2]=0;c[ad+16>>2]=0;Z=c[2461]|0;Q=1<<aE;if((Z&Q|0)==0){c[2461]=Z|Q;c[m>>2]=K;c[ad+24>>2]=m;c[ad+12>>2]=ad;c[ad+8>>2]=ad;break}if((aE|0)==31){aF=0}else{aF=25-(aE>>>1)|0}Q=_<<aF;Z=c[m>>2]|0;while(1){if((c[Z+4>>2]&-8|0)==(_|0)){break}aG=Z+16+(Q>>>31<<2)|0;m=c[aG>>2]|0;if((m|0)==0){T=841;break}else{Q=Q<<1;Z=m}}if((T|0)==841){if(aG>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[aG>>2]=K;c[ad+24>>2]=Z;c[ad+12>>2]=ad;c[ad+8>>2]=ad;break}}Q=Z+8|0;_=c[Q>>2]|0;m=c[2464]|0;if(Z>>>0<m>>>0){bG();return 0}if(_>>>0<m>>>0){bG();return 0}else{c[_+12>>2]=K;c[Q>>2]=K;c[ad+8>>2]=_;c[ad+12>>2]=Z;c[ad+24>>2]=0;break}}}while(0);ad=c[2463]|0;if(ad>>>0<=o>>>0){break}_=ad-o|0;c[2463]=_;ad=c[2466]|0;Q=ad;c[2466]=Q+o;c[Q+(o+4)>>2]=_|1;c[ad+4>>2]=o|3;n=ad+8|0;return n|0}}while(0);c[(bw()|0)>>2]=12;n=0;return n|0}function kK(a){a=a|0;var b=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0,O=0;if((a|0)==0){return}b=a-8|0;d=b;e=c[2464]|0;if(b>>>0<e>>>0){bG()}f=c[a-4>>2]|0;g=f&3;if((g|0)==1){bG()}h=f&-8;i=a+(h-8)|0;j=i;L1189:do{if((f&1|0)==0){k=c[b>>2]|0;if((g|0)==0){return}l=-8-k|0;m=a+l|0;n=m;o=k+h|0;if(m>>>0<e>>>0){bG()}if((n|0)==(c[2465]|0)){p=a+(h-4)|0;if((c[p>>2]&3|0)!=3){q=n;r=o;break}c[2462]=o;c[p>>2]=c[p>>2]&-2;c[a+(l+4)>>2]=o|1;c[i>>2]=o;return}p=k>>>3;if(k>>>0<256){k=c[a+(l+8)>>2]|0;s=c[a+(l+12)>>2]|0;t=9880+(p<<1<<2)|0;do{if((k|0)!=(t|0)){if(k>>>0<e>>>0){bG()}if((c[k+12>>2]|0)==(n|0)){break}bG()}}while(0);if((s|0)==(k|0)){c[2460]=c[2460]&~(1<<p);q=n;r=o;break}do{if((s|0)==(t|0)){u=s+8|0}else{if(s>>>0<e>>>0){bG()}v=s+8|0;if((c[v>>2]|0)==(n|0)){u=v;break}bG()}}while(0);c[k+12>>2]=s;c[u>>2]=k;q=n;r=o;break}t=m;p=c[a+(l+24)>>2]|0;v=c[a+(l+12)>>2]|0;do{if((v|0)==(t|0)){w=a+(l+20)|0;x=c[w>>2]|0;if((x|0)==0){y=a+(l+16)|0;z=c[y>>2]|0;if((z|0)==0){A=0;break}else{B=z;C=y}}else{B=x;C=w}while(1){w=B+20|0;x=c[w>>2]|0;if((x|0)!=0){B=x;C=w;continue}w=B+16|0;x=c[w>>2]|0;if((x|0)==0){break}else{B=x;C=w}}if(C>>>0<e>>>0){bG()}else{c[C>>2]=0;A=B;break}}else{w=c[a+(l+8)>>2]|0;if(w>>>0<e>>>0){bG()}x=w+12|0;if((c[x>>2]|0)!=(t|0)){bG()}y=v+8|0;if((c[y>>2]|0)==(t|0)){c[x>>2]=v;c[y>>2]=w;A=v;break}else{bG()}}}while(0);if((p|0)==0){q=n;r=o;break}v=a+(l+28)|0;m=10144+(c[v>>2]<<2)|0;do{if((t|0)==(c[m>>2]|0)){c[m>>2]=A;if((A|0)!=0){break}c[2461]=c[2461]&~(1<<c[v>>2]);q=n;r=o;break L1189}else{if(p>>>0<(c[2464]|0)>>>0){bG()}k=p+16|0;if((c[k>>2]|0)==(t|0)){c[k>>2]=A}else{c[p+20>>2]=A}if((A|0)==0){q=n;r=o;break L1189}}}while(0);if(A>>>0<(c[2464]|0)>>>0){bG()}c[A+24>>2]=p;t=c[a+(l+16)>>2]|0;do{if((t|0)!=0){if(t>>>0<(c[2464]|0)>>>0){bG()}else{c[A+16>>2]=t;c[t+24>>2]=A;break}}}while(0);t=c[a+(l+20)>>2]|0;if((t|0)==0){q=n;r=o;break}if(t>>>0<(c[2464]|0)>>>0){bG()}else{c[A+20>>2]=t;c[t+24>>2]=A;q=n;r=o;break}}else{q=d;r=h}}while(0);d=q;if(d>>>0>=i>>>0){bG()}A=a+(h-4)|0;e=c[A>>2]|0;if((e&1|0)==0){bG()}do{if((e&2|0)==0){if((j|0)==(c[2466]|0)){B=(c[2463]|0)+r|0;c[2463]=B;c[2466]=q;c[q+4>>2]=B|1;if((q|0)!=(c[2465]|0)){return}c[2465]=0;c[2462]=0;return}if((j|0)==(c[2465]|0)){B=(c[2462]|0)+r|0;c[2462]=B;c[2465]=q;c[q+4>>2]=B|1;c[d+B>>2]=B;return}B=(e&-8)+r|0;C=e>>>3;L1292:do{if(e>>>0<256){u=c[a+h>>2]|0;g=c[a+(h|4)>>2]|0;b=9880+(C<<1<<2)|0;do{if((u|0)!=(b|0)){if(u>>>0<(c[2464]|0)>>>0){bG()}if((c[u+12>>2]|0)==(j|0)){break}bG()}}while(0);if((g|0)==(u|0)){c[2460]=c[2460]&~(1<<C);break}do{if((g|0)==(b|0)){D=g+8|0}else{if(g>>>0<(c[2464]|0)>>>0){bG()}f=g+8|0;if((c[f>>2]|0)==(j|0)){D=f;break}bG()}}while(0);c[u+12>>2]=g;c[D>>2]=u}else{b=i;f=c[a+(h+16)>>2]|0;t=c[a+(h|4)>>2]|0;do{if((t|0)==(b|0)){p=a+(h+12)|0;v=c[p>>2]|0;if((v|0)==0){m=a+(h+8)|0;k=c[m>>2]|0;if((k|0)==0){E=0;break}else{F=k;G=m}}else{F=v;G=p}while(1){p=F+20|0;v=c[p>>2]|0;if((v|0)!=0){F=v;G=p;continue}p=F+16|0;v=c[p>>2]|0;if((v|0)==0){break}else{F=v;G=p}}if(G>>>0<(c[2464]|0)>>>0){bG()}else{c[G>>2]=0;E=F;break}}else{p=c[a+h>>2]|0;if(p>>>0<(c[2464]|0)>>>0){bG()}v=p+12|0;if((c[v>>2]|0)!=(b|0)){bG()}m=t+8|0;if((c[m>>2]|0)==(b|0)){c[v>>2]=t;c[m>>2]=p;E=t;break}else{bG()}}}while(0);if((f|0)==0){break}t=a+(h+20)|0;u=10144+(c[t>>2]<<2)|0;do{if((b|0)==(c[u>>2]|0)){c[u>>2]=E;if((E|0)!=0){break}c[2461]=c[2461]&~(1<<c[t>>2]);break L1292}else{if(f>>>0<(c[2464]|0)>>>0){bG()}g=f+16|0;if((c[g>>2]|0)==(b|0)){c[g>>2]=E}else{c[f+20>>2]=E}if((E|0)==0){break L1292}}}while(0);if(E>>>0<(c[2464]|0)>>>0){bG()}c[E+24>>2]=f;b=c[a+(h+8)>>2]|0;do{if((b|0)!=0){if(b>>>0<(c[2464]|0)>>>0){bG()}else{c[E+16>>2]=b;c[b+24>>2]=E;break}}}while(0);b=c[a+(h+12)>>2]|0;if((b|0)==0){break}if(b>>>0<(c[2464]|0)>>>0){bG()}else{c[E+20>>2]=b;c[b+24>>2]=E;break}}}while(0);c[q+4>>2]=B|1;c[d+B>>2]=B;if((q|0)!=(c[2465]|0)){H=B;break}c[2462]=B;return}else{c[A>>2]=e&-2;c[q+4>>2]=r|1;c[d+r>>2]=r;H=r}}while(0);r=H>>>3;if(H>>>0<256){d=r<<1;e=9880+(d<<2)|0;A=c[2460]|0;E=1<<r;do{if((A&E|0)==0){c[2460]=A|E;I=e;J=9880+(d+2<<2)|0}else{r=9880+(d+2<<2)|0;h=c[r>>2]|0;if(h>>>0>=(c[2464]|0)>>>0){I=h;J=r;break}bG()}}while(0);c[J>>2]=q;c[I+12>>2]=q;c[q+8>>2]=I;c[q+12>>2]=e;return}e=q;I=H>>>8;do{if((I|0)==0){K=0}else{if(H>>>0>16777215){K=31;break}J=(I+1048320|0)>>>16&8;d=I<<J;E=(d+520192|0)>>>16&4;A=d<<E;d=(A+245760|0)>>>16&2;r=14-(E|J|d)+(A<<d>>>15)|0;K=H>>>((r+7|0)>>>0)&1|r<<1}}while(0);I=10144+(K<<2)|0;c[q+28>>2]=K;c[q+20>>2]=0;c[q+16>>2]=0;r=c[2461]|0;d=1<<K;do{if((r&d|0)==0){c[2461]=r|d;c[I>>2]=e;c[q+24>>2]=I;c[q+12>>2]=q;c[q+8>>2]=q}else{if((K|0)==31){L=0}else{L=25-(K>>>1)|0}A=H<<L;J=c[I>>2]|0;while(1){if((c[J+4>>2]&-8|0)==(H|0)){break}M=J+16+(A>>>31<<2)|0;E=c[M>>2]|0;if((E|0)==0){N=1018;break}else{A=A<<1;J=E}}if((N|0)==1018){if(M>>>0<(c[2464]|0)>>>0){bG()}else{c[M>>2]=e;c[q+24>>2]=J;c[q+12>>2]=q;c[q+8>>2]=q;break}}A=J+8|0;B=c[A>>2]|0;E=c[2464]|0;if(J>>>0<E>>>0){bG()}if(B>>>0<E>>>0){bG()}else{c[B+12>>2]=e;c[A>>2]=e;c[q+8>>2]=B;c[q+12>>2]=J;c[q+24>>2]=0;break}}}while(0);q=(c[2468]|0)-1|0;c[2468]=q;if((q|0)==0){O=10296}else{return}while(1){q=c[O>>2]|0;if((q|0)==0){break}else{O=q+8|0}}c[2468]=-1;return}function kL(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0;if((a|0)==0){d=kJ(b)|0;return d|0}if(b>>>0>4294967231){c[(bw()|0)>>2]=12;d=0;return d|0}if(b>>>0<11){e=16}else{e=b+11&-8}f=kM(a-8|0,e)|0;if((f|0)!=0){d=f+8|0;return d|0}f=kJ(b)|0;if((f|0)==0){d=0;return d|0}e=c[a-4>>2]|0;g=(e&-8)-((e&3|0)==0?8:4)|0;e=g>>>0<b>>>0?g:b;kY(f|0,a|0,e)|0;kK(a);d=f;return d|0}function kM(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0;d=a+4|0;e=c[d>>2]|0;f=e&-8;g=a;h=g+f|0;i=h;j=c[2464]|0;if(g>>>0<j>>>0){bG();return 0}k=e&3;if(!((k|0)!=1&g>>>0<h>>>0)){bG();return 0}l=g+(f|4)|0;m=c[l>>2]|0;if((m&1|0)==0){bG();return 0}if((k|0)==0){if(b>>>0<256){n=0;return n|0}do{if(f>>>0>=(b+4|0)>>>0){if((f-b|0)>>>0>c[2452]<<1>>>0){break}else{n=a}return n|0}}while(0);n=0;return n|0}if(f>>>0>=b>>>0){k=f-b|0;if(k>>>0<=15){n=a;return n|0}c[d>>2]=e&1|b|2;c[g+(b+4)>>2]=k|3;c[l>>2]=c[l>>2]|1;kN(g+b|0,k);n=a;return n|0}if((i|0)==(c[2466]|0)){k=(c[2463]|0)+f|0;if(k>>>0<=b>>>0){n=0;return n|0}l=k-b|0;c[d>>2]=e&1|b|2;c[g+(b+4)>>2]=l|1;c[2466]=g+b;c[2463]=l;n=a;return n|0}if((i|0)==(c[2465]|0)){l=(c[2462]|0)+f|0;if(l>>>0<b>>>0){n=0;return n|0}k=l-b|0;if(k>>>0>15){c[d>>2]=e&1|b|2;c[g+(b+4)>>2]=k|1;c[g+l>>2]=k;o=g+(l+4)|0;c[o>>2]=c[o>>2]&-2;p=g+b|0;q=k}else{c[d>>2]=e&1|l|2;e=g+(l+4)|0;c[e>>2]=c[e>>2]|1;p=0;q=0}c[2462]=q;c[2465]=p;n=a;return n|0}if((m&2|0)!=0){n=0;return n|0}p=(m&-8)+f|0;if(p>>>0<b>>>0){n=0;return n|0}q=p-b|0;e=m>>>3;L1478:do{if(m>>>0<256){l=c[g+(f+8)>>2]|0;k=c[g+(f+12)>>2]|0;o=9880+(e<<1<<2)|0;do{if((l|0)!=(o|0)){if(l>>>0<j>>>0){bG();return 0}if((c[l+12>>2]|0)==(i|0)){break}bG();return 0}}while(0);if((k|0)==(l|0)){c[2460]=c[2460]&~(1<<e);break}do{if((k|0)==(o|0)){r=k+8|0}else{if(k>>>0<j>>>0){bG();return 0}s=k+8|0;if((c[s>>2]|0)==(i|0)){r=s;break}bG();return 0}}while(0);c[l+12>>2]=k;c[r>>2]=l}else{o=h;s=c[g+(f+24)>>2]|0;t=c[g+(f+12)>>2]|0;do{if((t|0)==(o|0)){u=g+(f+20)|0;v=c[u>>2]|0;if((v|0)==0){w=g+(f+16)|0;x=c[w>>2]|0;if((x|0)==0){y=0;break}else{z=x;A=w}}else{z=v;A=u}while(1){u=z+20|0;v=c[u>>2]|0;if((v|0)!=0){z=v;A=u;continue}u=z+16|0;v=c[u>>2]|0;if((v|0)==0){break}else{z=v;A=u}}if(A>>>0<j>>>0){bG();return 0}else{c[A>>2]=0;y=z;break}}else{u=c[g+(f+8)>>2]|0;if(u>>>0<j>>>0){bG();return 0}v=u+12|0;if((c[v>>2]|0)!=(o|0)){bG();return 0}w=t+8|0;if((c[w>>2]|0)==(o|0)){c[v>>2]=t;c[w>>2]=u;y=t;break}else{bG();return 0}}}while(0);if((s|0)==0){break}t=g+(f+28)|0;l=10144+(c[t>>2]<<2)|0;do{if((o|0)==(c[l>>2]|0)){c[l>>2]=y;if((y|0)!=0){break}c[2461]=c[2461]&~(1<<c[t>>2]);break L1478}else{if(s>>>0<(c[2464]|0)>>>0){bG();return 0}k=s+16|0;if((c[k>>2]|0)==(o|0)){c[k>>2]=y}else{c[s+20>>2]=y}if((y|0)==0){break L1478}}}while(0);if(y>>>0<(c[2464]|0)>>>0){bG();return 0}c[y+24>>2]=s;o=c[g+(f+16)>>2]|0;do{if((o|0)!=0){if(o>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[y+16>>2]=o;c[o+24>>2]=y;break}}}while(0);o=c[g+(f+20)>>2]|0;if((o|0)==0){break}if(o>>>0<(c[2464]|0)>>>0){bG();return 0}else{c[y+20>>2]=o;c[o+24>>2]=y;break}}}while(0);if(q>>>0<16){c[d>>2]=p|c[d>>2]&1|2;y=g+(p|4)|0;c[y>>2]=c[y>>2]|1;n=a;return n|0}else{c[d>>2]=c[d>>2]&1|b|2;c[g+(b+4)>>2]=q|3;d=g+(p|4)|0;c[d>>2]=c[d>>2]|1;kN(g+b|0,q);n=a;return n|0}return 0}function kN(a,b){a=a|0;b=b|0;var d=0,e=0,f=0,g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,K=0,L=0;d=a;e=d+b|0;f=e;g=c[a+4>>2]|0;L1554:do{if((g&1|0)==0){h=c[a>>2]|0;if((g&3|0)==0){return}i=d+(-h|0)|0;j=i;k=h+b|0;l=c[2464]|0;if(i>>>0<l>>>0){bG()}if((j|0)==(c[2465]|0)){m=d+(b+4)|0;if((c[m>>2]&3|0)!=3){n=j;o=k;break}c[2462]=k;c[m>>2]=c[m>>2]&-2;c[d+(4-h)>>2]=k|1;c[e>>2]=k;return}m=h>>>3;if(h>>>0<256){p=c[d+(8-h)>>2]|0;q=c[d+(12-h)>>2]|0;r=9880+(m<<1<<2)|0;do{if((p|0)!=(r|0)){if(p>>>0<l>>>0){bG()}if((c[p+12>>2]|0)==(j|0)){break}bG()}}while(0);if((q|0)==(p|0)){c[2460]=c[2460]&~(1<<m);n=j;o=k;break}do{if((q|0)==(r|0)){s=q+8|0}else{if(q>>>0<l>>>0){bG()}t=q+8|0;if((c[t>>2]|0)==(j|0)){s=t;break}bG()}}while(0);c[p+12>>2]=q;c[s>>2]=p;n=j;o=k;break}r=i;m=c[d+(24-h)>>2]|0;t=c[d+(12-h)>>2]|0;do{if((t|0)==(r|0)){u=16-h|0;v=d+(u+4)|0;w=c[v>>2]|0;if((w|0)==0){x=d+u|0;u=c[x>>2]|0;if((u|0)==0){y=0;break}else{z=u;A=x}}else{z=w;A=v}while(1){v=z+20|0;w=c[v>>2]|0;if((w|0)!=0){z=w;A=v;continue}v=z+16|0;w=c[v>>2]|0;if((w|0)==0){break}else{z=w;A=v}}if(A>>>0<l>>>0){bG()}else{c[A>>2]=0;y=z;break}}else{v=c[d+(8-h)>>2]|0;if(v>>>0<l>>>0){bG()}w=v+12|0;if((c[w>>2]|0)!=(r|0)){bG()}x=t+8|0;if((c[x>>2]|0)==(r|0)){c[w>>2]=t;c[x>>2]=v;y=t;break}else{bG()}}}while(0);if((m|0)==0){n=j;o=k;break}t=d+(28-h)|0;l=10144+(c[t>>2]<<2)|0;do{if((r|0)==(c[l>>2]|0)){c[l>>2]=y;if((y|0)!=0){break}c[2461]=c[2461]&~(1<<c[t>>2]);n=j;o=k;break L1554}else{if(m>>>0<(c[2464]|0)>>>0){bG()}i=m+16|0;if((c[i>>2]|0)==(r|0)){c[i>>2]=y}else{c[m+20>>2]=y}if((y|0)==0){n=j;o=k;break L1554}}}while(0);if(y>>>0<(c[2464]|0)>>>0){bG()}c[y+24>>2]=m;r=16-h|0;t=c[d+r>>2]|0;do{if((t|0)!=0){if(t>>>0<(c[2464]|0)>>>0){bG()}else{c[y+16>>2]=t;c[t+24>>2]=y;break}}}while(0);t=c[d+(r+4)>>2]|0;if((t|0)==0){n=j;o=k;break}if(t>>>0<(c[2464]|0)>>>0){bG()}else{c[y+20>>2]=t;c[t+24>>2]=y;n=j;o=k;break}}else{n=a;o=b}}while(0);a=c[2464]|0;if(e>>>0<a>>>0){bG()}y=d+(b+4)|0;z=c[y>>2]|0;do{if((z&2|0)==0){if((f|0)==(c[2466]|0)){A=(c[2463]|0)+o|0;c[2463]=A;c[2466]=n;c[n+4>>2]=A|1;if((n|0)!=(c[2465]|0)){return}c[2465]=0;c[2462]=0;return}if((f|0)==(c[2465]|0)){A=(c[2462]|0)+o|0;c[2462]=A;c[2465]=n;c[n+4>>2]=A|1;c[n+A>>2]=A;return}A=(z&-8)+o|0;s=z>>>3;L1653:do{if(z>>>0<256){g=c[d+(b+8)>>2]|0;t=c[d+(b+12)>>2]|0;h=9880+(s<<1<<2)|0;do{if((g|0)!=(h|0)){if(g>>>0<a>>>0){bG()}if((c[g+12>>2]|0)==(f|0)){break}bG()}}while(0);if((t|0)==(g|0)){c[2460]=c[2460]&~(1<<s);break}do{if((t|0)==(h|0)){B=t+8|0}else{if(t>>>0<a>>>0){bG()}m=t+8|0;if((c[m>>2]|0)==(f|0)){B=m;break}bG()}}while(0);c[g+12>>2]=t;c[B>>2]=g}else{h=e;m=c[d+(b+24)>>2]|0;l=c[d+(b+12)>>2]|0;do{if((l|0)==(h|0)){i=d+(b+20)|0;p=c[i>>2]|0;if((p|0)==0){q=d+(b+16)|0;v=c[q>>2]|0;if((v|0)==0){C=0;break}else{D=v;E=q}}else{D=p;E=i}while(1){i=D+20|0;p=c[i>>2]|0;if((p|0)!=0){D=p;E=i;continue}i=D+16|0;p=c[i>>2]|0;if((p|0)==0){break}else{D=p;E=i}}if(E>>>0<a>>>0){bG()}else{c[E>>2]=0;C=D;break}}else{i=c[d+(b+8)>>2]|0;if(i>>>0<a>>>0){bG()}p=i+12|0;if((c[p>>2]|0)!=(h|0)){bG()}q=l+8|0;if((c[q>>2]|0)==(h|0)){c[p>>2]=l;c[q>>2]=i;C=l;break}else{bG()}}}while(0);if((m|0)==0){break}l=d+(b+28)|0;g=10144+(c[l>>2]<<2)|0;do{if((h|0)==(c[g>>2]|0)){c[g>>2]=C;if((C|0)!=0){break}c[2461]=c[2461]&~(1<<c[l>>2]);break L1653}else{if(m>>>0<(c[2464]|0)>>>0){bG()}t=m+16|0;if((c[t>>2]|0)==(h|0)){c[t>>2]=C}else{c[m+20>>2]=C}if((C|0)==0){break L1653}}}while(0);if(C>>>0<(c[2464]|0)>>>0){bG()}c[C+24>>2]=m;h=c[d+(b+16)>>2]|0;do{if((h|0)!=0){if(h>>>0<(c[2464]|0)>>>0){bG()}else{c[C+16>>2]=h;c[h+24>>2]=C;break}}}while(0);h=c[d+(b+20)>>2]|0;if((h|0)==0){break}if(h>>>0<(c[2464]|0)>>>0){bG()}else{c[C+20>>2]=h;c[h+24>>2]=C;break}}}while(0);c[n+4>>2]=A|1;c[n+A>>2]=A;if((n|0)!=(c[2465]|0)){F=A;break}c[2462]=A;return}else{c[y>>2]=z&-2;c[n+4>>2]=o|1;c[n+o>>2]=o;F=o}}while(0);o=F>>>3;if(F>>>0<256){z=o<<1;y=9880+(z<<2)|0;C=c[2460]|0;b=1<<o;do{if((C&b|0)==0){c[2460]=C|b;G=y;H=9880+(z+2<<2)|0}else{o=9880+(z+2<<2)|0;d=c[o>>2]|0;if(d>>>0>=(c[2464]|0)>>>0){G=d;H=o;break}bG()}}while(0);c[H>>2]=n;c[G+12>>2]=n;c[n+8>>2]=G;c[n+12>>2]=y;return}y=n;G=F>>>8;do{if((G|0)==0){I=0}else{if(F>>>0>16777215){I=31;break}H=(G+1048320|0)>>>16&8;z=G<<H;b=(z+520192|0)>>>16&4;C=z<<b;z=(C+245760|0)>>>16&2;o=14-(b|H|z)+(C<<z>>>15)|0;I=F>>>((o+7|0)>>>0)&1|o<<1}}while(0);G=10144+(I<<2)|0;c[n+28>>2]=I;c[n+20>>2]=0;c[n+16>>2]=0;o=c[2461]|0;z=1<<I;if((o&z|0)==0){c[2461]=o|z;c[G>>2]=y;c[n+24>>2]=G;c[n+12>>2]=n;c[n+8>>2]=n;return}if((I|0)==31){J=0}else{J=25-(I>>>1)|0}I=F<<J;J=c[G>>2]|0;while(1){if((c[J+4>>2]&-8|0)==(F|0)){break}K=J+16+(I>>>31<<2)|0;G=c[K>>2]|0;if((G|0)==0){L=1298;break}else{I=I<<1;J=G}}if((L|0)==1298){if(K>>>0<(c[2464]|0)>>>0){bG()}c[K>>2]=y;c[n+24>>2]=J;c[n+12>>2]=n;c[n+8>>2]=n;return}K=J+8|0;L=c[K>>2]|0;I=c[2464]|0;if(J>>>0<I>>>0){bG()}if(L>>>0<I>>>0){bG()}c[L+12>>2]=y;c[K>>2]=y;c[n+8>>2]=L;c[n+12>>2]=J;c[n+24>>2]=0;return}function kO(a){a=a|0;var b=0,d=0,e=0;b=(a|0)==0?1:a;while(1){d=kJ(b)|0;if((d|0)!=0){e=1342;break}a=(I=c[4048]|0,c[4048]=I+0,I);if((a|0)==0){break}b5[a&3]()}if((e|0)==1342){return d|0}d=bO(4)|0;c[d>>2]=2408;bl(d|0,8160,30);return 0}function kP(a){a=a|0;return kO(a)|0}function kQ(a){a=a|0;return}function kR(a){a=a|0;return 1160|0}function kS(a){a=a|0;if((a|0)!=0){kK(a)}return}function kT(a){a=a|0;kS(a);return}function kU(a){a=a|0;kS(a);return}function kV(b,d){b=b|0;d=d|0;var e=0,f=0,g=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0.0,r=0,s=0,t=0,u=0,v=0.0,w=0,x=0,y=0,z=0.0,A=0.0,B=0,C=0,D=0,E=0.0,F=0,G=0,H=0,I=0,J=0,K=0,L=0,M=0,N=0.0,O=0,P=0,Q=0.0,R=0.0,S=0.0;e=b;while(1){f=e+1|0;if((aP(a[e]|0)|0)==0){break}else{e=f}}switch(a[e]|0){case 43:{g=f;i=0;break};case 45:{g=f;i=1;break};default:{g=e;i=0}}e=-1;f=0;j=g;while(1){k=a[j]|0;if(((k<<24>>24)-48|0)>>>0<10){l=e}else{if(k<<24>>24!=46|(e|0)>-1){break}else{l=f}}e=l;f=f+1|0;j=j+1|0}l=j+(-f|0)|0;g=(e|0)<0;m=((g^1)<<31>>31)+f|0;n=(m|0)>18;o=(n?-18:-m|0)+(g?f:e)|0;e=n?18:m;do{if((e|0)==0){p=b;q=0.0}else{if((e|0)>9){m=l;n=e;f=0;while(1){g=a[m]|0;r=m+1|0;if(g<<24>>24==46){s=a[r]|0;t=m+2|0}else{s=g;t=r}u=(f*10|0)-48+(s<<24>>24)|0;r=n-1|0;if((r|0)>9){m=t;n=r;f=u}else{break}}v=+(u|0)*1.0e9;w=9;x=t;y=1370}else{if((e|0)>0){v=0.0;w=e;x=l;y=1370}else{z=0.0;A=0.0}}if((y|0)==1370){f=x;n=w;m=0;while(1){r=a[f]|0;g=f+1|0;if(r<<24>>24==46){B=a[g]|0;C=f+2|0}else{B=r;C=g}D=(m*10|0)-48+(B<<24>>24)|0;g=n-1|0;if((g|0)>0){f=C;n=g;m=D}else{break}}z=+(D|0);A=v}E=A+z;L1816:do{switch(k<<24>>24){case 69:case 101:{m=j+1|0;switch(a[m]|0){case 45:{F=j+2|0;G=1;break};case 43:{F=j+2|0;G=0;break};default:{F=m;G=0}}m=a[F]|0;if(((m<<24>>24)-48|0)>>>0<10){H=F;I=0;J=m}else{K=0;L=F;M=G;break L1816}while(1){m=(I*10|0)-48+(J<<24>>24)|0;n=H+1|0;f=a[n]|0;if(((f<<24>>24)-48|0)>>>0<10){H=n;I=m;J=f}else{K=m;L=n;M=G;break}}break};default:{K=0;L=j;M=0}}}while(0);n=o+((M|0)==0?K:-K|0)|0;m=(n|0)<0?-n|0:n;if((m|0)>511){c[(bw()|0)>>2]=34;N=1.0;O=8;P=511;y=1387}else{if((m|0)==0){Q=1.0}else{N=1.0;O=8;P=m;y=1387}}if((y|0)==1387){while(1){y=0;if((P&1|0)==0){R=N}else{R=N*+h[O>>3]}m=P>>1;if((m|0)==0){Q=R;break}else{N=R;O=O+8|0;P=m;y=1387}}}if((n|0)>-1){p=L;q=E*Q;break}else{p=L;q=E/Q;break}}}while(0);if((d|0)!=0){c[d>>2]=p}if((i|0)==0){S=q;return+S}S=-0.0-q;return+S}function kW(a,b,c){a=a|0;b=b|0;c=c|0;return+(+kV(a,b))}function kX(){var a=0;a=bO(4)|0;c[a>>2]=2408;bl(a|0,8160,30)}function kY(b,d,e){b=b|0;d=d|0;e=e|0;var f=0;f=b|0;if((b&3)==(d&3)){while(b&3){if((e|0)==0)return f|0;a[b]=a[d]|0;b=b+1|0;d=d+1|0;e=e-1|0}while((e|0)>=4){c[b>>2]=c[d>>2];b=b+4|0;d=d+4|0;e=e-4|0}}while((e|0)>0){a[b]=a[d]|0;b=b+1|0;d=d+1|0;e=e-1|0}return f|0}function kZ(b){b=b|0;var c=0;c=a[n+(b>>>24)|0]|0;if((c|0)<8)return c|0;c=a[n+(b>>16&255)|0]|0;if((c|0)<8)return c+8|0;c=a[n+(b>>8&255)|0]|0;if((c|0)<8)return c+16|0;return(a[n+(b&255)|0]|0)+24|0}function k_(b,c,d){b=b|0;c=c|0;d=d|0;if((c|0)<(b|0)&(b|0)<(c+d|0)){c=c+d|0;b=b+d|0;while((d|0)>0){b=b-1|0;c=c-1|0;d=d-1|0;a[b]=a[c]|0}}else{kY(b,c,d)|0}}function k$(b,d,e){b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0;f=b+e|0;if((e|0)>=20){d=d&255;e=b&3;g=d|d<<8|d<<16|d<<24;h=f&~3;if(e){e=b+4-e|0;while((b|0)<(e|0)){a[b]=d;b=b+1|0}}while((b|0)<(h|0)){c[b>>2]=g;b=b+4|0}}while((b|0)<(f|0)){a[b]=d;b=b+1|0}}function k0(b){b=b|0;var c=0;c=b;while(a[c]|0){c=c+1|0}return c-b|0}function k1(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;var e=0;e=a+c>>>0;return(K=b+d+(e>>>0<a>>>0|0)>>>0,e|0)|0}function k2(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;var e=0;e=b-d>>>0;e=b-d-(c>>>0>a>>>0|0)>>>0;return(K=e,a-c>>>0|0)|0}function k3(a,b,c){a=a|0;b=b|0;c=c|0;if((c|0)<32){K=b<<c|(a&(1<<c)-1<<32-c)>>>32-c;return a<<c}K=a<<c-32;return 0}function k4(a,b,c){a=a|0;b=b|0;c=c|0;if((c|0)<32){K=b>>>c;return a>>>c|(b&(1<<c)-1)<<32-c}K=0;return b>>>c-32|0}function k5(a,b,c){a=a|0;b=b|0;c=c|0;if((c|0)<32){K=b>>c;return a>>>c|(b&(1<<c)-1)<<32-c}K=(b|0)<0?-1:0;return b>>c-32|0}function k6(b){b=b|0;var c=0;c=a[m+(b&255)|0]|0;if((c|0)<8)return c|0;c=a[m+(b>>8&255)|0]|0;if((c|0)<8)return c+8|0;c=a[m+(b>>16&255)|0]|0;if((c|0)<8)return c+16|0;return(a[m+(b>>>24)|0]|0)+24|0}function k7(a,b){a=a|0;b=b|0;var c=0,d=0,e=0,f=0;c=a&65535;d=b&65535;e=ag(d,c)|0;f=a>>>16;a=(e>>>16)+(ag(d,f)|0)|0;d=b>>>16;b=ag(d,c)|0;return(K=(a>>>16)+(ag(d,f)|0)+(((a&65535)+b|0)>>>16)|0,a+b<<16|e&65535|0)|0}function k8(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;var e=0,f=0,g=0,h=0,i=0;e=b>>31|((b|0)<0?-1:0)<<1;f=((b|0)<0?-1:0)>>31|((b|0)<0?-1:0)<<1;g=d>>31|((d|0)<0?-1:0)<<1;h=((d|0)<0?-1:0)>>31|((d|0)<0?-1:0)<<1;i=k2(e^a,f^b,e,f)|0;b=K;a=g^e;e=h^f;f=k2((ld(i,b,k2(g^c,h^d,g,h)|0,K,0)|0)^a,K^e,a,e)|0;return(K=K,f)|0}function k9(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0,h=0,j=0,k=0,l=0,m=0;f=i;i=i+8|0;g=f|0;h=b>>31|((b|0)<0?-1:0)<<1;j=((b|0)<0?-1:0)>>31|((b|0)<0?-1:0)<<1;k=e>>31|((e|0)<0?-1:0)<<1;l=((e|0)<0?-1:0)>>31|((e|0)<0?-1:0)<<1;m=k2(h^a,j^b,h,j)|0;b=K;a=k2(k^d,l^e,k,l)|0;ld(m,b,a,K,g)|0;a=k2(c[g>>2]^h,c[g+4>>2]^j,h,j)|0;j=K;i=f;return(K=j,a)|0}function la(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;var e=0,f=0;e=a;a=c;c=k7(e,a)|0;f=K;return(K=(ag(b,a)|0)+(ag(d,e)|0)+f|f&0,c|0|0)|0}function lb(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;var e=0;e=ld(a,b,c,d,0)|0;return(K=K,e)|0}function lc(a,b,d,e){a=a|0;b=b|0;d=d|0;e=e|0;var f=0,g=0;f=i;i=i+8|0;g=f|0;ld(a,b,d,e,g)|0;i=f;return(K=c[g+4>>2]|0,c[g>>2]|0)|0}function ld(a,b,d,e,f){a=a|0;b=b|0;d=d|0;e=e|0;f=f|0;var g=0,h=0,i=0,j=0,k=0,l=0,m=0,n=0,o=0,p=0,q=0,r=0,s=0,t=0,u=0,v=0,w=0,x=0,y=0,z=0,A=0,B=0,C=0,D=0,E=0,F=0,G=0,H=0,I=0,J=0,L=0,M=0;g=a;h=b;i=h;j=d;k=e;l=k;if((i|0)==0){m=(f|0)!=0;if((l|0)==0){if(m){c[f>>2]=(g>>>0)%(j>>>0);c[f+4>>2]=0}n=0;o=(g>>>0)/(j>>>0)>>>0;return(K=n,o)|0}else{if(!m){n=0;o=0;return(K=n,o)|0}c[f>>2]=a|0;c[f+4>>2]=b&0;n=0;o=0;return(K=n,o)|0}}m=(l|0)==0;do{if((j|0)==0){if(m){if((f|0)!=0){c[f>>2]=(i>>>0)%(j>>>0);c[f+4>>2]=0}n=0;o=(i>>>0)/(j>>>0)>>>0;return(K=n,o)|0}if((g|0)==0){if((f|0)!=0){c[f>>2]=0;c[f+4>>2]=(i>>>0)%(l>>>0)}n=0;o=(i>>>0)/(l>>>0)>>>0;return(K=n,o)|0}p=l-1|0;if((p&l|0)==0){if((f|0)!=0){c[f>>2]=a|0;c[f+4>>2]=p&i|b&0}n=0;o=i>>>((k6(l|0)|0)>>>0);return(K=n,o)|0}p=(kZ(l|0)|0)-(kZ(i|0)|0)|0;if(p>>>0<=30){q=p+1|0;r=31-p|0;s=q;t=i<<r|g>>>(q>>>0);u=i>>>(q>>>0);v=0;w=g<<r;break}if((f|0)==0){n=0;o=0;return(K=n,o)|0}c[f>>2]=a|0;c[f+4>>2]=h|b&0;n=0;o=0;return(K=n,o)|0}else{if(!m){r=(kZ(l|0)|0)-(kZ(i|0)|0)|0;if(r>>>0<=31){q=r+1|0;p=31-r|0;x=r-31>>31;s=q;t=g>>>(q>>>0)&x|i<<p;u=i>>>(q>>>0)&x;v=0;w=g<<p;break}if((f|0)==0){n=0;o=0;return(K=n,o)|0}c[f>>2]=a|0;c[f+4>>2]=h|b&0;n=0;o=0;return(K=n,o)|0}p=j-1|0;if((p&j|0)!=0){x=(kZ(j|0)|0)+33-(kZ(i|0)|0)|0;q=64-x|0;r=32-x|0;y=r>>31;z=x-32|0;A=z>>31;s=x;t=r-1>>31&i>>>(z>>>0)|(i<<r|g>>>(x>>>0))&A;u=A&i>>>(x>>>0);v=g<<q&y;w=(i<<q|g>>>(z>>>0))&y|g<<r&x-33>>31;break}if((f|0)!=0){c[f>>2]=p&g;c[f+4>>2]=0}if((j|0)==1){n=h|b&0;o=a|0|0;return(K=n,o)|0}else{p=k6(j|0)|0;n=i>>>(p>>>0)|0;o=i<<32-p|g>>>(p>>>0)|0;return(K=n,o)|0}}}while(0);if((s|0)==0){B=w;C=v;D=u;E=t;F=0;G=0}else{g=d|0|0;d=k|e&0;e=k1(g,d,-1,-1)|0;k=K;i=w;w=v;v=u;u=t;t=s;s=0;while(1){H=w>>>31|i<<1;I=s|w<<1;j=u<<1|i>>>31|0;a=u>>>31|v<<1|0;k2(e,k,j,a)|0;b=K;h=b>>31|((b|0)<0?-1:0)<<1;J=h&1;L=k2(j,a,h&g,(((b|0)<0?-1:0)>>31|((b|0)<0?-1:0)<<1)&d)|0;M=K;b=t-1|0;if((b|0)==0){break}else{i=H;w=I;v=M;u=L;t=b;s=J}}B=H;C=I;D=M;E=L;F=0;G=J}J=C;C=0;if((f|0)!=0){c[f>>2]=E;c[f+4>>2]=D}n=(J|0)>>>31|(B|C)<<1|(C<<1|J>>>31)&0|F;o=(J<<1|0>>>31)&-2|G;return(K=n,o)|0}function le(){bP()}function lf(a,b,c,d,e,f){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;bW[a&7](b|0,c|0,d|0,e|0,f|0)}function lg(a,b,c,d,e,f,g,h){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;bX[a&127](b|0,c|0,d|0,e|0,f|0,g|0,h|0)}function lh(a,b){a=a|0;b=b|0;bY[a&511](b|0)}function li(a,b,c){a=a|0;b=b|0;c=c|0;bZ[a&127](b|0,c|0)}function lj(a,b,c){a=a|0;b=b|0;c=c|0;return b_[a&31](b|0,c|0)|0}function lk(a,b,c,d,e,f){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;return b$[a&31](b|0,c|0,d|0,e|0,f|0)|0}function ll(a,b){a=a|0;b=b|0;return b0[a&127](b|0)|0}function lm(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;return b1[a&63](b|0,c|0,d|0)|0}function ln(a,b,c,d,e,f,g){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=+g;b2[a&15](b|0,c|0,d|0,e|0,f|0,+g)}function lo(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;b3[a&7](b|0,c|0,d|0)}function lp(a,b,c,d,e,f,g,h,i){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;b4[a&15](b|0,c|0,d|0,e|0,f|0,g|0,h|0,i|0)}function lq(a){a=a|0;b5[a&3]()}function lr(a,b,c,d,e,f,g,h,i){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;return b6[a&31](b|0,c|0,d|0,e|0,f|0,g|0,h|0,i|0)|0}function ls(a,b,c,d,e,f,g,h,i,j){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;b7[a&7](b|0,c|0,d|0,e|0,f|0,g|0,h|0,i|0,j|0)}function lt(a,b,c,d,e,f,g,h){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=+h;b8[a&7](b|0,c|0,d|0,e|0,f|0,g|0,+h)}function lu(a,b,c,d,e,f,g){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;b9[a&31](b|0,c|0,d|0,e|0,f|0,g|0)}function lv(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;return ca[a&15](b|0,c|0,d|0,e|0)|0}function lw(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;cb[a&15](b|0,c|0,d|0,e|0)}function lx(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;ah(0)}function ly(a,b,c,d,e,f,g){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;ah(1)}function lz(a){a=a|0;ah(2)}function lA(a,b){a=a|0;b=b|0;ah(3)}function lB(a,b){a=a|0;b=b|0;ah(4);return 0}function lC(a,b,c,d,e){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;ah(5);return 0}function lD(a){a=a|0;ah(6);return 0}function lE(a,b,c){a=a|0;b=b|0;c=c|0;ah(7);return 0}function lF(a,b,c,d,e,f){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=+f;ah(8)}function lG(a,b,c){a=a|0;b=b|0;c=c|0;ah(9)}function lH(a,b,c,d,e,f,g,h){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;ah(10)}function lI(){ah(11)}function lJ(a,b,c,d,e,f,g,h){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;ah(12);return 0}function lK(a,b,c,d,e,f,g,h,i){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;ah(13)}function lL(a,b,c,d,e,f,g){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=+g;ah(14)}function lM(a,b,c,d,e,f){a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;ah(15)}function lN(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;ah(16);return 0}function lO(a,b,c,d){a=a|0;b=b|0;c=c|0;d=d|0;ah(17)}
// EMSCRIPTEN_END_FUNCS
var bW=[lx,lx,kG,lx,kD,lx,kF,lx];var bX=[ly,ly,gG,ly,gP,ly,gS,ly,ih,ly,gt,ly,gr,ly,h9,ly,gB,ly,gF,ly,gT,ly,gf,ly,f7,ly,gE,ly,fH,ly,fZ,ly,gQ,ly,gd,ly,f$,ly,fX,ly,fY,ly,fQ,ly,f_,ly,fV,ly,fT,ly,f4,ly,f3,ly,f0,ly,gU,ly,fA,ly,gC,ly,fE,ly,fv,ly,fy,ly,fC,ly,fr,ly,fM,ly,fK,ly,fp,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly,ly];var bY=[lz,lz,ik,lz,fn,lz,gl,lz,dH,lz,en,lz,iE,lz,dr,lz,hA,lz,f5,lz,dB,lz,dG,lz,fO,lz,fb,lz,e7,lz,kQ,lz,dC,lz,iP,lz,iS,lz,gO,lz,fP,lz,kq,lz,e1,lz,ie,lz,iQ,lz,eS,lz,fo,lz,hr,lz,iL,lz,jI,lz,iU,lz,kk,lz,jH,lz,e5,lz,c5,lz,dG,lz,ct,lz,ff,lz,hg,lz,jK,lz,iR,lz,kK,lz,h7,lz,jG,lz,eV,lz,c4,lz,fe,lz,dC,lz,j3,lz,f6,lz,df,lz,hs,lz,eR,lz,e$,lz,hy,lz,gN,lz,e8,lz,hl,lz,kU,lz,em,lz,j6,lz,j7,lz,e2,lz,eu,lz,kx,lz,iV,lz,i9,lz,ki,lz,eT,lz,e6,lz,hO,lz,dd,lz,hx,lz,e3,lz,j9,lz,kl,lz,jq,lz,db,lz,iK,lz,iO,lz,hz,lz,hU,lz,ku,lz,hG,lz,h8,lz,j8,lz,hf,lz,gk,lz,fa,lz,dc,lz,fc,lz,eX,lz,dN,lz,h$,lz,hT,lz,jJ,lz,ee,lz,dI,lz,ki,lz,ky,lz,e_,lz,dO,lz,eQ,lz,e0,lz,id,lz,eD,lz,eZ,lz,ds,lz,iW,lz,e9,lz,hJ,lz,gz,lz,hm,lz,c$,lz,iq,lz,h0,lz,eW,lz,et,lz,eY,lz,jF,lz,j4,lz,gA,lz,kw,lz,il,lz,c_,lz,kv,lz,hB,lz,hP,lz,cW,lz,dP,lz,iT,lz,dF,lz,eU,lz,ka,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz,lz];var bZ=[lA,lA,jP,lA,c0,lA,hM,lA,jM,lA,hb,lA,jL,lA,ij,lA,d9,lA,hk,lA,hH,lA,hw,lA,hj,lA,hh,lA,hN,lA,iN,lA,g9,lA,dm,lA,hI,lA,jO,lA,hK,lA,ho,lA,dE,lA,hn,lA,jQ,lA,ha,lA,hq,lA,jN,lA,g8,lA,d2,lA,dt,lA,c6,lA,io,lA,ht,lA,g7,lA,g6,lA,hi,lA,hu,lA,hv,lA,hp,lA,hL,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA,lA];var b_=[lB,lB,i4,lB,dw,lB,iA,lB,it,lB,d8,lB,c9,lB,dp,lB,i0,lB,i6,lB,eI,lB,i2,lB,d7,lB,c2,lB,eH,lB,lB,lB];var b$=[lC,lC,jh,lC,jf,lC,jE,lC,iw,lC,iG,lC,jp,lC,iI,lC,fk,lC,iD,lC,jd,lC,eL,lC,js,lC,lC,lC,lC,lC,lC,lC];var b0=[lD,lD,j2,lD,g4,lD,d6,lD,jU,lD,er,lD,j0,lD,g0,lD,je,lD,jk,lD,gy,lD,jS,lD,dv,lD,eO,lD,eG,lD,jY,lD,jW,lD,jt,lD,kj,lD,di,lD,jC,lD,jz,lD,jX,lD,jj,lD,jA,lD,d4,lD,hF,lD,jZ,lD,du,lD,c1,lD,g1,lD,i8,lD,j1,lD,jv,lD,hD,lD,jR,lD,c7,lD,ju,lD,jg,lD,eJ,lD,g$,lD,c8,lD,jB,lD,d5,lD,eE,lD,dn,lD,g2,lD,iz,lD,iy,lD,kR,lD,eF,lD,gZ,lD,jT,lD,ix,lD,g_,lD,dh,lD,g3,lD,g5,lD,jV,lD,hE,lD,gM,lD,j$,lD,j_,lD,ji,lD];var b1=[lE,lE,eM,lE,i5,lE,i3,lE,kz,lE,iB,lE,fl,lE,dJ,lE,es,lE,eq,lE,iY,lE,ea,lE,im,lE,iC,lE,i1,lE,eN,lE,dl,lE,i7,lE,ig,lE,d3,lE,eP,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE,lE];var b2=[lF,lF,gw,lF,gu,lF,gi,lF,gg,lF,lF,lF,lF,lF,lF,lF];var b3=[lG,lG,dk,lG,e4,lG,lG,lG];var b4=[lH,lH,he,lH,hd,lH,hQ,lH,hY,lH,hW,lH,h1,lH,lH,lH];var b5=[lI,lI,le,lI];var b6=[lJ,lJ,iu,lJ,jc,lJ,jo,lJ,iv,lJ,jr,lJ,jD,lJ,jb,lJ,ja,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ,lJ];var b7=[lK,lK,gW,lK,gI,lK,lK,lK];var b8=[lL,lL,ia,lL,h4,lL,lL,lL];var b9=[lM,lM,kH,lM,gs,lM,gm,lM,go,lM,kI,lM,gx,lM,ii,lM,ed,lM,gn,lM,ga,lM,ge,lM,f8,lM,kE,lM,eb,lM,ip,lM];var ca=[lN,lN,iZ,lN,i_,lN,iH,lN,iF,lN,i$,lN,lN,lN,lN,lN];var cb=[lO,lO,kA,lO,kB,lO,ec,lO,kr,lO,eK,lO,fm,lO,fd,lO];return{_strlen:k0,_free:kK,_main:cD,_realloc:kL,_memmove:k_,__GLOBAL__I_a:cT,_memset:k$,_malloc:kJ,_memcpy:kY,_llvm_ctlz_i32:kZ,__GLOBAL__I_a8:dy,runPostSets:cs,stackAlloc:cc,stackSave:cd,stackRestore:ce,setThrew:cf,setTempRet0:ci,setTempRet1:cj,setTempRet2:ck,setTempRet3:cl,setTempRet4:cm,setTempRet5:cn,setTempRet6:co,setTempRet7:cp,setTempRet8:cq,setTempRet9:cr,dynCall_viiiii:lf,dynCall_viiiiiii:lg,dynCall_vi:lh,dynCall_vii:li,dynCall_iii:lj,dynCall_iiiiii:lk,dynCall_ii:ll,dynCall_iiii:lm,dynCall_viiiiif:ln,dynCall_viii:lo,dynCall_viiiiiiii:lp,dynCall_v:lq,dynCall_iiiiiiiii:lr,dynCall_viiiiiiiii:ls,dynCall_viiiiiif:lt,dynCall_viiiiii:lu,dynCall_iiiii:lv,dynCall_viiii:lw}})
// EMSCRIPTEN_END_ASM
({ "Math": Math, "Int8Array": Int8Array, "Int16Array": Int16Array, "Int32Array": Int32Array, "Uint8Array": Uint8Array, "Uint16Array": Uint16Array, "Uint32Array": Uint32Array, "Float32Array": Float32Array, "Float64Array": Float64Array }, { "abort": abort, "assert": assert, "asmPrintInt": asmPrintInt, "asmPrintFloat": asmPrintFloat, "min": Math_min, "invoke_viiiii": invoke_viiiii, "invoke_viiiiiii": invoke_viiiiiii, "invoke_vi": invoke_vi, "invoke_vii": invoke_vii, "invoke_iii": invoke_iii, "invoke_iiiiii": invoke_iiiiii, "invoke_ii": invoke_ii, "invoke_iiii": invoke_iiii, "invoke_viiiiif": invoke_viiiiif, "invoke_viii": invoke_viii, "invoke_viiiiiiii": invoke_viiiiiiii, "invoke_v": invoke_v, "invoke_iiiiiiiii": invoke_iiiiiiiii, "invoke_viiiiiiiii": invoke_viiiiiiiii, "invoke_viiiiiif": invoke_viiiiiif, "invoke_viiiiii": invoke_viiiiii, "invoke_iiiii": invoke_iiiii, "invoke_viiii": invoke_viiii, "_llvm_lifetime_end": _llvm_lifetime_end, "__scanString": __scanString, "_pthread_mutex_lock": _pthread_mutex_lock, "___cxa_end_catch": ___cxa_end_catch, "_strtoull": _strtoull, "__isFloat": __isFloat, "_fflush": _fflush, "__isLeapYear": __isLeapYear, "_fwrite": _fwrite, "_send": _send, "_llvm_umul_with_overflow_i32": _llvm_umul_with_overflow_i32, "_isspace": _isspace, "_read": _read, "___cxa_guard_abort": ___cxa_guard_abort, "_newlocale": _newlocale, "___gxx_personality_v0": ___gxx_personality_v0, "_pthread_cond_wait": _pthread_cond_wait, "___cxa_rethrow": ___cxa_rethrow, "___resumeException": ___resumeException, "_llvm_va_end": _llvm_va_end, "_vsscanf": _vsscanf, "_snprintf": _snprintf, "_fgetc": _fgetc, "_atexit": _atexit, "___cxa_free_exception": ___cxa_free_exception, "__Z8catcloseP8_nl_catd": __Z8catcloseP8_nl_catd, "___setErrNo": ___setErrNo, "_isxdigit": _isxdigit, "_exit": _exit, "_sprintf": _sprintf, "___ctype_b_loc": ___ctype_b_loc, "_freelocale": _freelocale, "__Z7catopenPKci": __Z7catopenPKci, "_asprintf": _asprintf, "___cxa_is_number_type": ___cxa_is_number_type, "___cxa_does_inherit": ___cxa_does_inherit, "___cxa_guard_acquire": ___cxa_guard_acquire, "___locale_mb_cur_max": ___locale_mb_cur_max, "___cxa_begin_catch": ___cxa_begin_catch, "_recv": _recv, "__parseInt64": __parseInt64, "__ZSt18uncaught_exceptionv": __ZSt18uncaught_exceptionv, "___cxa_call_unexpected": ___cxa_call_unexpected, "__exit": __exit, "_strftime": _strftime, "___cxa_throw": ___cxa_throw, "_llvm_eh_exception": _llvm_eh_exception, "_pread": _pread, "__arraySum": __arraySum, "_log": _log, "___cxa_find_matching_catch": ___cxa_find_matching_catch, "__formatString": __formatString, "_pthread_cond_broadcast": _pthread_cond_broadcast, "__ZSt9terminatev": __ZSt9terminatev, "_pthread_mutex_unlock": _pthread_mutex_unlock, "_sbrk": _sbrk, "___errno_location": ___errno_location, "_strerror": _strerror, "_llvm_lifetime_start": _llvm_lifetime_start, "___cxa_guard_release": ___cxa_guard_release, "_ungetc": _ungetc, "_uselocale": _uselocale, "_vsnprintf": _vsnprintf, "_sscanf": _sscanf, "_sysconf": _sysconf, "_fread": _fread, "_abort": _abort, "_isdigit": _isdigit, "_strtoll": _strtoll, "__addDays": __addDays, "_floor": _floor, "__reallyNegative": __reallyNegative, "__Z7catgetsP8_nl_catdiiPKc": __Z7catgetsP8_nl_catdiiPKc, "_write": _write, "___cxa_allocate_exception": ___cxa_allocate_exception, "___cxa_pure_virtual": ___cxa_pure_virtual, "_vasprintf": _vasprintf, "___ctype_toupper_loc": ___ctype_toupper_loc, "___ctype_tolower_loc": ___ctype_tolower_loc, "_pwrite": _pwrite, "_strerror_r": _strerror_r, "_time": _time, "STACKTOP": STACKTOP, "STACK_MAX": STACK_MAX, "tempDoublePtr": tempDoublePtr, "ABORT": ABORT, "cttz_i8": cttz_i8, "ctlz_i8": ctlz_i8, "NaN": NaN, "Infinity": Infinity, "_stdin": _stdin, "__ZTVN10__cxxabiv117__class_type_infoE": __ZTVN10__cxxabiv117__class_type_infoE, "__ZTVN10__cxxabiv120__si_class_type_infoE": __ZTVN10__cxxabiv120__si_class_type_infoE, "_stderr": _stderr, "___fsmu8": ___fsmu8, "_stdout": _stdout, "___dso_handle": ___dso_handle }, buffer);
var _strlen = Module["_strlen"] = asm["_strlen"];
var _free = Module["_free"] = asm["_free"];
var _main = Module["_main"] = asm["_main"];
var _realloc = Module["_realloc"] = asm["_realloc"];
var _memmove = Module["_memmove"] = asm["_memmove"];
var __GLOBAL__I_a = Module["__GLOBAL__I_a"] = asm["__GLOBAL__I_a"];
var _memset = Module["_memset"] = asm["_memset"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var _llvm_ctlz_i32 = Module["_llvm_ctlz_i32"] = asm["_llvm_ctlz_i32"];
var __GLOBAL__I_a8 = Module["__GLOBAL__I_a8"] = asm["__GLOBAL__I_a8"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];
var dynCall_viiiii = Module["dynCall_viiiii"] = asm["dynCall_viiiii"];
var dynCall_viiiiiii = Module["dynCall_viiiiiii"] = asm["dynCall_viiiiiii"];
var dynCall_vi = Module["dynCall_vi"] = asm["dynCall_vi"];
var dynCall_vii = Module["dynCall_vii"] = asm["dynCall_vii"];
var dynCall_iii = Module["dynCall_iii"] = asm["dynCall_iii"];
var dynCall_iiiiii = Module["dynCall_iiiiii"] = asm["dynCall_iiiiii"];
var dynCall_ii = Module["dynCall_ii"] = asm["dynCall_ii"];
var dynCall_iiii = Module["dynCall_iiii"] = asm["dynCall_iiii"];
var dynCall_viiiiif = Module["dynCall_viiiiif"] = asm["dynCall_viiiiif"];
var dynCall_viii = Module["dynCall_viii"] = asm["dynCall_viii"];
var dynCall_viiiiiiii = Module["dynCall_viiiiiiii"] = asm["dynCall_viiiiiiii"];
var dynCall_v = Module["dynCall_v"] = asm["dynCall_v"];
var dynCall_iiiiiiiii = Module["dynCall_iiiiiiiii"] = asm["dynCall_iiiiiiiii"];
var dynCall_viiiiiiiii = Module["dynCall_viiiiiiiii"] = asm["dynCall_viiiiiiiii"];
var dynCall_viiiiiif = Module["dynCall_viiiiiif"] = asm["dynCall_viiiiiif"];
var dynCall_viiiiii = Module["dynCall_viiiiii"] = asm["dynCall_viiiiii"];
var dynCall_iiiii = Module["dynCall_iiiii"] = asm["dynCall_iiiii"];
var dynCall_viiii = Module["dynCall_viiii"] = asm["dynCall_viiii"];
Runtime.stackAlloc = function(size) { return asm['stackAlloc'](size) };
Runtime.stackSave = function() { return asm['stackSave']() };
Runtime.stackRestore = function(top) { asm['stackRestore'](top) };
// TODO: strip out parts of this we do not need
//======= begin closure i64 code =======
// Copyright 2009 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/**
 * @fileoverview Defines a Long class for representing a 64-bit two's-complement
 * integer value, which faithfully simulates the behavior of a Java "long". This
 * implementation is derived from LongLib in GWT.
 *
 */
var i64Math = (function() { // Emscripten wrapper
  var goog = { math: {} };
  /**
   * Constructs a 64-bit two's-complement integer, given its low and high 32-bit
   * values as *signed* integers.  See the from* functions below for more
   * convenient ways of constructing Longs.
   *
   * The internal representation of a long is the two given signed, 32-bit values.
   * We use 32-bit pieces because these are the size of integers on which
   * Javascript performs bit-operations.  For operations like addition and
   * multiplication, we split each number into 16-bit pieces, which can easily be
   * multiplied within Javascript's floating-point representation without overflow
   * or change in sign.
   *
   * In the algorithms below, we frequently reduce the negative case to the
   * positive case by negating the input(s) and then post-processing the result.
   * Note that we must ALWAYS check specially whether those values are MIN_VALUE
   * (-2^63) because -MIN_VALUE == MIN_VALUE (since 2^63 cannot be represented as
   * a positive number, it overflows back into a negative).  Not handling this
   * case would often result in infinite recursion.
   *
   * @param {number} low  The low (signed) 32 bits of the long.
   * @param {number} high  The high (signed) 32 bits of the long.
   * @constructor
   */
  goog.math.Long = function(low, high) {
    /**
     * @type {number}
     * @private
     */
    this.low_ = low | 0;  // force into 32 signed bits.
    /**
     * @type {number}
     * @private
     */
    this.high_ = high | 0;  // force into 32 signed bits.
  };
  // NOTE: Common constant values ZERO, ONE, NEG_ONE, etc. are defined below the
  // from* methods on which they depend.
  /**
   * A cache of the Long representations of small integer values.
   * @type {!Object}
   * @private
   */
  goog.math.Long.IntCache_ = {};
  /**
   * Returns a Long representing the given (32-bit) integer value.
   * @param {number} value The 32-bit integer in question.
   * @return {!goog.math.Long} The corresponding Long value.
   */
  goog.math.Long.fromInt = function(value) {
    if (-128 <= value && value < 128) {
      var cachedObj = goog.math.Long.IntCache_[value];
      if (cachedObj) {
        return cachedObj;
      }
    }
    var obj = new goog.math.Long(value | 0, value < 0 ? -1 : 0);
    if (-128 <= value && value < 128) {
      goog.math.Long.IntCache_[value] = obj;
    }
    return obj;
  };
  /**
   * Returns a Long representing the given value, provided that it is a finite
   * number.  Otherwise, zero is returned.
   * @param {number} value The number in question.
   * @return {!goog.math.Long} The corresponding Long value.
   */
  goog.math.Long.fromNumber = function(value) {
    if (isNaN(value) || !isFinite(value)) {
      return goog.math.Long.ZERO;
    } else if (value <= -goog.math.Long.TWO_PWR_63_DBL_) {
      return goog.math.Long.MIN_VALUE;
    } else if (value + 1 >= goog.math.Long.TWO_PWR_63_DBL_) {
      return goog.math.Long.MAX_VALUE;
    } else if (value < 0) {
      return goog.math.Long.fromNumber(-value).negate();
    } else {
      return new goog.math.Long(
          (value % goog.math.Long.TWO_PWR_32_DBL_) | 0,
          (value / goog.math.Long.TWO_PWR_32_DBL_) | 0);
    }
  };
  /**
   * Returns a Long representing the 64-bit integer that comes by concatenating
   * the given high and low bits.  Each is assumed to use 32 bits.
   * @param {number} lowBits The low 32-bits.
   * @param {number} highBits The high 32-bits.
   * @return {!goog.math.Long} The corresponding Long value.
   */
  goog.math.Long.fromBits = function(lowBits, highBits) {
    return new goog.math.Long(lowBits, highBits);
  };
  /**
   * Returns a Long representation of the given string, written using the given
   * radix.
   * @param {string} str The textual representation of the Long.
   * @param {number=} opt_radix The radix in which the text is written.
   * @return {!goog.math.Long} The corresponding Long value.
   */
  goog.math.Long.fromString = function(str, opt_radix) {
    if (str.length == 0) {
      throw Error('number format error: empty string');
    }
    var radix = opt_radix || 10;
    if (radix < 2 || 36 < radix) {
      throw Error('radix out of range: ' + radix);
    }
    if (str.charAt(0) == '-') {
      return goog.math.Long.fromString(str.substring(1), radix).negate();
    } else if (str.indexOf('-') >= 0) {
      throw Error('number format error: interior "-" character: ' + str);
    }
    // Do several (8) digits each time through the loop, so as to
    // minimize the calls to the very expensive emulated div.
    var radixToPower = goog.math.Long.fromNumber(Math.pow(radix, 8));
    var result = goog.math.Long.ZERO;
    for (var i = 0; i < str.length; i += 8) {
      var size = Math.min(8, str.length - i);
      var value = parseInt(str.substring(i, i + size), radix);
      if (size < 8) {
        var power = goog.math.Long.fromNumber(Math.pow(radix, size));
        result = result.multiply(power).add(goog.math.Long.fromNumber(value));
      } else {
        result = result.multiply(radixToPower);
        result = result.add(goog.math.Long.fromNumber(value));
      }
    }
    return result;
  };
  // NOTE: the compiler should inline these constant values below and then remove
  // these variables, so there should be no runtime penalty for these.
  /**
   * Number used repeated below in calculations.  This must appear before the
   * first call to any from* function below.
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_16_DBL_ = 1 << 16;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_24_DBL_ = 1 << 24;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_32_DBL_ =
      goog.math.Long.TWO_PWR_16_DBL_ * goog.math.Long.TWO_PWR_16_DBL_;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_31_DBL_ =
      goog.math.Long.TWO_PWR_32_DBL_ / 2;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_48_DBL_ =
      goog.math.Long.TWO_PWR_32_DBL_ * goog.math.Long.TWO_PWR_16_DBL_;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_64_DBL_ =
      goog.math.Long.TWO_PWR_32_DBL_ * goog.math.Long.TWO_PWR_32_DBL_;
  /**
   * @type {number}
   * @private
   */
  goog.math.Long.TWO_PWR_63_DBL_ =
      goog.math.Long.TWO_PWR_64_DBL_ / 2;
  /** @type {!goog.math.Long} */
  goog.math.Long.ZERO = goog.math.Long.fromInt(0);
  /** @type {!goog.math.Long} */
  goog.math.Long.ONE = goog.math.Long.fromInt(1);
  /** @type {!goog.math.Long} */
  goog.math.Long.NEG_ONE = goog.math.Long.fromInt(-1);
  /** @type {!goog.math.Long} */
  goog.math.Long.MAX_VALUE =
      goog.math.Long.fromBits(0xFFFFFFFF | 0, 0x7FFFFFFF | 0);
  /** @type {!goog.math.Long} */
  goog.math.Long.MIN_VALUE = goog.math.Long.fromBits(0, 0x80000000 | 0);
  /**
   * @type {!goog.math.Long}
   * @private
   */
  goog.math.Long.TWO_PWR_24_ = goog.math.Long.fromInt(1 << 24);
  /** @return {number} The value, assuming it is a 32-bit integer. */
  goog.math.Long.prototype.toInt = function() {
    return this.low_;
  };
  /** @return {number} The closest floating-point representation to this value. */
  goog.math.Long.prototype.toNumber = function() {
    return this.high_ * goog.math.Long.TWO_PWR_32_DBL_ +
           this.getLowBitsUnsigned();
  };
  /**
   * @param {number=} opt_radix The radix in which the text should be written.
   * @return {string} The textual representation of this value.
   */
  goog.math.Long.prototype.toString = function(opt_radix) {
    var radix = opt_radix || 10;
    if (radix < 2 || 36 < radix) {
      throw Error('radix out of range: ' + radix);
    }
    if (this.isZero()) {
      return '0';
    }
    if (this.isNegative()) {
      if (this.equals(goog.math.Long.MIN_VALUE)) {
        // We need to change the Long value before it can be negated, so we remove
        // the bottom-most digit in this base and then recurse to do the rest.
        var radixLong = goog.math.Long.fromNumber(radix);
        var div = this.div(radixLong);
        var rem = div.multiply(radixLong).subtract(this);
        return div.toString(radix) + rem.toInt().toString(radix);
      } else {
        return '-' + this.negate().toString(radix);
      }
    }
    // Do several (6) digits each time through the loop, so as to
    // minimize the calls to the very expensive emulated div.
    var radixToPower = goog.math.Long.fromNumber(Math.pow(radix, 6));
    var rem = this;
    var result = '';
    while (true) {
      var remDiv = rem.div(radixToPower);
      var intval = rem.subtract(remDiv.multiply(radixToPower)).toInt();
      var digits = intval.toString(radix);
      rem = remDiv;
      if (rem.isZero()) {
        return digits + result;
      } else {
        while (digits.length < 6) {
          digits = '0' + digits;
        }
        result = '' + digits + result;
      }
    }
  };
  /** @return {number} The high 32-bits as a signed value. */
  goog.math.Long.prototype.getHighBits = function() {
    return this.high_;
  };
  /** @return {number} The low 32-bits as a signed value. */
  goog.math.Long.prototype.getLowBits = function() {
    return this.low_;
  };
  /** @return {number} The low 32-bits as an unsigned value. */
  goog.math.Long.prototype.getLowBitsUnsigned = function() {
    return (this.low_ >= 0) ?
        this.low_ : goog.math.Long.TWO_PWR_32_DBL_ + this.low_;
  };
  /**
   * @return {number} Returns the number of bits needed to represent the absolute
   *     value of this Long.
   */
  goog.math.Long.prototype.getNumBitsAbs = function() {
    if (this.isNegative()) {
      if (this.equals(goog.math.Long.MIN_VALUE)) {
        return 64;
      } else {
        return this.negate().getNumBitsAbs();
      }
    } else {
      var val = this.high_ != 0 ? this.high_ : this.low_;
      for (var bit = 31; bit > 0; bit--) {
        if ((val & (1 << bit)) != 0) {
          break;
        }
      }
      return this.high_ != 0 ? bit + 33 : bit + 1;
    }
  };
  /** @return {boolean} Whether this value is zero. */
  goog.math.Long.prototype.isZero = function() {
    return this.high_ == 0 && this.low_ == 0;
  };
  /** @return {boolean} Whether this value is negative. */
  goog.math.Long.prototype.isNegative = function() {
    return this.high_ < 0;
  };
  /** @return {boolean} Whether this value is odd. */
  goog.math.Long.prototype.isOdd = function() {
    return (this.low_ & 1) == 1;
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long equals the other.
   */
  goog.math.Long.prototype.equals = function(other) {
    return (this.high_ == other.high_) && (this.low_ == other.low_);
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long does not equal the other.
   */
  goog.math.Long.prototype.notEquals = function(other) {
    return (this.high_ != other.high_) || (this.low_ != other.low_);
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long is less than the other.
   */
  goog.math.Long.prototype.lessThan = function(other) {
    return this.compare(other) < 0;
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long is less than or equal to the other.
   */
  goog.math.Long.prototype.lessThanOrEqual = function(other) {
    return this.compare(other) <= 0;
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long is greater than the other.
   */
  goog.math.Long.prototype.greaterThan = function(other) {
    return this.compare(other) > 0;
  };
  /**
   * @param {goog.math.Long} other Long to compare against.
   * @return {boolean} Whether this Long is greater than or equal to the other.
   */
  goog.math.Long.prototype.greaterThanOrEqual = function(other) {
    return this.compare(other) >= 0;
  };
  /**
   * Compares this Long with the given one.
   * @param {goog.math.Long} other Long to compare against.
   * @return {number} 0 if they are the same, 1 if the this is greater, and -1
   *     if the given one is greater.
   */
  goog.math.Long.prototype.compare = function(other) {
    if (this.equals(other)) {
      return 0;
    }
    var thisNeg = this.isNegative();
    var otherNeg = other.isNegative();
    if (thisNeg && !otherNeg) {
      return -1;
    }
    if (!thisNeg && otherNeg) {
      return 1;
    }
    // at this point, the signs are the same, so subtraction will not overflow
    if (this.subtract(other).isNegative()) {
      return -1;
    } else {
      return 1;
    }
  };
  /** @return {!goog.math.Long} The negation of this value. */
  goog.math.Long.prototype.negate = function() {
    if (this.equals(goog.math.Long.MIN_VALUE)) {
      return goog.math.Long.MIN_VALUE;
    } else {
      return this.not().add(goog.math.Long.ONE);
    }
  };
  /**
   * Returns the sum of this and the given Long.
   * @param {goog.math.Long} other Long to add to this one.
   * @return {!goog.math.Long} The sum of this and the given Long.
   */
  goog.math.Long.prototype.add = function(other) {
    // Divide each number into 4 chunks of 16 bits, and then sum the chunks.
    var a48 = this.high_ >>> 16;
    var a32 = this.high_ & 0xFFFF;
    var a16 = this.low_ >>> 16;
    var a00 = this.low_ & 0xFFFF;
    var b48 = other.high_ >>> 16;
    var b32 = other.high_ & 0xFFFF;
    var b16 = other.low_ >>> 16;
    var b00 = other.low_ & 0xFFFF;
    var c48 = 0, c32 = 0, c16 = 0, c00 = 0;
    c00 += a00 + b00;
    c16 += c00 >>> 16;
    c00 &= 0xFFFF;
    c16 += a16 + b16;
    c32 += c16 >>> 16;
    c16 &= 0xFFFF;
    c32 += a32 + b32;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c48 += a48 + b48;
    c48 &= 0xFFFF;
    return goog.math.Long.fromBits((c16 << 16) | c00, (c48 << 16) | c32);
  };
  /**
   * Returns the difference of this and the given Long.
   * @param {goog.math.Long} other Long to subtract from this.
   * @return {!goog.math.Long} The difference of this and the given Long.
   */
  goog.math.Long.prototype.subtract = function(other) {
    return this.add(other.negate());
  };
  /**
   * Returns the product of this and the given long.
   * @param {goog.math.Long} other Long to multiply with this.
   * @return {!goog.math.Long} The product of this and the other.
   */
  goog.math.Long.prototype.multiply = function(other) {
    if (this.isZero()) {
      return goog.math.Long.ZERO;
    } else if (other.isZero()) {
      return goog.math.Long.ZERO;
    }
    if (this.equals(goog.math.Long.MIN_VALUE)) {
      return other.isOdd() ? goog.math.Long.MIN_VALUE : goog.math.Long.ZERO;
    } else if (other.equals(goog.math.Long.MIN_VALUE)) {
      return this.isOdd() ? goog.math.Long.MIN_VALUE : goog.math.Long.ZERO;
    }
    if (this.isNegative()) {
      if (other.isNegative()) {
        return this.negate().multiply(other.negate());
      } else {
        return this.negate().multiply(other).negate();
      }
    } else if (other.isNegative()) {
      return this.multiply(other.negate()).negate();
    }
    // If both longs are small, use float multiplication
    if (this.lessThan(goog.math.Long.TWO_PWR_24_) &&
        other.lessThan(goog.math.Long.TWO_PWR_24_)) {
      return goog.math.Long.fromNumber(this.toNumber() * other.toNumber());
    }
    // Divide each long into 4 chunks of 16 bits, and then add up 4x4 products.
    // We can skip products that would overflow.
    var a48 = this.high_ >>> 16;
    var a32 = this.high_ & 0xFFFF;
    var a16 = this.low_ >>> 16;
    var a00 = this.low_ & 0xFFFF;
    var b48 = other.high_ >>> 16;
    var b32 = other.high_ & 0xFFFF;
    var b16 = other.low_ >>> 16;
    var b00 = other.low_ & 0xFFFF;
    var c48 = 0, c32 = 0, c16 = 0, c00 = 0;
    c00 += a00 * b00;
    c16 += c00 >>> 16;
    c00 &= 0xFFFF;
    c16 += a16 * b00;
    c32 += c16 >>> 16;
    c16 &= 0xFFFF;
    c16 += a00 * b16;
    c32 += c16 >>> 16;
    c16 &= 0xFFFF;
    c32 += a32 * b00;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c32 += a16 * b16;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c32 += a00 * b32;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c48 += a48 * b00 + a32 * b16 + a16 * b32 + a00 * b48;
    c48 &= 0xFFFF;
    return goog.math.Long.fromBits((c16 << 16) | c00, (c48 << 16) | c32);
  };
  /**
   * Returns this Long divided by the given one.
   * @param {goog.math.Long} other Long by which to divide.
   * @return {!goog.math.Long} This Long divided by the given one.
   */
  goog.math.Long.prototype.div = function(other) {
    if (other.isZero()) {
      throw Error('division by zero');
    } else if (this.isZero()) {
      return goog.math.Long.ZERO;
    }
    if (this.equals(goog.math.Long.MIN_VALUE)) {
      if (other.equals(goog.math.Long.ONE) ||
          other.equals(goog.math.Long.NEG_ONE)) {
        return goog.math.Long.MIN_VALUE;  // recall that -MIN_VALUE == MIN_VALUE
      } else if (other.equals(goog.math.Long.MIN_VALUE)) {
        return goog.math.Long.ONE;
      } else {
        // At this point, we have |other| >= 2, so |this/other| < |MIN_VALUE|.
        var halfThis = this.shiftRight(1);
        var approx = halfThis.div(other).shiftLeft(1);
        if (approx.equals(goog.math.Long.ZERO)) {
          return other.isNegative() ? goog.math.Long.ONE : goog.math.Long.NEG_ONE;
        } else {
          var rem = this.subtract(other.multiply(approx));
          var result = approx.add(rem.div(other));
          return result;
        }
      }
    } else if (other.equals(goog.math.Long.MIN_VALUE)) {
      return goog.math.Long.ZERO;
    }
    if (this.isNegative()) {
      if (other.isNegative()) {
        return this.negate().div(other.negate());
      } else {
        return this.negate().div(other).negate();
      }
    } else if (other.isNegative()) {
      return this.div(other.negate()).negate();
    }
    // Repeat the following until the remainder is less than other:  find a
    // floating-point that approximates remainder / other *from below*, add this
    // into the result, and subtract it from the remainder.  It is critical that
    // the approximate value is less than or equal to the real value so that the
    // remainder never becomes negative.
    var res = goog.math.Long.ZERO;
    var rem = this;
    while (rem.greaterThanOrEqual(other)) {
      // Approximate the result of division. This may be a little greater or
      // smaller than the actual value.
      var approx = Math.max(1, Math.floor(rem.toNumber() / other.toNumber()));
      // We will tweak the approximate result by changing it in the 48-th digit or
      // the smallest non-fractional digit, whichever is larger.
      var log2 = Math.ceil(Math.log(approx) / Math.LN2);
      var delta = (log2 <= 48) ? 1 : Math.pow(2, log2 - 48);
      // Decrease the approximation until it is smaller than the remainder.  Note
      // that if it is too large, the product overflows and is negative.
      var approxRes = goog.math.Long.fromNumber(approx);
      var approxRem = approxRes.multiply(other);
      while (approxRem.isNegative() || approxRem.greaterThan(rem)) {
        approx -= delta;
        approxRes = goog.math.Long.fromNumber(approx);
        approxRem = approxRes.multiply(other);
      }
      // We know the answer can't be zero... and actually, zero would cause
      // infinite recursion since we would make no progress.
      if (approxRes.isZero()) {
        approxRes = goog.math.Long.ONE;
      }
      res = res.add(approxRes);
      rem = rem.subtract(approxRem);
    }
    return res;
  };
  /**
   * Returns this Long modulo the given one.
   * @param {goog.math.Long} other Long by which to mod.
   * @return {!goog.math.Long} This Long modulo the given one.
   */
  goog.math.Long.prototype.modulo = function(other) {
    return this.subtract(this.div(other).multiply(other));
  };
  /** @return {!goog.math.Long} The bitwise-NOT of this value. */
  goog.math.Long.prototype.not = function() {
    return goog.math.Long.fromBits(~this.low_, ~this.high_);
  };
  /**
   * Returns the bitwise-AND of this Long and the given one.
   * @param {goog.math.Long} other The Long with which to AND.
   * @return {!goog.math.Long} The bitwise-AND of this and the other.
   */
  goog.math.Long.prototype.and = function(other) {
    return goog.math.Long.fromBits(this.low_ & other.low_,
                                   this.high_ & other.high_);
  };
  /**
   * Returns the bitwise-OR of this Long and the given one.
   * @param {goog.math.Long} other The Long with which to OR.
   * @return {!goog.math.Long} The bitwise-OR of this and the other.
   */
  goog.math.Long.prototype.or = function(other) {
    return goog.math.Long.fromBits(this.low_ | other.low_,
                                   this.high_ | other.high_);
  };
  /**
   * Returns the bitwise-XOR of this Long and the given one.
   * @param {goog.math.Long} other The Long with which to XOR.
   * @return {!goog.math.Long} The bitwise-XOR of this and the other.
   */
  goog.math.Long.prototype.xor = function(other) {
    return goog.math.Long.fromBits(this.low_ ^ other.low_,
                                   this.high_ ^ other.high_);
  };
  /**
   * Returns this Long with bits shifted to the left by the given amount.
   * @param {number} numBits The number of bits by which to shift.
   * @return {!goog.math.Long} This shifted to the left by the given amount.
   */
  goog.math.Long.prototype.shiftLeft = function(numBits) {
    numBits &= 63;
    if (numBits == 0) {
      return this;
    } else {
      var low = this.low_;
      if (numBits < 32) {
        var high = this.high_;
        return goog.math.Long.fromBits(
            low << numBits,
            (high << numBits) | (low >>> (32 - numBits)));
      } else {
        return goog.math.Long.fromBits(0, low << (numBits - 32));
      }
    }
  };
  /**
   * Returns this Long with bits shifted to the right by the given amount.
   * @param {number} numBits The number of bits by which to shift.
   * @return {!goog.math.Long} This shifted to the right by the given amount.
   */
  goog.math.Long.prototype.shiftRight = function(numBits) {
    numBits &= 63;
    if (numBits == 0) {
      return this;
    } else {
      var high = this.high_;
      if (numBits < 32) {
        var low = this.low_;
        return goog.math.Long.fromBits(
            (low >>> numBits) | (high << (32 - numBits)),
            high >> numBits);
      } else {
        return goog.math.Long.fromBits(
            high >> (numBits - 32),
            high >= 0 ? 0 : -1);
      }
    }
  };
  /**
   * Returns this Long with bits shifted to the right by the given amount, with
   * the new top bits matching the current sign bit.
   * @param {number} numBits The number of bits by which to shift.
   * @return {!goog.math.Long} This shifted to the right by the given amount, with
   *     zeros placed into the new leading bits.
   */
  goog.math.Long.prototype.shiftRightUnsigned = function(numBits) {
    numBits &= 63;
    if (numBits == 0) {
      return this;
    } else {
      var high = this.high_;
      if (numBits < 32) {
        var low = this.low_;
        return goog.math.Long.fromBits(
            (low >>> numBits) | (high << (32 - numBits)),
            high >>> numBits);
      } else if (numBits == 32) {
        return goog.math.Long.fromBits(high, 0);
      } else {
        return goog.math.Long.fromBits(high >>> (numBits - 32), 0);
      }
    }
  };
  //======= begin jsbn =======
  var navigator = { appName: 'Modern Browser' }; // polyfill a little
  // Copyright (c) 2005  Tom Wu
  // All Rights Reserved.
  // http://www-cs-students.stanford.edu/~tjw/jsbn/
  /*
   * Copyright (c) 2003-2005  Tom Wu
   * All Rights Reserved.
   *
   * Permission is hereby granted, free of charge, to any person obtaining
   * a copy of this software and associated documentation files (the
   * "Software"), to deal in the Software without restriction, including
   * without limitation the rights to use, copy, modify, merge, publish,
   * distribute, sublicense, and/or sell copies of the Software, and to
   * permit persons to whom the Software is furnished to do so, subject to
   * the following conditions:
   *
   * The above copyright notice and this permission notice shall be
   * included in all copies or substantial portions of the Software.
   *
   * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
   * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
   * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
   *
   * IN NO EVENT SHALL TOM WU BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
   * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
   * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF
   * THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT
   * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
   *
   * In addition, the following condition applies:
   *
   * All redistributions must retain an intact copy of this copyright notice
   * and disclaimer.
   */
  // Basic JavaScript BN library - subset useful for RSA encryption.
  // Bits per digit
  var dbits;
  // JavaScript engine analysis
  var canary = 0xdeadbeefcafe;
  var j_lm = ((canary&0xffffff)==0xefcafe);
  // (public) Constructor
  function BigInteger(a,b,c) {
    if(a != null)
      if("number" == typeof a) this.fromNumber(a,b,c);
      else if(b == null && "string" != typeof a) this.fromString(a,256);
      else this.fromString(a,b);
  }
  // return new, unset BigInteger
  function nbi() { return new BigInteger(null); }
  // am: Compute w_j += (x*this_i), propagate carries,
  // c is initial carry, returns final carry.
  // c < 3*dvalue, x < 2*dvalue, this_i < dvalue
  // We need to select the fastest one that works in this environment.
  // am1: use a single mult and divide to get the high bits,
  // max digit bits should be 26 because
  // max internal value = 2*dvalue^2-2*dvalue (< 2^53)
  function am1(i,x,w,j,c,n) {
    while(--n >= 0) {
      var v = x*this[i++]+w[j]+c;
      c = Math.floor(v/0x4000000);
      w[j++] = v&0x3ffffff;
    }
    return c;
  }
  // am2 avoids a big mult-and-extract completely.
  // Max digit bits should be <= 30 because we do bitwise ops
  // on values up to 2*hdvalue^2-hdvalue-1 (< 2^31)
  function am2(i,x,w,j,c,n) {
    var xl = x&0x7fff, xh = x>>15;
    while(--n >= 0) {
      var l = this[i]&0x7fff;
      var h = this[i++]>>15;
      var m = xh*l+h*xl;
      l = xl*l+((m&0x7fff)<<15)+w[j]+(c&0x3fffffff);
      c = (l>>>30)+(m>>>15)+xh*h+(c>>>30);
      w[j++] = l&0x3fffffff;
    }
    return c;
  }
  // Alternately, set max digit bits to 28 since some
  // browsers slow down when dealing with 32-bit numbers.
  function am3(i,x,w,j,c,n) {
    var xl = x&0x3fff, xh = x>>14;
    while(--n >= 0) {
      var l = this[i]&0x3fff;
      var h = this[i++]>>14;
      var m = xh*l+h*xl;
      l = xl*l+((m&0x3fff)<<14)+w[j]+c;
      c = (l>>28)+(m>>14)+xh*h;
      w[j++] = l&0xfffffff;
    }
    return c;
  }
  if(j_lm && (navigator.appName == "Microsoft Internet Explorer")) {
    BigInteger.prototype.am = am2;
    dbits = 30;
  }
  else if(j_lm && (navigator.appName != "Netscape")) {
    BigInteger.prototype.am = am1;
    dbits = 26;
  }
  else { // Mozilla/Netscape seems to prefer am3
    BigInteger.prototype.am = am3;
    dbits = 28;
  }
  BigInteger.prototype.DB = dbits;
  BigInteger.prototype.DM = ((1<<dbits)-1);
  BigInteger.prototype.DV = (1<<dbits);
  var BI_FP = 52;
  BigInteger.prototype.FV = Math.pow(2,BI_FP);
  BigInteger.prototype.F1 = BI_FP-dbits;
  BigInteger.prototype.F2 = 2*dbits-BI_FP;
  // Digit conversions
  var BI_RM = "0123456789abcdefghijklmnopqrstuvwxyz";
  var BI_RC = new Array();
  var rr,vv;
  rr = "0".charCodeAt(0);
  for(vv = 0; vv <= 9; ++vv) BI_RC[rr++] = vv;
  rr = "a".charCodeAt(0);
  for(vv = 10; vv < 36; ++vv) BI_RC[rr++] = vv;
  rr = "A".charCodeAt(0);
  for(vv = 10; vv < 36; ++vv) BI_RC[rr++] = vv;
  function int2char(n) { return BI_RM.charAt(n); }
  function intAt(s,i) {
    var c = BI_RC[s.charCodeAt(i)];
    return (c==null)?-1:c;
  }
  // (protected) copy this to r
  function bnpCopyTo(r) {
    for(var i = this.t-1; i >= 0; --i) r[i] = this[i];
    r.t = this.t;
    r.s = this.s;
  }
  // (protected) set from integer value x, -DV <= x < DV
  function bnpFromInt(x) {
    this.t = 1;
    this.s = (x<0)?-1:0;
    if(x > 0) this[0] = x;
    else if(x < -1) this[0] = x+DV;
    else this.t = 0;
  }
  // return bigint initialized to value
  function nbv(i) { var r = nbi(); r.fromInt(i); return r; }
  // (protected) set from string and radix
  function bnpFromString(s,b) {
    var k;
    if(b == 16) k = 4;
    else if(b == 8) k = 3;
    else if(b == 256) k = 8; // byte array
    else if(b == 2) k = 1;
    else if(b == 32) k = 5;
    else if(b == 4) k = 2;
    else { this.fromRadix(s,b); return; }
    this.t = 0;
    this.s = 0;
    var i = s.length, mi = false, sh = 0;
    while(--i >= 0) {
      var x = (k==8)?s[i]&0xff:intAt(s,i);
      if(x < 0) {
        if(s.charAt(i) == "-") mi = true;
        continue;
      }
      mi = false;
      if(sh == 0)
        this[this.t++] = x;
      else if(sh+k > this.DB) {
        this[this.t-1] |= (x&((1<<(this.DB-sh))-1))<<sh;
        this[this.t++] = (x>>(this.DB-sh));
      }
      else
        this[this.t-1] |= x<<sh;
      sh += k;
      if(sh >= this.DB) sh -= this.DB;
    }
    if(k == 8 && (s[0]&0x80) != 0) {
      this.s = -1;
      if(sh > 0) this[this.t-1] |= ((1<<(this.DB-sh))-1)<<sh;
    }
    this.clamp();
    if(mi) BigInteger.ZERO.subTo(this,this);
  }
  // (protected) clamp off excess high words
  function bnpClamp() {
    var c = this.s&this.DM;
    while(this.t > 0 && this[this.t-1] == c) --this.t;
  }
  // (public) return string representation in given radix
  function bnToString(b) {
    if(this.s < 0) return "-"+this.negate().toString(b);
    var k;
    if(b == 16) k = 4;
    else if(b == 8) k = 3;
    else if(b == 2) k = 1;
    else if(b == 32) k = 5;
    else if(b == 4) k = 2;
    else return this.toRadix(b);
    var km = (1<<k)-1, d, m = false, r = "", i = this.t;
    var p = this.DB-(i*this.DB)%k;
    if(i-- > 0) {
      if(p < this.DB && (d = this[i]>>p) > 0) { m = true; r = int2char(d); }
      while(i >= 0) {
        if(p < k) {
          d = (this[i]&((1<<p)-1))<<(k-p);
          d |= this[--i]>>(p+=this.DB-k);
        }
        else {
          d = (this[i]>>(p-=k))&km;
          if(p <= 0) { p += this.DB; --i; }
        }
        if(d > 0) m = true;
        if(m) r += int2char(d);
      }
    }
    return m?r:"0";
  }
  // (public) -this
  function bnNegate() { var r = nbi(); BigInteger.ZERO.subTo(this,r); return r; }
  // (public) |this|
  function bnAbs() { return (this.s<0)?this.negate():this; }
  // (public) return + if this > a, - if this < a, 0 if equal
  function bnCompareTo(a) {
    var r = this.s-a.s;
    if(r != 0) return r;
    var i = this.t;
    r = i-a.t;
    if(r != 0) return (this.s<0)?-r:r;
    while(--i >= 0) if((r=this[i]-a[i]) != 0) return r;
    return 0;
  }
  // returns bit length of the integer x
  function nbits(x) {
    var r = 1, t;
    if((t=x>>>16) != 0) { x = t; r += 16; }
    if((t=x>>8) != 0) { x = t; r += 8; }
    if((t=x>>4) != 0) { x = t; r += 4; }
    if((t=x>>2) != 0) { x = t; r += 2; }
    if((t=x>>1) != 0) { x = t; r += 1; }
    return r;
  }
  // (public) return the number of bits in "this"
  function bnBitLength() {
    if(this.t <= 0) return 0;
    return this.DB*(this.t-1)+nbits(this[this.t-1]^(this.s&this.DM));
  }
  // (protected) r = this << n*DB
  function bnpDLShiftTo(n,r) {
    var i;
    for(i = this.t-1; i >= 0; --i) r[i+n] = this[i];
    for(i = n-1; i >= 0; --i) r[i] = 0;
    r.t = this.t+n;
    r.s = this.s;
  }
  // (protected) r = this >> n*DB
  function bnpDRShiftTo(n,r) {
    for(var i = n; i < this.t; ++i) r[i-n] = this[i];
    r.t = Math.max(this.t-n,0);
    r.s = this.s;
  }
  // (protected) r = this << n
  function bnpLShiftTo(n,r) {
    var bs = n%this.DB;
    var cbs = this.DB-bs;
    var bm = (1<<cbs)-1;
    var ds = Math.floor(n/this.DB), c = (this.s<<bs)&this.DM, i;
    for(i = this.t-1; i >= 0; --i) {
      r[i+ds+1] = (this[i]>>cbs)|c;
      c = (this[i]&bm)<<bs;
    }
    for(i = ds-1; i >= 0; --i) r[i] = 0;
    r[ds] = c;
    r.t = this.t+ds+1;
    r.s = this.s;
    r.clamp();
  }
  // (protected) r = this >> n
  function bnpRShiftTo(n,r) {
    r.s = this.s;
    var ds = Math.floor(n/this.DB);
    if(ds >= this.t) { r.t = 0; return; }
    var bs = n%this.DB;
    var cbs = this.DB-bs;
    var bm = (1<<bs)-1;
    r[0] = this[ds]>>bs;
    for(var i = ds+1; i < this.t; ++i) {
      r[i-ds-1] |= (this[i]&bm)<<cbs;
      r[i-ds] = this[i]>>bs;
    }
    if(bs > 0) r[this.t-ds-1] |= (this.s&bm)<<cbs;
    r.t = this.t-ds;
    r.clamp();
  }
  // (protected) r = this - a
  function bnpSubTo(a,r) {
    var i = 0, c = 0, m = Math.min(a.t,this.t);
    while(i < m) {
      c += this[i]-a[i];
      r[i++] = c&this.DM;
      c >>= this.DB;
    }
    if(a.t < this.t) {
      c -= a.s;
      while(i < this.t) {
        c += this[i];
        r[i++] = c&this.DM;
        c >>= this.DB;
      }
      c += this.s;
    }
    else {
      c += this.s;
      while(i < a.t) {
        c -= a[i];
        r[i++] = c&this.DM;
        c >>= this.DB;
      }
      c -= a.s;
    }
    r.s = (c<0)?-1:0;
    if(c < -1) r[i++] = this.DV+c;
    else if(c > 0) r[i++] = c;
    r.t = i;
    r.clamp();
  }
  // (protected) r = this * a, r != this,a (HAC 14.12)
  // "this" should be the larger one if appropriate.
  function bnpMultiplyTo(a,r) {
    var x = this.abs(), y = a.abs();
    var i = x.t;
    r.t = i+y.t;
    while(--i >= 0) r[i] = 0;
    for(i = 0; i < y.t; ++i) r[i+x.t] = x.am(0,y[i],r,i,0,x.t);
    r.s = 0;
    r.clamp();
    if(this.s != a.s) BigInteger.ZERO.subTo(r,r);
  }
  // (protected) r = this^2, r != this (HAC 14.16)
  function bnpSquareTo(r) {
    var x = this.abs();
    var i = r.t = 2*x.t;
    while(--i >= 0) r[i] = 0;
    for(i = 0; i < x.t-1; ++i) {
      var c = x.am(i,x[i],r,2*i,0,1);
      if((r[i+x.t]+=x.am(i+1,2*x[i],r,2*i+1,c,x.t-i-1)) >= x.DV) {
        r[i+x.t] -= x.DV;
        r[i+x.t+1] = 1;
      }
    }
    if(r.t > 0) r[r.t-1] += x.am(i,x[i],r,2*i,0,1);
    r.s = 0;
    r.clamp();
  }
  // (protected) divide this by m, quotient and remainder to q, r (HAC 14.20)
  // r != q, this != m.  q or r may be null.
  function bnpDivRemTo(m,q,r) {
    var pm = m.abs();
    if(pm.t <= 0) return;
    var pt = this.abs();
    if(pt.t < pm.t) {
      if(q != null) q.fromInt(0);
      if(r != null) this.copyTo(r);
      return;
    }
    if(r == null) r = nbi();
    var y = nbi(), ts = this.s, ms = m.s;
    var nsh = this.DB-nbits(pm[pm.t-1]);	// normalize modulus
    if(nsh > 0) { pm.lShiftTo(nsh,y); pt.lShiftTo(nsh,r); }
    else { pm.copyTo(y); pt.copyTo(r); }
    var ys = y.t;
    var y0 = y[ys-1];
    if(y0 == 0) return;
    var yt = y0*(1<<this.F1)+((ys>1)?y[ys-2]>>this.F2:0);
    var d1 = this.FV/yt, d2 = (1<<this.F1)/yt, e = 1<<this.F2;
    var i = r.t, j = i-ys, t = (q==null)?nbi():q;
    y.dlShiftTo(j,t);
    if(r.compareTo(t) >= 0) {
      r[r.t++] = 1;
      r.subTo(t,r);
    }
    BigInteger.ONE.dlShiftTo(ys,t);
    t.subTo(y,y);	// "negative" y so we can replace sub with am later
    while(y.t < ys) y[y.t++] = 0;
    while(--j >= 0) {
      // Estimate quotient digit
      var qd = (r[--i]==y0)?this.DM:Math.floor(r[i]*d1+(r[i-1]+e)*d2);
      if((r[i]+=y.am(0,qd,r,j,0,ys)) < qd) {	// Try it out
        y.dlShiftTo(j,t);
        r.subTo(t,r);
        while(r[i] < --qd) r.subTo(t,r);
      }
    }
    if(q != null) {
      r.drShiftTo(ys,q);
      if(ts != ms) BigInteger.ZERO.subTo(q,q);
    }
    r.t = ys;
    r.clamp();
    if(nsh > 0) r.rShiftTo(nsh,r);	// Denormalize remainder
    if(ts < 0) BigInteger.ZERO.subTo(r,r);
  }
  // (public) this mod a
  function bnMod(a) {
    var r = nbi();
    this.abs().divRemTo(a,null,r);
    if(this.s < 0 && r.compareTo(BigInteger.ZERO) > 0) a.subTo(r,r);
    return r;
  }
  // Modular reduction using "classic" algorithm
  function Classic(m) { this.m = m; }
  function cConvert(x) {
    if(x.s < 0 || x.compareTo(this.m) >= 0) return x.mod(this.m);
    else return x;
  }
  function cRevert(x) { return x; }
  function cReduce(x) { x.divRemTo(this.m,null,x); }
  function cMulTo(x,y,r) { x.multiplyTo(y,r); this.reduce(r); }
  function cSqrTo(x,r) { x.squareTo(r); this.reduce(r); }
  Classic.prototype.convert = cConvert;
  Classic.prototype.revert = cRevert;
  Classic.prototype.reduce = cReduce;
  Classic.prototype.mulTo = cMulTo;
  Classic.prototype.sqrTo = cSqrTo;
  // (protected) return "-1/this % 2^DB"; useful for Mont. reduction
  // justification:
  //         xy == 1 (mod m)
  //         xy =  1+km
  //   xy(2-xy) = (1+km)(1-km)
  // x[y(2-xy)] = 1-k^2m^2
  // x[y(2-xy)] == 1 (mod m^2)
  // if y is 1/x mod m, then y(2-xy) is 1/x mod m^2
  // should reduce x and y(2-xy) by m^2 at each step to keep size bounded.
  // JS multiply "overflows" differently from C/C++, so care is needed here.
  function bnpInvDigit() {
    if(this.t < 1) return 0;
    var x = this[0];
    if((x&1) == 0) return 0;
    var y = x&3;		// y == 1/x mod 2^2
    y = (y*(2-(x&0xf)*y))&0xf;	// y == 1/x mod 2^4
    y = (y*(2-(x&0xff)*y))&0xff;	// y == 1/x mod 2^8
    y = (y*(2-(((x&0xffff)*y)&0xffff)))&0xffff;	// y == 1/x mod 2^16
    // last step - calculate inverse mod DV directly;
    // assumes 16 < DB <= 32 and assumes ability to handle 48-bit ints
    y = (y*(2-x*y%this.DV))%this.DV;		// y == 1/x mod 2^dbits
    // we really want the negative inverse, and -DV < y < DV
    return (y>0)?this.DV-y:-y;
  }
  // Montgomery reduction
  function Montgomery(m) {
    this.m = m;
    this.mp = m.invDigit();
    this.mpl = this.mp&0x7fff;
    this.mph = this.mp>>15;
    this.um = (1<<(m.DB-15))-1;
    this.mt2 = 2*m.t;
  }
  // xR mod m
  function montConvert(x) {
    var r = nbi();
    x.abs().dlShiftTo(this.m.t,r);
    r.divRemTo(this.m,null,r);
    if(x.s < 0 && r.compareTo(BigInteger.ZERO) > 0) this.m.subTo(r,r);
    return r;
  }
  // x/R mod m
  function montRevert(x) {
    var r = nbi();
    x.copyTo(r);
    this.reduce(r);
    return r;
  }
  // x = x/R mod m (HAC 14.32)
  function montReduce(x) {
    while(x.t <= this.mt2)	// pad x so am has enough room later
      x[x.t++] = 0;
    for(var i = 0; i < this.m.t; ++i) {
      // faster way of calculating u0 = x[i]*mp mod DV
      var j = x[i]&0x7fff;
      var u0 = (j*this.mpl+(((j*this.mph+(x[i]>>15)*this.mpl)&this.um)<<15))&x.DM;
      // use am to combine the multiply-shift-add into one call
      j = i+this.m.t;
      x[j] += this.m.am(0,u0,x,i,0,this.m.t);
      // propagate carry
      while(x[j] >= x.DV) { x[j] -= x.DV; x[++j]++; }
    }
    x.clamp();
    x.drShiftTo(this.m.t,x);
    if(x.compareTo(this.m) >= 0) x.subTo(this.m,x);
  }
  // r = "x^2/R mod m"; x != r
  function montSqrTo(x,r) { x.squareTo(r); this.reduce(r); }
  // r = "xy/R mod m"; x,y != r
  function montMulTo(x,y,r) { x.multiplyTo(y,r); this.reduce(r); }
  Montgomery.prototype.convert = montConvert;
  Montgomery.prototype.revert = montRevert;
  Montgomery.prototype.reduce = montReduce;
  Montgomery.prototype.mulTo = montMulTo;
  Montgomery.prototype.sqrTo = montSqrTo;
  // (protected) true iff this is even
  function bnpIsEven() { return ((this.t>0)?(this[0]&1):this.s) == 0; }
  // (protected) this^e, e < 2^32, doing sqr and mul with "r" (HAC 14.79)
  function bnpExp(e,z) {
    if(e > 0xffffffff || e < 1) return BigInteger.ONE;
    var r = nbi(), r2 = nbi(), g = z.convert(this), i = nbits(e)-1;
    g.copyTo(r);
    while(--i >= 0) {
      z.sqrTo(r,r2);
      if((e&(1<<i)) > 0) z.mulTo(r2,g,r);
      else { var t = r; r = r2; r2 = t; }
    }
    return z.revert(r);
  }
  // (public) this^e % m, 0 <= e < 2^32
  function bnModPowInt(e,m) {
    var z;
    if(e < 256 || m.isEven()) z = new Classic(m); else z = new Montgomery(m);
    return this.exp(e,z);
  }
  // protected
  BigInteger.prototype.copyTo = bnpCopyTo;
  BigInteger.prototype.fromInt = bnpFromInt;
  BigInteger.prototype.fromString = bnpFromString;
  BigInteger.prototype.clamp = bnpClamp;
  BigInteger.prototype.dlShiftTo = bnpDLShiftTo;
  BigInteger.prototype.drShiftTo = bnpDRShiftTo;
  BigInteger.prototype.lShiftTo = bnpLShiftTo;
  BigInteger.prototype.rShiftTo = bnpRShiftTo;
  BigInteger.prototype.subTo = bnpSubTo;
  BigInteger.prototype.multiplyTo = bnpMultiplyTo;
  BigInteger.prototype.squareTo = bnpSquareTo;
  BigInteger.prototype.divRemTo = bnpDivRemTo;
  BigInteger.prototype.invDigit = bnpInvDigit;
  BigInteger.prototype.isEven = bnpIsEven;
  BigInteger.prototype.exp = bnpExp;
  // public
  BigInteger.prototype.toString = bnToString;
  BigInteger.prototype.negate = bnNegate;
  BigInteger.prototype.abs = bnAbs;
  BigInteger.prototype.compareTo = bnCompareTo;
  BigInteger.prototype.bitLength = bnBitLength;
  BigInteger.prototype.mod = bnMod;
  BigInteger.prototype.modPowInt = bnModPowInt;
  // "constants"
  BigInteger.ZERO = nbv(0);
  BigInteger.ONE = nbv(1);
  // jsbn2 stuff
  // (protected) convert from radix string
  function bnpFromRadix(s,b) {
    this.fromInt(0);
    if(b == null) b = 10;
    var cs = this.chunkSize(b);
    var d = Math.pow(b,cs), mi = false, j = 0, w = 0;
    for(var i = 0; i < s.length; ++i) {
      var x = intAt(s,i);
      if(x < 0) {
        if(s.charAt(i) == "-" && this.signum() == 0) mi = true;
        continue;
      }
      w = b*w+x;
      if(++j >= cs) {
        this.dMultiply(d);
        this.dAddOffset(w,0);
        j = 0;
        w = 0;
      }
    }
    if(j > 0) {
      this.dMultiply(Math.pow(b,j));
      this.dAddOffset(w,0);
    }
    if(mi) BigInteger.ZERO.subTo(this,this);
  }
  // (protected) return x s.t. r^x < DV
  function bnpChunkSize(r) { return Math.floor(Math.LN2*this.DB/Math.log(r)); }
  // (public) 0 if this == 0, 1 if this > 0
  function bnSigNum() {
    if(this.s < 0) return -1;
    else if(this.t <= 0 || (this.t == 1 && this[0] <= 0)) return 0;
    else return 1;
  }
  // (protected) this *= n, this >= 0, 1 < n < DV
  function bnpDMultiply(n) {
    this[this.t] = this.am(0,n-1,this,0,0,this.t);
    ++this.t;
    this.clamp();
  }
  // (protected) this += n << w words, this >= 0
  function bnpDAddOffset(n,w) {
    if(n == 0) return;
    while(this.t <= w) this[this.t++] = 0;
    this[w] += n;
    while(this[w] >= this.DV) {
      this[w] -= this.DV;
      if(++w >= this.t) this[this.t++] = 0;
      ++this[w];
    }
  }
  // (protected) convert to radix string
  function bnpToRadix(b) {
    if(b == null) b = 10;
    if(this.signum() == 0 || b < 2 || b > 36) return "0";
    var cs = this.chunkSize(b);
    var a = Math.pow(b,cs);
    var d = nbv(a), y = nbi(), z = nbi(), r = "";
    this.divRemTo(d,y,z);
    while(y.signum() > 0) {
      r = (a+z.intValue()).toString(b).substr(1) + r;
      y.divRemTo(d,y,z);
    }
    return z.intValue().toString(b) + r;
  }
  // (public) return value as integer
  function bnIntValue() {
    if(this.s < 0) {
      if(this.t == 1) return this[0]-this.DV;
      else if(this.t == 0) return -1;
    }
    else if(this.t == 1) return this[0];
    else if(this.t == 0) return 0;
    // assumes 16 < DB < 32
    return ((this[1]&((1<<(32-this.DB))-1))<<this.DB)|this[0];
  }
  // (protected) r = this + a
  function bnpAddTo(a,r) {
    var i = 0, c = 0, m = Math.min(a.t,this.t);
    while(i < m) {
      c += this[i]+a[i];
      r[i++] = c&this.DM;
      c >>= this.DB;
    }
    if(a.t < this.t) {
      c += a.s;
      while(i < this.t) {
        c += this[i];
        r[i++] = c&this.DM;
        c >>= this.DB;
      }
      c += this.s;
    }
    else {
      c += this.s;
      while(i < a.t) {
        c += a[i];
        r[i++] = c&this.DM;
        c >>= this.DB;
      }
      c += a.s;
    }
    r.s = (c<0)?-1:0;
    if(c > 0) r[i++] = c;
    else if(c < -1) r[i++] = this.DV+c;
    r.t = i;
    r.clamp();
  }
  BigInteger.prototype.fromRadix = bnpFromRadix;
  BigInteger.prototype.chunkSize = bnpChunkSize;
  BigInteger.prototype.signum = bnSigNum;
  BigInteger.prototype.dMultiply = bnpDMultiply;
  BigInteger.prototype.dAddOffset = bnpDAddOffset;
  BigInteger.prototype.toRadix = bnpToRadix;
  BigInteger.prototype.intValue = bnIntValue;
  BigInteger.prototype.addTo = bnpAddTo;
  //======= end jsbn =======
  // Emscripten wrapper
  var Wrapper = {
    abs: function(l, h) {
      var x = new goog.math.Long(l, h);
      var ret;
      if (x.isNegative()) {
        ret = x.negate();
      } else {
        ret = x;
      }
      HEAP32[tempDoublePtr>>2] = ret.low_;
      HEAP32[tempDoublePtr+4>>2] = ret.high_;
    },
    ensureTemps: function() {
      if (Wrapper.ensuredTemps) return;
      Wrapper.ensuredTemps = true;
      Wrapper.two32 = new BigInteger();
      Wrapper.two32.fromString('4294967296', 10);
      Wrapper.two64 = new BigInteger();
      Wrapper.two64.fromString('18446744073709551616', 10);
      Wrapper.temp1 = new BigInteger();
      Wrapper.temp2 = new BigInteger();
    },
    lh2bignum: function(l, h) {
      var a = new BigInteger();
      a.fromString(h.toString(), 10);
      var b = new BigInteger();
      a.multiplyTo(Wrapper.two32, b);
      var c = new BigInteger();
      c.fromString(l.toString(), 10);
      var d = new BigInteger();
      c.addTo(b, d);
      return d;
    },
    stringify: function(l, h, unsigned) {
      var ret = new goog.math.Long(l, h).toString();
      if (unsigned && ret[0] == '-') {
        // unsign slowly using jsbn bignums
        Wrapper.ensureTemps();
        var bignum = new BigInteger();
        bignum.fromString(ret, 10);
        ret = new BigInteger();
        Wrapper.two64.addTo(bignum, ret);
        ret = ret.toString(10);
      }
      return ret;
    },
    fromString: function(str, base, min, max, unsigned) {
      Wrapper.ensureTemps();
      var bignum = new BigInteger();
      bignum.fromString(str, base);
      var bigmin = new BigInteger();
      bigmin.fromString(min, 10);
      var bigmax = new BigInteger();
      bigmax.fromString(max, 10);
      if (unsigned && bignum.compareTo(BigInteger.ZERO) < 0) {
        var temp = new BigInteger();
        bignum.addTo(Wrapper.two64, temp);
        bignum = temp;
      }
      var error = false;
      if (bignum.compareTo(bigmin) < 0) {
        bignum = bigmin;
        error = true;
      } else if (bignum.compareTo(bigmax) > 0) {
        bignum = bigmax;
        error = true;
      }
      var ret = goog.math.Long.fromString(bignum.toString()); // min-max checks should have clamped this to a range goog.math.Long can handle well
      HEAP32[tempDoublePtr>>2] = ret.low_;
      HEAP32[tempDoublePtr+4>>2] = ret.high_;
      if (error) throw 'range error';
    }
  };
  return Wrapper;
})();
//======= end closure i64 code =======
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
    //Module.printErr('run() called, but dependencies remain, so not running');
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
run();
// {{POST_RUN_ADDITIONS}}
// {{MODULE_ADDITIONS}}

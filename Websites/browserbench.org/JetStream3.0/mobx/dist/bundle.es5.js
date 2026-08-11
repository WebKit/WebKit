!function(){var __webpack_require__={d:function(exports,definition){for(var key in definition)__webpack_require__.o(definition,key)&&!__webpack_require__.o(exports,key)&&Object.defineProperty(exports,key,{enumerable:!0,get:definition[key]})}};__webpack_require__.g=function(){if("object"==typeof globalThis)return globalThis;try{return this||new Function("return this")()}catch(e){if("object"==typeof window)return window}}(),__webpack_require__.o=function(obj,prop){return Object.prototype.hasOwnProperty.call(obj,prop)},__webpack_require__.r=function(exports){"undefined"!=typeof Symbol&&Symbol.toStringTag&&Object.defineProperty(exports,Symbol.toStringTag,{value:"Module"}),Object.defineProperty(exports,"__esModule",{value:!0})};var __webpack_exports__={};!function(){"use strict";__webpack_require__.r(__webpack_exports__),__webpack_require__.d(__webpack_exports__,{runTest:function(){return runTest}});function die(error){
/*ThouShaltNotCache*/
for(var _len=arguments.length,args=new Array(_len>1?_len-1:0),_key=1;_key<_len;_key++)args[_key-1]=arguments[_key];throw new Error("number"==typeof error?"[MobX] minified error nr: "+error+(args.length?" "+args.map(String).join(","):"")+". Find the full error at: https://github.com/mobxjs/mobx/blob/main/packages/mobx/src/errors.ts":"[MobX] "+error)}var mockGlobal={};function getGlobal(){
/*ThouShaltNotCache*/
return"undefined"!=typeof globalThis?globalThis:"undefined"!=typeof window?window:void 0!==__webpack_require__.g?__webpack_require__.g:"undefined"!=typeof self?self:mockGlobal}var mobx_esm_assign=Object.assign,getDescriptor=Object.getOwnPropertyDescriptor,defineProperty=Object.defineProperty,objectPrototype=Object.prototype,EMPTY_ARRAY=[];Object.freeze(EMPTY_ARRAY);var EMPTY_OBJECT={};Object.freeze(EMPTY_OBJECT);var hasProxy="undefined"!=typeof Proxy,plainObjectString=Object.toString();function assertProxies(){
/*ThouShaltNotCache*/
hasProxy||die("Proxy not available")}function once(func){
/*ThouShaltNotCache*/
var invoked=!1;return function(){
/*ThouShaltNotCache*/
if(!invoked)return invoked=!0,func.apply(this,arguments)}}var noop=function(){};function isFunction(fn){
/*ThouShaltNotCache*/
return"function"==typeof fn}function isStringish(value){switch(typeof value){case"string":case"symbol":case"number":return!0}return!1}function isObject(value){
/*ThouShaltNotCache*/
return null!==value&&"object"==typeof value}function isPlainObject(value){
/*ThouShaltNotCache*/
if(!isObject(value))return!1;var proto=Object.getPrototypeOf(value);if(null==proto)return!0;var protoConstructor=Object.hasOwnProperty.call(proto,"constructor")&&proto.constructor;return"function"==typeof protoConstructor&&protoConstructor.toString()===plainObjectString}function isGenerator(obj){
/*ThouShaltNotCache*/
var constructor=null==obj?void 0:obj.constructor;return!!constructor&&("GeneratorFunction"===constructor.name||"GeneratorFunction"===constructor.displayName)}function addHiddenProp(object,propName,value){
/*ThouShaltNotCache*/
defineProperty(object,propName,{enumerable:!1,writable:!0,configurable:!0,value:value})}function addHiddenFinalProp(object,propName,value){
/*ThouShaltNotCache*/
defineProperty(object,propName,{enumerable:!1,writable:!1,configurable:!0,value:value})}function createInstanceofPredicate(name,theClass){
/*ThouShaltNotCache*/
var propName="isMobX"+name;return theClass.prototype[propName]=!0,function(x){
/*ThouShaltNotCache*/
return isObject(x)&&!0===x[propName]}}function isES6Map(thing){
/*ThouShaltNotCache*/
return null!=thing&&"[object Map]"===Object.prototype.toString.call(thing)}function isES6Set(thing){
/*ThouShaltNotCache*/
return null!=thing&&"[object Set]"===Object.prototype.toString.call(thing)}var hasGetOwnPropertySymbols=void 0!==Object.getOwnPropertySymbols;var ownKeys="undefined"!=typeof Reflect&&Reflect.ownKeys?Reflect.ownKeys:hasGetOwnPropertySymbols?function(obj){
/*ThouShaltNotCache*/
return Object.getOwnPropertyNames(obj).concat(Object.getOwnPropertySymbols(obj))}:Object.getOwnPropertyNames;function toPrimitive(value){
/*ThouShaltNotCache*/
return null===value?null:"object"==typeof value?""+value:value}function hasProp(target,prop){
/*ThouShaltNotCache*/
return objectPrototype.hasOwnProperty.call(target,prop)}var getOwnPropertyDescriptors=Object.getOwnPropertyDescriptors||function(target){
/*ThouShaltNotCache*/
var res={};return ownKeys(target).forEach(function(key){
/*ThouShaltNotCache*/
res[key]=getDescriptor(target,key)}),res};function getFlag(flags,mask){
/*ThouShaltNotCache*/
return!!(flags&mask)}function setFlag(flags,mask,newValue){
/*ThouShaltNotCache*/
return newValue?flags|=mask:flags&=~mask,flags}function _arrayLikeToArray(r,a){
/*ThouShaltNotCache*/
(null==a||a>r.length)&&(a=r.length);for(var e=0,n=Array(a);e<a;e++)n[e]=r[e];return n}function _defineProperties(e,r){
/*ThouShaltNotCache*/
for(var t=0;t<r.length;t++){var o=r[t];o.enumerable=o.enumerable||!1,o.configurable=!0,"value"in o&&(o.writable=!0),Object.defineProperty(e,_toPropertyKey(o.key),o)}}function _createClass(e,r,t){
/*ThouShaltNotCache*/
return r&&_defineProperties(e.prototype,r),t&&_defineProperties(e,t),Object.defineProperty(e,"prototype",{writable:!1}),e}function _createForOfIteratorHelperLoose(r,e){
/*ThouShaltNotCache*/
var t="undefined"!=typeof Symbol&&r[Symbol.iterator]||r["@@iterator"];if(t)return(t=t.call(r)).next.bind(t);if(Array.isArray(r)||(t=function(r,a){
/*ThouShaltNotCache*/
if(r){if("string"==typeof r)return _arrayLikeToArray(r,a);var t={}.toString.call(r).slice(8,-1);return"Object"===t&&r.constructor&&(t=r.constructor.name),"Map"===t||"Set"===t?Array.from(r):"Arguments"===t||/^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t)?_arrayLikeToArray(r,a):void 0}}(r))||e&&r&&"number"==typeof r.length){t&&(r=t);var o=0;return function(){
/*ThouShaltNotCache*/
return o>=r.length?{done:!0}:{done:!1,value:r[o++]}}}throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.")}function _extends(){
/*ThouShaltNotCache*/
return _extends=Object.assign?Object.assign.bind():function(n){
/*ThouShaltNotCache*/
for(var e=1;e<arguments.length;e++){var t=arguments[e];for(var r in t)({}).hasOwnProperty.call(t,r)&&(n[r]=t[r])}return n},_extends.apply(null,arguments)}function _inheritsLoose(t,o){
/*ThouShaltNotCache*/
t.prototype=Object.create(o.prototype),t.prototype.constructor=t,_setPrototypeOf(t,o)}function _setPrototypeOf(t,e){
/*ThouShaltNotCache*/
return _setPrototypeOf=Object.setPrototypeOf?Object.setPrototypeOf.bind():function(t,e){
/*ThouShaltNotCache*/
return t.__proto__=e,t},_setPrototypeOf(t,e)}function _toPropertyKey(t){
/*ThouShaltNotCache*/
var i=function(t,r){
/*ThouShaltNotCache*/
if("object"!=typeof t||!t)return t;var e=t[Symbol.toPrimitive];if(void 0!==e){var i=e.call(t,r||"default");if("object"!=typeof i)return i;throw new TypeError("@@toPrimitive must return a primitive value.")}return("string"===r?String:Number)(t)}(t,"string");return"symbol"==typeof i?i:i+""}var storedAnnotationsSymbol=Symbol("mobx-stored-annotations");function createDecoratorAnnotation(annotation){return Object.assign(
/*ThouShaltNotCache*/
function(target,property){
/*ThouShaltNotCache*/
if(is20223Decorator(property))return annotation.decorate_20223_(target,property);storeAnnotation(target,property,annotation)},annotation)}function storeAnnotation(prototype,key,annotation){
/*ThouShaltNotCache*/
hasProp(prototype,storedAnnotationsSymbol)||addHiddenProp(prototype,storedAnnotationsSymbol,_extends({},prototype[storedAnnotationsSymbol])),function(annotation){
/*ThouShaltNotCache*/
return annotation.annotationType_===OVERRIDE}(annotation)||(prototype[storedAnnotationsSymbol][key]=annotation)}function is20223Decorator(context){
/*ThouShaltNotCache*/
return"object"==typeof context&&"string"==typeof context.kind}var $mobx=Symbol("mobx administration"),Atom=function(){
/*ThouShaltNotCache*/
function Atom(name_){
/*ThouShaltNotCache*/
void 0===name_&&(name_="Atom"),this.name_=void 0,this.flags_=0,this.observers_=new Set,this.lastAccessedBy_=0,this.lowestObserverState_=IDerivationState_.NOT_TRACKING_,this.onBOL=void 0,this.onBUOL=void 0,this.name_=name_}var _proto=Atom.prototype;return _proto.onBO=function(){
/*ThouShaltNotCache*/
this.onBOL&&this.onBOL.forEach(function(listener){
/*ThouShaltNotCache*/
return listener()})},_proto.onBUO=function(){
/*ThouShaltNotCache*/
this.onBUOL&&this.onBUOL.forEach(function(listener){
/*ThouShaltNotCache*/
return listener()})},_proto.reportObserved=function(){
/*ThouShaltNotCache*/
return reportObserved(this)},_proto.reportChanged=function(){
/*ThouShaltNotCache*/
startBatch(),propagateChanged(this),endBatch()},_proto.toString=function(){
/*ThouShaltNotCache*/
return this.name_},_createClass(Atom,[{key:"isBeingObserved",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Atom.isBeingObservedMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Atom.isBeingObservedMask_,newValue)}},{key:"isPendingUnobservation",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Atom.isPendingUnobservationMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Atom.isPendingUnobservationMask_,newValue)}},{key:"diffValue",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Atom.diffValueMask_)?1:0},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Atom.diffValueMask_,1===newValue)}}])}();Atom.isBeingObservedMask_=1,Atom.isPendingUnobservationMask_=2,Atom.diffValueMask_=4;var isAtom=createInstanceofPredicate("Atom",Atom);function createAtom(name,onBecomeObservedHandler,onBecomeUnobservedHandler){
/*ThouShaltNotCache*/
void 0===onBecomeObservedHandler&&(onBecomeObservedHandler=noop),void 0===onBecomeUnobservedHandler&&(onBecomeUnobservedHandler=noop);var arg3,atom=new Atom(name);return onBecomeObservedHandler!==noop&&interceptHook(ON_BECOME_OBSERVED,atom,onBecomeObservedHandler,arg3),onBecomeUnobservedHandler!==noop&&onBecomeUnobserved(atom,onBecomeUnobservedHandler),atom}var comparer={identity:function(a,b){
/*ThouShaltNotCache*/
return a===b},structural:function(a,b){
/*ThouShaltNotCache*/
return deepEqual(a,b)},default:function(a,b){
/*ThouShaltNotCache*/
return Object.is?Object.is(a,b):a===b?0!==a||1/a==1/b:a!=a&&b!=b},shallow:function(a,b){
/*ThouShaltNotCache*/
return deepEqual(a,b,1)}};function deepEnhancer(v,_,name){
/*ThouShaltNotCache*/
return isObservable(v)?v:Array.isArray(v)?observable.array(v,{name:name}):isPlainObject(v)?observable.object(v,void 0,{name:name}):isES6Map(v)?observable.map(v,{name:name}):isES6Set(v)?observable.set(v,{name:name}):"function"!=typeof v||isAction(v)||isFlow(v)?v:isGenerator(v)?flow(v):autoAction(name,v)}function referenceEnhancer(newValue){
/*ThouShaltNotCache*/
return newValue}var OVERRIDE="override";function createActionAnnotation(name,options){
/*ThouShaltNotCache*/
return{annotationType_:name,options_:options,make_:make_$1,extend_:extend_$1,decorate_20223_:decorate_20223_$1}}function make_$1(adm,key,descriptor,source){
/*ThouShaltNotCache*/
var _this$options_;if(null!=(_this$options_=this.options_)&&_this$options_.bound)return null===this.extend_(adm,key,descriptor,!1)?0:1;if(source===adm.target_)return null===this.extend_(adm,key,descriptor,!1)?0:2;if(isAction(descriptor.value))return 1;var actionDescriptor=createActionDescriptor(adm,this,key,descriptor,!1);return defineProperty(source,key,actionDescriptor),2}function extend_$1(adm,key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
var actionDescriptor=createActionDescriptor(adm,this,key,descriptor);return adm.defineProperty_(key,actionDescriptor,proxyTrap)}function decorate_20223_$1(mthd,context){var _this$options_2,kind=context.kind,name=context.name,addInitializer=context.addInitializer,ann=this,_createAction=function(m){
/*ThouShaltNotCache*/
var _ann$options_$name,_ann$options_,_ann$options_$autoAct,_ann$options_2;return createAction(null!=(_ann$options_$name=null==(_ann$options_=ann.options_)?void 0:_ann$options_.name)?_ann$options_$name:name.toString(),m,null!=(_ann$options_$autoAct=null==(_ann$options_2=ann.options_)?void 0:_ann$options_2.autoAction)&&_ann$options_$autoAct)};return"field"==kind?function(initMthd){
/*ThouShaltNotCache*/
var _ann$options_3,mthd=initMthd;return isAction(mthd)||(mthd=_createAction(mthd)),null!=(_ann$options_3=ann.options_)&&_ann$options_3.bound&&((mthd=mthd.bind(this)).isMobxAction=!0),mthd}:"method"==kind?(isAction(mthd)||(mthd=_createAction(mthd)),null!=(_this$options_2=this.options_)&&_this$options_2.bound&&addInitializer(function(){
/*ThouShaltNotCache*/
var bound=this[name].bind(this);bound.isMobxAction=!0,this[name]=bound}),mthd):void die("Cannot apply '"+ann.annotationType_+"' to '"+String(name)+"' (kind: "+kind+"):\n'"+ann.annotationType_+"' can only be used on properties with a function value.")}function createActionDescriptor(adm,annotation,key,descriptor,safeDescriptors){
/*ThouShaltNotCache*/
var _annotation$options_,_annotation$options_$,_annotation$options_2,_annotation$options_$2,_annotation$options_3,_annotation$options_4,_adm$proxy_2,_ref2;void 0===safeDescriptors&&(safeDescriptors=globalState.safeDescriptors),_ref2=descriptor,annotation.annotationType_,_ref2.value;var _adm$proxy_,value=descriptor.value;null!=(_annotation$options_=annotation.options_)&&_annotation$options_.bound&&(value=value.bind(null!=(_adm$proxy_=adm.proxy_)?_adm$proxy_:adm.target_));return{value:createAction(null!=(_annotation$options_$=null==(_annotation$options_2=annotation.options_)?void 0:_annotation$options_2.name)?_annotation$options_$:key.toString(),value,null!=(_annotation$options_$2=null==(_annotation$options_3=annotation.options_)?void 0:_annotation$options_3.autoAction)&&_annotation$options_$2,null!=(_annotation$options_4=annotation.options_)&&_annotation$options_4.bound?null!=(_adm$proxy_2=adm.proxy_)?_adm$proxy_2:adm.target_:void 0),configurable:!safeDescriptors||adm.isPlainObject_,enumerable:!1,writable:!safeDescriptors}}function createFlowAnnotation(name,options){
/*ThouShaltNotCache*/
return{annotationType_:name,options_:options,make_:make_$2,extend_:extend_$2,decorate_20223_:decorate_20223_$2}}function make_$2(adm,key,descriptor,source){
/*ThouShaltNotCache*/
var _this$options_;if(source===adm.target_)return null===this.extend_(adm,key,descriptor,!1)?0:2;if(null!=(_this$options_=this.options_)&&_this$options_.bound&&(!hasProp(adm.target_,key)||!isFlow(adm.target_[key]))&&null===this.extend_(adm,key,descriptor,!1))return 0;if(isFlow(descriptor.value))return 1;var flowDescriptor=createFlowDescriptor(adm,this,key,descriptor,!1,!1);return defineProperty(source,key,flowDescriptor),2}function extend_$2(adm,key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
var _this$options_2,flowDescriptor=createFlowDescriptor(adm,this,key,descriptor,null==(_this$options_2=this.options_)?void 0:_this$options_2.bound);return adm.defineProperty_(key,flowDescriptor,proxyTrap)}function decorate_20223_$2(mthd,context){
/*ThouShaltNotCache*/
var _this$options_3;var name=context.name,addInitializer=context.addInitializer;return isFlow(mthd)||(mthd=flow(mthd)),null!=(_this$options_3=this.options_)&&_this$options_3.bound&&addInitializer(function(){
/*ThouShaltNotCache*/
var bound=this[name].bind(this);bound.isMobXFlow=!0,this[name]=bound}),mthd}function createFlowDescriptor(adm,annotation,key,descriptor,bound,safeDescriptors){var _ref2;
/*ThouShaltNotCache*/
void 0===safeDescriptors&&(safeDescriptors=globalState.safeDescriptors),_ref2=descriptor,annotation.annotationType_,_ref2.value;var _adm$proxy_,value=descriptor.value;(isFlow(value)||(value=flow(value)),bound)&&((value=value.bind(null!=(_adm$proxy_=adm.proxy_)?_adm$proxy_:adm.target_)).isMobXFlow=!0);return{value:value,configurable:!safeDescriptors||adm.isPlainObject_,enumerable:!1,writable:!safeDescriptors}}function createComputedAnnotation(name,options){
/*ThouShaltNotCache*/
return{annotationType_:name,options_:options,make_:make_$3,extend_:extend_$3,decorate_20223_:decorate_20223_$3}}function make_$3(adm,key,descriptor){
/*ThouShaltNotCache*/
return null===this.extend_(adm,key,descriptor,!1)?0:1}function extend_$3(adm,key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
return function(adm,_ref,key,_ref2){
/*ThouShaltNotCache*/
_ref.annotationType_,_ref2.get;0}(0,this,0,descriptor),adm.defineComputedProperty_(key,_extends({},this.options_,{get:descriptor.get,set:descriptor.set}),proxyTrap)}function decorate_20223_$3(get,context){var ann=this,key=context.name;return(0,context.addInitializer)(function(){
/*ThouShaltNotCache*/
var adm=asObservableObject(this)[$mobx],options=_extends({},ann.options_,{get:get,context:this});options.name||(options.name="ObservableObject."+key.toString()),adm.values_.set(key,new ComputedValue(options))}),function(){
/*ThouShaltNotCache*/
return this[$mobx].getObservablePropValue_(key)}}function createObservableAnnotation(name,options){
/*ThouShaltNotCache*/
return{annotationType_:name,options_:options,make_:make_$4,extend_:extend_$4,decorate_20223_:decorate_20223_$4}}function make_$4(adm,key,descriptor){
/*ThouShaltNotCache*/
return null===this.extend_(adm,key,descriptor,!1)?0:1}function extend_$4(adm,key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
var _this$options_$enhanc,_this$options_;return function(adm,_ref){
/*ThouShaltNotCache*/
_ref.annotationType_;0}(0,this),adm.defineObservableProperty_(key,descriptor.value,null!=(_this$options_$enhanc=null==(_this$options_=this.options_)?void 0:_this$options_.enhancer)?_this$options_$enhanc:deepEnhancer,proxyTrap)}function decorate_20223_$4(desc,context){var ann=this,kind=context.kind,name=context.name,initializedObjects=new WeakSet;function initializeObservable(target,value){
/*ThouShaltNotCache*/
var _ann$options_$enhance,_ann$options_,adm=asObservableObject(target)[$mobx],observable=new ObservableValue(value,null!=(_ann$options_$enhance=null==(_ann$options_=ann.options_)?void 0:_ann$options_.enhancer)?_ann$options_$enhance:deepEnhancer,"ObservableObject."+name.toString(),!1);adm.values_.set(name,observable),initializedObjects.add(target)}if("accessor"==kind)return{get:function(){
/*ThouShaltNotCache*/
return initializedObjects.has(this)||initializeObservable(this,desc.get.call(this)),this[$mobx].getObservablePropValue_(name)},set:function(value){
/*ThouShaltNotCache*/
return initializedObjects.has(this)||initializeObservable(this,value),this[$mobx].setObservablePropValue_(name,value)},init:function(value){
/*ThouShaltNotCache*/
return initializedObjects.has(this)||initializeObservable(this,value),value}}}var AUTO="true",autoAnnotation=createAutoAnnotation();function createAutoAnnotation(options){
/*ThouShaltNotCache*/
return{annotationType_:AUTO,options_:options,make_:make_$5,extend_:extend_$5,decorate_20223_:decorate_20223_$5}}function make_$5(adm,key,descriptor,source){
/*ThouShaltNotCache*/
var _this$options_3,_this$options_4,_this$options_2,_this$options_;if(descriptor.get)return computed.make_(adm,key,descriptor,source);if(descriptor.set){var set=createAction(key.toString(),descriptor.set);return source===adm.target_?null===adm.defineProperty_(key,{configurable:!globalState.safeDescriptors||adm.isPlainObject_,set:set})?0:2:(defineProperty(source,key,{configurable:!0,set:set}),2)}if(source!==adm.target_&&"function"==typeof descriptor.value)return isGenerator(descriptor.value)?(null!=(_this$options_=this.options_)&&_this$options_.autoBind?flow.bound:flow).make_(adm,key,descriptor,source):(null!=(_this$options_2=this.options_)&&_this$options_2.autoBind?autoAction.bound:autoAction).make_(adm,key,descriptor,source);var _adm$proxy_,observableAnnotation=!1===(null==(_this$options_3=this.options_)?void 0:_this$options_3.deep)?observable.ref:observable;"function"==typeof descriptor.value&&null!=(_this$options_4=this.options_)&&_this$options_4.autoBind&&(descriptor.value=descriptor.value.bind(null!=(_adm$proxy_=adm.proxy_)?_adm$proxy_:adm.target_));return observableAnnotation.make_(adm,key,descriptor,source)}function extend_$5(adm,key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
var _this$options_5,_this$options_6,_adm$proxy_2;if(descriptor.get)return computed.extend_(adm,key,descriptor,proxyTrap);if(descriptor.set)return adm.defineProperty_(key,{configurable:!globalState.safeDescriptors||adm.isPlainObject_,set:createAction(key.toString(),descriptor.set)},proxyTrap);"function"==typeof descriptor.value&&null!=(_this$options_5=this.options_)&&_this$options_5.autoBind&&(descriptor.value=descriptor.value.bind(null!=(_adm$proxy_2=adm.proxy_)?_adm$proxy_2:adm.target_));return(!1===(null==(_this$options_6=this.options_)?void 0:_this$options_6.deep)?observable.ref:observable).extend_(adm,key,descriptor,proxyTrap)}function decorate_20223_$5(desc,context){
/*ThouShaltNotCache*/
die("'"+this.annotationType_+"' cannot be used as a decorator")}var defaultCreateObservableOptions={deep:!0,name:void 0,defaultDecorator:void 0,proxy:!0};function asCreateObservableOptions(thing){
/*ThouShaltNotCache*/
return thing||defaultCreateObservableOptions}Object.freeze(defaultCreateObservableOptions);var observableAnnotation=createObservableAnnotation("observable"),observableRefAnnotation=createObservableAnnotation("observable.ref",{enhancer:referenceEnhancer}),observableShallowAnnotation=createObservableAnnotation("observable.shallow",{enhancer:function(v,_,name){
/*ThouShaltNotCache*/
return null==v||isObservableObject(v)||isObservableArray(v)||isObservableMap(v)||isObservableSet(v)?v:Array.isArray(v)?observable.array(v,{name:name,deep:!1}):isPlainObject(v)?observable.object(v,void 0,{name:name,deep:!1}):isES6Map(v)?observable.map(v,{name:name,deep:!1}):isES6Set(v)?observable.set(v,{name:name,deep:!1}):void 0}}),observableStructAnnotation=createObservableAnnotation("observable.struct",{enhancer:function(v,oldValue){return deepEqual(v,oldValue)?oldValue:v}}),observableDecoratorAnnotation=createDecoratorAnnotation(observableAnnotation);function getEnhancerFromOptions(options){
/*ThouShaltNotCache*/
return!0===options.deep?deepEnhancer:!1===options.deep?referenceEnhancer:(annotation=options.defaultDecorator)&&null!=(_annotation$options_$=null==(_annotation$options_=annotation.options_)?void 0:_annotation$options_.enhancer)?_annotation$options_$:deepEnhancer;var annotation,_annotation$options_$,_annotation$options_}function createObservable(v,arg2,arg3){
/*ThouShaltNotCache*/
return is20223Decorator(arg2)?observableAnnotation.decorate_20223_(v,arg2):isStringish(arg2)?void storeAnnotation(v,arg2,observableAnnotation):isObservable(v)?v:isPlainObject(v)?observable.object(v,arg2,arg3):Array.isArray(v)?observable.array(v,arg2):isES6Map(v)?observable.map(v,arg2):isES6Set(v)?observable.set(v,arg2):"object"==typeof v&&null!==v?v:observable.box(v,arg2)}mobx_esm_assign(createObservable,observableDecoratorAnnotation);var _getDescriptor$config,_getDescriptor,observable=mobx_esm_assign(createObservable,{box:function(value,options){
/*ThouShaltNotCache*/
var o=asCreateObservableOptions(options);return new ObservableValue(value,getEnhancerFromOptions(o),o.name,!0,o.equals)},array:function(initialValues,options){
/*ThouShaltNotCache*/
var o=asCreateObservableOptions(options);return(!1===globalState.useProxies||!1===o.proxy?createLegacyArray:createObservableArray)(initialValues,getEnhancerFromOptions(o),o.name)},map:function(initialValues,options){
/*ThouShaltNotCache*/
var o=asCreateObservableOptions(options);return new ObservableMap(initialValues,getEnhancerFromOptions(o),o.name)},set:function(initialValues,options){
/*ThouShaltNotCache*/
var o=asCreateObservableOptions(options);return new ObservableSet(initialValues,getEnhancerFromOptions(o),o.name)},object:function(props,decorators,options){
/*ThouShaltNotCache*/
return initObservable(function(){
/*ThouShaltNotCache*/
return extendObservable(!1===globalState.useProxies||!1===(null==options?void 0:options.proxy)?asObservableObject({},options):function(target,options){
/*ThouShaltNotCache*/
var _target$$mobx,_target$$mobx$proxy_;return assertProxies(),target=asObservableObject(target,options),null!=(_target$$mobx$proxy_=(_target$$mobx=target[$mobx]).proxy_)?_target$$mobx$proxy_:_target$$mobx.proxy_=new Proxy(target,objectProxyTraps)}({},options),props,decorators)})},ref:createDecoratorAnnotation(observableRefAnnotation),shallow:createDecoratorAnnotation(observableShallowAnnotation),deep:observableDecoratorAnnotation,struct:createDecoratorAnnotation(observableStructAnnotation)}),computedAnnotation=createComputedAnnotation("computed"),computedStructAnnotation=createComputedAnnotation("computed.struct",{equals:comparer.structural}),computed=function(arg1,arg2){
/*ThouShaltNotCache*/
if(is20223Decorator(arg2))return computedAnnotation.decorate_20223_(arg1,arg2);if(isStringish(arg2))return storeAnnotation(arg1,arg2,computedAnnotation);if(isPlainObject(arg1))return createDecoratorAnnotation(createComputedAnnotation("computed",arg1));var opts=isPlainObject(arg2)?arg2:{};return opts.get=arg1,opts.name||(opts.name=arg1.name||""),new ComputedValue(opts)};Object.assign(computed,computedAnnotation),computed.struct=createDecoratorAnnotation(computedStructAnnotation);var currentActionId=0,nextActionId=1,isFunctionNameConfigurable=null!=(_getDescriptor$config=null==(_getDescriptor=getDescriptor(function(){},"name"))?void 0:_getDescriptor.configurable)&&_getDescriptor$config,tmpNameDescriptor={value:"action",configurable:!0,writable:!1,enumerable:!1};function createAction(actionName,fn,autoAction,ref){function res(){
/*ThouShaltNotCache*/
return executeAction(actionName,autoAction,fn,ref||this,arguments)}
/*ThouShaltNotCache*/
return void 0===autoAction&&(autoAction=!1),res.isMobxAction=!0,res.toString=function(){
/*ThouShaltNotCache*/
return fn.toString()},isFunctionNameConfigurable&&(tmpNameDescriptor.value=actionName,defineProperty(res,"name",tmpNameDescriptor)),res}function executeAction(actionName,canRunAsDerivation,fn,scope,args){
/*ThouShaltNotCache*/
var runInfo=function(actionName,canRunAsDerivation){
/*ThouShaltNotCache*/
var notifySpy_=!1,startTime_=0;0;var prevDerivation_=globalState.trackingDerivation,runAsAction=!canRunAsDerivation||!prevDerivation_;startBatch();var prevAllowStateChanges_=globalState.allowStateChanges;runAsAction&&(untrackedStart(),prevAllowStateChanges_=allowStateChangesStart(!0));var prevAllowStateReads_=allowStateReadsStart(!0),runInfo={runAsAction_:runAsAction,prevDerivation_:prevDerivation_,prevAllowStateChanges_:prevAllowStateChanges_,prevAllowStateReads_:prevAllowStateReads_,notifySpy_:notifySpy_,startTime_:startTime_,actionId_:nextActionId++,parentActionId_:currentActionId};return currentActionId=runInfo.actionId_,runInfo}(0,canRunAsDerivation);try{return fn.apply(scope,args)}catch(err){throw runInfo.error_=err,err}finally{!function(runInfo){
/*ThouShaltNotCache*/
currentActionId!==runInfo.actionId_&&die(30);currentActionId=runInfo.parentActionId_,void 0!==runInfo.error_&&(globalState.suppressReactionErrors=!0);allowStateChangesEnd(runInfo.prevAllowStateChanges_),allowStateReadsEnd(runInfo.prevAllowStateReads_),endBatch(),runInfo.runAsAction_&&untrackedEnd(runInfo.prevDerivation_);0;globalState.suppressReactionErrors=!1}(runInfo)}}function allowStateChanges(allowStateChanges,func){
/*ThouShaltNotCache*/
var prev=allowStateChangesStart(allowStateChanges);try{return func()}finally{allowStateChangesEnd(prev)}}function allowStateChangesStart(allowStateChanges){
/*ThouShaltNotCache*/
var prev=globalState.allowStateChanges;return globalState.allowStateChanges=allowStateChanges,prev}function allowStateChangesEnd(prev){
/*ThouShaltNotCache*/
globalState.allowStateChanges=prev}var ObservableValue=function(_Atom){
/*ThouShaltNotCache*/
function ObservableValue(value,enhancer,name_,notifySpy,equals){
/*ThouShaltNotCache*/
var _this;return void 0===name_&&(name_="ObservableValue"),void 0===notifySpy&&(notifySpy=!0),void 0===equals&&(equals=comparer.default),(_this=_Atom.call(this,name_)||this).enhancer=void 0,_this.name_=void 0,_this.equals=void 0,_this.hasUnreportedChange_=!1,_this.interceptors_=void 0,_this.changeListeners_=void 0,_this.value_=void 0,_this.dehancer=void 0,_this.enhancer=enhancer,_this.name_=name_,_this.equals=equals,_this.value_=enhancer(value,void 0,name_),_this}_inheritsLoose(ObservableValue,_Atom);var _proto=ObservableValue.prototype;return _proto.dehanceValue=function(value){
/*ThouShaltNotCache*/
return void 0!==this.dehancer?this.dehancer(value):value},_proto.set=function(newValue){
/*ThouShaltNotCache*/
this.value_;if((newValue=this.prepareNewValue_(newValue))!==globalState.UNCHANGED){0,this.setNewValue_(newValue)}},_proto.prepareNewValue_=function(newValue){if(
/*ThouShaltNotCache*/
checkIfStateModificationsAreAllowed(this),hasInterceptors(this)){var change=interceptChange(this,{object:this,type:UPDATE,newValue:newValue});if(!change)return globalState.UNCHANGED;newValue=change.newValue}return newValue=this.enhancer(newValue,this.value_,this.name_),this.equals(this.value_,newValue)?globalState.UNCHANGED:newValue},_proto.setNewValue_=function(newValue){
/*ThouShaltNotCache*/
var oldValue=this.value_;this.value_=newValue,this.reportChanged(),hasListeners(this)&&notifyListeners(this,{type:UPDATE,object:this,newValue:newValue,oldValue:oldValue})},_proto.get=function(){
/*ThouShaltNotCache*/
return this.reportObserved(),this.dehanceValue(this.value_)},_proto.intercept_=function(handler){
/*ThouShaltNotCache*/
return registerInterceptor(this,handler)},_proto.observe_=function(listener,fireImmediately){
/*ThouShaltNotCache*/
return fireImmediately&&listener({observableKind:"value",debugObjectName:this.name_,object:this,type:UPDATE,newValue:this.value_,oldValue:void 0}),registerListener(this,listener)},_proto.raw=function(){
/*ThouShaltNotCache*/
return this.value_},_proto.toJSON=function(){
/*ThouShaltNotCache*/
return this.get()},_proto.toString=function(){
/*ThouShaltNotCache*/
return this.name_+"["+this.value_+"]"},_proto.valueOf=function(){
/*ThouShaltNotCache*/
return toPrimitive(this.get())},_proto[Symbol.toPrimitive]=function(){
/*ThouShaltNotCache*/
return this.valueOf()},ObservableValue}(Atom),ComputedValue=function(){
/*ThouShaltNotCache*/
function ComputedValue(options){
/*ThouShaltNotCache*/
this.dependenciesState_=IDerivationState_.NOT_TRACKING_,this.observing_=[],this.newObserving_=null,this.observers_=new Set,this.runId_=0,this.lastAccessedBy_=0,this.lowestObserverState_=IDerivationState_.UP_TO_DATE_,this.unboundDepsCount_=0,this.value_=new CaughtException(null),this.name_=void 0,this.triggeredBy_=void 0,this.flags_=0,this.derivation=void 0,this.setter_=void 0,this.isTracing_=TraceMode.NONE,this.scope_=void 0,this.equals_=void 0,this.requiresReaction_=void 0,this.keepAlive_=void 0,this.onBOL=void 0,this.onBUOL=void 0,options.get||die(31),this.derivation=options.get,this.name_=options.name||"ComputedValue",options.set&&(this.setter_=createAction("ComputedValue-setter",options.set)),this.equals_=options.equals||(options.compareStructural||options.struct?comparer.structural:comparer.default),this.scope_=options.context,this.requiresReaction_=options.requiresReaction,this.keepAlive_=!!options.keepAlive}var _proto=ComputedValue.prototype;return _proto.onBecomeStale_=function(){
/*ThouShaltNotCache*/
!function(observable){
/*ThouShaltNotCache*/
if(observable.lowestObserverState_!==IDerivationState_.UP_TO_DATE_)return;observable.lowestObserverState_=IDerivationState_.POSSIBLY_STALE_,observable.observers_.forEach(function(d){
/*ThouShaltNotCache*/
d.dependenciesState_===IDerivationState_.UP_TO_DATE_&&(d.dependenciesState_=IDerivationState_.POSSIBLY_STALE_,d.onBecomeStale_())})}(this)},_proto.onBO=function(){
/*ThouShaltNotCache*/
this.onBOL&&this.onBOL.forEach(function(listener){
/*ThouShaltNotCache*/
return listener()})},_proto.onBUO=function(){
/*ThouShaltNotCache*/
this.onBUOL&&this.onBUOL.forEach(function(listener){
/*ThouShaltNotCache*/
return listener()})},_proto.get=function(){if(
/*ThouShaltNotCache*/
this.isComputing&&die(32,this.name_,this.derivation),0!==globalState.inBatch||0!==this.observers_.size||this.keepAlive_){if(reportObserved(this),shouldCompute(this)){var prevTrackingContext=globalState.trackingContext;this.keepAlive_&&!prevTrackingContext&&(globalState.trackingContext=this),this.trackAndCompute()&&function(observable){
/*ThouShaltNotCache*/
if(observable.lowestObserverState_===IDerivationState_.STALE_)return;observable.lowestObserverState_=IDerivationState_.STALE_,observable.observers_.forEach(function(d){
/*ThouShaltNotCache*/
d.dependenciesState_===IDerivationState_.POSSIBLY_STALE_?d.dependenciesState_=IDerivationState_.STALE_:d.dependenciesState_===IDerivationState_.UP_TO_DATE_&&(observable.lowestObserverState_=IDerivationState_.UP_TO_DATE_)})}(this),globalState.trackingContext=prevTrackingContext}}else shouldCompute(this)&&(this.warnAboutUntrackedRead_(),startBatch(),this.value_=this.computeValue_(!1),endBatch());var result=this.value_;if(isCaughtException(result))throw result.cause;return result},_proto.set=function(value){
/*ThouShaltNotCache*/
if(this.setter_){this.isRunningSetter&&die(33,this.name_),this.isRunningSetter=!0;try{this.setter_.call(this.scope_,value)}finally{this.isRunningSetter=!1}}else die(34,this.name_)},_proto.trackAndCompute=function(){
/*ThouShaltNotCache*/
var oldValue=this.value_,wasSuspended=this.dependenciesState_===IDerivationState_.NOT_TRACKING_,newValue=this.computeValue_(!0),changed=wasSuspended||isCaughtException(oldValue)||isCaughtException(newValue)||!this.equals_(oldValue,newValue);return changed&&(this.value_=newValue),changed},_proto.computeValue_=function(track){
/*ThouShaltNotCache*/
this.isComputing=!0;var res,prev=allowStateChangesStart(!1);if(track)res=trackDerivedFunction(this,this.derivation,this.scope_);else if(!0===globalState.disableErrorBoundaries)res=this.derivation.call(this.scope_);else try{res=this.derivation.call(this.scope_)}catch(e){res=new CaughtException(e)}return allowStateChangesEnd(prev),this.isComputing=!1,res},_proto.suspend_=function(){
/*ThouShaltNotCache*/
this.keepAlive_||(clearObserving(this),this.value_=void 0)},_proto.observe_=function(listener,fireImmediately){
/*ThouShaltNotCache*/
var _this=this,firstTime=!0,prevValue=void 0;return autorun(function(){
/*ThouShaltNotCache*/
var newValue=_this.get();if(!firstTime||fireImmediately){var prevU=untrackedStart();listener({observableKind:"computed",debugObjectName:_this.name_,type:UPDATE,object:_this,newValue:newValue,oldValue:prevValue}),untrackedEnd(prevU)}firstTime=!1,prevValue=newValue})},_proto.warnAboutUntrackedRead_=function(){},_proto.toString=function(){
/*ThouShaltNotCache*/
return this.name_+"["+this.derivation.toString()+"]"},_proto.valueOf=function(){
/*ThouShaltNotCache*/
return toPrimitive(this.get())},_proto[Symbol.toPrimitive]=function(){
/*ThouShaltNotCache*/
return this.valueOf()},_createClass(ComputedValue,[{key:"isComputing",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,ComputedValue.isComputingMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,ComputedValue.isComputingMask_,newValue)}},{key:"isRunningSetter",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,ComputedValue.isRunningSetterMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,ComputedValue.isRunningSetterMask_,newValue)}},{key:"isBeingObserved",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,ComputedValue.isBeingObservedMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,ComputedValue.isBeingObservedMask_,newValue)}},{key:"isPendingUnobservation",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,ComputedValue.isPendingUnobservationMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,ComputedValue.isPendingUnobservationMask_,newValue)}},{key:"diffValue",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,ComputedValue.diffValueMask_)?1:0},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,ComputedValue.diffValueMask_,1===newValue)}}])}();ComputedValue.isComputingMask_=1,ComputedValue.isRunningSetterMask_=2,ComputedValue.isBeingObservedMask_=4,ComputedValue.isPendingUnobservationMask_=8,ComputedValue.diffValueMask_=16;var IDerivationState_,TraceMode,isComputedValue=createInstanceofPredicate("ComputedValue",ComputedValue);!function(IDerivationState_){
/*ThouShaltNotCache*/
IDerivationState_[IDerivationState_.NOT_TRACKING_=-1]="NOT_TRACKING_",IDerivationState_[IDerivationState_.UP_TO_DATE_=0]="UP_TO_DATE_",IDerivationState_[IDerivationState_.POSSIBLY_STALE_=1]="POSSIBLY_STALE_",IDerivationState_[IDerivationState_.STALE_=2]="STALE_"}(IDerivationState_||(IDerivationState_={})),function(TraceMode){
/*ThouShaltNotCache*/
TraceMode[TraceMode.NONE=0]="NONE",TraceMode[TraceMode.LOG=1]="LOG",TraceMode[TraceMode.BREAK=2]="BREAK"}(TraceMode||(TraceMode={}));var CaughtException=function(cause){
/*ThouShaltNotCache*/
this.cause=void 0,this.cause=cause};function isCaughtException(e){
/*ThouShaltNotCache*/
return e instanceof CaughtException}function shouldCompute(derivation){
/*ThouShaltNotCache*/
switch(derivation.dependenciesState_){case IDerivationState_.UP_TO_DATE_:return!1;case IDerivationState_.NOT_TRACKING_:case IDerivationState_.STALE_:return!0;case IDerivationState_.POSSIBLY_STALE_:for(var prevAllowStateReads=allowStateReadsStart(!0),prevUntracked=untrackedStart(),obs=derivation.observing_,l=obs.length,i=0;i<l;i++){var obj=obs[i];if(isComputedValue(obj)){if(globalState.disableErrorBoundaries)obj.get();else try{obj.get()}catch(e){return untrackedEnd(prevUntracked),allowStateReadsEnd(prevAllowStateReads),!0}if(derivation.dependenciesState_===IDerivationState_.STALE_)return untrackedEnd(prevUntracked),allowStateReadsEnd(prevAllowStateReads),!0}}return changeDependenciesStateTo0(derivation),untrackedEnd(prevUntracked),allowStateReadsEnd(prevAllowStateReads),!1}}function checkIfStateModificationsAreAllowed(atom){}function trackDerivedFunction(derivation,f,context){
/*ThouShaltNotCache*/
var prevAllowStateReads=allowStateReadsStart(!0);changeDependenciesStateTo0(derivation),derivation.newObserving_=new Array(0===derivation.runId_?100:derivation.observing_.length),derivation.unboundDepsCount_=0,derivation.runId_=++globalState.runId;var result,prevTracking=globalState.trackingDerivation;if(globalState.trackingDerivation=derivation,globalState.inBatch++,!0===globalState.disableErrorBoundaries)result=f.call(context);else try{result=f.call(context)}catch(e){result=new CaughtException(e)}return globalState.inBatch--,globalState.trackingDerivation=prevTracking,function(derivation){for(
/*ThouShaltNotCache*/
var prevObserving=derivation.observing_,observing=derivation.observing_=derivation.newObserving_,lowestNewObservingDerivationState=IDerivationState_.UP_TO_DATE_,i0=0,l=derivation.unboundDepsCount_,i=0;i<l;i++){var dep=observing[i];0===dep.diffValue&&(dep.diffValue=1,i0!==i&&(observing[i0]=dep),i0++),dep.dependenciesState_>lowestNewObservingDerivationState&&(lowestNewObservingDerivationState=dep.dependenciesState_)}observing.length=i0,derivation.newObserving_=null,l=prevObserving.length;for(;l--;){var _dep=prevObserving[l];0===_dep.diffValue&&removeObserver(_dep,derivation),_dep.diffValue=0}for(;i0--;){var _dep2=observing[i0];1===_dep2.diffValue&&(_dep2.diffValue=0,addObserver(_dep2,derivation))}lowestNewObservingDerivationState!==IDerivationState_.UP_TO_DATE_&&(derivation.dependenciesState_=lowestNewObservingDerivationState,derivation.onBecomeStale_())}(derivation),allowStateReadsEnd(prevAllowStateReads),result}function clearObserving(derivation){
/*ThouShaltNotCache*/
var obs=derivation.observing_;derivation.observing_=[];for(var i=obs.length;i--;)removeObserver(obs[i],derivation);derivation.dependenciesState_=IDerivationState_.NOT_TRACKING_}function untracked(action){
/*ThouShaltNotCache*/
var prev=untrackedStart();try{return action()}finally{untrackedEnd(prev)}}function untrackedStart(){
/*ThouShaltNotCache*/
var prev=globalState.trackingDerivation;return globalState.trackingDerivation=null,prev}function untrackedEnd(prev){
/*ThouShaltNotCache*/
globalState.trackingDerivation=prev}function allowStateReadsStart(allowStateReads){
/*ThouShaltNotCache*/
var prev=globalState.allowStateReads;return globalState.allowStateReads=allowStateReads,prev}function allowStateReadsEnd(prev){
/*ThouShaltNotCache*/
globalState.allowStateReads=prev}function changeDependenciesStateTo0(derivation){
/*ThouShaltNotCache*/
if(derivation.dependenciesState_!==IDerivationState_.UP_TO_DATE_){derivation.dependenciesState_=IDerivationState_.UP_TO_DATE_;for(var obs=derivation.observing_,i=obs.length;i--;)obs[i].lowestObserverState_=IDerivationState_.UP_TO_DATE_}}var MobXGlobals=function(){
/*ThouShaltNotCache*/
this.version=6,this.UNCHANGED={},this.trackingDerivation=null,this.trackingContext=null,this.runId=0,this.mobxGuid=0,this.inBatch=0,this.pendingUnobservations=[],this.pendingReactions=[],this.isRunningReactions=!1,this.allowStateChanges=!1,this.allowStateReads=!0,this.enforceActions=!0,this.spyListeners=[],this.globalReactionErrorHandlers=[],this.computedRequiresReaction=!1,this.reactionRequiresObservable=!1,this.observableRequiresReaction=!1,this.disableErrorBoundaries=!1,this.suppressReactionErrors=!1,this.useProxies=!0,this.verifyProxies=!1,this.safeDescriptors=!0},canMergeGlobalState=!0,isolateCalled=!1,globalState=function(){
/*ThouShaltNotCache*/
var global=getGlobal();return global.__mobxInstanceCount>0&&!global.__mobxGlobals&&(canMergeGlobalState=!1),global.__mobxGlobals&&global.__mobxGlobals.version!==(new MobXGlobals).version&&(canMergeGlobalState=!1),canMergeGlobalState?global.__mobxGlobals?(global.__mobxInstanceCount+=1,global.__mobxGlobals.UNCHANGED||(global.__mobxGlobals.UNCHANGED={}),global.__mobxGlobals):(global.__mobxInstanceCount=1,global.__mobxGlobals=new MobXGlobals):(setTimeout(function(){
/*ThouShaltNotCache*/
isolateCalled||die(35)},1),new MobXGlobals)}();function addObserver(observable,node){
/*ThouShaltNotCache*/
observable.observers_.add(node),observable.lowestObserverState_>node.dependenciesState_&&(observable.lowestObserverState_=node.dependenciesState_)}function removeObserver(observable,node){
/*ThouShaltNotCache*/
observable.observers_.delete(node),0===observable.observers_.size&&queueForUnobservation(observable)}function queueForUnobservation(observable){
/*ThouShaltNotCache*/
!1===observable.isPendingUnobservation&&(observable.isPendingUnobservation=!0,globalState.pendingUnobservations.push(observable))}function startBatch(){
/*ThouShaltNotCache*/
globalState.inBatch++}function endBatch(){
/*ThouShaltNotCache*/
if(0===--globalState.inBatch){runReactions();for(var list=globalState.pendingUnobservations,i=0;i<list.length;i++){var observable=list[i];observable.isPendingUnobservation=!1,0===observable.observers_.size&&(observable.isBeingObserved&&(observable.isBeingObserved=!1,observable.onBUO()),observable instanceof ComputedValue&&observable.suspend_())}globalState.pendingUnobservations=[]}}function reportObserved(observable){var derivation=globalState.trackingDerivation;return null!==derivation?(derivation.runId_!==observable.lastAccessedBy_&&(observable.lastAccessedBy_=derivation.runId_,derivation.newObserving_[derivation.unboundDepsCount_++]=observable,!observable.isBeingObserved&&globalState.trackingContext&&(observable.isBeingObserved=!0,observable.onBO())),observable.isBeingObserved):(0===observable.observers_.size&&globalState.inBatch>0&&queueForUnobservation(observable),!1)}function propagateChanged(observable){
/*ThouShaltNotCache*/
observable.lowestObserverState_!==IDerivationState_.STALE_&&(observable.lowestObserverState_=IDerivationState_.STALE_,observable.observers_.forEach(function(d){
/*ThouShaltNotCache*/
d.dependenciesState_===IDerivationState_.UP_TO_DATE_&&d.onBecomeStale_(),d.dependenciesState_=IDerivationState_.STALE_}))}var Reaction=function(){
/*ThouShaltNotCache*/
function Reaction(name_,onInvalidate_,errorHandler_,requiresObservable_){
/*ThouShaltNotCache*/
void 0===name_&&(name_="Reaction"),this.name_=void 0,this.onInvalidate_=void 0,this.errorHandler_=void 0,this.requiresObservable_=void 0,this.observing_=[],this.newObserving_=[],this.dependenciesState_=IDerivationState_.NOT_TRACKING_,this.runId_=0,this.unboundDepsCount_=0,this.flags_=0,this.isTracing_=TraceMode.NONE,this.name_=name_,this.onInvalidate_=onInvalidate_,this.errorHandler_=errorHandler_,this.requiresObservable_=requiresObservable_}var _proto=Reaction.prototype;return _proto.onBecomeStale_=function(){
/*ThouShaltNotCache*/
this.schedule_()},_proto.schedule_=function(){
/*ThouShaltNotCache*/
this.isScheduled||(this.isScheduled=!0,globalState.pendingReactions.push(this),runReactions())},_proto.runReaction_=function(){
/*ThouShaltNotCache*/
if(!this.isDisposed){startBatch(),this.isScheduled=!1;var prev=globalState.trackingContext;if(globalState.trackingContext=this,shouldCompute(this)){this.isTrackPending=!0;try{this.onInvalidate_()}catch(e){this.reportExceptionInDerivation_(e)}}globalState.trackingContext=prev,endBatch()}},_proto.track=function(fn){
/*ThouShaltNotCache*/
if(!this.isDisposed){startBatch();0,this.isRunning=!0;var prevReaction=globalState.trackingContext;globalState.trackingContext=this;var result=trackDerivedFunction(this,fn,void 0);globalState.trackingContext=prevReaction,this.isRunning=!1,this.isTrackPending=!1,this.isDisposed&&clearObserving(this),isCaughtException(result)&&this.reportExceptionInDerivation_(result.cause),endBatch()}},_proto.reportExceptionInDerivation_=function(error){
/*ThouShaltNotCache*/
var _this=this;if(this.errorHandler_)this.errorHandler_(error,this);else{if(globalState.disableErrorBoundaries)throw error;var message="[mobx] uncaught error in '"+this+"'";globalState.suppressReactionErrors||console.error(message,error),globalState.globalReactionErrorHandlers.forEach(function(f){
/*ThouShaltNotCache*/
return f(error,_this)})}},_proto.dispose=function(){
/*ThouShaltNotCache*/
this.isDisposed||(this.isDisposed=!0,this.isRunning||(startBatch(),clearObserving(this),endBatch()))},_proto.getDisposer_=function(abortSignal){
/*ThouShaltNotCache*/
var _this2=this,dispose=function dispose(){
/*ThouShaltNotCache*/
_this2.dispose(),null==abortSignal||null==abortSignal.removeEventListener||abortSignal.removeEventListener("abort",dispose)};return null==abortSignal||null==abortSignal.addEventListener||abortSignal.addEventListener("abort",dispose),dispose[$mobx]=this,dispose},_proto.toString=function(){
/*ThouShaltNotCache*/
return"Reaction["+this.name_+"]"},_proto.trace=function(enterBreakPoint){
/*ThouShaltNotCache*/
void 0===enterBreakPoint&&(enterBreakPoint=!1)},_createClass(Reaction,[{key:"isDisposed",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Reaction.isDisposedMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Reaction.isDisposedMask_,newValue)}},{key:"isScheduled",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Reaction.isScheduledMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Reaction.isScheduledMask_,newValue)}},{key:"isTrackPending",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Reaction.isTrackPendingMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Reaction.isTrackPendingMask_,newValue)}},{key:"isRunning",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Reaction.isRunningMask_)},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Reaction.isRunningMask_,newValue)}},{key:"diffValue",get:function(){
/*ThouShaltNotCache*/
return getFlag(this.flags_,Reaction.diffValueMask_)?1:0},set:function(newValue){
/*ThouShaltNotCache*/
this.flags_=setFlag(this.flags_,Reaction.diffValueMask_,1===newValue)}}])}();Reaction.isDisposedMask_=1,Reaction.isScheduledMask_=2,Reaction.isTrackPendingMask_=4,Reaction.isRunningMask_=8,Reaction.diffValueMask_=16;var MAX_REACTION_ITERATIONS=100,reactionScheduler=function(f){
/*ThouShaltNotCache*/
return f()};function runReactions(){
/*ThouShaltNotCache*/
globalState.inBatch>0||globalState.isRunningReactions||reactionScheduler(runReactionsHelper)}function runReactionsHelper(){
/*ThouShaltNotCache*/
globalState.isRunningReactions=!0;for(var allReactions=globalState.pendingReactions,iterations=0;allReactions.length>0;){++iterations===MAX_REACTION_ITERATIONS&&(console.error("[mobx] cycle in reaction: "+allReactions[0]),allReactions.splice(0));for(var remainingReactions=allReactions.splice(0),i=0,l=remainingReactions.length;i<l;i++)remainingReactions[i].runReaction_()}globalState.isRunningReactions=!1}var isReaction=createInstanceofPredicate("Reaction",Reaction);var actionAnnotation=createActionAnnotation("action"),actionBoundAnnotation=createActionAnnotation("action.bound",{bound:!0}),autoActionAnnotation=createActionAnnotation("autoAction",{autoAction:!0}),autoActionBoundAnnotation=createActionAnnotation("autoAction.bound",{autoAction:!0,bound:!0});function createActionFactory(autoAction){return function(arg1,arg2){
/*ThouShaltNotCache*/
return isFunction(arg1)?createAction(arg1.name||"<unnamed action>",arg1,autoAction):isFunction(arg2)?createAction(arg1,arg2,autoAction):is20223Decorator(arg2)?(autoAction?autoActionAnnotation:actionAnnotation).decorate_20223_(arg1,arg2):isStringish(arg2)?storeAnnotation(arg1,arg2,autoAction?autoActionAnnotation:actionAnnotation):isStringish(arg1)?createDecoratorAnnotation(createActionAnnotation(autoAction?"autoAction":"action",{name:arg1,autoAction:autoAction})):void 0}}var action=createActionFactory(!1);Object.assign(action,actionAnnotation);var autoAction=createActionFactory(!0);function isAction(thing){
/*ThouShaltNotCache*/
return isFunction(thing)&&!0===thing.isMobxAction}function autorun(view,opts){
/*ThouShaltNotCache*/
var _opts$name,_opts,_opts2,_opts3;void 0===opts&&(opts=EMPTY_OBJECT);var reaction,name=null!=(_opts$name=null==(_opts=opts)?void 0:_opts.name)?_opts$name:"Autorun";if(!opts.scheduler&&!opts.delay)reaction=new Reaction(name,function(){
/*ThouShaltNotCache*/
this.track(reactionRunner)},opts.onError,opts.requiresObservable);else{var scheduler=createSchedulerFromOptions(opts),isScheduled=!1;reaction=new Reaction(name,function(){
/*ThouShaltNotCache*/
isScheduled||(isScheduled=!0,scheduler(function(){
/*ThouShaltNotCache*/
isScheduled=!1,reaction.isDisposed||reaction.track(reactionRunner)}))},opts.onError,opts.requiresObservable)}function reactionRunner(){
/*ThouShaltNotCache*/
view(reaction)}return null!=(_opts2=opts)&&null!=(_opts2=_opts2.signal)&&_opts2.aborted||reaction.schedule_(),reaction.getDisposer_(null==(_opts3=opts)?void 0:_opts3.signal)}Object.assign(autoAction,autoActionAnnotation),action.bound=createDecoratorAnnotation(actionBoundAnnotation),autoAction.bound=createDecoratorAnnotation(autoActionBoundAnnotation);var run=function(f){
/*ThouShaltNotCache*/
return f()};function createSchedulerFromOptions(opts){
/*ThouShaltNotCache*/
return opts.scheduler?opts.scheduler:opts.delay?function(f){
/*ThouShaltNotCache*/
return setTimeout(f,opts.delay)}:run}var ON_BECOME_OBSERVED="onBO",ON_BECOME_UNOBSERVED="onBUO";function onBecomeUnobserved(thing,arg2,arg3){
/*ThouShaltNotCache*/
return interceptHook(ON_BECOME_UNOBSERVED,thing,arg2,arg3)}function interceptHook(hook,thing,arg2,arg3){
/*ThouShaltNotCache*/
var atom="function"==typeof arg3?getAtom(thing,arg2):getAtom(thing),cb=isFunction(arg3)?arg3:arg2,listenersKey=hook+"L";return atom[listenersKey]?atom[listenersKey].add(cb):atom[listenersKey]=new Set([cb]),function(){
/*ThouShaltNotCache*/
var hookListeners=atom[listenersKey];hookListeners&&(hookListeners.delete(cb),0===hookListeners.size&&delete atom[listenersKey])}}function extendObservable(target,properties,annotations,options){var descriptors=getOwnPropertyDescriptors(properties);return initObservable(function(){
/*ThouShaltNotCache*/
var adm=asObservableObject(target,options)[$mobx];ownKeys(descriptors).forEach(function(key){
/*ThouShaltNotCache*/
adm.extend_(key,descriptors[key],!annotations||(!(key in annotations)||annotations[key]))})}),target}var generatorId=0;function FlowCancellationError(){
/*ThouShaltNotCache*/
this.message="FLOW_CANCELLED"}FlowCancellationError.prototype=Object.create(Error.prototype);var flowAnnotation=createFlowAnnotation("flow"),flowBoundAnnotation=createFlowAnnotation("flow.bound",{bound:!0}),flow=Object.assign(function(arg1,arg2){
/*ThouShaltNotCache*/
if(is20223Decorator(arg2))return flowAnnotation.decorate_20223_(arg1,arg2);if(isStringish(arg2))return storeAnnotation(arg1,arg2,flowAnnotation);var generator=arg1,name=generator.name||"<unnamed flow>",res=function(){
/*ThouShaltNotCache*/
var rejector,args=arguments,runId=++generatorId,gen=action(name+" - runid: "+runId+" - init",generator).apply(this,args),pendingPromise=void 0,promise=new Promise(function(resolve,reject){
/*ThouShaltNotCache*/
var stepId=0;function onFulfilled(res){var ret;
/*ThouShaltNotCache*/
pendingPromise=void 0;try{ret=action(name+" - runid: "+runId+" - yield "+stepId++,gen.next).call(gen,res)}catch(e){return reject(e)}next(ret)}function onRejected(err){var ret;
/*ThouShaltNotCache*/
pendingPromise=void 0;try{ret=action(name+" - runid: "+runId+" - yield "+stepId++,gen.throw).call(gen,err)}catch(e){return reject(e)}next(ret)}function next(ret){
/*ThouShaltNotCache*/
if(!isFunction(null==ret?void 0:ret.then))return ret.done?resolve(ret.value):(pendingPromise=Promise.resolve(ret.value)).then(onFulfilled,onRejected);ret.then(next,reject)}rejector=reject,onFulfilled(void 0)});return promise.cancel=action(name+" - runid: "+runId+" - cancel",function(){
/*ThouShaltNotCache*/
try{pendingPromise&&cancelPromise(pendingPromise);var _res=gen.return(void 0),yieldedPromise=Promise.resolve(_res.value);yieldedPromise.then(noop,noop),cancelPromise(yieldedPromise),rejector(new FlowCancellationError)}catch(e){rejector(e)}}),promise};return res.isMobXFlow=!0,res},flowAnnotation);function cancelPromise(promise){
/*ThouShaltNotCache*/
isFunction(promise.cancel)&&promise.cancel()}function isFlow(fn){
/*ThouShaltNotCache*/
return!0===(null==fn?void 0:fn.isMobXFlow)}function _isObservable(value,property){
/*ThouShaltNotCache*/
return!!value&&(void 0!==property?!!isObservableObject(value)&&value[$mobx].values_.has(property):isObservableObject(value)||!!value[$mobx]||isAtom(value)||isReaction(value)||isComputedValue(value))}function isObservable(value){return _isObservable(value)}function transaction(action,thisArg){
/*ThouShaltNotCache*/
void 0===thisArg&&(thisArg=void 0),startBatch();try{return action.apply(thisArg)}finally{endBatch()}}function getAdm(target){
/*ThouShaltNotCache*/
return target[$mobx]}flow.bound=createDecoratorAnnotation(flowBoundAnnotation);var objectProxyTraps={has:function(target,name){return getAdm(target).has_(name)},get:function(target,name){
/*ThouShaltNotCache*/
return getAdm(target).get_(name)},set:function(target,name,value){
/*ThouShaltNotCache*/
var _getAdm$set_;return!!isStringish(name)&&(null==(_getAdm$set_=getAdm(target).set_(name,value,!0))||_getAdm$set_)},deleteProperty:function(target,name){
/*ThouShaltNotCache*/
var _getAdm$delete_;return!!isStringish(name)&&(null==(_getAdm$delete_=getAdm(target).delete_(name,!0))||_getAdm$delete_)},defineProperty:function(target,name,descriptor){
/*ThouShaltNotCache*/
var _getAdm$definePropert;return null==(_getAdm$definePropert=getAdm(target).defineProperty_(name,descriptor))||_getAdm$definePropert},ownKeys:function(target){return getAdm(target).ownKeys_()},preventExtensions:function(target){
/*ThouShaltNotCache*/
die(13)}};function hasInterceptors(interceptable){
/*ThouShaltNotCache*/
return void 0!==interceptable.interceptors_&&interceptable.interceptors_.length>0}function registerInterceptor(interceptable,handler){
/*ThouShaltNotCache*/
var interceptors=interceptable.interceptors_||(interceptable.interceptors_=[]);return interceptors.push(handler),once(function(){
/*ThouShaltNotCache*/
var idx=interceptors.indexOf(handler);-1!==idx&&interceptors.splice(idx,1)})}function interceptChange(interceptable,change){
/*ThouShaltNotCache*/
var prevU=untrackedStart();try{for(var interceptors=[].concat(interceptable.interceptors_||[]),i=0,l=interceptors.length;i<l&&((change=interceptors[i](change))&&!change.type&&die(14),change);i++);return change}finally{untrackedEnd(prevU)}}function hasListeners(listenable){
/*ThouShaltNotCache*/
return void 0!==listenable.changeListeners_&&listenable.changeListeners_.length>0}function registerListener(listenable,handler){
/*ThouShaltNotCache*/
var listeners=listenable.changeListeners_||(listenable.changeListeners_=[]);return listeners.push(handler),once(function(){
/*ThouShaltNotCache*/
var idx=listeners.indexOf(handler);-1!==idx&&listeners.splice(idx,1)})}function notifyListeners(listenable,change){
/*ThouShaltNotCache*/
var prevU=untrackedStart(),listeners=listenable.changeListeners_;if(listeners){for(var i=0,l=(listeners=listeners.slice()).length;i<l;i++)listeners[i](change);untrackedEnd(prevU)}}function makeObservable(target,annotations,options){
/*ThouShaltNotCache*/
return initObservable(function(){
/*ThouShaltNotCache*/
var adm=asObservableObject(target,options)[$mobx];null!=annotations||(annotations=function(target){
/*ThouShaltNotCache*/
return hasProp(target,storedAnnotationsSymbol)||addHiddenProp(target,storedAnnotationsSymbol,_extends({},target[storedAnnotationsSymbol])),target[storedAnnotationsSymbol]}(target)),ownKeys(annotations).forEach(function(key){
/*ThouShaltNotCache*/
return adm.make_(key,annotations[key])})}),target}var UPDATE="update",arrayTraps={get:function(target,name){
/*ThouShaltNotCache*/
var adm=target[$mobx];return name===$mobx?adm:"length"===name?adm.getArrayLength_():"string"!=typeof name||isNaN(name)?hasProp(arrayExtensions,name)?arrayExtensions[name]:target[name]:adm.get_(parseInt(name))},set:function(target,name,value){
/*ThouShaltNotCache*/
var adm=target[$mobx];return"length"===name&&adm.setArrayLength_(value),"symbol"==typeof name||isNaN(name)?target[name]=value:adm.set_(parseInt(name),value),!0},preventExtensions:function(){
/*ThouShaltNotCache*/
die(15)}},ObservableArrayAdministration=function(){
/*ThouShaltNotCache*/
function ObservableArrayAdministration(name,enhancer,owned_,legacyMode_){
/*ThouShaltNotCache*/
void 0===name&&(name="ObservableArray"),this.owned_=void 0,this.legacyMode_=void 0,this.atom_=void 0,this.values_=[],this.interceptors_=void 0,this.changeListeners_=void 0,this.enhancer_=void 0,this.dehancer=void 0,this.proxy_=void 0,this.lastKnownLength_=0,this.owned_=owned_,this.legacyMode_=legacyMode_,this.atom_=new Atom(name),this.enhancer_=function(newV,oldV){
/*ThouShaltNotCache*/
return enhancer(newV,oldV,"ObservableArray[..]")}}var _proto=ObservableArrayAdministration.prototype;return _proto.dehanceValue_=function(value){
/*ThouShaltNotCache*/
return void 0!==this.dehancer?this.dehancer(value):value},_proto.dehanceValues_=function(values){
/*ThouShaltNotCache*/
return void 0!==this.dehancer&&values.length>0?values.map(this.dehancer):values},_proto.intercept_=function(handler){
/*ThouShaltNotCache*/
return registerInterceptor(this,handler)},_proto.observe_=function(listener,fireImmediately){
/*ThouShaltNotCache*/
return void 0===fireImmediately&&(fireImmediately=!1),fireImmediately&&listener({observableKind:"array",object:this.proxy_,debugObjectName:this.atom_.name_,type:"splice",index:0,added:this.values_.slice(),addedCount:this.values_.length,removed:[],removedCount:0}),registerListener(this,listener)},_proto.getArrayLength_=function(){
/*ThouShaltNotCache*/
return this.atom_.reportObserved(),this.values_.length},_proto.setArrayLength_=function(newLength){
/*ThouShaltNotCache*/
("number"!=typeof newLength||isNaN(newLength)||newLength<0)&&die("Out of range: "+newLength);var currentLength=this.values_.length;if(newLength!==currentLength)if(newLength>currentLength){for(var newItems=new Array(newLength-currentLength),i=0;i<newLength-currentLength;i++)newItems[i]=void 0;this.spliceWithArray_(currentLength,0,newItems)}else this.spliceWithArray_(newLength,currentLength-newLength)},_proto.updateArrayLength_=function(oldLength,delta){
/*ThouShaltNotCache*/
oldLength!==this.lastKnownLength_&&die(16),this.lastKnownLength_+=delta,this.legacyMode_&&delta>0&&reserveArrayBuffer(oldLength+delta+1)},_proto.spliceWithArray_=function(index,deleteCount,newItems){
/*ThouShaltNotCache*/
var _this=this;this.atom_;var length=this.values_.length;if(void 0===index?index=0:index>length?index=length:index<0&&(index=Math.max(0,length+index)),deleteCount=1===arguments.length?length-index:null==deleteCount?0:Math.max(0,Math.min(deleteCount,length-index)),void 0===newItems&&(newItems=EMPTY_ARRAY),hasInterceptors(this)){var change=interceptChange(this,{object:this.proxy_,type:"splice",index:index,removedCount:deleteCount,added:newItems});if(!change)return EMPTY_ARRAY;deleteCount=change.removedCount,newItems=change.added}if(newItems=0===newItems.length?newItems:newItems.map(function(v){
/*ThouShaltNotCache*/
return _this.enhancer_(v,void 0)}),this.legacyMode_){var lengthDelta=newItems.length-deleteCount;this.updateArrayLength_(length,lengthDelta)}var res=this.spliceItemsIntoValues_(index,deleteCount,newItems);return 0===deleteCount&&0===newItems.length||this.notifyArraySplice_(index,newItems,res),this.dehanceValues_(res)},_proto.spliceItemsIntoValues_=function(index,deleteCount,newItems){var _this$values_;
/*ThouShaltNotCache*/
if(newItems.length<1e4)return(_this$values_=this.values_).splice.apply(_this$values_,[index,deleteCount].concat(newItems));var res=this.values_.slice(index,index+deleteCount),oldItems=this.values_.slice(index+deleteCount);this.values_.length+=newItems.length-deleteCount;for(var i=0;i<newItems.length;i++)this.values_[index+i]=newItems[i];for(var _i=0;_i<oldItems.length;_i++)this.values_[index+newItems.length+_i]=oldItems[_i];return res},_proto.notifyArrayChildUpdate_=function(index,newValue,oldValue){
/*ThouShaltNotCache*/
var notifySpy=!this.owned_&&!1,notify=hasListeners(this),change=notify||notifySpy?{observableKind:"array",object:this.proxy_,type:UPDATE,debugObjectName:this.atom_.name_,index:index,newValue:newValue,oldValue:oldValue}:null;this.atom_.reportChanged(),notify&&notifyListeners(this,change)},_proto.notifyArraySplice_=function(index,added,removed){
/*ThouShaltNotCache*/
var notifySpy=!this.owned_&&!1,notify=hasListeners(this),change=notify||notifySpy?{observableKind:"array",object:this.proxy_,debugObjectName:this.atom_.name_,type:"splice",index:index,removed:removed,added:added,removedCount:removed.length,addedCount:added.length}:null;this.atom_.reportChanged(),notify&&notifyListeners(this,change)},_proto.get_=function(index){
/*ThouShaltNotCache*/
if(!(this.legacyMode_&&index>=this.values_.length))return this.atom_.reportObserved(),this.dehanceValue_(this.values_[index]);console.warn("[mobx] Out of bounds read: "+index)},_proto.set_=function(index,newValue){
/*ThouShaltNotCache*/
var values=this.values_;if(this.legacyMode_&&index>values.length&&die(17,index,values.length),index<values.length){this.atom_;var oldValue=values[index];if(hasInterceptors(this)){var change=interceptChange(this,{type:UPDATE,object:this.proxy_,index:index,newValue:newValue});if(!change)return;newValue=change.newValue}(newValue=this.enhancer_(newValue,oldValue))!==oldValue&&(values[index]=newValue,this.notifyArrayChildUpdate_(index,newValue,oldValue))}else{for(var newItems=new Array(index+1-values.length),i=0;i<newItems.length-1;i++)newItems[i]=void 0;newItems[newItems.length-1]=newValue,this.spliceWithArray_(values.length,0,newItems)}},ObservableArrayAdministration}();function createObservableArray(initialValues,enhancer,name,owned){
/*ThouShaltNotCache*/
return void 0===name&&(name="ObservableArray"),void 0===owned&&(owned=!1),assertProxies(),initObservable(function(){
/*ThouShaltNotCache*/
var adm=new ObservableArrayAdministration(name,enhancer,owned,!1);addHiddenFinalProp(adm.values_,$mobx,adm);var proxy=new Proxy(adm.values_,arrayTraps);return adm.proxy_=proxy,initialValues&&initialValues.length&&adm.spliceWithArray_(0,0,initialValues),proxy})}var arrayExtensions={clear:function(){
/*ThouShaltNotCache*/
return this.splice(0)},replace:function(newItems){
/*ThouShaltNotCache*/
var adm=this[$mobx];return adm.spliceWithArray_(0,adm.values_.length,newItems)},toJSON:function(){
/*ThouShaltNotCache*/
return this.slice()},splice:function(index,deleteCount){
/*ThouShaltNotCache*/
for(var _len=arguments.length,newItems=new Array(_len>2?_len-2:0),_key=2;_key<_len;_key++)newItems[_key-2]=arguments[_key];var adm=this[$mobx];switch(arguments.length){case 0:return[];case 1:return adm.spliceWithArray_(index);case 2:return adm.spliceWithArray_(index,deleteCount)}return adm.spliceWithArray_(index,deleteCount,newItems)},spliceWithArray:function(index,deleteCount,newItems){
/*ThouShaltNotCache*/
return this[$mobx].spliceWithArray_(index,deleteCount,newItems)},push:function(){for(
/*ThouShaltNotCache*/
var adm=this[$mobx],_len2=arguments.length,items=new Array(_len2),_key2=0;_key2<_len2;_key2++)items[_key2]=arguments[_key2];return adm.spliceWithArray_(adm.values_.length,0,items),adm.values_.length},pop:function(){
/*ThouShaltNotCache*/
return this.splice(Math.max(this[$mobx].values_.length-1,0),1)[0]},shift:function(){
/*ThouShaltNotCache*/
return this.splice(0,1)[0]},unshift:function(){for(
/*ThouShaltNotCache*/
var adm=this[$mobx],_len3=arguments.length,items=new Array(_len3),_key3=0;_key3<_len3;_key3++)items[_key3]=arguments[_key3];return adm.spliceWithArray_(0,0,items),adm.values_.length},reverse:function(){
/*ThouShaltNotCache*/
return globalState.trackingDerivation&&die(37,"reverse"),this.replace(this.slice().reverse()),this},sort:function(){
/*ThouShaltNotCache*/
globalState.trackingDerivation&&die(37,"sort");var copy=this.slice();return copy.sort.apply(copy,arguments),this.replace(copy),this},remove:function(value){
/*ThouShaltNotCache*/
var adm=this[$mobx],idx=adm.dehanceValues_(adm.values_).indexOf(value);return idx>-1&&(this.splice(idx,1),!0)}};function addArrayExtension(funcName,funcFactory){
/*ThouShaltNotCache*/
"function"==typeof Array.prototype[funcName]&&(arrayExtensions[funcName]=funcFactory(funcName))}function simpleFunc(funcName){
/*ThouShaltNotCache*/
return function(){
/*ThouShaltNotCache*/
var adm=this[$mobx];adm.atom_.reportObserved();var dehancedValues=adm.dehanceValues_(adm.values_);return dehancedValues[funcName].apply(dehancedValues,arguments)}}function mapLikeFunc(funcName){
/*ThouShaltNotCache*/
return function(callback,thisArg){
/*ThouShaltNotCache*/
var _this2=this,adm=this[$mobx];return adm.atom_.reportObserved(),adm.dehanceValues_(adm.values_)[funcName](function(element,index){
/*ThouShaltNotCache*/
return callback.call(thisArg,element,index,_this2)})}}function reduceLikeFunc(funcName){
/*ThouShaltNotCache*/
return function(){
/*ThouShaltNotCache*/
var _this3=this,adm=this[$mobx];adm.atom_.reportObserved();var dehancedValues=adm.dehanceValues_(adm.values_),callback=arguments[0];return arguments[0]=function(accumulator,currentValue,index){
/*ThouShaltNotCache*/
return callback(accumulator,currentValue,index,_this3)},dehancedValues[funcName].apply(dehancedValues,arguments)}}addArrayExtension("at",simpleFunc),addArrayExtension("concat",simpleFunc),addArrayExtension("flat",simpleFunc),addArrayExtension("includes",simpleFunc),addArrayExtension("indexOf",simpleFunc),addArrayExtension("join",simpleFunc),addArrayExtension("lastIndexOf",simpleFunc),addArrayExtension("slice",simpleFunc),addArrayExtension("toString",simpleFunc),addArrayExtension("toLocaleString",simpleFunc),addArrayExtension("toSorted",simpleFunc),addArrayExtension("toSpliced",simpleFunc),addArrayExtension("with",simpleFunc),addArrayExtension("every",mapLikeFunc),addArrayExtension("filter",mapLikeFunc),addArrayExtension("find",mapLikeFunc),addArrayExtension("findIndex",mapLikeFunc),addArrayExtension("findLast",mapLikeFunc),addArrayExtension("findLastIndex",mapLikeFunc),addArrayExtension("flatMap",mapLikeFunc),addArrayExtension("forEach",mapLikeFunc),addArrayExtension("map",mapLikeFunc),addArrayExtension("some",mapLikeFunc),addArrayExtension("toReversed",mapLikeFunc),addArrayExtension("reduce",reduceLikeFunc),addArrayExtension("reduceRight",reduceLikeFunc);var isObservableArrayAdministration=createInstanceofPredicate("ObservableArrayAdministration",ObservableArrayAdministration);function isObservableArray(thing){
/*ThouShaltNotCache*/
return isObject(thing)&&isObservableArrayAdministration(thing[$mobx])}var ObservableMapMarker={},ADD="add",ObservableMap=function(){
/*ThouShaltNotCache*/
function ObservableMap(initialData,enhancer_,name_){
/*ThouShaltNotCache*/
var _this=this;void 0===enhancer_&&(enhancer_=deepEnhancer),void 0===name_&&(name_="ObservableMap"),this.enhancer_=void 0,this.name_=void 0,this[$mobx]=ObservableMapMarker,this.data_=void 0,this.hasMap_=void 0,this.keysAtom_=void 0,this.interceptors_=void 0,this.changeListeners_=void 0,this.dehancer=void 0,this.enhancer_=enhancer_,this.name_=name_,isFunction(Map)||die(18),initObservable(function(){
/*ThouShaltNotCache*/
_this.keysAtom_=createAtom("ObservableMap.keys()"),_this.data_=new Map,_this.hasMap_=new Map,initialData&&_this.merge(initialData)})}var _proto=ObservableMap.prototype;return _proto.has_=function(key){
/*ThouShaltNotCache*/
return this.data_.has(key)},_proto.has=function(key){
/*ThouShaltNotCache*/
var _this2=this;if(!globalState.trackingDerivation)return this.has_(key);var entry=this.hasMap_.get(key);if(!entry){var newEntry=entry=new ObservableValue(this.has_(key),referenceEnhancer,"ObservableMap.key?",!1);this.hasMap_.set(key,newEntry),onBecomeUnobserved(newEntry,function(){
/*ThouShaltNotCache*/
return _this2.hasMap_.delete(key)})}return entry.get()},_proto.set=function(key,value){
/*ThouShaltNotCache*/
var hasKey=this.has_(key);if(hasInterceptors(this)){var change=interceptChange(this,{type:hasKey?UPDATE:ADD,object:this,newValue:value,name:key});if(!change)return this;value=change.newValue}return hasKey?this.updateValue_(key,value):this.addValue_(key,value),this},_proto.delete=function(key){
/*ThouShaltNotCache*/
var _this3=this;if((this.keysAtom_,hasInterceptors(this))&&!interceptChange(this,{type:"delete",object:this,name:key}))return!1;if(this.has_(key)){var notify=hasListeners(this),_change=notify?{observableKind:"map",debugObjectName:this.name_,type:"delete",object:this,oldValue:this.data_.get(key).value_,name:key}:null;return transaction(function(){
/*ThouShaltNotCache*/
var _this3$hasMap_$get;_this3.keysAtom_.reportChanged(),null==(_this3$hasMap_$get=_this3.hasMap_.get(key))||_this3$hasMap_$get.setNewValue_(!1),_this3.data_.get(key).setNewValue_(void 0),_this3.data_.delete(key)}),notify&&notifyListeners(this,_change),!0}return!1},_proto.updateValue_=function(key,newValue){
/*ThouShaltNotCache*/
var observable=this.data_.get(key);if((newValue=observable.prepareNewValue_(newValue))!==globalState.UNCHANGED){var notify=hasListeners(this),change=notify?{observableKind:"map",debugObjectName:this.name_,type:UPDATE,object:this,oldValue:observable.value_,name:key,newValue:newValue}:null;0,observable.setNewValue_(newValue),notify&&notifyListeners(this,change)}},_proto.addValue_=function(key,newValue){
/*ThouShaltNotCache*/
var _this4=this;this.keysAtom_,transaction(function(){
/*ThouShaltNotCache*/
var _this4$hasMap_$get,observable=new ObservableValue(newValue,_this4.enhancer_,"ObservableMap.key",!1);_this4.data_.set(key,observable),newValue=observable.value_,null==(_this4$hasMap_$get=_this4.hasMap_.get(key))||_this4$hasMap_$get.setNewValue_(!0),_this4.keysAtom_.reportChanged()});var notify=hasListeners(this),change=notify?{observableKind:"map",debugObjectName:this.name_,type:ADD,object:this,name:key,newValue:newValue}:null;notify&&notifyListeners(this,change)},_proto.get=function(key){
/*ThouShaltNotCache*/
return this.has(key)?this.dehanceValue_(this.data_.get(key).get()):this.dehanceValue_(void 0)},_proto.dehanceValue_=function(value){
/*ThouShaltNotCache*/
return void 0!==this.dehancer?this.dehancer(value):value},_proto.keys=function(){
/*ThouShaltNotCache*/
return this.keysAtom_.reportObserved(),this.data_.keys()},_proto.values=function(){
/*ThouShaltNotCache*/
var self=this,keys=this.keys();return makeIterableForMap({next:function(){
/*ThouShaltNotCache*/
var _keys$next=keys.next(),done=_keys$next.done,value=_keys$next.value;return{done:done,value:done?void 0:self.get(value)}}})},_proto.entries=function(){
/*ThouShaltNotCache*/
var self=this,keys=this.keys();return makeIterableForMap({next:function(){
/*ThouShaltNotCache*/
var _keys$next2=keys.next(),done=_keys$next2.done,value=_keys$next2.value;return{done:done,value:done?void 0:[value,self.get(value)]}}})},_proto[Symbol.iterator]=function(){
/*ThouShaltNotCache*/
return this.entries()},_proto.forEach=function(callback,thisArg){
/*ThouShaltNotCache*/
for(var _step,_iterator=_createForOfIteratorHelperLoose(this);!(_step=_iterator()).done;){var _step$value=_step.value,key=_step$value[0],value=_step$value[1];callback.call(thisArg,value,key,this)}},_proto.merge=function(other){
/*ThouShaltNotCache*/
var _this5=this;return isObservableMap(other)&&(other=new Map(other)),transaction(function(){var thing,mapProto,objectProto;
/*ThouShaltNotCache*/
isPlainObject(other)?function(object){
/*ThouShaltNotCache*/
var keys=Object.keys(object);if(!hasGetOwnPropertySymbols)return keys;var symbols=Object.getOwnPropertySymbols(object);return symbols.length?[].concat(keys,symbols.filter(function(s){
/*ThouShaltNotCache*/
return objectPrototype.propertyIsEnumerable.call(object,s)})):keys}(other).forEach(function(key){
/*ThouShaltNotCache*/
return _this5.set(key,other[key])}):Array.isArray(other)?other.forEach(function(_ref){
/*ThouShaltNotCache*/
var key=_ref[0],value=_ref[1];return _this5.set(key,value)}):isES6Map(other)?(thing=other,mapProto=Object.getPrototypeOf(thing),objectProto=Object.getPrototypeOf(mapProto),null!==Object.getPrototypeOf(objectProto)&&die(19,other),other.forEach(function(value,key){
/*ThouShaltNotCache*/
return _this5.set(key,value)})):null!=other&&die(20,other)}),this},_proto.clear=function(){
/*ThouShaltNotCache*/
var _this6=this;transaction(function(){
/*ThouShaltNotCache*/
untracked(function(){
/*ThouShaltNotCache*/
for(var _step2,_iterator2=_createForOfIteratorHelperLoose(_this6.keys());!(_step2=_iterator2()).done;){var key=_step2.value;_this6.delete(key)}})})},_proto.replace=function(values){
/*ThouShaltNotCache*/
var _this7=this;return transaction(function(){for(
/*ThouShaltNotCache*/
var _step3,replacementMap=function(dataStructure){
/*ThouShaltNotCache*/
if(isES6Map(dataStructure)||isObservableMap(dataStructure))return dataStructure;if(Array.isArray(dataStructure))return new Map(dataStructure);if(isPlainObject(dataStructure)){var map=new Map;for(var key in dataStructure)map.set(key,dataStructure[key]);return map}return die(21,dataStructure)}(values),orderedData=new Map,keysReportChangedCalled=!1,_iterator3=_createForOfIteratorHelperLoose(_this7.data_.keys());!(_step3=_iterator3()).done;){var key=_step3.value;if(!replacementMap.has(key))if(_this7.delete(key))keysReportChangedCalled=!0;else{var value=_this7.data_.get(key);orderedData.set(key,value)}}for(var _step4,_iterator4=_createForOfIteratorHelperLoose(replacementMap.entries());!(_step4=_iterator4()).done;){var _step4$value=_step4.value,_key=_step4$value[0],_value=_step4$value[1],keyExisted=_this7.data_.has(_key);if(_this7.set(_key,_value),_this7.data_.has(_key)){var _value2=_this7.data_.get(_key);orderedData.set(_key,_value2),keyExisted||(keysReportChangedCalled=!0)}}if(!keysReportChangedCalled)if(_this7.data_.size!==orderedData.size)_this7.keysAtom_.reportChanged();else for(var iter1=_this7.data_.keys(),iter2=orderedData.keys(),next1=iter1.next(),next2=iter2.next();!next1.done;){if(next1.value!==next2.value){_this7.keysAtom_.reportChanged();break}next1=iter1.next(),next2=iter2.next()}_this7.data_=orderedData}),this},_proto.toString=function(){
/*ThouShaltNotCache*/
return"[object ObservableMap]"},_proto.toJSON=function(){
/*ThouShaltNotCache*/
return Array.from(this)},_proto.observe_=function(listener,fireImmediately){return registerListener(this,listener)},_proto.intercept_=function(handler){
/*ThouShaltNotCache*/
return registerInterceptor(this,handler)},_createClass(ObservableMap,[{key:"size",get:function(){
/*ThouShaltNotCache*/
return this.keysAtom_.reportObserved(),this.data_.size}},{key:Symbol.toStringTag,get:function(){
/*ThouShaltNotCache*/
return"Map"}}])}(),isObservableMap=createInstanceofPredicate("ObservableMap",ObservableMap);function makeIterableForMap(iterator){
/*ThouShaltNotCache*/
return iterator[Symbol.toStringTag]="MapIterator",makeIterable(iterator)}var ObservableSetMarker={},ObservableSet=function(){
/*ThouShaltNotCache*/
function ObservableSet(initialData,enhancer,name_){
/*ThouShaltNotCache*/
var _this=this;void 0===enhancer&&(enhancer=deepEnhancer),void 0===name_&&(name_="ObservableSet"),this.name_=void 0,this[$mobx]=ObservableSetMarker,this.data_=new Set,this.atom_=void 0,this.changeListeners_=void 0,this.interceptors_=void 0,this.dehancer=void 0,this.enhancer_=void 0,this.name_=name_,isFunction(Set)||die(22),this.enhancer_=function(newV,oldV){
/*ThouShaltNotCache*/
return enhancer(newV,oldV,name_)},initObservable(function(){
/*ThouShaltNotCache*/
_this.atom_=createAtom(_this.name_),initialData&&_this.replace(initialData)})}var _proto=ObservableSet.prototype;return _proto.dehanceValue_=function(value){
/*ThouShaltNotCache*/
return void 0!==this.dehancer?this.dehancer(value):value},_proto.clear=function(){
/*ThouShaltNotCache*/
var _this2=this;transaction(function(){
/*ThouShaltNotCache*/
untracked(function(){
/*ThouShaltNotCache*/
for(var _step,_iterator=_createForOfIteratorHelperLoose(_this2.data_.values());!(_step=_iterator()).done;){var value=_step.value;_this2.delete(value)}})})},_proto.forEach=function(callbackFn,thisArg){
/*ThouShaltNotCache*/
for(var _step2,_iterator2=_createForOfIteratorHelperLoose(this);!(_step2=_iterator2()).done;){var value=_step2.value;callbackFn.call(thisArg,value,value,this)}},_proto.add=function(value){
/*ThouShaltNotCache*/
var _this3=this;if(this.atom_,hasInterceptors(this)){var change=interceptChange(this,{type:ADD,object:this,newValue:value});if(!change)return this;value=change.newValue}if(!this.has(value)){transaction(function(){
/*ThouShaltNotCache*/
_this3.data_.add(_this3.enhancer_(value,void 0)),_this3.atom_.reportChanged()});var notify=hasListeners(this),_change=notify?{observableKind:"set",debugObjectName:this.name_,type:ADD,object:this,newValue:value}:null;false,notify&&notifyListeners(this,_change)}return this},_proto.delete=function(value){
/*ThouShaltNotCache*/
var _this4=this;if(hasInterceptors(this)&&!interceptChange(this,{type:"delete",object:this,oldValue:value}))return!1;if(this.has(value)){var notify=hasListeners(this),_change2=notify?{observableKind:"set",debugObjectName:this.name_,type:"delete",object:this,oldValue:value}:null;return transaction(function(){
/*ThouShaltNotCache*/
_this4.atom_.reportChanged(),_this4.data_.delete(value)}),notify&&notifyListeners(this,_change2),!0}return!1},_proto.has=function(value){
/*ThouShaltNotCache*/
return this.atom_.reportObserved(),this.data_.has(this.dehanceValue_(value))},_proto.entries=function(){
/*ThouShaltNotCache*/
var values=this.values();return makeIterableForSet({next:function(){
/*ThouShaltNotCache*/
var _values$next=values.next(),value=_values$next.value,done=_values$next.done;return done?{value:void 0,done:done}:{value:[value,value],done:done}}})},_proto.keys=function(){
/*ThouShaltNotCache*/
return this.values()},_proto.values=function(){
/*ThouShaltNotCache*/
this.atom_.reportObserved();var self=this,values=this.data_.values();return makeIterableForSet({next:function(){
/*ThouShaltNotCache*/
var _values$next2=values.next(),value=_values$next2.value,done=_values$next2.done;return done?{value:void 0,done:done}:{value:self.dehanceValue_(value),done:done}}})},_proto.intersection=function(otherSet){
/*ThouShaltNotCache*/
return isES6Set(otherSet)&&!isObservableSet(otherSet)?otherSet.intersection(this):new Set(this).intersection(otherSet)},_proto.union=function(otherSet){
/*ThouShaltNotCache*/
return isES6Set(otherSet)&&!isObservableSet(otherSet)?otherSet.union(this):new Set(this).union(otherSet)},_proto.difference=function(otherSet){
/*ThouShaltNotCache*/
return new Set(this).difference(otherSet)},_proto.symmetricDifference=function(otherSet){
/*ThouShaltNotCache*/
return isES6Set(otherSet)&&!isObservableSet(otherSet)?otherSet.symmetricDifference(this):new Set(this).symmetricDifference(otherSet)},_proto.isSubsetOf=function(otherSet){
/*ThouShaltNotCache*/
return new Set(this).isSubsetOf(otherSet)},_proto.isSupersetOf=function(otherSet){
/*ThouShaltNotCache*/
return new Set(this).isSupersetOf(otherSet)},_proto.isDisjointFrom=function(otherSet){
/*ThouShaltNotCache*/
return isES6Set(otherSet)&&!isObservableSet(otherSet)?otherSet.isDisjointFrom(this):new Set(this).isDisjointFrom(otherSet)},_proto.replace=function(other){
/*ThouShaltNotCache*/
var _this5=this;return isObservableSet(other)&&(other=new Set(other)),transaction(function(){
/*ThouShaltNotCache*/
Array.isArray(other)||isES6Set(other)?(_this5.clear(),other.forEach(function(value){
/*ThouShaltNotCache*/
return _this5.add(value)})):null!=other&&die("Cannot initialize set from "+other)}),this},_proto.observe_=function(listener,fireImmediately){return registerListener(this,listener)},_proto.intercept_=function(handler){
/*ThouShaltNotCache*/
return registerInterceptor(this,handler)},_proto.toJSON=function(){
/*ThouShaltNotCache*/
return Array.from(this)},_proto.toString=function(){
/*ThouShaltNotCache*/
return"[object ObservableSet]"},_proto[Symbol.iterator]=function(){
/*ThouShaltNotCache*/
return this.values()},_createClass(ObservableSet,[{key:"size",get:function(){
/*ThouShaltNotCache*/
return this.atom_.reportObserved(),this.data_.size}},{key:Symbol.toStringTag,get:function(){
/*ThouShaltNotCache*/
return"Set"}}])}(),isObservableSet=createInstanceofPredicate("ObservableSet",ObservableSet);function makeIterableForSet(iterator){
/*ThouShaltNotCache*/
return iterator[Symbol.toStringTag]="SetIterator",makeIterable(iterator)}var descriptorCache=Object.create(null),ObservableObjectAdministration=function(){
/*ThouShaltNotCache*/
function ObservableObjectAdministration(target_,values_,name_,defaultAnnotation_){
/*ThouShaltNotCache*/
void 0===values_&&(values_=new Map),void 0===defaultAnnotation_&&(defaultAnnotation_=autoAnnotation),this.target_=void 0,this.values_=void 0,this.name_=void 0,this.defaultAnnotation_=void 0,this.keysAtom_=void 0,this.changeListeners_=void 0,this.interceptors_=void 0,this.proxy_=void 0,this.isPlainObject_=void 0,this.appliedAnnotations_=void 0,this.pendingKeys_=void 0,this.target_=target_,this.values_=values_,this.name_=name_,this.defaultAnnotation_=defaultAnnotation_,this.keysAtom_=new Atom("ObservableObject.keys"),this.isPlainObject_=isPlainObject(this.target_)}var _proto=ObservableObjectAdministration.prototype;return _proto.getObservablePropValue_=function(key){
/*ThouShaltNotCache*/
return this.values_.get(key).get()},_proto.setObservablePropValue_=function(key,newValue){
/*ThouShaltNotCache*/
var observable=this.values_.get(key);if(observable instanceof ComputedValue)return observable.set(newValue),!0;if(hasInterceptors(this)){var change=interceptChange(this,{type:UPDATE,object:this.proxy_||this.target_,name:key,newValue:newValue});if(!change)return null;newValue=change.newValue}if((newValue=observable.prepareNewValue_(newValue))!==globalState.UNCHANGED){var notify=hasListeners(this),_change=notify?{type:UPDATE,observableKind:"object",debugObjectName:this.name_,object:this.proxy_||this.target_,oldValue:observable.value_,name:key,newValue:newValue}:null;0,observable.setNewValue_(newValue),notify&&notifyListeners(this,_change)}return!0},_proto.get_=function(key){
/*ThouShaltNotCache*/
return globalState.trackingDerivation&&!hasProp(this.target_,key)&&this.has_(key),this.target_[key]},_proto.set_=function(key,value,proxyTrap){
/*ThouShaltNotCache*/
return void 0===proxyTrap&&(proxyTrap=!1),hasProp(this.target_,key)?this.values_.has(key)?this.setObservablePropValue_(key,value):proxyTrap?Reflect.set(this.target_,key,value):(this.target_[key]=value,!0):this.extend_(key,{value:value,enumerable:!0,writable:!0,configurable:!0},this.defaultAnnotation_,proxyTrap)},_proto.has_=function(key){
/*ThouShaltNotCache*/
if(!globalState.trackingDerivation)return key in this.target_;this.pendingKeys_||(this.pendingKeys_=new Map);var entry=this.pendingKeys_.get(key);return entry||(entry=new ObservableValue(key in this.target_,referenceEnhancer,"ObservableObject.key?",!1),this.pendingKeys_.set(key,entry)),entry.get()},_proto.make_=function(key,annotation){if(
/*ThouShaltNotCache*/
!0===annotation&&(annotation=this.defaultAnnotation_),!1!==annotation){if(assertAnnotable(this,annotation,key),!(key in this.target_)){var _this$target_$storedA;if(null!=(_this$target_$storedA=this.target_[storedAnnotationsSymbol])&&_this$target_$storedA[key])return;die(1,annotation.annotationType_,this.name_+"."+key.toString())}for(var source=this.target_;source&&source!==objectPrototype;){var descriptor=getDescriptor(source,key);if(descriptor){var outcome=annotation.make_(this,key,descriptor,source);if(0===outcome)return;if(1===outcome)break}source=Object.getPrototypeOf(source)}recordAnnotationApplied(this,annotation,key)}},_proto.extend_=function(key,descriptor,annotation,proxyTrap){if(
/*ThouShaltNotCache*/
void 0===proxyTrap&&(proxyTrap=!1),!0===annotation&&(annotation=this.defaultAnnotation_),!1===annotation)return this.defineProperty_(key,descriptor,proxyTrap);assertAnnotable(this,annotation,key);var outcome=annotation.extend_(this,key,descriptor,proxyTrap);return outcome&&recordAnnotationApplied(this,annotation,key),outcome},_proto.defineProperty_=function(key,descriptor,proxyTrap){
/*ThouShaltNotCache*/
void 0===proxyTrap&&(proxyTrap=!1),this.keysAtom_;try{startBatch();var deleteOutcome=this.delete_(key);if(!deleteOutcome)return deleteOutcome;if(hasInterceptors(this)){var change=interceptChange(this,{object:this.proxy_||this.target_,name:key,type:ADD,newValue:descriptor.value});if(!change)return null;var newValue=change.newValue;descriptor.value!==newValue&&(descriptor=_extends({},descriptor,{value:newValue}))}if(proxyTrap){if(!Reflect.defineProperty(this.target_,key,descriptor))return!1}else defineProperty(this.target_,key,descriptor);this.notifyPropertyAddition_(key,descriptor.value)}finally{endBatch()}return!0},_proto.defineObservableProperty_=function(key,value,enhancer,proxyTrap){
/*ThouShaltNotCache*/
void 0===proxyTrap&&(proxyTrap=!1),this.keysAtom_;try{startBatch();var deleteOutcome=this.delete_(key);if(!deleteOutcome)return deleteOutcome;if(hasInterceptors(this)){var change=interceptChange(this,{object:this.proxy_||this.target_,name:key,type:ADD,newValue:value});if(!change)return null;value=change.newValue}var cachedDescriptor=getCachedObservablePropDescriptor(key),descriptor={configurable:!globalState.safeDescriptors||this.isPlainObject_,enumerable:!0,get:cachedDescriptor.get,set:cachedDescriptor.set};if(proxyTrap){if(!Reflect.defineProperty(this.target_,key,descriptor))return!1}else defineProperty(this.target_,key,descriptor);var observable=new ObservableValue(value,enhancer,"ObservableObject.key",!1);this.values_.set(key,observable),this.notifyPropertyAddition_(key,observable.value_)}finally{endBatch()}return!0},_proto.defineComputedProperty_=function(key,options,proxyTrap){
/*ThouShaltNotCache*/
void 0===proxyTrap&&(proxyTrap=!1),this.keysAtom_;try{startBatch();var deleteOutcome=this.delete_(key);if(!deleteOutcome)return deleteOutcome;if(hasInterceptors(this))if(!interceptChange(this,{object:this.proxy_||this.target_,name:key,type:ADD,newValue:void 0}))return null;options.name||(options.name="ObservableObject.key"),options.context=this.proxy_||this.target_;var cachedDescriptor=getCachedObservablePropDescriptor(key),descriptor={configurable:!globalState.safeDescriptors||this.isPlainObject_,enumerable:!1,get:cachedDescriptor.get,set:cachedDescriptor.set};if(proxyTrap){if(!Reflect.defineProperty(this.target_,key,descriptor))return!1}else defineProperty(this.target_,key,descriptor);this.values_.set(key,new ComputedValue(options)),this.notifyPropertyAddition_(key,void 0)}finally{endBatch()}return!0},_proto.delete_=function(key,proxyTrap){if(
/*ThouShaltNotCache*/
void 0===proxyTrap&&(proxyTrap=!1),this.keysAtom_,!hasProp(this.target_,key))return!0;if(hasInterceptors(this)&&!interceptChange(this,{object:this.proxy_||this.target_,name:key,type:"remove"}))return null;try{var _this$pendingKeys_;startBatch();var _getDescriptor,notify=hasListeners(this),observable=this.values_.get(key),value=void 0;if(!observable&&notify)value=null==(_getDescriptor=getDescriptor(this.target_,key))?void 0:_getDescriptor.value;if(proxyTrap){if(!Reflect.deleteProperty(this.target_,key))return!1}else delete this.target_[key];if(observable&&(this.values_.delete(key),observable instanceof ObservableValue&&(value=observable.value_),propagateChanged(observable)),this.keysAtom_.reportChanged(),null==(_this$pendingKeys_=this.pendingKeys_)||null==(_this$pendingKeys_=_this$pendingKeys_.get(key))||_this$pendingKeys_.set(key in this.target_),notify){var _change2={type:"remove",observableKind:"object",object:this.proxy_||this.target_,debugObjectName:this.name_,oldValue:value,name:key};0,notify&&notifyListeners(this,_change2)}}finally{endBatch()}return!0},_proto.observe_=function(callback,fireImmediately){return registerListener(this,callback)},_proto.intercept_=function(handler){
/*ThouShaltNotCache*/
return registerInterceptor(this,handler)},_proto.notifyPropertyAddition_=function(key,value){
/*ThouShaltNotCache*/
var _this$pendingKeys_2,notify=hasListeners(this);if(notify){var change=notify?{type:ADD,observableKind:"object",debugObjectName:this.name_,object:this.proxy_||this.target_,name:key,newValue:value}:null;0,notify&&notifyListeners(this,change)}null==(_this$pendingKeys_2=this.pendingKeys_)||null==(_this$pendingKeys_2=_this$pendingKeys_2.get(key))||_this$pendingKeys_2.set(!0),this.keysAtom_.reportChanged()},_proto.ownKeys_=function(){
/*ThouShaltNotCache*/
return this.keysAtom_.reportObserved(),ownKeys(this.target_)},_proto.keys_=function(){
/*ThouShaltNotCache*/
return this.keysAtom_.reportObserved(),Object.keys(this.target_)},ObservableObjectAdministration}();function asObservableObject(target,options){
/*ThouShaltNotCache*/
var _options$name;if(hasProp(target,$mobx))return target;var name=null!=(_options$name=null==options?void 0:options.name)?_options$name:"ObservableObject",adm=new ObservableObjectAdministration(target,new Map,String(name),function(options){
/*ThouShaltNotCache*/
var _options$defaultDecor;return options?null!=(_options$defaultDecor=options.defaultDecorator)?_options$defaultDecor:createAutoAnnotation(options):void 0}(options));return addHiddenProp(target,$mobx,adm),target}var isObservableObjectAdministration=createInstanceofPredicate("ObservableObjectAdministration",ObservableObjectAdministration);function getCachedObservablePropDescriptor(key){
/*ThouShaltNotCache*/
return descriptorCache[key]||(descriptorCache[key]={get:function(){
/*ThouShaltNotCache*/
return this[$mobx].getObservablePropValue_(key)},set:function(value){
/*ThouShaltNotCache*/
return this[$mobx].setObservablePropValue_(key,value)}})}function isObservableObject(thing){
/*ThouShaltNotCache*/
return!!isObject(thing)&&isObservableObjectAdministration(thing[$mobx])}function recordAnnotationApplied(adm,annotation,key){
/*ThouShaltNotCache*/
var _adm$target_$storedAn;null==(_adm$target_$storedAn=adm.target_[storedAnnotationsSymbol])||delete _adm$target_$storedAn[key]}function assertAnnotable(adm,annotation,key){}var ctor,proto,ENTRY_0=createArrayEntryDescriptor(0),safariPrototypeSetterInheritanceBug=function(){
/*ThouShaltNotCache*/
var v=!1,p={};return Object.defineProperty(p,"0",{set:function(){
/*ThouShaltNotCache*/
v=!0}}),Object.create(p)[0]=1,!1===v}(),OBSERVABLE_ARRAY_BUFFER_SIZE=0,StubArray=function(){};ctor=StubArray,proto=Array.prototype,
/*ThouShaltNotCache*/
Object.setPrototypeOf?Object.setPrototypeOf(ctor.prototype,proto):void 0!==ctor.prototype.__proto__?ctor.prototype.__proto__=proto:ctor.prototype=proto;var LegacyObservableArray=function(_StubArray){
/*ThouShaltNotCache*/
function LegacyObservableArray(initialValues,enhancer,name,owned){
/*ThouShaltNotCache*/
var _this;return void 0===name&&(name="ObservableArray"),void 0===owned&&(owned=!1),_this=_StubArray.call(this)||this,initObservable(function(){
/*ThouShaltNotCache*/
var adm=new ObservableArrayAdministration(name,enhancer,owned,!0);adm.proxy_=_this,addHiddenFinalProp(_this,$mobx,adm),initialValues&&initialValues.length&&_this.spliceWithArray(0,0,initialValues),safariPrototypeSetterInheritanceBug&&Object.defineProperty(_this,"0",ENTRY_0)}),_this}_inheritsLoose(LegacyObservableArray,_StubArray);var _proto=LegacyObservableArray.prototype;return _proto.concat=function(){
/*ThouShaltNotCache*/
this[$mobx].atom_.reportObserved();for(var _len=arguments.length,arrays=new Array(_len),_key=0;_key<_len;_key++)arrays[_key]=arguments[_key];return Array.prototype.concat.apply(this.slice(),arrays.map(function(a){
/*ThouShaltNotCache*/
return isObservableArray(a)?a.slice():a}))},_proto[Symbol.iterator]=function(){
/*ThouShaltNotCache*/
var self=this,nextIndex=0;return makeIterable({next:function(){
/*ThouShaltNotCache*/
return nextIndex<self.length?{value:self[nextIndex++],done:!1}:{done:!0,value:void 0}}})},_createClass(LegacyObservableArray,[{key:"length",get:function(){
/*ThouShaltNotCache*/
return this[$mobx].getArrayLength_()},set:function(newLength){
/*ThouShaltNotCache*/
this[$mobx].setArrayLength_(newLength)}},{key:Symbol.toStringTag,get:function(){
/*ThouShaltNotCache*/
return"Array"}}])}(StubArray);function createArrayEntryDescriptor(index){
/*ThouShaltNotCache*/
return{enumerable:!1,configurable:!0,get:function(){
/*ThouShaltNotCache*/
return this[$mobx].get_(index)},set:function(value){
/*ThouShaltNotCache*/
this[$mobx].set_(index,value)}}}function createArrayBufferItem(index){
/*ThouShaltNotCache*/
defineProperty(LegacyObservableArray.prototype,""+index,createArrayEntryDescriptor(index))}function reserveArrayBuffer(max){
/*ThouShaltNotCache*/
if(max>OBSERVABLE_ARRAY_BUFFER_SIZE){for(var index=OBSERVABLE_ARRAY_BUFFER_SIZE;index<max+100;index++)createArrayBufferItem(index);OBSERVABLE_ARRAY_BUFFER_SIZE=max}}function createLegacyArray(initialValues,enhancer,name){
/*ThouShaltNotCache*/
return new LegacyObservableArray(initialValues,enhancer,name)}function getAtom(thing,property){
/*ThouShaltNotCache*/
if("object"==typeof thing&&null!==thing){if(isObservableArray(thing))return void 0!==property&&die(23),thing[$mobx].atom_;if(isObservableSet(thing))return thing.atom_;if(isObservableMap(thing)){if(void 0===property)return thing.keysAtom_;var observable=thing.data_.get(property)||thing.hasMap_.get(property);return observable||die(25,property,getDebugName(thing)),observable}if(isObservableObject(thing)){if(!property)return die(26);var _observable=thing[$mobx].values_.get(property);return _observable||die(27,property,getDebugName(thing)),_observable}if(isAtom(thing)||isComputedValue(thing)||isReaction(thing))return thing}else if(isFunction(thing)&&isReaction(thing[$mobx]))return thing[$mobx];die(28)}function getAdministration(thing,property){
/*ThouShaltNotCache*/
return thing||die(29),void 0!==property?getAdministration(getAtom(thing,property)):isAtom(thing)||isComputedValue(thing)||isReaction(thing)||isObservableMap(thing)||isObservableSet(thing)?thing:thing[$mobx]?thing[$mobx]:void die(24,thing)}function getDebugName(thing,property){
/*ThouShaltNotCache*/
var named;if(void 0!==property)named=getAtom(thing,property);else{if(isAction(thing))return thing.name;named=isObservableObject(thing)||isObservableMap(thing)||isObservableSet(thing)?getAdministration(thing):getAtom(thing)}return named.name_}function initObservable(cb){
/*ThouShaltNotCache*/
var derivation=untrackedStart(),allowStateChanges=allowStateChangesStart(!0);startBatch();try{return cb()}finally{endBatch(),allowStateChangesEnd(allowStateChanges),untrackedEnd(derivation)}}Object.entries(arrayExtensions).forEach(function(_ref){
/*ThouShaltNotCache*/
var prop=_ref[0],fn=_ref[1];"concat"!==prop&&addHiddenProp(LegacyObservableArray.prototype,prop,fn)}),reserveArrayBuffer(1e3);var _getGlobal$Iterator,mobx_esm_toString=objectPrototype.toString;function deepEqual(a,b,depth){
/*ThouShaltNotCache*/
return void 0===depth&&(depth=-1),eq(a,b,depth)}function eq(a,b,depth,aStack,bStack){
/*ThouShaltNotCache*/
if(a===b)return 0!==a||1/a==1/b;if(null==a||null==b)return!1;if(a!=a)return b!=b;var type=typeof a;if("function"!==type&&"object"!==type&&"object"!=typeof b)return!1;var className=mobx_esm_toString.call(a);if(className!==mobx_esm_toString.call(b))return!1;switch(className){case"[object RegExp]":case"[object String]":return""+a==""+b;case"[object Number]":return+a!=+a?+b!=+b:0===+a?1/+a==1/b:+a===+b;case"[object Date]":case"[object Boolean]":return+a===+b;case"[object Symbol]":return"undefined"!=typeof Symbol&&Symbol.valueOf.call(a)===Symbol.valueOf.call(b);case"[object Map]":case"[object Set]":depth>=0&&depth++}a=unwrap(a),b=unwrap(b);var areArrays="[object Array]"===className;if(!areArrays){if("object"!=typeof a||"object"!=typeof b)return!1;var aCtor=a.constructor,bCtor=b.constructor;if(aCtor!==bCtor&&!(isFunction(aCtor)&&aCtor instanceof aCtor&&isFunction(bCtor)&&bCtor instanceof bCtor)&&"constructor"in a&&"constructor"in b)return!1}if(0===depth)return!1;depth<0&&(depth=-1),bStack=bStack||[];for(var length=(aStack=aStack||[]).length;length--;)if(aStack[length]===a)return bStack[length]===b;if(aStack.push(a),bStack.push(b),areArrays){if((length=a.length)!==b.length)return!1;for(;length--;)if(!eq(a[length],b[length],depth-1,aStack,bStack))return!1}else{var keys=Object.keys(a),_length=keys.length;if(Object.keys(b).length!==_length)return!1;for(var i=0;i<_length;i++){var key=keys[i];if(!hasProp(b,key)||!eq(a[key],b[key],depth-1,aStack,bStack))return!1}}return aStack.pop(),bStack.pop(),!0}function unwrap(a){
/*ThouShaltNotCache*/
return isObservableArray(a)?a.slice():isES6Map(a)||isObservableMap(a)||isES6Set(a)||isObservableSet(a)?Array.from(a.entries()):a}var maybeIteratorPrototype=(null==(_getGlobal$Iterator=getGlobal().Iterator)?void 0:_getGlobal$Iterator.prototype)||{};function makeIterable(iterator){
/*ThouShaltNotCache*/
return iterator[Symbol.iterator]=getSelf,Object.assign(Object.create(maybeIteratorPrototype),iterator)}function getSelf(){
/*ThouShaltNotCache*/
return this}["Symbol","Map","Set"].forEach(function(m){void 0===getGlobal()[m]&&die("MobX requires global '"+m+"' to be available or polyfilled")}),"object"==typeof __MOBX_DEVTOOLS_GLOBAL_HOOK__&&__MOBX_DEVTOOLS_GLOBAL_HOOK__.injectMobx({spy:function(listener){return console.warn("[mobx.spy] Is a no-op in production builds"),function(){}},extras:{getDebugName:getDebugName},$mobx:$mobx});class Person{id;name;lastMessage=null;rooms=new Set;status="Online";constructor(id,name){
/*ThouShaltNotCache*/
makeObservable(this,{name:observable,lastMessage:observable,rooms:observable,status:observable,setName:action,setLastMessage:action,addRoom:action,removeRoom:action,setStatus:action}),this.id=id,this.name=name}setName(name){
/*ThouShaltNotCache*/
this.name=name}setLastMessage(message){
/*ThouShaltNotCache*/
this.lastMessage=message}addRoom(room){
/*ThouShaltNotCache*/
this.rooms.add(room)}removeRoom(room){
/*ThouShaltNotCache*/
this.rooms.delete(room)}setStatus(status){
/*ThouShaltNotCache*/
this.status=status,this.rooms.forEach(room=>{
/*ThouShaltNotCache*/
room.addStatusUpdate(this,status)})}}class Room{name;people=new Map;messages=[];roomUpdates=[];constructor(name){
/*ThouShaltNotCache*/
makeObservable(this,{name:observable,people:observable,messages:observable,roomUpdates:observable,messageCount:computed,peopleCount:computed,lastMessage:computed,members:computed,addPerson:action,removePerson:action,addMessage:action,addStatusUpdate:action,dispose:action}),this.name=name}get messageCount(){
/*ThouShaltNotCache*/
return this.messages.length}get peopleCount(){
/*ThouShaltNotCache*/
return this.people.size}get lastMessage(){
/*ThouShaltNotCache*/
return 0===this.messages.length?null:this.messages[this.messages.length-1]}get members(){
/*ThouShaltNotCache*/
return Array.from(this.people.values())}addPerson(person){
/*ThouShaltNotCache*/
this.people.has(person.id)||(this.people.set(person.id,person),person.addRoom(this))}removePerson(personId){
/*ThouShaltNotCache*/
const person=this.people.get(personId);person&&(person.removeRoom(this),this.people.delete(personId))}addMessage(message){
/*ThouShaltNotCache*/
this.messages.push(message),message.author.setLastMessage(message)}addStatusUpdate(person,status){
/*ThouShaltNotCache*/
this.roomUpdates.push(`${person.name} is now ${status}`),this.roomUpdates.length>20&&(this.roomUpdates=this.roomUpdates.slice(10))}dispose(notifications){
/*ThouShaltNotCache*/
this.people.forEach(person=>person.removeRoom(this)),notifications&&notifications.removeRoom(this.name)}}class Message{static _nextId=0;id;text;author;timestamp;static nextId(){
/*ThouShaltNotCache*/
return Message._nextId++}constructor(text,author){
/*ThouShaltNotCache*/
makeObservable(this,{text:observable,updateText:action}),this.id=Message.nextId(),this.text=text,this.author=author,this.timestamp=new Date}updateText(text){
/*ThouShaltNotCache*/
this.text=text}}class Notifications{lastMessages=new Map;lastMessage;roomDisposers=new Map;constructor(rooms){
/*ThouShaltNotCache*/
makeObservable(this,{lastMessages:observable,updateLastMessage:action,addRoom:action,removeRoom:action}),rooms.forEach(room=>this.addRoom(room))}addRoom(room){
/*ThouShaltNotCache*/
const disposer=function(expression,effect,opts){
/*ThouShaltNotCache*/
var _opts$name2,_opts4,_opts5;void 0===opts&&(opts=EMPTY_OBJECT);var errorHandler,baseFn,value,name=null!=(_opts$name2=opts.name)?_opts$name2:"Reaction",effectAction=action(name,opts.onError?(errorHandler=opts.onError,baseFn=effect,function(){
/*ThouShaltNotCache*/
try{return baseFn.apply(this,arguments)}catch(e){errorHandler.call(this,e)}}):effect),runSync=!opts.scheduler&&!opts.delay,scheduler=createSchedulerFromOptions(opts),firstTime=!0,isScheduled=!1,equals=opts.compareStructural?comparer.structural:opts.equals||comparer.default,r=new Reaction(name,function(){
/*ThouShaltNotCache*/
firstTime||runSync?reactionRunner():isScheduled||(isScheduled=!0,scheduler(reactionRunner))},opts.onError,opts.requiresObservable);function reactionRunner(){if(
/*ThouShaltNotCache*/
isScheduled=!1,!r.isDisposed){var changed=!1,oldValue=value;r.track(function(){
/*ThouShaltNotCache*/
var nextValue=allowStateChanges(!1,function(){
/*ThouShaltNotCache*/
return expression(r)});changed=firstTime||!equals(value,nextValue),value=nextValue}),(firstTime&&opts.fireImmediately||!firstTime&&changed)&&effectAction(value,oldValue,r),firstTime=!1}}return null!=(_opts4=opts)&&null!=(_opts4=_opts4.signal)&&_opts4.aborted||r.schedule_(),r.getDisposer_(null==(_opts5=opts)?void 0:_opts5.signal)}(()=>room.lastMessage,message=>{
/*ThouShaltNotCache*/
message&&this.updateLastMessage(room.name,message)});this.roomDisposers.set(room.name,disposer)}removeRoom(roomName){
/*ThouShaltNotCache*/
this.roomDisposers.has(roomName)&&(this.roomDisposers.get(roomName)(),this.roomDisposers.delete(roomName),this.lastMessages.delete(roomName))}updateLastMessage(roomName,message){
/*ThouShaltNotCache*/
this.lastMessages.set(roomName,message)}}function assert(condition){
/*ThouShaltNotCache*/
if(!condition)throw new Error("Assertion failure")}function runTest(){
/*ThouShaltNotCache*/
const logs=[],customLog=(...args)=>logs.push(args.join(" ")),techRoom=new Room("Tech Talk"),generalRoom=new Room("General Chat"),largeRoom=new Room("Large Meeting Room"),alice=new Person(1,"Alice"),bob=new Person(2,"Bob");techRoom.addPerson(alice),techRoom.addPerson(bob),generalRoom.addPerson(alice);const rooms=[techRoom,generalRoom,largeRoom],notifications=new Notifications(rooms);customLog("\n--- Setting up Large Meeting Room ---");const largeRoomMembers=[];for(let i=3;i<203;i++){const person=new Person(i,`Person ${i}`);largeRoomMembers.push(person),largeRoom.addPerson(person)}customLog(`[INFO] Added ${largeRoom.peopleCount} members to the large room.`),largeRoom.addPerson(alice),largeRoom.addPerson(bob),customLog("[INFO] Added Alice and Bob to the Large Meeting Room."),assert(3===alice.rooms.size),assert(alice.rooms.has(techRoom)&&alice.rooms.has(generalRoom)&&alice.rooms.has(largeRoom)),assert(2===bob.rooms.size&&bob.rooms.has(techRoom)&&bob.rooms.has(largeRoom)),customLog("[PASS] Verified Person.rooms associations with Set.");const notificationsDisposer=autorun(()=>{
/*ThouShaltNotCache*/
customLog("\n--- Global Notifications (Last Messages) ---"),0===notifications.lastMessages.size?customLog("No messages yet."):notifications.lastMessages.forEach((message,roomName)=>{
/*ThouShaltNotCache*/
customLog(`[${roomName}] "${message.text}" - ${message.author.name}`)}),customLog("------------------------------------------")}),roomUpdatesDisposer=autorun(()=>{
/*ThouShaltNotCache*/
rooms.forEach(room=>{
/*ThouShaltNotCache*/
room.roomUpdates.length>0&&customLog(`[${room.name} Update] ${room.roomUpdates[room.roomUpdates.length-1]}`)})});customLog("\n--- Status Update Scenario ---"),alice.setStatus("Away"),assert(techRoom.roomUpdates.includes("Alice is now Away")),assert(generalRoom.roomUpdates.includes("Alice is now Away")),assert(largeRoom.roomUpdates.includes("Alice is now Away")),customLog("[PASS] Verified that status updates are sent to all of Alice's rooms."),bob.setStatus("Busy"),assert(techRoom.roomUpdates.includes("Bob is now Busy")),assert(!generalRoom.roomUpdates.includes("Bob is now Busy")),assert(largeRoom.roomUpdates.includes("Bob is now Busy")),customLog("[PASS] Verified that status updates are only sent to Bob's rooms."),customLog("\n--- Sending Messages & Verifying Notifications ---");const msg1=new Message("First message to Tech Talk!",alice);techRoom.addMessage(msg1),assert(notifications.lastMessages.get("Tech Talk")===msg1),customLog("[PASS] Verified Tech Talk notification.");for(let i=0;i<20;i++){const msg2=new Message(`ping ${i}`,alice);generalRoom.addMessage(msg2),assert(notifications.lastMessages.get("General Chat")===msg2)}customLog("[PASS] Verified General Chat notification.");for(let i=0;i<=5;i++)for(const member of largeRoomMembers){const msg=new Message(`Hi, I am ${member.name}`,member);largeRoom.addMessage(msg),assert(largeRoom.lastMessage==msg),assert(notifications.lastMessages.get("Large Meeting Room")===msg)}customLog("[PASS] Verified Large Meeting Room notification.");for(let i=0;i<20;i++){const msg4=new Message(`ping ${i}`,bob);techRoom.addMessage(msg4),assert(notifications.lastMessages.get("Tech Talk")===msg4)}customLog("[PASS] Verified Tech Talk notification update.");const tempRooms=[];for(let i=0;i<10;i++){const roomId=`General Chat ${i}`,tempRoom=new Room(roomId);tempRooms.push(tempRoom),notifications.addRoom(tempRoom);for(const person of largeRoomMembers.slice(0,10))tempRoom.addPerson(person);for(const person of tempRoom.members.slice(0,5)){const msg=new Message(`ping ${i}`,person);tempRoom.addMessage(msg),assert(notifications.lastMessages.get(roomId)===msg)}}return tempRooms.forEach(room=>room.dispose(notifications)),notificationsDisposer(),roomUpdatesDisposer(),techRoom.dispose(notifications),generalRoom.dispose(notifications),largeRoom.dispose(notifications),logs}}(),MobXBenchmark=__webpack_exports__}();
//# sourceMappingURL=bundle.es5.js.map
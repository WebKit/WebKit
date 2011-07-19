document.getElementById("description").innerHTML = "Tests that we clone object hierarchies";

tryPostMessage('null');
tryPostMessage('undefined');
tryPostMessage('1');
tryPostMessage('true');
tryPostMessage('"1"');
tryPostMessage('({})');
tryPostMessage('({a:1})');
tryPostMessage('({a:"a"})');
tryPostMessage('({b:"a", a:"b"})');
tryPostMessage('({p0:"string0", p1:"string1", p2:"string2", p3:"string3", p4:"string4", p5:"string5", p6:"string6", p7:"string7", p8:"string8", p9:"string9", p10:"string10", p11:"string11", p12:"string12", p13:"string13", p14:"string14", p15:"string15", p16:"string16", p17:"string17", p18:"string18", p19:"string19"})');
tryPostMessage('({p0:"string1", p1:"string1", p2:"string2", p3:"string3", p4:"string4", p5:"string5", p6:"string6", p7:"string7", p8:"string8", p9:"string9", p10:"string10", p11:"string11", p12:"string12", p13:"string13", p14:"string14", p15:"string15", p16:"string16", p17:"string17", p18:"string18", p19:"string19"})');
tryPostMessage('({a:""})');
tryPostMessage('({a:0})');
tryPostMessage('({a:1})');
tryPostMessage('[]');
tryPostMessage('["a", "a", "b", "a", "b"]');
tryPostMessage('["a", "a", "b", {a:"b", b:"a"}]');
tryPostMessage('[1,2,3]');
tryPostMessage('[,,1]');
tryPostMessage('(function(){})', true, null, DOMException.DATA_CLONE_ERR);
tryPostMessage('new Date(1234567890000)');
tryPostMessage('new ConstructorWithPrototype("foo")', false, '({field:"foo"})');
cyclicObject={};
cyclicObject.self = cyclicObject;
tryPostMessage('cyclicObject', false, "cyclicObject");
cyclicArray=[];
cyclicArray[0] = cyclicArray;
tryPostMessage('cyclicArray', false, "cyclicArray");
objectGraph = {};
object = {};
objectGraph.graph1 = object;
objectGraph.graph2 = object;
tryPostMessage('objectGraph', false, "objectGraph");
arrayGraph = [object, object];
tryPostMessage('arrayGraph', false, "arrayGraph");
tryPostMessage('window', true);
tryPostMessage('({get a() { throw "x" }})', true);

if (window.eventSender) {
    var fileInput = document.getElementById("fileInput");
    var fileRect = fileInput.getClientRects()[0];
    var targetX = fileRect.left + fileRect.width / 2;
    var targetY = fileRect.top + fileRect.height / 2;
    eventSender.beginDragWithFiles(['got-file-upload.html', 'got-file-upload-2.html']);
    eventSender.mouseMoveTo(targetX, targetY);
    eventSender.mouseUp();
}
var imageData = document.createElement("canvas").getContext("2d").createImageData(10,10);
for (var i = 0; i < imageData.data.length * 4; i++)
    imageData.data[i] = i % 256;
var mutatedImageData = document.createElement("canvas").getContext("2d").createImageData(10,10);
for (var i = 0; i < imageData.data.length * 4; i++)
    mutatedImageData.data[i] = i % 256;
tryPostMessage('imageData', false, imageData);
tryPostMessage('imageData.data', true, null, DOMException.DATA_CLONE_ERR)
tryPostMessage('mutatedImageData', false, imageData);
tryPostMessage('mutatedImageData.data', true, null, DOMException.DATA_CLONE_ERR)
for (var i = 0; i < imageData.data.length * 4; i++)
    mutatedImageData.data[i] = 0;

function thunk(s) {
    return "(function() {" + s + "})()";
}
tryPostMessage(thunk('return 42;'), false, '42');
tryPostMessage(thunk('return 42;'), false, thunk('return 40 + 2;'));
tryPostMessage(thunk('return 42;'), false, "evalThunk",
               function(v) { doPassFail(v == 42, "evalThunk OK"); })

// Only enumerable properties should be serialized.
tryPostMessage(thunk('var o = {x:"hello"}; Object.defineProperty(o, "y", {value:"goodbye"}); return o;'), false, '({x:"hello"})');

// It's unclear what we should do if an accessor modifies an object out from under us
// while we're serializing it; the standard does mandate certain aspects about evaluation
// order, though, including that properties must be processed in their enumeration order.
tryPostMessage(thunk(
        'var a = [0, 1, 2]; ' +
        'var b = { get x() { a[0] = 40; a[2] = 42; a.push(43); return 41; }}; ' +
        'a[1] = b; ' +
        'return a;'
    ), false, "evalThunk", function(v) {
        doPassFail(v.length === 3, "length correct"); // undefined
        doPassFail(v[0] === 0, "evaluation order OK"); // mandatory
        doPassFail(v[1].x === 41, "evaluation order OK/accessor reached"); // mandatory
        doPassFail(v[2] === 42, "evaluation order OK"); // mandatory
    });

tryPostMessage(thunk(
        'var a = [0, 1, 2]; ' +
        'var b = { get x() { a.pop(); return 41; } }; ' +
        'a[1] = b; ' +
        'return a;'
    ), false, "evalThunk", function(v) {
        doPassFail(v.length === 3, "length correct"); // undefined
        doPassFail(v[0] === 0, "index 0 OK"); // mandatory
        doPassFail(v[1].x === 41, "accessor reached"); // mandatory
        doPassFail(v[2] === undefined, "index 2 undefined"); // undefined
    });

tryPostMessage(thunk(
        'var a = [0, 1, 2]; ' +
        'var b = { get x() { a.pop(); return 41; } }; ' +
        'a[2] = b; ' +
        'return a;'
    ), false, "evalThunk", function(v) {
        doPassFail(v.length === 3, "length correct"); // undefined
        doPassFail(v[0] === 0, "index 0 OK"); // mandatory
        doPassFail(v[1] === 1, "index 1 OK"); // mandatory
        doPassFail(v[2].x === 41, "index 2 OK"); // undefined
    });

// Now with objects! This is a little more tricky because the standard does not
// define an enumeration order.
tryPostMessage(thunk(
        'var a = {p0: 0, p1: 1}; ' +
        'Object.defineProperty(a, "p2", {get:function() {delete a.p3; return 42; }, enumerable: true, configurable: true}); ' +
        'Object.defineProperty(a, "p3", {get:function() {delete a.p2; return 43; }, enumerable: true, configurable: true}); ' +
        'Object.defineProperty(a, "p4", {get:function() { a.p5 = 45; return 44; }, enumerable: true, configurable: true}); ' +
        'return a;'
    ), false, "evalThunk", function(v) {
        doPassFail(v.p0 === 0 && v.p1 === 1, "basic properties OK"); // mandatory
        doPassFail(v.p2 === undefined && v.p3 !== undefined ||
                   v.p2 !== undefined && v.p3 === undefined, "one accessor was run"); // mandatory
        doPassFail(v.p2 !== undefined || Object.getOwnPropertyDescriptor(v, "p2") === undefined, "property was removed"); // undefined
        doPassFail(v.p3 !== undefined || Object.getOwnPropertyDescriptor(v, "p3") === undefined, "property was removed"); // undefined
        doPassFail(v.p4 === 44, "accessor was run"); // mandatory
        doPassFail(Object.getOwnPropertyDescriptor(v, "p5") === undefined, "dynamic property not sent"); // undefined
    });

// Objects returned from accessors should still be coalesced.
tryPostMessage(thunk(
        'var obja = {get p() { return 42; }}; ' +
        'var msg = {get a() { return obja; }, b: obja}; ' +
        'return msg;'
    ), false, "evalThunk", function(v) {
        // Interestingly, the standard admits two answers here!
        doPassFail(v.a === v.b, "reference equality preserved");
        doPassFail((v.b.p === 42 && v.a.p === 42) ||
            (v.b.p === null && v.a.p === null), "accessors used");
    });
tryPostMessage(thunk(
        'var obja = {get p() { return 42; }}; ' +
        'var msg = {a: obja, get b() { return obja; }}; ' +
        'return msg;'
    ), false, "evalThunk", function(v) {
        // Interestingly, the standard admits two answers here!
        doPassFail(v.a === v.b, "reference equality preserved (opposite order)");
        doPassFail((v.b.p === 42 && v.a.p === 42) ||
                   (v.b.p === null && v.a.p === null), "accessors used (opposite order)");
    });
// We should nullify the results from accessors more than one level deep,
// but leave other fields untouched.
tryPostMessage(thunk(
        'var obja = {get p() { return 42; }, q: 43}; ' +
        'return {get a() { return obja; }};'
    ), false, "evalThunk", function(v) {
        doPassFail(v.a.p === null, "accessor value was nullified");
        doPassFail(v.a.q === 43, "non-accessor value was not nullified");
    });
tryPostMessage(thunk(
        'var objb = {get r() { return 44; }, t: 45}; ' +
        'var obja = {get p() { return 42; }, q: 43, s: objb}; ' +
        'return {get a() { return obja; }};'
    ), false, "evalThunk", function(v) {
        doPassFail(v.a.p === null, "accessor value was nullified");
        doPassFail(v.a.q === 43, "non-accessor value was not nullified");
        doPassFail(v.s !== null, "non-accessor value was not nullified");
        doPassFail(v.a.s.r === null, "accessor value was nullified");
        doPassFail(v.a.s.t === 45, "non-accessor value was not nullified");
    });
tryPostMessage(thunk(
        'var objb = {get r() { return 44; }, t: 45}; ' +
        'var obja = {get p() { return 42; }, q: 43, s: [objb]}; ' +
        'return {get c() { return 47; }, get a() { return obja; }, get b() { return 46; } };'
    ), false, "evalThunk", function(v) {
        doPassFail(v.b === 46, "accessor value was not nullified");
        doPassFail(v.c === 47, "accessor value was not nullified");
        doPassFail(v.a.p === null, "accessor value was nullified");
        doPassFail(v.a.q === 43, "non-accessor value was not nullified");
        doPassFail(v.a.s !== null, "non-accessor value was not nullified");
        doPassFail(v.a.s !== undefined, "non-accessor value is defined");
        doPassFail(v.a.s[0] !== null, "non-accessor value was not nullified");
        doPassFail(v.a.s[0] !== undefined, "non-accessor value is defined");
        doPassFail(v.a.s[0].r === null, "accessor value was nullified");
        doPassFail(v.a.s[0].t === 45, "non-accessor value was not nullified");
    });

// We need to pass out the exception raised from internal accessors.
tryPostMessage(thunk(
        'return {get a() { throw "accessor-exn"; }};'
    ), true, null, 'accessor-exn');
// We should still return the exception raised from nulled-out accessors.
tryPostMessage(thunk(
        'var obja = {get p() { throw "accessor-exn"; }}; ' +
        'return {get a() { return obja; }};'
    ), true, null, 'accessor-exn');
// We should run the first nullified accessor, but no more.
tryPostMessage(thunk(
        'window.bcalled = undefined; ' +
        'window.acalled = undefined; ' +
        'window.pcalled = undefined; ' +
        'var objb = {get b() { window.bcalled = true; return 42; }}; ' +
        'var obja = {get a() { window.acalled = true; return objb; }}; ' +
        'return { get p() { window.pcalled = true; return obja; }};'
    ), false, "evalThunk", function(v) {
        doPassFail(v.p.a === null, "accessor value was nullified");
        doPassFail(window.pcalled === true, "window.pcalled === true");
        doPassFail(window.acalled === true, "window.acalled === true");
        doPassFail(window.bcalled === undefined, "window.bcalled === undefined");
    });

// Reference equality between dates must be maintained.
tryPostMessage(thunk(
        'var d1 = new Date(1,2,3); ' +
        'var d2 = new Date(1,2,3); ' +
        'return [d1,d1,d2];'
    ), false, "evalThunk", function(v) {
        doPassFail(equal(v[0], new Date(1,2,3)), "Date values correct (0)");
        doPassFail(equal(v[0], v[2]), "Date values correct (1)");
        doPassFail(v[0] === v[1], "References to dates correct (0)");
        doPassFail(v[0] !== v[2], "References to dates correct (1)");
    });

// Reference equality between regexps must be preserved.
tryPostMessage(thunk(
        'var rx1 = new RegExp("foo"); ' +
        'var rx2 = new RegExp("foo"); ' +
        'var rx3 = new RegExp("foo", "gim"); ' +
        'rx3.exec("foofoofoo"); ' +
        'doPassFail(rx3.lastIndex === 3, "lastIndex initially correct: was " + rx3.lastIndex); ' +
        'return [rx1,rx1,rx2,rx3];'
    ), false, "evalThunk", function(v) {
        doPassFail(v[0].source === "foo", "Regexp value correct (0)");
        doPassFail(v[0] === v[1], "References to regexps correct (0)");
        doPassFail(v[0] !== v[2], "References to regexps correct (1)");
        doPassFail(v[0].global === false, "global set (0)");
        doPassFail(v[3].global === true, "global set (1)");
        doPassFail(v[0].ignoreCase === false, "ignoreCase set (0)");
        doPassFail(v[3].ignoreCase === true, "ignoreCase set (1)");
        doPassFail(v[0].multiline === false, "multiline set (0)");
        doPassFail(v[3].multiline === true, "multiline set (1)");
        doPassFail(v[0].lastIndex === 0, "lastIndex correct (0)");
        doPassFail(v[3].lastIndex === 0, "lastIndex correct (1)");
    });

if (window.eventSender) {
    tryPostMessage(thunk(
        'window.fileList = fileInput.files; ' +
        'window.file0 = fileList[0]; ' +
        'window.file1 = fileList[1]; ' +
        'return window.fileList.length'), false, 2);
    doPassFail(window.fileList[0] === window.file0, "sanity on file reference equality")
    // The standard mandates that we do _not_ maintain reference equality for files in a transferred FileList.
    tryPostMessage(thunk('return [window.file0, window.file0];'
        ), false, "evalThunk", function(v) { doPassFail(v[0] === v[1], "file references transfer")});
    tryPostMessage(thunk('return [window.fileList, window.file0];'
        ), false, "evalThunk", function(v) { doPassFail(v[0][0] !== v[1], "FileList should not respect reference equality")});
    tryPostMessage(thunk('return [window.file0, window.fileList];'
        ), false, "evalThunk", function(v) { doPassFail(v[1][0] !== v[0], "FileList should not respect reference equality")});
    tryPostMessage(thunk('return [window.fileList, window.fileList];'
        ), false, "evalThunk", function(v) { doPassFail(v[0] === v[1], "FileList respects self-reference equality")});
    tryPostMessage(thunk('return [window.fileList, window.file0, window.file1]'
        ), false, "evalThunk", function(v) {
            doPassFail(v[0].length === window.fileList.length, "FileList length sent correctly");
            doPassFail(v[0][0] !== v[1], "FileList should not respect reference equality (0)");
            doPassFail(v[0][1] !== v[2], "FileList should not respect reference equality (1)");
            doPassFail(v[0][0].name == window.file0.name, "FileList preserves order and data (name0)");
            doPassFail(v[0][1].name == window.file1.name, "FileList preserves order and data (name1)");
            doPassFail(equal(v[0][0].lastModifiedDate, window.file0.lastModifiedDate), "FileList preserves order and data (date0)");
            doPassFail(equal(v[0][1].lastModifiedDate, window.file1.lastModifiedDate), "FileList preserves order and data (date1)");
        });
}
tryPostMessage('"done"');

//@ runDefault("--useDollarVM=1", "--jitPolicyScale=0.01")
var createDOMJITCheckJSCastObject = $vm.createDOMJITCheckJSCastObject;

function calling(obj)
{
    return obj.func();
}
noInline(calling);

for (var i = 0; i < 1e5; ++i)
    calling(createDOMJITCheckJSCastObject());

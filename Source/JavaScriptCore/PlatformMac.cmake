include(PlatformCocoa.cmake)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSCallbackFunction.h
    API/JSContext.h
    API/JSContextPrivate.h
    API/JSContextRefPrivate.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSStringRefCF.h
    API/JSValue.h
    API/JSValuePrivate.h
    API/JSVirtualMachine.h
    API/JavaScriptCore.h
)

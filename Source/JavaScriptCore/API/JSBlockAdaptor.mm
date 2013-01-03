/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#import "JavaScriptCore.h"

#if JS_OBJC_API_ENABLED

#import "APICast.h"
#import "APIShims.h"
#import "Error.h"
#import "JSBlockAdaptor.h"
#import "JSContextInternal.h"
#import "JSWrapperMap.h"
#import "JSValue.h"
#import "JSValueInternal.h"
#import "ObjcRuntimeExtras.h"
#import "wtf/OwnPtr.h"

extern "C" {
id __NSMakeSpecialForwardingCaptureBlock(const char *signature, void (^handler)(NSInvocation *));
}

class BlockArgument {
public:
    virtual ~BlockArgument();
    virtual JSValueRef get(NSInvocation *, NSInteger, JSContext *, JSValueRef*) = 0;

    OwnPtr<BlockArgument> m_next;
};

BlockArgument::~BlockArgument()
{
}

class BlockArgumentBoolean : public BlockArgument {
    virtual JSValueRef get(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef*) override
    {
        bool value;
        [invocation getArgument:&value atIndex:argumentNumber];
        return JSValueMakeBoolean(contextInternalContext(context), value);
    }
};

template<typename T>
class BlockArgumentNumeric : public BlockArgument {
    virtual JSValueRef get(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef*) override
    {
        T value;
        [invocation getArgument:&value atIndex:argumentNumber];
        return JSValueMakeNumber(contextInternalContext(context), value);
    }
};

class BlockArgumentId : public BlockArgument {
    virtual JSValueRef get(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef*) override
    {
        id value;
        [invocation getArgument:&value atIndex:argumentNumber];
        return objectToValue(context, value);
    }
};

class BlockArgumentStruct : public BlockArgument {
public:
    BlockArgumentStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    virtual JSValueRef get(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef*) override
    {
        [invocation getArgument:m_buffer atIndex:argumentNumber];

        [m_conversionInvocation setArgument:m_buffer atIndex:2];
        [m_conversionInvocation setArgument:&context atIndex:3];
        [m_conversionInvocation invokeWithTarget:[JSValue class]];

        JSValue *value;
        [m_conversionInvocation getReturnValue:&value];
        return valueInternalValue(value);
    }

    RetainPtr<NSInvocation> m_conversionInvocation;
    StructBuffer m_buffer;
};

class BlockArgumentTypeDelegate {
public:
    typedef BlockArgument* ResultType;

    template<typename T>
    static ResultType typeInteger()
    {
        return new BlockArgumentNumeric<T>;
    }

    template<typename T>
    static ResultType typeDouble()
    {
        return new BlockArgumentNumeric<T>;
    }

    static ResultType typeBool()
    {
        return new BlockArgumentBoolean;
    }

    static ResultType typeVoid()
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    static ResultType typeId()
    {
        return new BlockArgumentId;
    }

    static ResultType typeOfClass(const char*, const char*)
    {
        return new BlockArgumentId;
    }

    static ResultType typeBlock(const char*, const char*)
    {
        return 0;
    }

    static ResultType typeStruct(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        if (NSInvocation *invocation = valueToTypeInvocationFor(copy))
            return new BlockArgumentStruct(invocation, copy);
        return 0;
    }
};

class BlockResult {
public:
    virtual ~BlockResult()
    {
    }

    virtual void set(NSInvocation *, JSContext *, JSValueRef, JSValueRef*) = 0;
};

class BlockResultVoid : public BlockResult {
    virtual void set(NSInvocation *, JSContext *, JSValueRef, JSValueRef*) override
    {
    }
};

template<typename T>
class BlockResultInteger : public BlockResult {
    virtual void set(NSInvocation *invocation, JSContext *context, JSValueRef result, JSValueRef* exception) override
    {
        T value = (T)JSC::toInt32(JSValueToNumber(contextInternalContext(context), result, exception));
        [invocation setReturnValue:&value];
    }
};

template<typename T>
class BlockResultDouble : public BlockResult {
    virtual void set(NSInvocation *invocation, JSContext *context, JSValueRef result, JSValueRef* exception) override
    {
        T value = (T)JSValueToNumber(contextInternalContext(context), result, exception);
        [invocation setReturnValue:&value];
    }
};

class BlockResultBoolean : public BlockResult {
    virtual void set(NSInvocation *invocation, JSContext *context, JSValueRef result, JSValueRef*) override
    {
        bool value = JSValueToBoolean(contextInternalContext(context), result);
        [invocation setReturnValue:&value];
    }
};

class BlockResultStruct : public BlockResult {
public:
    BlockResultStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    virtual void set(NSInvocation *invocation, JSContext *context, JSValueRef result, JSValueRef*) override
    {
        JSValue *value = [JSValue valueWithValue:result inContext:context];
        [m_conversionInvocation invokeWithTarget:value];
        [m_conversionInvocation getReturnValue:m_buffer];
        [invocation setReturnValue:&value];
    }

    RetainPtr<NSInvocation> m_conversionInvocation;
    StructBuffer m_buffer;
};

@implementation JSBlockAdaptor {
    RetainPtr<NSString> m_signatureWithOffsets;
    RetainPtr<NSString> m_signatureWithoutOffsets;
    OwnPtr<BlockResult> m_result;
    OwnPtr<BlockArgument> m_arguments;
    size_t m_argumentCount;
}

// Helper function to add offset information back into a block signature.
static NSString *buildBlockSignature(NSString *prefix, const char* begin, const char* end, unsigned long long& offset, NSString *postfix)
{
    StringRange copy(begin, end);
    NSString *result = [NSString stringWithFormat:@"%@%s%@%@", prefix, copy.get(), @(offset), postfix];
    NSUInteger size;
    NSGetSizeAndAlignment(copy, &size, 0);
    if (size < 4)
        size = 4;
    offset += size;
    return result;
}

- (id)initWithBlockSignatureFromProtocol:(const char*)encodedType
{
    self = [super init];
    if (!self)
        return nil;

    m_signatureWithoutOffsets = [NSString stringWithUTF8String:encodedType];
    m_argumentCount = 0;

    // Currently only adapting JavaScript functions to blocks returning void.
    const char* resultTypeBegin = encodedType;
    if (*encodedType++ != 'v')
        return self;
    OwnPtr<BlockResult> result = adoptPtr(new BlockResultVoid);
    const char* resultTypeEnd = encodedType;

    if (encodedType[0] != '@' || encodedType[1] != '?')
        return self;
    encodedType += 2;

    // The first argument to a block is the block itself.
    NSString *signature = @"@?0";
    unsigned long long offset = sizeof(void*);

    OwnPtr<BlockArgument> arguments;
    OwnPtr<BlockArgument>* nextArgument = &arguments;
    while (*encodedType) {
        const char* begin = encodedType;
        OwnPtr<BlockArgument> argument = adoptPtr(parseObjCType<BlockArgumentTypeDelegate>(encodedType));
        if (!argument)
            return self;
        const char* end = encodedType;

        // Append the type & stack offset of this argument to the signature string.
        signature = buildBlockSignature(signature, begin, end, offset, @"");

        *nextArgument = argument.release();
        nextArgument = &(*nextArgument)->m_next;
        ++m_argumentCount;
    }

    // Prefix the signature with the return type & total stackframe size.
    // (this call will also add the return type size to the offset,
    // which is a nonsense, but harmless since this is the last use).
    signature = buildBlockSignature(@"", resultTypeBegin, resultTypeEnd, offset, signature);

    m_signatureWithOffsets = signature;
    m_result = result.release();
    m_arguments = arguments.release();

    return self;
}

// The JSBlockAdaptor stores the type string taken from the protocol. This string
// does not contain invocation layout information. The signature from the block
// does contain the stackframe offsets. Skip these when comparing strings.
- (bool)blockMatchesSignature:(id)block
{
    if (!_Block_has_signature(block))
        return false;

    const char* withoutOffsets = [m_signatureWithoutOffsets UTF8String];
    const char* signature = _Block_signature(block);

    while (true) {
        while (isdigit(*withoutOffsets))
            ++withoutOffsets;
        while (isdigit(*signature))
            ++signature;
        
        if (*withoutOffsets != *signature)
            return false;
        if (!*withoutOffsets)
            return true;
        ++withoutOffsets;
        ++signature;
    }
}

- (id)blockFromValue:(JSValueRef)argument inContext:(JSContext *)context withException:(JSValueRef*)exception
{
    JSGlobalContextRef contextRef = contextInternalContext(context);

    // Check for null/undefined.
    if (JSValueIsUndefined(contextRef, argument) || JSValueIsNull(contextRef, argument))
        return nil;

    JSObjectRef function = 0;
    if (id object = tryUnwrapObjcObject(contextRef, argument)) {
        // Check if the argument is an Objective-C block.
        if ([object isKindOfClass:getNSBlockClass()]) {
            if ([self blockMatchesSignature:object])
                return object;
        }
    } else if (m_signatureWithOffsets && JSValueIsObject(contextRef, argument)) {
        // Check if the argument is a JavaScript function
        // (and that were able to create a forwarding block for this signature).
        JSObjectRef object = JSValueToObject(contextRef, argument, exception);
        if (JSObjectIsFunction(contextRef, object))
            function = object;
    }

    if (!function) {
        JSC::APIEntryShim entryShim(toJS(contextRef));
        *exception = toRef(JSC::createTypeError(toJS(contextRef), "Invalid argument supplied for Objective-C block"));
        return nil;
    }

    // Captured variables.
    JSBlockAdaptor *adaptor = self;
    JSValue *value = [JSValue valueWithValue:function inContext:context];

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=105895
    // Currently only supporting void functions.
    return __NSMakeSpecialForwardingCaptureBlock([m_signatureWithOffsets UTF8String], ^(NSInvocation *invocation){
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=105894
        // Rather than requiring that the context be retained, it would probably be more
        // appropriate to use a new JSContext instance (creating one if necessary).
        // It would be nice if we could throw a JS exception here, but without a context we have
        // nothing to work with.
        JSContext *context = value.context;
        if (!context)
            return;
        JSGlobalContextRef contextRef = contextInternalContext(context);

        JSValueRef argumentArray[adaptor->m_argumentCount];

        size_t argumentNumber = 0;
        BlockArgument* arguments = adaptor->m_arguments.get();
        for (BlockArgument* argument = arguments; argument; argument = argument->m_next.get()) {
            JSValueRef exception = 0;
            argumentArray[argumentNumber] = argument->get(invocation, argumentNumber + 1, context, &exception);
            if (exception) {
                [context notifyException:exception];
                return;
            }
            
            ++argumentNumber;
        }
        size_t argumentCount = argumentNumber;

        JSValueRef exception = 0;
        JSObjectCallAsFunction(contextRef, JSValueToObject(contextRef, valueInternalValue(value), 0), 0, argumentCount, argumentArray, &exception);
        if (exception) {
            [context notifyException:exception];
            return;
        }
    });
}

@end

#endif

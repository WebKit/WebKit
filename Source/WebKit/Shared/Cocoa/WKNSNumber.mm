/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKNSNumber.h"

using namespace WebKit;

@implementation WKNSNumber {
    union {
        API::ObjectStorage<API::Boolean> _boolean;
        API::ObjectStorage<API::Double> _double;
        API::ObjectStorage<API::UInt64> _uint64;
        API::ObjectStorage<API::Int64> _int64;
    } _number;
}

- (void)dealloc
{
    switch (_type) {
    case API::Object::Type::Boolean:
        _number._boolean->~Number<bool, API::Object::Type::Boolean>();
        break;

    case API::Object::Type::Double:
        _number._double->~Number<double, API::Object::Type::Double>();
        break;

    case API::Object::Type::UInt64:
        _number._uint64->~Number<uint64_t, API::Object::Type::UInt64>();
        break;

    case API::Object::Type::Int64:
        _number._int64->~Number<int64_t, API::Object::Type::Int64>();
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    [super dealloc];
}

// MARK: NSValue primitive methods

- (const char *)objCType
{
    switch (_type) {
    case API::Object::Type::Boolean:
        return @encode(bool);
        break;

    case API::Object::Type::Double:
        return @encode(double);
        break;

    case API::Object::Type::UInt64:
        return @encode(uint64_t);
        break;

    case API::Object::Type::Int64:
        return @encode(int64_t);
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    return nullptr;
}

- (void)getValue:(void *)value
{
    switch (_type) {
    case API::Object::Type::Boolean:
        *reinterpret_cast<bool*>(value) = _number._boolean->value();
        break;

    case API::Object::Type::Double:
        *reinterpret_cast<double*>(value) = _number._double->value();
        break;

    case API::Object::Type::UInt64:
        *reinterpret_cast<uint64_t*>(value) = _number._uint64->value();
        break;

    case API::Object::Type::Int64:
        *reinterpret_cast<int64_t*>(value) = _number._int64->value();
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

// MARK: NSNumber primitive methods

- (char)charValue
{
    if (_type == API::Object::Type::Boolean)
        return _number._boolean->value();

    return super.charValue;
}

- (double)doubleValue
{
    if (_type == API::Object::Type::Double)
        return _number._double->value();

    return super.doubleValue;
}

- (unsigned long long)unsignedLongLongValue
{
    if (_type == API::Object::Type::UInt64)
        return _number._uint64->value();

    return super.unsignedLongLongValue;
}

- (long long)longLongValue
{
    if (_type == API::Object::Type::Int64)
        return _number._int64->value();

    return super.longLongValue;
}

// MARK: NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

// MARK: WKObject protocol implementation

- (API::Object&)_apiObject
{
    switch (_type) {
    case API::Object::Type::Boolean:
        return *_number._boolean;
        break;

    case API::Object::Type::Double:
        return *_number._double;
        break;

    case API::Object::Type::UInt64:
        return *_number._uint64;
        break;

    case API::Object::Type::Int64:
        return *_number._int64;
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    return *_number._boolean;
}

@end

/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CommonHeader.h"
#if USE(GLIB)
#include "CoreIPCGFooBar.h"
#endif
#include "CustomEncoded.h"
#if ENABLE(TEST_FEATURE)
#include "FirstMemberType.h"
#endif
#include "FooWrapper.h"
#include "FormDataReference.h"
#include "GeneratedSerializers.h"
#include "GeneratedWebKitSecureCoding.h"
#include "HeaderWithoutCondition"
#include "LayerProperties.h"
#include "RValueWithFunctionCalls.h"
#include "RemoteVideoFrameIdentifier.h"
#if ENABLE(TEST_FEATURE)
#include "SecondMemberType.h"
#endif
#if ENABLE(TEST_FEATURE)
#include "StructHeader.h"
#endif
#include "TemplateTest.h"

#include <Namespace/EmptyConstructorStruct.h>
#include <Namespace/EmptyConstructorWithIf.h>
#if !(ENABLE(OUTER_CONDITION))
#include <Namespace/OtherOuterClass.h>
#endif
#if ENABLE(OUTER_CONDITION)
#include <Namespace/OuterClass.h>
#endif
#include <Namespace/ReturnRefClass.h>
#if USE(APPKIT)
#include <WebCore/AppKitControlSystemImage.h>
#endif
#include <WebCore/FloatBoxExtent.h>
#include <WebCore/InheritanceGrandchild.h>
#include <WebCore/InheritsFrom.h>
#include <WebCore/MoveOnlyBaseClass.h>
#include <WebCore/MoveOnlyDerivedClass.h>
#include <WebCore/OpaqueTypeObject.h>
#if USE(APPKIT)
#include <WebCore/ScrollbarTrackCornerSystemImageMac.h>
#endif
#include <WebCore/ScrollingStateFrameHostingNode.h>
#include <WebCore/ScrollingStateFrameHostingNodeWithStuffAfterTuple.h>
#include <WebCore/TimingFunction.h>
#include <wtf/CreateUsingClass.h>
#include <wtf/IsIncreasing.h>
#include <wtf/Seconds.h>

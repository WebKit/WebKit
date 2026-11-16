/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"

#if HAVE(CORE_MATERIAL)

#import <pal/spi/cocoa/CoreMaterialSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, PAL_EXPORT)

SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTMaterialLayer, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipeNone, NSString *, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentLight, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformChromeLight, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentThickLight, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentThinLight, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentUltraThinLight, NSString *, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentDark, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformChromeDark, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentThickDark, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentThinDark, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialRecipePlatformContentUltraThinDark, NSString *, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleCategoryStroke, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleCategoryFill, NSString *, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleNone, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStylePrimary, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleSecondary, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleTertiary, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleQuaternary, NSString *, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTCoreMaterialVisualStyleSeparator, NSString *, PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMaterial, MTVisualStylingCreateDictionaryRepresentation, NSDictionary *, (MTCoreMaterialRecipe recipe, MTCoreMaterialVisualStyleCategory category, MTCoreMaterialVisualStyle style, NSDictionary *options), (recipe, category, style, options), PAL_EXPORT)

#endif // HAVE(CORE_MATERIAL)

/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "FilterFunction.h"

#include "ImageBuffer.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FilterFunction::FilterFunction(Type filterType)
    : m_filterType(filterType)
{
}

AtomString FilterFunction::filterName(Type filterType)
{
    static constexpr std::pair<FilterFunction::Type, ASCIILiteral> namesArray[] = {
        { FilterFunction::Type::CSSFilter,           "CSSFilter"_s           },
        { FilterFunction::Type::SVGFilter,           "SVGFilter"_s           },
        
        { FilterFunction::Type::FEBlend,             "FEBlend"_s             },
        { FilterFunction::Type::FEColorMatrix,       "FEColorMatrix"_s       },
        { FilterFunction::Type::FEComponentTransfer, "FEComponentTransfer"_s },
        { FilterFunction::Type::FEComposite,         "FEComposite"_s         },
        { FilterFunction::Type::FEConvolveMatrix,    "FEConvolveMatrix"_s    },
        { FilterFunction::Type::FEDiffuseLighting,   "FEDiffuseLighting"_s   },
        { FilterFunction::Type::FEDisplacementMap,   "FEDisplacementMap"_s   },
        { FilterFunction::Type::FEDropShadow,        "FEDropShadow"_s        },
        { FilterFunction::Type::FEFlood,             "FEFlood"_s             },
        { FilterFunction::Type::FEGaussianBlur,      "FEGaussianBlur"_s      },
        { FilterFunction::Type::FEImage,             "FEImage"_s             },
        { FilterFunction::Type::FEMerge,             "FEMerge"_s             },
        { FilterFunction::Type::FEMorphology,        "FEMorphology"_s        },
        { FilterFunction::Type::FEOffset,            "FEOffset"_s            },
        { FilterFunction::Type::FESpecularLighting,  "FESpecularLighting"_s  },
        { FilterFunction::Type::FETile,              "FETile"_s              },
        { FilterFunction::Type::FETurbulence,        "FETurbulence"_s        },

        { FilterFunction::Type::SourceAlpha,         "SourceAlpha"_s         },
        { FilterFunction::Type::SourceGraphic,       "SourceGraphic"_s       }
    };

    static constexpr SortedArrayMap namesMap { namesArray };
    
    ASSERT(namesMap.tryGet(filterType));
    return namesMap.get(filterType, ""_s);
}

TextStream& operator<<(TextStream& ts, const FilterFunction& filterFunction)
{
    return filterFunction.externalRepresentation(ts, FilterRepresentation::Debugging);
}

} // namespace WebCore

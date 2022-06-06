/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TextFlags.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior::Behavior behavior)
{
    switch (behavior) {
    case ExpansionBehavior::Behavior::Forbid: ts << "forbid"; break;
    case ExpansionBehavior::Behavior::Allow: ts << "allow"; break;
    case ExpansionBehavior::Behavior::Force: ts << "force"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior expansionBehavior)
{
    ts << expansionBehavior.left;
    ts << ' ';
    ts << expansionBehavior.right;
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, Kerning kerning)
{
    switch (kerning) {
    case Kerning::Auto: ts << "auto"; break;
    case Kerning::Normal: ts << "normal"; break;
    case Kerning::NoShift: ts << "no-shift"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantAlternates alternates)
{
    switch (alternates) {
    case FontVariantAlternates::Normal: ts << "normal"; break;
    case FontVariantAlternates::HistoricalForms: ts << "historical-forms"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantPosition position)
{
    switch (position) {
    case FontVariantPosition::Normal: ts << "normal"; break;
    case FontVariantPosition::Subscript: ts << "subscript"; break;
    case FontVariantPosition::Superscript: ts << "superscript"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantCaps caps)
{
    switch (caps) {
    case FontVariantCaps::Normal: ts << "normal"; break;
    case FontVariantCaps::Small: ts << "small"; break;
    case FontVariantCaps::AllSmall: ts << "all-small"; break;
    case FontVariantCaps::Petite: ts << "petite"; break;
    case FontVariantCaps::AllPetite: ts << "all-petite"; break;
    case FontVariantCaps::Unicase: ts << "unicase"; break;
    case FontVariantCaps::Titling: ts << "titling"; break;
    }
    return ts;
}

} // namespace WebCore

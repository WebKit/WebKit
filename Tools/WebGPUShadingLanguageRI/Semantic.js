/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
"use strict";

class Semantic extends Node {
    constructor(origin)
    {
        super();
        this._origin = origin;
    }

    get origin() { return this._origin; }

    equalToOtherSemantic(otherSemantic)
    {
        class Comparer extends Visitor {
            visitBuiltInSemantic(node)
            {
                if (!(otherSemantic instanceof BuiltInSemantic))
                    return false;
                if (node.name != otherSemantic.name)
                    return false;
                if (node.extraArguments && otherSemantic.extraArguments) {
                    if (node.extraArguments.length != otherSemantic.extraArguments.length)
                        return false;
                    for (let i = 0; i < node.extraArguments.length; ++i) {
                        if (node.extraArguments[i] != otherSemantic.extraArguments[i])
                            return false;
                    }
                    return true;
                }
                if (node.extraArguments)
                    return node.extraArguments.length == 0;
                if (otherSemantic.extraArguments)
                    return otherSemantic.extraArguments.length == 0;
                return true;
            }

            visitResourceSemantic(node)
            {
                if (!(otherSemantic instanceof ResourceSemantic))
                    return false;
                return node.resourceMode == otherSemantic.resourceMode && node.index == otherSemantic.index && node.space == otherSemantic.space;
            }

            visitStageInOutSemantic(node)
            {
                if (!(otherSemantic instanceof StageInOutSemantic))
                    return false;
                return node.index == otherSemantic.index;
            }

            visitSpecializationConstantSemantic(node)
            {
                return other instanceof SpecializationConstantSemantic;
            }
        }
        return this.visit(new Comparer());
    }
}


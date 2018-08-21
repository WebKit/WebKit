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

function removeTypeArguments(program)
{
    class RemoveTypeArguments extends Visitor {
        static resolveNameAndArguments(node)
        {
            if (!node.typeArguments)
                return node.name;

            switch (node.name) {
            case "vector":
                if (node.typeArguments.length != 2)
                    throw new WSyntaxError(node.originString, `${node.name} should have 2 type arguments, got ${node.typeArguments.length}.`);

                const elementTypeName = node.typeArguments[0].name;
                const lengthValue = node.typeArguments[1].value;

                if (VectorElementTypes.indexOf(elementTypeName) < 0)
                    throw new WSyntaxError(node.originString, `${elementTypeName} is not a valid vector element type.`);
                if (VectorElementSizes.indexOf(lengthValue) < 0)
                    throw new WSyntaxError(node.originString, `${lengthValue} is not a valid size for vectors with element type ${elementTypeName}.`);

                return `${elementTypeName}${lengthValue}`;
            // FIXME: Further cases for matrices, textures, etc.
            default:
                if (node.typeArguments.length)
                    throw new WSyntaxError(`${node.name}${arguments.join(", ")} is not a permitted generic type or function`);
                return node.name;
            }
        }

        visitTypeRef(node)
        {
            node._name = RemoveTypeArguments.resolveNameAndArguments(node);
            node._typeArguments = null;
        }
    }

    program.visit(new RemoveTypeArguments());
}

/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.RecordingActionTreeElement = class RecordingActionTreeElement extends WI.GeneralTreeElement
{
    constructor(representedObject, index, recording)
    {
        console.assert(representedObject instanceof WI.RecordingAction);
        console.assert(recording instanceof WI.Recording);

        let titleFragment = WI.RecordingActionTreeElement._generateDOM(representedObject, recording);
        let classNames = WI.RecordingActionTreeElement._getClassNames(representedObject);

        super(classNames, titleFragment, representedObject.result, representedObject);

        this._index = index;

        if (this.representedObject.valid)
            this.representedObject.singleFireEventListener(WI.RecordingAction.Event.ValidityChanged, this._handleValidityChanged, this);
    }

    // Static

    static _generateDOM(recordingAction, recording)
    {
        let recordingType = recording.type;
        let parameterCount = recordingAction.parameters.length;

        function appendJSON(parent, value, objectReferences, index, path = [], indent = 0) {
            if (objectReferences.has(value)) {
                let objectReferenceElement = parent.appendChild(document.createElement("span"));
                objectReferenceElement.classList.add("object-handle");
                let [identifier, swizzleType] = value;
                if (WI.Recording.isObjectSwizzleType(swizzleType))
                    objectReferenceElement.textContent = recording.displayNameForReference(value);
                else if (swizzleType === WI.Recording.Swizzle.None)
                    objectReferenceElement.textContent = recording.data[identifier];
                else
                    objectReferenceElement.textContent = WI.Recording.displayNameForSwizzleType(swizzleType);
                return;
            }

            let braceIndent = WI.indentString().repeat(indent);
            let valueIndent = WI.indentString().repeat(indent + 1);

            if (Array.isArray(value)) {
                parent.appendChild(document.createTextNode("["));
                let added = false;
                for (let item of value) {
                    let comma = added ? "," : "";
                    parent.appendChild(document.createTextNode(`${comma}\n${valueIndent}`));
                    appendJSON(parent, item, objectReferences, index, path, indent + 1);
                    added = true;
                }
                parent.appendChild(document.createTextNode(added ? `\n${braceIndent}]` : " ]"));
                return;
            }

            if (value && typeof value === "object") {
                parent.appendChild(document.createTextNode("{"));
                let added = false;
                for (let [name, item] of Object.entries(value)) {
                    let comma = added ? "," : "";
                    let propertyName = WI.ScriptSyntaxTree.isIdentifierName(name) ? name : JSON.stringify(name);
                    parent.appendChild(document.createTextNode(`${comma}\n${valueIndent}`));

                    let propertyNameElement = parent.appendChild(document.createElement("span"));
                    propertyNameElement.classList.add("name");
                    propertyNameElement.textContent = propertyName;

                    parent.appendChild(document.createTextNode(": "));
                    appendJSON(parent, item, objectReferences, index, [...path, name], indent + 1);
                    added = true;
                }
                parent.appendChild(document.createTextNode(added ? `\n${braceIndent}}` : " }"));
                return;
            }

            if (typeof value === "number") {
                let bitfieldNames = WI.RecordingAction.bitfieldNamesForParameter(recordingType, recordingAction.name, value, index, parameterCount, path);
                if (bitfieldNames) {
                    let constantElement = parent.appendChild(document.createElement("span"));
                    constantElement.classList.add("parameter", "constant");
                    constantElement.textContent = bitfieldNames.join(" | ");
                    return;
                }
            }

            let type = value === null ? "object" : typeof value;
            let subtype = value === null ? "null" : null;
            parent.appendChild(WI.FormattedValue.createElementForTypesAndValue(type, subtype, String(value)));
        }

        function createParameterElement(parameter, swizzleType, index) {
            let parameterElement = document.createElement("span");
            parameterElement.classList.add("parameter");

            if (WI.Recording.isObjectSwizzleType(swizzleType)) {
                if (!isNaN(parameter)) {
                    parameterElement.classList.add("object-handle");
                    parameterElement.textContent = recording.displayNameForReference([parameter, swizzleType]);
                } else {
                    parameterElement.classList.add("swizzled");
                    parameterElement.textContent = WI.Recording.displayNameForSwizzleType(swizzleType);
                }
                return parameterElement;
            }

            if (WI.Recording.isReferenceSwizzleType(swizzleType)) {
                let objectReferences = WI.RecordingAction.objectReferencesForParameter(recordingType, recordingAction.name, parameter, index);
                parameterElement.classList.add("object-preview", "lossless");
                appendJSON(parameterElement, parameter, objectReferences, index);
                return parameterElement;
            }

            switch (swizzleType) {
            case WI.Recording.Swizzle.Number:
                var constantNameForParameter = WI.RecordingAction.constantNameForParameter(recordingType, recordingAction.name, parameter, index, parameterCount);
                var bitfieldNamesForParameter = WI.RecordingAction.bitfieldNamesForParameter(recordingType, recordingAction.name, parameter, index, parameterCount);
                var displayString = parameter.maxDecimals(2);
                if (constantNameForParameter) {
                    parameterElement.classList.add("constant");
                    displayString = "context." + constantNameForParameter;
                } else if (bitfieldNamesForParameter) {
                    parameterElement.classList.add("constant");
                    displayString = bitfieldNamesForParameter.join(" | ");
                }
                parameterElement.appendChild(WI.FormattedValue.createElementForTypesAndValue("number", null, displayString));
                break;

            case WI.Recording.Swizzle.Boolean:
                parameterElement.appendChild(WI.FormattedValue.createElementForTypesAndValue("boolean", null, String(parameter)));
                break;

            case WI.Recording.Swizzle.String:
                parameterElement.appendChild(WI.FormattedValue.createElementForTypesAndValue("string", null, parameter));
                break;

            case WI.Recording.Swizzle.TypedArray:
            case WI.Recording.Swizzle.Image:
            case WI.Recording.Swizzle.ImageBitmap:
            case WI.Recording.Swizzle.ImageData:
            case WI.Recording.Swizzle.DOMMatrix:
            case WI.Recording.Swizzle.Path2D:
            case WI.Recording.Swizzle.CanvasGradient:
            case WI.Recording.Swizzle.CanvasPattern:
                    parameterElement.classList.add("swizzled");
                    parameterElement.textContent = WI.Recording.displayNameForSwizzleType(swizzleType);
                    break;

            }

            if (!parameterElement.textContent) {
                parameterElement.classList.add("invalid");
                parameterElement.textContent = swizzleType === WI.Recording.Swizzle.None ? parameter : WI.Recording.displayNameForSwizzleType(swizzleType);
            }

            return parameterElement;
        }

        let titleFragment = document.createDocumentFragment();

        if (recordingAction.receiver) {
            let receiverContainer = titleFragment.appendChild(document.createElement("span"));
            receiverContainer.classList.add("receiver");
            receiverContainer.textContent = recordingAction.receiver;

            titleFragment.appendChild(document.createTextNode("."));
        }

        let nameContainer = titleFragment.appendChild(document.createElement("span"));
        nameContainer.classList.add("name");
        nameContainer.textContent = recordingAction.name;

        if (!parameterCount) {
            if (recordingAction.isFunction)
                titleFragment.appendChild(document.createTextNode("()"));
            return titleFragment;
        }

        titleFragment.appendChild(document.createTextNode(recordingAction.isFunction ? "(" : " = "));

        let parametersContainer = titleFragment.appendChild(document.createElement("span"));
        parametersContainer.classList.add("parameters");

        for (let i = 0; i < parameterCount; ++i) {
            let parameter = recordingAction.parameters[i];
            let swizzleType = recordingAction.swizzleTypes[i];

            if (i)
                parametersContainer.appendChild(document.createTextNode(", "));

            let parameterElement = createParameterElement(parameter, swizzleType, i);
            parametersContainer.appendChild(parameterElement);
        }

        if (recordingAction.isFunction)
            titleFragment.appendChild(document.createTextNode(")"));

        let colorParameters = recordingAction.getColorParameters();
        if (colorParameters.length) {
            let swatch = WI.RecordingActionTreeElement._createSwatchForColorParameters(colorParameters);
            if (swatch) {
                let insertionIndex = recordingAction.parameters.indexOf(colorParameters[0]);
                parametersContainer.insertBefore(swatch.element, parametersContainer.children[insertionIndex]);
            }
        }

        let imageParameters = recordingAction.getImageParameters();
        let isImage = imageParameters[0] instanceof HTMLImageElement;
        let isImageBitmap = imageParameters[0] instanceof ImageBitmap;
        let isImageData = imageParameters[0] instanceof ImageData;
        let isCanvasGradient = imageParameters[0] instanceof CanvasGradient;
        let isCanvasPattern = imageParameters[0] instanceof CanvasPattern;
        if (imageParameters.length && (isImage || isImageBitmap || isImageData || isCanvasGradient || isCanvasPattern)) {
            let image = imageParameters[0];

            if (isImageBitmap)
                image = WI.ImageUtilities.imageFromImageBitmap(image);
            else if (isImageData)
                image = WI.ImageUtilities.imageFromImageData(image);
            else if (isCanvasGradient)
                image = WI.ImageUtilities.imageFromCanvasGradient(image, 100, 100);
            else if (isCanvasPattern)
                image = image.__image;

            if (image) {
                let swatch = new WI.InlineSwatch(WI.InlineSwatch.Type.Image, image);
                let insertionIndex = recordingAction.parameters.indexOf(imageParameters[0]);
                let parameterElement = parametersContainer.children[insertionIndex];
                parametersContainer.insertBefore(swatch.element, parameterElement);
            }
        }

        return titleFragment;
    }

    static _createSwatchForColorParameters(parameters)
    {
        let rgb = [];
        let color = null;

        switch (parameters.length) {
        case 1:
        case 2:
            if (typeof parameters[0] === "string")
                color = WI.Color.fromString(parameters[0].trim());
            else if (!isNaN(parameters[0]))
                rgb = WI.Color.normalized2rgb(parameters[0], parameters[0], parameters[0]);
            break;

        case 4:
            rgb = WI.Color.normalized2rgb(parameters[0], parameters[1], parameters[2]);
            break;

        case 5:
            rgb = WI.Color.cmyk2rgb(...parameters);
            break;

        default:
            console.error("Unexpected number of color parameters.");
            return null;
        }

        if (!color) {
            if (rgb.length !== 3)
                return null;

            let alpha = parameters.length === 1 ? 1 : parameters.lastValue;
            color = new WI.Color(WI.Color.Format.RGBA, [...rgb, alpha]);
            if (!color)
                return null;
        }

        return new WI.InlineSwatch(WI.InlineSwatch.Type.Color, color, {readOnly: true});
    }

    static _getClassNames(recordingAction)
    {
        let classNames = ["recording-action"];

        if (recordingAction instanceof WI.RecordingInitialStateAction) {
            classNames.push("initial-state");
            return classNames;
        }

        if (!recordingAction.isFunction)
            classNames.push("attribute");

        let actionClassName = WI.RecordingActionTreeElement._classNameForAction(recordingAction);
        if (actionClassName.length)
            classNames.push(actionClassName);

        if (recordingAction.isVisual)
            classNames.push("visual");

        if (!recordingAction.valid)
            classNames.push("invalid");

        return classNames;
    }

    static _classNameForAction(recordingAction)
    {
        switch (recordingAction.name) {
        case "activeTexture":
        case "attachShader":
        case "bindAttribLocation":
        case "bindBuffer":
        case "bindBufferBase":
        case "bindBufferRange":
        case "bindFramebuffer":
        case "bindRenderbuffer":
        case "bindSampler":
        case "bindTexture":
        case "bindTransformFeedback":
        case "bindVertexArray":
        case "bindVertexArrayOES":
        case "compileShader":
        case "detachShader":
        case "framebufferRenderbuffer":
        case "framebufferTexture2D":
        case "framebufferTextureLayer":
        case "getBindGroupLayout":
        case "linkProgram":
        case "setBindGroup":
        case "setIndexBuffer":
        case "setPipeline":
        case "setVertexBuffer":
        case "useProgram":
        case "validateProgram":
            return "bind";

        case "blitFramebuffer":
        case "copyBufferSubData":
        case "copyBufferToBuffer":
        case "copyBufferToTexture":
        case "copyElementImageToTexture":
        case "copyExternalImageToTexture":
        case "copyTexImage2D":
        case "copyTexSubImage2D":
        case "copyTexSubImage3D":
        case "copyTextureToBuffer":
        case "copyTextureToTexture":
            return "copy";

        case "createBindGroup":
        case "createBindGroupLayout":
        case "createBuffer":
        case "createCommandEncoder":
        case "createComputePipeline":
        case "createComputePipelineAsync":
        case "createFramebuffer":
        case "createPipelineLayout":
        case "createProgram":
        case "createQuery":
        case "createQueryEXT":
        case "createQuerySet":
        case "createRenderbuffer":
        case "createRenderBundleEncoder":
        case "createRenderPipeline":
        case "createRenderPipelineAsync":
        case "createSampler":
        case "createShader":
        case "createShaderModule":
        case "createTexture":
        case "createTransformFeedback":
        case "createVertexArray":
        case "createVertexArrayOES":
        case "createView":
        case "deleteBuffer":
        case "deleteFramebuffer":
        case "deleteProgram":
        case "deleteQuery":
        case "deleteQueryEXT":
        case "deleteRenderbuffer":
        case "deleteSampler":
        case "deleteShader":
        case "deleteSync":
        case "deleteTexture":
        case "deleteTransformFeedback":
        case "deleteVertexArray":
        case "deleteVertexArrayOES":
        case "destroy":
        case "fenceSync":
        case "importExternalTexture":
        case "invalidateFramebuffer":
        case "invalidateSubFramebuffer":
        case "renderbufferStorage":
        case "renderbufferStorageMultisample":
        case "texStorage2D":
        case "texStorage3D":
            return "object";

        case "draw":
        case "drawArrays":
        case "drawArraysInstanced":
        case "drawArraysInstancedANGLE":
        case "drawArraysInstancedBaseInstanceWEBGL":
        case "drawElements":
        case "drawElementsInstanced":
        case "drawElementsInstancedANGLE":
        case "drawElementsInstancedBaseVertexBaseInstanceWEBGL":
        case "drawIndexed":
        case "drawIndexedIndirect":
        case "drawIndirect":
        case "drawRangeElements":
        case "multiDrawArraysInstancedBaseInstanceWEBGL":
        case "multiDrawArraysInstancedWEBGL":
        case "multiDrawArraysWEBGL":
        case "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL":
        case "multiDrawElementsInstancedWEBGL":
        case "multiDrawElementsWEBGL":
            return "draw";

        case "dispatchWorkgroups":
        case "dispatchWorkgroupsIndirect":
        case "executeBundles":
        case "flush":
        case "makeXRCompatible":
        case "submit":
            return "execute";

        case "beginOcclusionQuery":
        case "beginQuery":
        case "beginQueryEXT":
        case "clientWaitSync":
        case "count":
        case "endOcclusionQuery":
        case "endQuery":
        case "endQueryEXT":
        case "getQuery":
        case "getQueryEXT":
        case "getQueryObjectEXT":
        case "getQueryParameter":
        case "getSyncParameter":
        case "isQuery":
        case "isQueryEXT":
        case "isSync":
        case "lost":
        case "mapState":
        case "onSubmittedWorkDone":
        case "queryCounterEXT":
        case "resolveQuerySet":
        case "type":
        case "waitSync":
            return "query";

        case "adapterInfo":
        case "canvas":
        case "checkFramebufferStatus":
        case "features":
        case "getBufferParameter":
        case "getBufferSubData":
        case "getContextAttributes":
        case "getEffectiveRenderingModeForTesting":
        case "getError":
        case "getExtension":
        case "getFramebufferAttachmentParameter":
        case "getIndexedParameter":
        case "getInternalformatParameter":
        case "getMappedRange":
        case "getParameter":
        case "getRenderbufferParameter":
        case "getSupportedExtensions":
        case "isBuffer":
        case "isContextLost":
        case "isEnabled":
        case "isFramebuffer":
        case "isProgram":
        case "isRenderbuffer":
        case "isSampler":
        case "isShader":
        case "isTexture":
        case "isTransformFeedback":
        case "isVertexArray":
        case "isVertexArrayOES":
        case "label":
        case "limits":
        case "onuncapturederror":
        case "queue":
        case "readBuffer":
        case "readPixels":
        case "size":
        case "usage":
            return "read";

        case "popDebugGroup":
        case "popErrorScope":
        case "reset":
        case "restore":
        case "unmap":
            return "restore";

        case "mapAsync":
        case "pushDebugGroup":
        case "pushErrorScope":
        case "save":
            return "save";

        case "getActiveAttrib":
        case "getActiveUniform":
        case "getActiveUniformBlockName":
        case "getActiveUniformBlockParameter":
        case "getActiveUniforms":
        case "getAttachedShaders":
        case "getAttribLocation":
        case "getCompilationInfo":
        case "getFragDataLocation":
        case "getProgramInfoLog":
        case "getProgramParameter":
        case "getShaderInfoLog":
        case "getShaderParameter":
        case "getShaderPrecisionFormat":
        case "getShaderSource":
        case "getTransformFeedbackVarying":
        case "getTranslatedShaderSource":
        case "getUniform":
        case "getUniformBlockIndex":
        case "getUniformIndices":
        case "getUniformLocation":
            return "show";

        case "shaderSource":
            return "source";

        case "transformFeedbackVaryings":
        case "uniform1f":
        case "uniform1fv":
        case "uniform1i":
        case "uniform1iv":
        case "uniform1ui":
        case "uniform1uiv":
        case "uniform2f":
        case "uniform2fv":
        case "uniform2i":
        case "uniform2iv":
        case "uniform2ui":
        case "uniform2uiv":
        case "uniform3f":
        case "uniform3fv":
        case "uniform3i":
        case "uniform3iv":
        case "uniform3ui":
        case "uniform3uiv":
        case "uniform4f":
        case "uniform4fv":
        case "uniform4i":
        case "uniform4iv":
        case "uniform4ui":
        case "uniform4uiv":
        case "uniformBlockBinding":
        case "uniformMatrix2fv":
        case "uniformMatrix2x3fv":
        case "uniformMatrix2x4fv":
        case "uniformMatrix3fv":
        case "uniformMatrix3x2fv":
        case "uniformMatrix3x4fv":
        case "uniformMatrix4fv":
        case "uniformMatrix4x2fv":
        case "uniformMatrix4x3fv":
            return "uniform";

        case "beginComputePass":
        case "beginLayer":
        case "beginRenderPass":
        case "beginTransformFeedback":
        case "restoreContext":
        case "resumeTransformFeedback":
            return "begin";

        case "end":
        case "endLayer":
        case "endTransformFeedback":
        case "finish":
        case "loseContext":
        case "pauseTransformFeedback":
            return "end";

        case "depthRange":
        case "drawingBufferHeight":
        case "drawingBufferWidth":
        case "height":
        case "setViewport":
        case "viewport":
        case "width":
            return "viewport";

        case "bufferData":
        case "bufferSubData":
        case "writeBuffer":
        case "writeTexture":
            return "write";

        case "arc":
        case "arcTo":
            return "arc";

        case "blendColor":
        case "blendEquation":
        case "blendEquationiOES":
        case "blendEquationSeparate":
        case "blendEquationSeparateiOES":
        case "blendFunc":
        case "blendFunciOES":
        case "blendFuncSeparate":
        case "blendFuncSeparateiOES":
        case "colorMask":
        case "colorMaskiOES":
        case "cullFace":
        case "depthFunc":
        case "depthMask":
        case "disable":
        case "disableiOES":
        case "drawBuffers":
        case "drawBuffersWEBGL":
        case "enable":
        case "enableiOES":
        case "frontFace":
        case "globalAlpha":
        case "globalCompositeOperation":
        case "hint":
        case "sampleCoverage":
        case "setAlpha":
        case "setBlendConstant":
        case "setCompositeOperation":
        case "setGlobalAlpha":
        case "setGlobalCompositeOperation":
        case "setStencilReference":
        case "stencilFunc":
        case "stencilFuncSeparate":
        case "stencilMask":
        case "stencilMaskSeparate":
        case "stencilOp":
        case "stencilOpSeparate":
            return "composite";

        case "bezierCurveTo":
        case "quadraticCurveTo":
            return "curve";

        case "clear":
        case "clearBuffer":
        case "clearBufferfi":
        case "clearBufferfv":
        case "clearBufferiv":
        case "clearBufferuiv":
        case "clearColor":
        case "clearDepth":
        case "clearRect":
        case "clearStencil":
        case "fill":
        case "fillRect":
        case "fillText":
            return "fill";

        case "compressedTexImage2D":
        case "compressedTexImage3D":
        case "compressedTexSubImage2D":
        case "compressedTexSubImage3D":
        case "createImageData":
        case "depthOrArrayLayers":
        case "dimension":
        case "drawElementImage":
        case "drawFocusIfNeeded":
        case "drawImage":
        case "drawingBufferColorSpace":
        case "filter":
        case "format":
        case "generateMipmap":
        case "getImageData":
        case "getSamplerParameter":
        case "getSupportedProfiles":
        case "getTexParameter":
        case "imageSmoothingEnabled":
        case "imageSmoothingQuality":
        case "mipLevelCount":
        case "pixelStorei":
        case "putImageData":
        case "sampleCount":
        case "samplerParameterf":
        case "samplerParameteri":
        case "texElementImage2D":
        case "texImage2D":
        case "texImage3D":
        case "texParameterf":
        case "texParameteri":
        case "texSubImage2D":
        case "texSubImage3D":
        case "transferFromImageBitmap":
        case "webkitImageSmoothingEnabled":
            return "image";

        case "getLineDash":
        case "lineCap":
        case "lineDashOffset":
        case "lineJoin":
        case "lineWidth":
        case "miterLimit":
        case "setLineCap":
        case "setLineDash":
        case "setLineJoin":
        case "setLineWidth":
        case "setMiterLimit":
        case "webkitLineDash":
        case "webkitLineDashOffset":
            return "line-style";

        case "closePath":
        case "lineTo":
            return "line-to";

        case "beginPath":
        case "moveTo":
            return "move-to";

        case "isPointInPath":
            return "point-in-path";

        case "isPointInStroke":
            return "point-in-stroke";

        case "clearShadow":
        case "setShadow":
        case "shadowBlur":
        case "shadowColor":
        case "shadowOffsetX":
        case "shadowOffsetY":
            return "shadow";

        case "createConicGradient":
        case "createLinearGradient":
        case "createPattern":
        case "createRadialGradient":
        case "fillStyle":
        case "setFillColor":
        case "setStrokeColor":
        case "strokeStyle":
            return "style";

        case "stroke":
        case "strokeRect":
        case "strokeText":
            return "stroke";

        case "direction":
        case "font":
        case "letterSpacing":
        case "measureText":
        case "textAlign":
        case "textBaseline":
        case "wordSpacing":
            return "text";

        case "disableVertexAttribArray":
        case "enableVertexAttribArray":
        case "getTransform":
        case "getVertexAttrib":
        case "getVertexAttribOffset":
        case "polygonOffset":
        case "polygonModeWEBGL":
        case "polygonOffsetClampEXT":
        case "provokingVertexWEBGL":
        case "resetTransform":
        case "rotate":
        case "scale":
        case "setTransform":
        case "transform":
        case "translate":
        case "vertexAttrib1f":
        case "vertexAttrib1fv":
        case "vertexAttrib2f":
        case "vertexAttrib2fv":
        case "vertexAttrib3f":
        case "vertexAttrib3fv":
        case "vertexAttrib4f":
        case "vertexAttrib4fv":
        case "vertexAttribDivisor":
        case "vertexAttribDivisorANGLE":
        case "vertexAttribI4i":
        case "vertexAttribI4iv":
        case "vertexAttribI4ui":
        case "vertexAttribI4uiv":
        case "vertexAttribIPointer":
        case "vertexAttribPointer":
            return "transform";

        case "clip":
        case "clipControlEXT":
        case "scissor":
        case "setScissorRect":
            return "clip";

        case "ellipse":
            return "ellipse";

        case "rect":
        case "roundRect":
            return "rect";
        }

        return "name-unknown";
    }

    // Public

    get index() { return this._index; }

    get filterableData()
    {
        let text = [];

        function getText(stringOrElement) {
            if (typeof stringOrElement === "string")
                text.push(stringOrElement);
            else if (stringOrElement instanceof Node)
                text.push(stringOrElement.textContent);
        }

        getText(this._mainTitleElement || this._mainTitle);
        getText(this._subtitleElement || this._subtitle);

        return {text};
    }

    // Protected

    onattach()
    {
        super.onattach();

        this.element.dataset.index = this._index.toLocaleString();

        if (this.representedObject.valid && this.representedObject.warning) {
            this.addClassName("warning");
            this.status = WI.ImageUtilities.useSVGSymbol("Images/Warning.svg", "warning", this.representedObject.warning);
        }
    }

    populateContextMenu(contextMenu, event)
    {
        contextMenu.appendItem(WI.UIString("Copy Action"), () => {
            InspectorFrontendHost.copyText(`${this.representedObject.receiver ? "" : this.representedObject.isCanvasReceiver ? "canvas." : "context."}${this.mainTitle};`);
        });

        contextMenu.appendSeparator();

        let sourceCodeLocation = this.representedObject.stackTrace?.firstNonNativeNonAnonymousNotBlackboxedCallFrame;
        if (sourceCodeLocation) {
            contextMenu.appendItem(WI.UIString("Reveal in Sources Tab"), () => {
                WI.showSourceCodeLocation(sourceCodeLocation, {
                    ignoreNetworkTab: true,
                    ignoreSearchTab: true,
                    initiatorHint: WI.TabBrowser.TabNavigationInitiator.ContextMenu,
                });
            });

            contextMenu.appendSeparator();
        }

        super.populateContextMenu(contextMenu, event);
    }

    // Private

    _handleValidityChanged(event)
    {
        this.addClassName("invalid");
    }
};

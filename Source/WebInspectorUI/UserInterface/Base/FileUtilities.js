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

WI.saveDataToFile = function(saveData, forceSaveAs)
{
    console.assert(saveData);
    if (!saveData)
        return;

    if (typeof saveData.customSaveHandler === "function") {
        saveData.customSaveHandler(forceSaveAs);
        return;
    }

    console.assert(saveData.content);
    if (!saveData.content)
        return;

    let url = saveData.url || "";
    let suggestedName = parseURL(url).lastPathComponent;
    if (!suggestedName) {
        suggestedName = WI.UIString("Untitled");
        let dataURLTypeMatch = /^data:([^;]+)/.exec(url);
        if (dataURLTypeMatch) {
            let fileExtension = WI.fileExtensionForMIMEType(dataURLTypeMatch[1]);
            if (fileExtension)
                suggestedName += "." + fileExtension;
        }
    }

    if (typeof saveData.content === "string") {
        const base64Encoded = saveData.base64Encoded || false;
        InspectorFrontendHost.save(suggestedName, saveData.content, base64Encoded, forceSaveAs || saveData.forceSaveAs);
        return;
    }

    let fileReader = new FileReader;
    fileReader.readAsDataURL(saveData.content);
    fileReader.addEventListener("loadend", () => {
        let dataURLComponents = parseDataURL(fileReader.result);

        const base64Encoded = true;
        InspectorFrontendHost.save(suggestedName, dataURLComponents.data, base64Encoded, forceSaveAs || saveData.forceSaveAs);
    });
};

WI.loadDataFromFile = function(callback)
{
    let inputElement = document.createElement("input");
    inputElement.type = "file";
    inputElement.addEventListener("change", (event) => {
        if (!inputElement.files.length) {
            callback(null);
            return;
        }

        let file = inputElement.files[0];
        let reader = new FileReader;
        reader.addEventListener("loadend", (event) => {
            callback(reader.result, file.name);
        });
        reader.readAsText(file);
    });
    inputElement.click();
};

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

WI.FileUtilities = class FileUtilities {
    static screenshotString()
    {
        let date = new Date;
        let values = [
            date.getFullYear(),
            Number.zeroPad(date.getMonth() + 1, 2),
            Number.zeroPad(date.getDate(), 2),
            Number.zeroPad(date.getHours(), 2),
            Number.zeroPad(date.getMinutes(), 2),
            Number.zeroPad(date.getSeconds(), 2),
        ];
        return WI.UIString("Screen Shot %s-%s-%s at %s.%s.%s").format(...values);
    }

    static sanitizeFilename(filename)
    {
        return filename.replace(/:+/g, "-");
    }

    static inspectorURLForFilename(filename)
    {
        return "web-inspector:///" + encodeURIComponent(FileUtilities.sanitizeFilename(filename));
    }

    static save(saveData, forceSaveAs)
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
        fileReader.addEventListener("loadend", () => {
            let dataURLComponents = parseDataURL(fileReader.result);

            const base64Encoded = true;
            InspectorFrontendHost.save(suggestedName, dataURLComponents.data, base64Encoded, forceSaveAs || saveData.forceSaveAs);
        });
        fileReader.readAsDataURL(saveData.content);
    }

    static importText(callback)
    {
        if (!FileUtilities._importTextInputElement) {
            let inputElement = FileUtilities._importTextInputElement = document.createElement("input");
            inputElement.type = "file";
            inputElement.multiple = true;
            inputElement.addEventListener("change", (event) => {
                WI.FileUtilities.readText(inputElement.files, callback);
            });
        }

        FileUtilities._importTextInputElement.value = null;
        FileUtilities._importTextInputElement.click();
    }

    static importJSON(callback)
    {
        if (!FileUtilities._importJSONInputElement) {
            let inputElement = FileUtilities._importJSONInputElement = document.createElement("input");
            inputElement.type = "file";
            inputElement.multiple = true;
            inputElement.addEventListener("change", (event) => {
                WI.FileUtilities.readJSON(inputElement.files, callback);
            });
        }

        FileUtilities._importJSONInputElement.value = null;
        FileUtilities._importJSONInputElement.click();
    }

    static async readText(fileOrList, callback)
    {
        console.assert(fileOrList instanceof File || fileOrList instanceof FileList);

        let files = [];
        if (fileOrList instanceof File)
            files.push(fileOrList);
        else if (fileOrList instanceof FileList)
            files = Array.from(fileOrList);

        for (let file of files) {
            let result = {
                filename: file.name,
            };

            try {
                await new Promise((resolve, reject) => {
                    let reader = new FileReader;
                    reader.addEventListener("loadend", (event) => {
                        result.text = reader.result;
                        resolve(event);
                    });
                    reader.addEventListener("error", reject);
                    reader.readAsText(file);
                });
            } catch (e) {
                result.error = e;
            }

            let promise = callback(result);
            if (promise instanceof Promise)
                await promise;
        }
    }

    static async readJSON(fileOrList, callback)
    {
        return WI.FileUtilities.readText(fileOrList, (result) => {
            if (result.text && !result.error) {
                try {
                    result.json = JSON.parse(result.text);
                } catch (e) {
                    result.error = e;
                }
            }

            return callback(result);
        });
    }
};

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

        let suggestedName = saveData.suggestedName;
        if (!suggestedName) {
            let url = saveData.url || "";
            suggestedName = parseURL(url).lastPathComponent;
            if (!suggestedName) {
                suggestedName = WI.UIString("Untitled");
                let dataURLTypeMatch = /^data:([^;]+)/.exec(url);
                if (dataURLTypeMatch) {
                    let fileExtension = WI.fileExtensionForMIMEType(dataURLTypeMatch[1]);
                    if (fileExtension)
                        suggestedName += "." + fileExtension;
                }
            }
        }

        suggestedName = FileUtilities.inspectorURLForFilename(suggestedName);

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

    static import(callback, {multiple} = {})
    {
        let inputElement = document.createElement("input");
        inputElement.type = "file";
        inputElement.value = null;
        inputElement.multiple = !!multiple;
        inputElement.addEventListener("change", (event) => {
            callback(inputElement.files);
        });

        inputElement.click();

        // Cache the last used import element so that it doesn't get GCd while the native file
        // picker is shown, which would prevent the "change" event listener from firing.
        FileUtilities.importInputElement = inputElement;
    }

    static importText(callback, options = {})
    {
        FileUtilities.import((files) => {
            FileUtilities.readText(files, callback);
        }, options);
    }

    static importJSON(callback, options = {})
    {
        FileUtilities.import((files) => {
            FileUtilities.readJSON(files, callback);
        }, options);
    }

    static importData(callback, options = {})
    {
        FileUtilities.import((files) => {
            FileUtilities.readData(files, callback);
        }, options);
    }

    static async readText(fileOrList, callback)
    {
        await FileUtilities._read(fileOrList, async (file, result) => {
            await new Promise((resolve, reject) => {
                let reader = new FileReader;
                reader.addEventListener("loadend", (event) => {
                    result.text = reader.result;
                    resolve(event);
                });
                reader.addEventListener("error", reject);
                reader.readAsText(file);
            });
        }, callback);
    }

    static async readJSON(fileOrList, callback)
    {
        await WI.FileUtilities.readText(fileOrList, async (result) => {
            if (result.text && !result.error) {
                try {
                    result.json = JSON.parse(result.text);
                } catch (e) {
                    result.error = e;
                }
            }

            await callback(result);
        });
    }

    static async readData(fileOrList, callback)
    {
        await FileUtilities._read(fileOrList, async (file, result) => {
            await new Promise((resolve, reject) => {
                let reader = new FileReader;
                reader.addEventListener("loadend", (event) => {
                    let {mimeType, base64, data} = parseDataURL(reader.result);

                    // In case no mime type was determined, try to derive one from the file extension.
                    if (!mimeType || mimeType === "text/plain") {
                        let extension = WI.fileExtensionForFilename(result.filename);
                        if (extension)
                            mimeType = WI.mimeTypeForFileExtension(extension);
                    }

                    result.mimeType = mimeType;
                    result.base64Encoded = base64;
                    result.content = data;

                    resolve(event);
                });
                reader.addEventListener("error", reject);
                reader.readAsDataURL(file);
            });
        }, callback);
    }

    // Private

    static async _read(fileOrList, operation, callback)
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
                await operation(file, result);
            } catch (e) {
                result.error = e;
            }

            await callback(result);
        }
    }
};

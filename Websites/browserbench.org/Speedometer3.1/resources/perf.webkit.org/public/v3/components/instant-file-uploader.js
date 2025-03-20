class InstantFileUploader extends ComponentBase {
    constructor()
    {
        super('instant-file-uploader');
        this._fileInput = null;
        this._allowMultipleFiles = false;
        this._uploadedFiles = [];
        this._preuploadFiles = [];
        this._uploadProgress = new WeakMap;
        this._fileSizeFormatter = Metric.makeFormatter('B', 3);

        this._renderUploadedFilesLazily = new LazilyEvaluatedFunction(this._renderUploadedFiles.bind(this));
        this._renderPreuploadFilesLazily = new LazilyEvaluatedFunction(this._renderPreuploadFiles.bind(this));
    }

    clearUploads()
    {
        this._uploadedFiles = [];
        this._preuploadFiles = [];
        this._uploadProgress = new WeakMap;
    }
    hasFileToUpload() { return !!this._preuploadFiles.length; }
    uploadedFiles() { return this._uploadedFiles; }

    allowMultipleFiles()
    {
        this._allowMultipleFiles = true;
        this.enqueueToRender();
    }

    addUploadedFile(uploadedFile)
    {
        console.assert(uploadedFile instanceof UploadedFile);
        if (this._uploadedFiles.includes(uploadedFile))
            return;
        this._uploadedFiles.push(uploadedFile);
        this.enqueueToRender();
    }

    didConstructShadowTree()
    {
        const addButton = this.content('file-adder');
        addButton.onclick = () => {
            inputElement.click();
        }
        addButton.addEventListener('dragover', (event) => {
            event.dataTransfer.dropEffect = 'copy';
            event.preventDefault();
        });
        addButton.addEventListener('drop', (event) => {
            event.preventDefault();
            let files = event.dataTransfer.files;
            if (!files.length)
                return;
            if (files.length > 1 && !this._allowMultipleFiles)
                files = [files[0]];
            this._uploadFiles(files);
        });
        const inputElement = document.createElement('input');
        inputElement.type = 'file';
        inputElement.onchange = () => this._didFileInputChange(inputElement);
        this._fileInput = inputElement;
    }

    render()
    {
        this._renderUploadedFilesLazily.evaluate(...this._uploadedFiles);
        const uploadStatusElements = this._renderPreuploadFilesLazily.evaluate(...this._preuploadFiles);
        this._updateUploadStatus(uploadStatusElements);
        const fileCount = this._uploadedFiles.length + this._preuploadFiles.length;
        this.content('file-adder').style.display = this._allowMultipleFiles || !fileCount ? null : 'none';
    }

    _renderUploadedFiles(...uploadedFiles)
    {
        const element = ComponentBase.createElement;
        this.renderReplace(this.content('uploaded-files'), uploadedFiles.map((uploadedFile) => {
            const authorInfo = uploadedFile.author() ? ' by ' + uploadedFile.author() : '';
            const createdAt = Metric.formatTime(uploadedFile.createdAt());
            const deleteButton = new CloseButton;
            deleteButton.listenToAction('activate', () => this._removeUploadedFile(uploadedFile));
            return element('li', [
                deleteButton,
                element('code', {class: 'filename'}, uploadedFile.filename()),
                ' ',
                element('small', {class: 'filesize'}, '(' + this._fileSizeFormatter(uploadedFile.size()) + ')'),
                element('small', {class: 'meta'}, `Uploaded${authorInfo} on ${createdAt}`),
            ]);
        }));
    }

    _renderPreuploadFiles(...preuploadFiles)
    {
        const element = ComponentBase.createElement;
        const uploadStatusElements = [];
        this.renderReplace(this.content('preupload-files'), preuploadFiles.map((file) => {
            const progressBar = element('progress');
            const meta = element('small', {class: 'meta'}, progressBar);
            uploadStatusElements.push({file, meta, progressBar});

            return element('li', [
                element('code', file.name),
                ' ',
                element('small', {class: 'filesize'}, '(' + this._fileSizeFormatter(file.size) + ')'),
                meta,
            ]);
        }));
        return uploadStatusElements;
    }

    _updateUploadStatus(uploadStatusElements)
    {
        for (let entry of uploadStatusElements) {
            const progress = this._uploadProgress.get(entry.file);
            const progressBar = entry.progressBar;
            if (!progress) {
                progressBar.removeAttribute('max');
                progressBar.removeAttribute('value');
                return;
            }
            if (progress.error) {
                entry.meta.classList.add('hasError');
                entry.meta.textContent = this._formatUploadError(progress.error);
            } else {
                progressBar.max = progress.total;
                progressBar.value = progress.loaded;
            }
        }
    }

    _formatUploadError(error)
    {
        switch (error) {
        case 'NotSupported':
            return 'Failed: File uploading is disabled';
        case 'FileSizeLimitExceeded':
            return 'Failed: The uploaded file was too big';
        case 'FileSizeQuotaExceeded':
            return 'Failed: Exceeded file upload quota';
        }
        return 'Failed to upload the file';
    }

    _didFileInputChange(input)
    {
        if (!input.files.length)
            return;
        this._uploadFiles(input.files);
        input.value = null;
        this.enqueueToRender();
    }

    _uploadFiles(files)
    {
        const limit = UploadedFile.fileUploadSizeLimit;
        files = Array.from(files);
        for (let file of files) {
            if (file.size > limit) {
                alert(`The specified file "${file.name}" is too big (${this._fileSizeFormatter(file.size)}). It must be smaller than ${this._fileSizeFormatter(limit)}`);
                return;
            }
        }

        const uploadProgress = this._uploadProgress;
        for (let file of files) {
            UploadedFile.fetchUploadedFileWithIdenticalHash(file).then((uploadedFile) => {
                if (uploadedFile) {
                    this._didUploadFile(file, uploadedFile);
                    return;
                }

                UploadedFile.uploadFile(file, (progress) => {
                    uploadProgress.set(file, progress);
                    if (this._uploadProgress == uploadProgress)
                        this.enqueueToRender();
                }).then((uploadedFile) => {
                    if (this._uploadProgress == uploadProgress)
                        this._didUploadFile(file, uploadedFile);
                }, (error) => {
                    uploadProgress.set(file, {error: error === 0 ? 'UnknownError' : error});
                    if (this._uploadedProgress == uploadProgress)
                        this.enqueueToRender();
                });
            });
        }
        this._preuploadFiles = Array.from(files);
    }

    _removeUploadedFile(uploadedFile)
    {
        // FIXME: Send a request to delete the file.
        console.assert(uploadedFile instanceof UploadedFile);
        const index = this._uploadedFiles.indexOf(uploadedFile);
        if (index < 0)
            return;
        this._uploadedFiles.splice(index, 1);
        this.dispatchAction('removedFile', uploadedFile);
        this.enqueueToRender();
    }

    _didUploadFile(file, uploadedFile)
    {
        console.assert(file instanceof File);
        const index = this._preuploadFiles.indexOf(file);
        if (index >= 0)
            this._preuploadFiles.splice(index, 1);
        this._uploadedFiles.push(uploadedFile);
        this.dispatchAction('uploadedFile', uploadedFile);
        this.enqueueToRender();
    }

    static htmlTemplate()
    {
        return `<ul id="uploaded-files"></ul>
            <ul id="preupload-files"></ul>
            <button id="file-adder"><slot>Add a new file</slot></button>`;
    }

    static cssTemplate()
    {
        return `
            ul:empty {
                display: none;
            }

            ul, li {
                margin: 0;
                padding: 0;
                list-style: none;
            }

            li {
                position: relative;
                margin-bottom: 0.25rem;
                padding-left: 1.5rem;
                padding-bottom: 0.25rem;
                border-bottom: solid 1px #eee;
            }

            li:last-child {
                border-bottom: none;
            }

            li > close-button {
                position: absolute;
                left: 0;
                top: 50%;
                margin-top: -0.5rem;
            }

            li > progress {
                display: block;
            }

            code {
                font-size: 1.1rem;
                font-weight: inherit;
            }

            small {
                font-size: 0.8rem;
                font-weight: inherit;
                color: #666;
            }

            small.meta {
                display: block;
            }

            .hasError {
                color: #c60;
                font-weight: normal;
            }
        `;
    }
}

ComponentBase.defineElement('instant-file-uploader', InstantFileUploader);
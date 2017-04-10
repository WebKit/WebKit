class InstantFileUploader extends ComponentBase {
    constructor()
    {
        super('instant-file-uploader');
        this._uploadedFiles = [];
        this._preuploadFiles = [];
        this._uploadProgress = new WeakMap;
        this._fileSizeFormatter = Metric.makeFormatter('B', 3);

        this._renderUploadedFilesLazily = new LazilyEvaluatedFunction(this._renderUploadedFiles.bind(this));
        this._renderPreuploadFilesLazily = new LazilyEvaluatedFunction(this._renderPreuploadFiles.bind(this));
    }

    hasFileToUpload() { return !!this._preuploadFiles.length; }
    uploadedFiles() { return this._uploadedFiles; }

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
        const input = this.content('file-input');
        input.onchange = () => this._didFileInputChange(input);
    }

    render()
    {
        this._renderUploadedFilesLazily.evaluate(...this._uploadedFiles);
        const uploadStatusElements = this._renderPreuploadFilesLazily.evaluate(...this._preuploadFiles);
        this._updateUploadStatus(uploadStatusElements);
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
        const limit = UploadedFile.fileUploadSizeLimit;
        for (let file of input.files) {
            if (file.size > limit) {
                alert(`The specified file "${file.name}" is too big (${this._fileSizeFormatter(file.size)}). It must be smaller than ${this._fileSizeFormatter(limit)}`);
                input.value = null;
                return;
            }
            UploadedFile.fetchUnloadedFileWithIdenticalHash(file).then((uploadedFile) => {
                if (uploadedFile) {
                    this._didUploadFile(file, uploadedFile);
                    return;
                }

                UploadedFile.uploadFile(file, (progress) => {
                    this._uploadProgress.set(file, progress);
                    this.enqueueToRender();
                }).then((uploadedFile) => {
                    this._didUploadFile(file, uploadedFile);
                }, (error) => {
                    this._uploadProgress.set(file, {error: error === 0 ? 'UnknownError' : error});
                    this.enqueueToRender();
                });
            });
        }
        this._preuploadFiles = Array.from(input.files);
        input.value = null;

        this.enqueueToRender();
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
            <input id="file-input" type="file" multiple="false">`;
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
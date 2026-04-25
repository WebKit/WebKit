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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const PDFJSContentScript = {
    overrideSettings() {
        // Disable the findbar provided by PDF.js.
        delete PDFViewerApplication.supportsIntegratedFind;
        PDFViewerApplication.supportsIntegratedFind = true;

        // Hide sidebar on load.
        PDFViewerApplicationOptions.set("sidebarViewOnLoad", 0);
    },

    setPageMode({ pages, continuous }) {
        const { SpreadMode, ScrollMode } = PDFViewerApplicationConstants;
        PDFViewerApplication.pdfViewer.spreadMode = pages == "two" ? SpreadMode.ODD : SpreadMode.NONE;

        if (continuous)
            PDFViewerApplication.pdfViewer.scrollMode = ScrollMode.VERTICAL;
        else
            PDFViewerApplication.pdfViewer.scrollMode = ScrollMode.PAGE;

        this.autoResize();
    },

    autoResize() {
        if (PDFViewerApplication.pdfViewer.spreadMode == PDFViewerApplicationConstants.SpreadMode.ODD)
            PDFViewerApplication.pdfViewer.currentScaleValue = "page-fit";
        else
            PDFViewerApplication.pdfViewer.currentScaleValue = "page-width";
    },

    open(data) {
        PDFViewerApplication.open({ data });
    },

    init() {
        PDFViewerApplication.initializedPromise.then(() => {
            this.overrideSettings();
            PDFViewerApplication.eventBus.on("pagesinit", () => {
                this.autoResize();
            });
        });

        window.addEventListener("message", (event) => {
            const { message, data } = event.data;
            switch (message) {
                case "open-pdf":
                    this.open(data);
                    break;
                case "context-menu-auto-size":
                    this.autoResize();
                    break;
                case "context-menu-zoom-in":
                    PDFViewerApplication.zoomIn();
                    break;
                case "context-menu-zoom-out":
                    PDFViewerApplication.zoomOut();
                    break;
                case "context-menu-actual-size":
                    PDFViewerApplication.pdfViewer.currentScaleValue = "page-fit";
                    break;
                case "context-menu-single-page":
                    this.setPageMode({ pages: "single", continuous: false });
                    break;
                case "context-menu-single-page-continuous":
                    this.setPageMode({ pages: "single", continuous: true });
                    break;
                case "context-menu-two-pages":
                    this.setPageMode({ pages: "two", continuous: false });
                    break;
                case "context-menu-two-pages-continuous":
                    this.setPageMode({ pages: "two", continuous: true });
                    break;
                case "context-menu-next-page":
                    PDFViewerApplication.pdfViewer.nextPage();
                    break;
                case "context-menu-previous-page":
                    PDFViewerApplication.pdfViewer.previousPage();
                    break;
                default:
                    console.error("Unrecognized message:", event);
                    break;
            }
        });
    }
};

PDFJSContentScript.init();

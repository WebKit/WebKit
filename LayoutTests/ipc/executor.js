import { CoreIPC } from './coreipc.js';

export default class Executor {
    constructor() {
        this._streamConnection = CoreIPC.newStreamConnection();

        CoreIPC.GPU.GPUConnectionToWebProcess.CreateRenderingBackend(0, {
            renderingBackendIdentifier: 1000n,
            connectionHandle: this._streamConnection
        });

        this._remoteRenderingBackend = this._streamConnection.newInterface('RemoteRenderingBackend', 1000n);
        this._remoteImageBufferSet = this._streamConnection.newInterface('RemoteImageBufferSet', 1234n);
    }

    createInstance() {
        this._remoteRenderingBackend.CreateImageBufferSet({
            identifier: 1234n,
            contextIdentifier: 1235n,
        });

        this._remoteRenderingBackend.PrepareImageBufferSetsForDisplay({
            swapBuffersInput: [{
                remoteBufferSet: 1234n,
                dirtyRegion: {
                    data: {
                        m_segments: [],
                        m_spans: []
                    }
                },
                supportsPartialRepaint: true,
                hasEmptyDirtyRegion: true,
                requiresClearedPixels: true,
            }]
        });

        return {
            coreCrawler: this._coreCrawler,

            getInitIdentifierValues: () => {
                return [1235n];
            },

            sendMessage: (...args) => {
                this._streamConnection.connection.sendMessage(...args);
            },

            destroy: () => {
                this._remoteImageBufferSet.EndPrepareForDisplay({
                    renderingUpdateID: 1n
                });

                this._remoteRenderingBackend.ReleaseImageBufferSet({
                    identifier: 1234n,
                });
            }
        };
    }

    destroy() {
        this._streamConnection.connection.invalidate();
    }
}

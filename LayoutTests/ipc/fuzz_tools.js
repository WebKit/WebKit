if(!window.$F) {
    $F={};
    $F.enableListener= true;
}

if(window.testRunner) {
    testRunner.waitUntilDone();
}

if (window.IPC) {
    // Cache to avoid context switches from JavaScript to Native
    const IPCmessages = JSON.parse(JSON.stringify(IPC.messages));
    const IPCobjectIdentifiers = JSON.parse(JSON.stringify(IPC.objectIdentifiers));
    IPCobjectIdentifiers.push("PAL::SessionID");
    IPCobjectIdentifiers.push("WebCore::ProcessIdentifier");
    IPCobjectIdentifiers.push("WebKit::RemoteMediaSourceIdentifier");
    IPCobjectIdentifiers.push("WebCore::MediaPlayerIdentifier");
    IPCobjectIdentifiers.push("IPC::AsyncReplyID");
    IPCobjectIdentifiers.push("ActivityStateChangeID");
    IPCobjectIdentifiers.push("WebKit::EditorStateIdentifier");
    IPCobjectIdentifiers.push("WebCore::PageOverlay::PageOverlayID");
    IPCobjectIdentifiers.push("WebKit::DisplayLinkObserverID");
    IPCobjectIdentifiers.push("WebKit::TextCheckerRequestID");
    IPCobjectIdentifiers.push("WebKit::RemoteCDMIdentifier");
    IPCobjectIdentifiers.push("WebKit::RemoteCDMInstanceIdentifier");
    IPCobjectIdentifiers.push("WebKit::RemoteCDMInstanceSessionIdentifier");
    IPCobjectIdentifiers.push("WebKit::RemoteSourceBufferIdentifier");
    
    const IPCserializedTypeInfo = JSON.parse(JSON.stringify(IPC.serializedTypeInfo));
    const IPCserializedEnumInfo = JSON.parse(JSON.stringify(IPC.serializedEnumInfo));

    const processQualified = ["WebCore::FrameIdentifier","WebCore::ScriptExecutionContextIdentifier","WebCore::PolicyCheckIdentifier","WebCore::WebLockIdentifier","WebCore::PlatformLayerIdentifier","WebCore::BackForwardItemIdentifier","WebCore::SharedWorkerObjectIdentifier"];

    $F.GPUOutgoingHandler = {};
    $F.UIOutgoingHandler = {};
    $F.UIIncomingHandler = {};
    $F.GPUIncomingHandler = {};
    $F.NetworkingOutgoingHandler = {};
    $F.NetworkingIncomingHandler = {};

    function shouldDiscard(args) {
        for(let a of args) {
            if(Array.isArray(a)) {
                if(shouldDiscard(a)) return true;
            } else {
                if(a['type']) {
                    if(a['type'] == 'FrameID') {
                        if(!Array.isArray(a['value'])) return true;
                    } else if(a['type'] == 'Vector') {
                        if(shouldDiscard(a['value'])) return true;
                    }
                }
            }
        }
        return false;
    }

    $F.sendMessage = (process, connId, name, args) => {
        if(shouldDiscard(args)) return;
        if(window.$F) $F.enableListener = false;
        try{
            return IPC.sendMessage(process, connId, name, args);
        }catch(e) {
            $vm.print("[-] send exception: " + e);
        }
        finally {
            $F.enableListener = true;
        }
    }
    $F.sendSyncMessage = (process, connId, name, timeout, args) => {
        if(shouldDiscard(args)) return;
        if(window.$F) $F.enableListener = false;
        try{
            return IPC.sendSyncMessage(process, connId, name, timeout, args);
        }catch(e) {
            $vm.print("[-] sync send exception: " + e);
        }
        finally {
            $F.enableListener = true;
        }
    }

    IPC.addOutgoingMessageListener("GPU", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.GPUOutgoingHandler) {
                let func = $F.GPUOutgoingHandler[name];
                delete $F.GPUOutgoingHandler[name];
                func(msg);
            }
        }
    });

    IPC.addOutgoingMessageListener("UI", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.UIOutgoingHandler) {
                let func = $F.UIOutgoingHandler[name];
                delete $F.UIOutgoingHandler[name];
                func(msg);
            }
        }
    });

    IPC.addIncomingMessageListener("GPU", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.GPUIncomingHandler) {
                let func = $F.GPUIncomingHandler[name];
                delete $F.GPUIncomingHandler[name];
                func(msg);
            }
        }
    });

    IPC.addIncomingMessageListener("UI", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.UIIncomingHandler) {
                let func = $F.UIIncomingHandler[name];
                delete $F.UIIncomingHandler[name];
                func(msg);
            }
        }
    });

    IPC.addOutgoingMessageListener("Networking", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.NetworkingOutgoingHandler) {
                let func = $F.NetworkingOutgoingHandler[name];
                delete $F.NetworkingOutgoingHandler[name];
                func(msg);
            }
        }
    });

    IPC.addIncomingMessageListener("Networking", (msg) => {
        if(window.$F && $F.enableListener) {
            let name = msg.name;
            if (name in $F.NetworkingIncomingHandler) {
                let func = $F.NetworkingIncomingHandler[name];
                delete $F.NetworkingIncomingHandler[name];
                func(msg);
            }
        }
    });
}

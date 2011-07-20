/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamFrameController_h
#define MediaStreamFrameController_h

#if ENABLE(MEDIA_STREAM)

#include "ExceptionCode.h"
#include "MediaStreamClient.h"
#include "NavigatorUserMediaError.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Frame;
class LocalMediaStream;
class MediaStream;
class MediaStreamController;
class MediaStreamTrackList;
class NavigatorUserMediaErrorCallback;
class NavigatorUserMediaSuccessCallback;
class Page;
class ScriptExecutionContext;
class SecurityOrigin;

class MediaStreamFrameController {
    WTF_MAKE_NONCOPYABLE(MediaStreamFrameController);
public:
    template <typename IdType>
    class ClientBase {
        WTF_MAKE_NONCOPYABLE(ClientBase);
    public:
        ClientBase() : m_frameController(0), m_clientId(0) { }

        ClientBase(MediaStreamFrameController* frameController, const IdType& id)
            : m_frameController(frameController)
            , m_clientId(id) { }

        virtual ~ClientBase() { }

        MediaStreamFrameController* mediaStreamFrameController() const { return m_frameController; }
        const IdType& clientId() const { return m_clientId; }

        virtual bool isMediaStream() const { return false; }
        virtual bool isLocalMediaStream() const { return false; }
        virtual bool isGenericClient() const { return false; }

        // Called when the frame controller is being disconnected to the MediaStreamClient embedder.
        // Clients should override this to send any required shutdown messages.
        virtual void detachEmbedder() { }

    protected:
        // Used for objects that are optionally associated to the frame controller after construction, like the MediaStreamTracks.
        void associateFrameController(MediaStreamFrameController* frameController, const IdType& id)
        {
            ASSERT(!m_frameController && !m_clientId);
            m_frameController = frameController;
            m_clientId = id;
        }

        // Avoids repeating the same code in the unregister method of each derived class.
        // This is required since the controller's unregister methods make use of the isObject virtuals,
        // and the unregistration can be triggered either by the destructor of the client or by the disconnection of the frame.
        template <typename ClientType>
        void unregisterClient(ClientType* client)
        {
            if (!m_frameController)
                return;

            m_frameController->unregister(static_cast<ClientType*>(client));
            m_frameController = 0;
        }

        virtual void unregister() = 0;
        friend class MediaStreamFrameController;

        MediaStreamFrameController* m_frameController;
        IdType m_clientId;
    };

    class MediaStreamClient : public ClientBase<String> {
    public:
        MediaStreamClient(MediaStreamFrameController* frameController, const String& label, bool isLocalMediaStream)
            : ClientBase<String>(frameController, label)
            , m_isLocalMediaStream(isLocalMediaStream) { }

        virtual ~MediaStreamClient() { unregister(); }

        virtual bool isMediaStream() const { return true; }

        // Accessed by the destructor.
        virtual bool isLocalMediaStream() const { return m_isLocalMediaStream; }

        // Stream has ended for some external reason.
        virtual void streamEnded() = 0;

    private:
        virtual void unregister() { unregisterClient(this); }
        bool m_isLocalMediaStream;
    };

    // Generic clients are objects that require access to the frame controller but don't have a global id (like streams do)
    // or need to receive any messages from the embedder client.
    class GenericClient : public ClientBase<int> {
    public:
        GenericClient() { }
        GenericClient(MediaStreamFrameController* frameController, int id) : ClientBase<int>(frameController, id) { }
        virtual ~GenericClient() { unregister(); }
        virtual bool isGenericClient() const { return true; }

    private:
        virtual void unregister() { unregisterClient(this); }
    };

    MediaStreamFrameController(Frame*);
    virtual ~MediaStreamFrameController();

    SecurityOrigin* securityOrigin() const;
    ScriptExecutionContext* scriptExecutionContext() const;

    bool isClientAvailable() const;
    void disconnectPage();
    void disconnectFrame();
    void transferToNewPage(Page*);

    // Parse the options string provided to the generateStream method.
    static GenerateStreamOptionFlags parseGenerateStreamOptions(const String&);

    // Create a new generated stream asynchronously with the provided options.
    void generateStream(const String& options, PassRefPtr<NavigatorUserMediaSuccessCallback>, PassRefPtr<NavigatorUserMediaErrorCallback>, ExceptionCode&);

    // Stop a local media stream.
    void stopGeneratedStream(const String& streamLabel);

    // Enable/disable an track.
    void setMediaStreamTrackEnabled(const String& trackId, bool enabled);

    // --- Calls coming back from the controller. --- //

    // Report the generation of a new local stream.
    void streamGenerated(int requestId, const String& streamLabel, PassRefPtr<MediaStreamTrackList> tracks);

    // Report a failure in the generation of a new stream.
    void streamGenerationFailed(int requestId, NavigatorUserMediaError::ErrorCode);

    // Report the end of a stream for external reasons.
    void streamFailed(const String& streamLabel);

private:
    class Request;
    class GenerateStreamRequest;

    class IdGenerator {
        WTF_MAKE_NONCOPYABLE(IdGenerator);
    public:
        IdGenerator() : m_id(0) { }
        int getNextId() { return ++m_id; }

    private:
        int m_id;
    };

    class RequestMap : public IdGenerator, public HashMap<int, RefPtr<Request> > {
    public:
        void abort(int requestId);
        void abortAll();
    };

    template <typename IdType>
    class ClientMapBase : public HashMap<IdType, ClientBase<IdType>* > {
        WTF_MAKE_NONCOPYABLE(ClientMapBase);
    public:
        typedef HashMap<IdType, ClientBase<IdType>* > MapType;

        ClientMapBase() { }
        void unregisterAll();
        void detachEmbedder();
    };

    // Streams are a special class of clients since they are identified by a global label string instead of an id.
    typedef ClientMapBase<String> StreamMap;

    // All other types of clients use autogenerated integer ids.
    class ClientMap : public IdGenerator, public ClientMapBase<int> { };

    // Detached from a page, and hence from a embedder client.
    void enterDetachedState();

    void unregister(MediaStreamClient*);
    void unregister(GenericClient*);
    MediaStreamController* pageController() const;
    MediaStream* getStreamFromLabel(const String&) const;

    RequestMap m_requests;
    ClientMap m_clients;
    StreamMap m_streams;

    Frame* m_frame;
    bool m_isInDetachedState;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamFrameController_h

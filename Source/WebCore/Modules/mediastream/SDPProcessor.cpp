/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "SDPProcessor.h"

#include "Document.h"
#include "Frame.h"
#include "SDPProcessorScriptResource.h"
#include "ScriptController.h"
#include "ScriptGlobalObject.h"
#include "ScriptSourceCode.h"
#include "inspector/InspectorValues.h"
#include <bindings/ScriptObject.h>
#include <wtf/NeverDestroyed.h>

using namespace Inspector;

namespace WebCore {

#define STRING_FUNCTION(name) \
    static const String& name##String() \
    { \
        static NeverDestroyed<const String> name { ASCIILiteral(#name) }; \
        return name; \
    }

STRING_FUNCTION(address)
STRING_FUNCTION(apt)
STRING_FUNCTION(candidates)
STRING_FUNCTION(ccmfir)
STRING_FUNCTION(channels)
STRING_FUNCTION(clockRate)
STRING_FUNCTION(cname)
STRING_FUNCTION(componentId)
STRING_FUNCTION(dtls)
STRING_FUNCTION(encodingName)
STRING_FUNCTION(fingerprint)
STRING_FUNCTION(fingerprintHashFunction)
STRING_FUNCTION(foundation)
STRING_FUNCTION(ice)
STRING_FUNCTION(mediaDescriptions)
STRING_FUNCTION(mediaStreamId)
STRING_FUNCTION(mediaStreamTrackId)
STRING_FUNCTION(mode)
STRING_FUNCTION(mux)
STRING_FUNCTION(nack)
STRING_FUNCTION(nackpli)
STRING_FUNCTION(originator)
STRING_FUNCTION(packetizationMode)
STRING_FUNCTION(parameters)
STRING_FUNCTION(password)
STRING_FUNCTION(payloads)
STRING_FUNCTION(port)
STRING_FUNCTION(priority)
STRING_FUNCTION(relatedAddress)
STRING_FUNCTION(relatedPort)
STRING_FUNCTION(rtcp)
STRING_FUNCTION(rtcpAddress)
STRING_FUNCTION(rtcpPort)
STRING_FUNCTION(rtxTime)
STRING_FUNCTION(sessionId)
STRING_FUNCTION(sessionVersion)
STRING_FUNCTION(setup)
STRING_FUNCTION(ssrcs)
STRING_FUNCTION(tcpType)
STRING_FUNCTION(transport)
STRING_FUNCTION(type)
STRING_FUNCTION(ufrag)

SDPProcessor::SDPProcessor(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
{
}

// Note that MediaEndpointSessionConfiguration is a "flatter" structure that the JSON representation. For
// example, the JSON representation has an "ice" object which collects a set of properties under a
// namespace. MediaEndpointSessionConfiguration has "ice"-prefixes on the corresponding properties.

static RefPtr<InspectorObject> createCandidateObject(const IceCandidate& candidate)
{
    RefPtr<InspectorObject> candidateObject = InspectorObject::create();

    candidateObject->setString(typeString(), candidate.type());
    candidateObject->setString(foundationString(), candidate.foundation());
    candidateObject->setInteger(componentIdString(), candidate.componentId());
    candidateObject->setString(transportString(), candidate.transport());
    candidateObject->setInteger(priorityString(), candidate.priority());
    candidateObject->setString(addressString(), candidate.address());
    candidateObject->setInteger(portString(), candidate.port());
    if (!candidate.tcpType().isEmpty())
        candidateObject->setString(tcpTypeString(), candidate.tcpType());
    if (candidate.type().convertToASCIIUppercase() != "HOST") {
        candidateObject->setString(relatedAddressString(), candidate.relatedAddress());
        candidateObject->setInteger(relatedPortString(), candidate.relatedPort());
    }

    return candidateObject;
}

static RefPtr<IceCandidate> createCandidate(const InspectorObject& candidateObject)
{
    RefPtr<IceCandidate> candidate = IceCandidate::create();
    String stringValue;
    unsigned intValue;

    if (candidateObject.getString(typeString(), stringValue))
        candidate->setType(stringValue);

    if (candidateObject.getString(foundationString(), stringValue))
        candidate->setFoundation(stringValue);

    if (candidateObject.getInteger(componentIdString(), intValue))
        candidate->setComponentId(intValue);

    if (candidateObject.getString(transportString(), stringValue))
        candidate->setTransport(stringValue);

    if (candidateObject.getInteger(priorityString(), intValue))
        candidate->setPriority(intValue);

    if (candidateObject.getString(addressString(), stringValue))
        candidate->setAddress(stringValue);

    if (candidateObject.getInteger(portString(), intValue))
        candidate->setPort(intValue);

    if (candidateObject.getString(tcpTypeString(), stringValue))
        candidate->setTcpType(stringValue);

    if (candidateObject.getString(relatedAddressString(), stringValue))
        candidate->setRelatedAddress(stringValue);

    if (candidateObject.getInteger(relatedPortString(), intValue))
        candidate->setRelatedPort(intValue);

    return candidate;
}

static RefPtr<MediaEndpointSessionConfiguration> configurationFromJSON(const String& json)
{
    RefPtr<InspectorValue> value;
    if (!InspectorValue::parseJSON(json, value))
        return nullptr;

    RefPtr<InspectorObject> object;
    if (!value->asObject(object))
        return nullptr;

    RefPtr<MediaEndpointSessionConfiguration> configuration = MediaEndpointSessionConfiguration::create();

    String stringValue;
    unsigned intValue;
    unsigned long longValue;
    bool boolValue;

    RefPtr<InspectorObject> originatorObject = InspectorObject::create();
    if (object->getObject(originatorString(), originatorObject)) {
        if (originatorObject->getInteger(sessionIdString(), longValue))
            configuration->setSessionId(longValue);
        if (originatorObject->getInteger(sessionVersionString(), intValue))
            configuration->setSessionVersion(intValue);
    }

    RefPtr<InspectorArray> mediaDescriptionsArray = InspectorArray::create();
    object->getArray(mediaDescriptionsString(), mediaDescriptionsArray);

    for (unsigned i = 0; i < mediaDescriptionsArray->length(); ++i) {
        RefPtr<InspectorObject> mediaDescriptionObject = InspectorObject::create();
        mediaDescriptionsArray->get(i)->asObject(mediaDescriptionObject);

        RefPtr<PeerMediaDescription> mediaDescription = PeerMediaDescription::create();

        if (mediaDescriptionObject->getString(typeString(), stringValue))
            mediaDescription->setType(stringValue);

        if (mediaDescriptionObject->getInteger(portString(), intValue))
            mediaDescription->setPort(intValue);

        if (mediaDescriptionObject->getString(addressString(), stringValue))
            mediaDescription->setAddress(stringValue);

        if (mediaDescriptionObject->getString(modeString(), stringValue))
            mediaDescription->setMode(stringValue);

        RefPtr<InspectorArray> payloadsArray = InspectorArray::create();
        mediaDescriptionObject->getArray(payloadsString(), payloadsArray);

        for (unsigned j = 0; j < payloadsArray->length(); ++j) {
            RefPtr<InspectorObject> payloadsObject = InspectorObject::create();
            payloadsArray->get(j)->asObject(payloadsObject);

            RefPtr<MediaPayload> payload = MediaPayload::create();

            if (payloadsObject->getInteger(typeString(), intValue))
                payload->setType(intValue);

            if (payloadsObject->getString(encodingNameString(), stringValue))
                payload->setEncodingName(stringValue);

            if (payloadsObject->getInteger(clockRateString(), intValue))
                payload->setClockRate(intValue);

            if (payloadsObject->getInteger(channelsString(), intValue))
                payload->setChannels(intValue);

            if (payloadsObject->getBoolean(ccmfirString(), boolValue))
                payload->setCcmfir(boolValue);

            if (payloadsObject->getBoolean(nackpliString(), boolValue))
                payload->setNackpli(boolValue);

            if (payloadsObject->getBoolean(nackString(), boolValue))
                payload->setNack(boolValue);

            RefPtr<InspectorObject> parametersObject = InspectorObject::create();
            if (payloadsObject->getObject(parametersString(), parametersObject)) {
                if (parametersObject->getInteger(packetizationModeString(), intValue))
                    payload->addParameter("packetizationMode", intValue);

                if (parametersObject->getInteger(aptString(), intValue))
                    payload->addParameter("apt", intValue);

                if (parametersObject->getInteger(rtxTimeString(), intValue))
                    payload->addParameter("rtxTime", intValue);
            }

            mediaDescription->addPayload(WTFMove(payload));
        }

        RefPtr<InspectorObject> rtcpObject = InspectorObject::create();
        if (mediaDescriptionObject->getObject(rtcpString(), rtcpObject)) {
            if (rtcpObject->getBoolean(muxString(), boolValue))
                mediaDescription->setRtcpMux(boolValue);

            if (rtcpObject->getString(rtcpAddressString(), stringValue))
                mediaDescription->setRtcpAddress(stringValue);

            if (rtcpObject->getInteger(rtcpPortString(), intValue))
                mediaDescription->setRtcpPort(intValue);
        }

        if (mediaDescriptionObject->getString(mediaStreamIdString(), stringValue))
            mediaDescription->setMediaStreamId(stringValue);

        if (mediaDescriptionObject->getString(mediaStreamTrackIdString(), stringValue))
            mediaDescription->setMediaStreamTrackId(stringValue);

        RefPtr<InspectorObject> dtlsObject = InspectorObject::create();
        if (mediaDescriptionObject->getObject(dtlsString(), dtlsObject)) {
            if (dtlsObject->getString(setupString(), stringValue))
                mediaDescription->setDtlsSetup(stringValue);

            if (dtlsObject->getString(fingerprintHashFunctionString(), stringValue))
                mediaDescription->setDtlsFingerprintHashFunction(stringValue);

            if (dtlsObject->getString(fingerprintString(), stringValue))
                mediaDescription->setDtlsFingerprint(stringValue);
        }

        RefPtr<InspectorArray> ssrcsArray = InspectorArray::create();
        mediaDescriptionObject->getArray(ssrcsString(), ssrcsArray);

        for (unsigned j = 0; j < ssrcsArray->length(); ++j) {
            ssrcsArray->get(j)->asInteger(intValue);
            mediaDescription->addSsrc(intValue);
        }

        if (mediaDescriptionObject->getString(cnameString(), stringValue))
            mediaDescription->setCname(stringValue);

        RefPtr<InspectorObject> iceObject = InspectorObject::create();
        if (mediaDescriptionObject->getObject(iceString(), iceObject)) {
            if (iceObject->getString(ufragString(), stringValue))
                mediaDescription->setIceUfrag(stringValue);

            if (iceObject->getString(passwordString(), stringValue))
                mediaDescription->setIcePassword(stringValue);

            RefPtr<InspectorArray> candidatesArray = InspectorArray::create();
            iceObject->getArray(candidatesString(), candidatesArray);

            for (unsigned j = 0; j < candidatesArray->length(); ++j) {
                RefPtr<InspectorObject> candidateObject = InspectorObject::create();
                candidatesArray->get(j)->asObject(candidateObject);

                mediaDescription->addIceCandidate(createCandidate(*candidateObject));
            }
        }

        configuration->addMediaDescription(WTFMove(mediaDescription));
    }

    return configuration;
}

static RefPtr<IceCandidate> iceCandidateFromJSON(const String& json)
{
    RefPtr<InspectorValue> value;
    if (!InspectorValue::parseJSON(json, value))
        return nullptr;

    RefPtr<InspectorObject> candidateObject;
    if (!value->asObject(candidateObject))
        return nullptr;

    return createCandidate(*candidateObject);
}

static String configurationToJSON(const MediaEndpointSessionConfiguration& configuration)
{
    RefPtr<InspectorObject> object = InspectorObject::create();

    RefPtr<InspectorObject> originatorObject = InspectorObject::create();
    originatorObject->setDouble(sessionIdString(), configuration.sessionId());
    originatorObject->setInteger(sessionVersionString(), configuration.sessionVersion());
    object->setObject(originatorString(), originatorObject);

    RefPtr<InspectorArray> mediaDescriptionsArray = InspectorArray::create();

    for (const RefPtr<PeerMediaDescription>& mediaDescription : configuration.mediaDescriptions()) {
        RefPtr<InspectorObject> mediaDescriptionObject = InspectorObject::create();

        mediaDescriptionObject->setString(typeString(), mediaDescription->type());
        mediaDescriptionObject->setInteger(portString(), mediaDescription->port());
        mediaDescriptionObject->setString(addressString(), mediaDescription->address());
        mediaDescriptionObject->setString(modeString(), mediaDescription->mode());

        RefPtr<InspectorArray> payloadsArray = InspectorArray::create();

        for (RefPtr<MediaPayload> payload : mediaDescription->payloads()) {
            RefPtr<InspectorObject> payloadObject = InspectorObject::create();

            payloadObject->setInteger(typeString(), payload->type());
            payloadObject->setString(encodingNameString(), payload->encodingName());
            payloadObject->setInteger(clockRateString(), payload->clockRate());
            payloadObject->setInteger(channelsString(), payload->channels());
            payloadObject->setBoolean(ccmfirString(), payload->ccmfir());
            payloadObject->setBoolean(nackpliString(), payload->nackpli());
            payloadObject->setBoolean(nackString(), payload->nack());

            if (!payload->parameters().isEmpty()) {
                RefPtr<InspectorObject> parametersObject = InspectorObject::create();

                for (auto& name : payload->parameters().keys())
                    parametersObject->setInteger(name, payload->parameters().get(name));

                payloadObject->setObject(parametersString(), parametersObject);
            }

            payloadsArray->pushObject(payloadObject);
        }
        mediaDescriptionObject->setArray(payloadsString(), payloadsArray);

        RefPtr<InspectorObject> rtcpObject = InspectorObject::create();
        rtcpObject->setBoolean(muxString(), mediaDescription->rtcpMux());
        rtcpObject->setString(addressString(), mediaDescription->rtcpAddress());
        rtcpObject->setInteger(portString(), mediaDescription->rtcpPort());
        mediaDescriptionObject->setObject(rtcpString(), rtcpObject);

        mediaDescriptionObject->setString(mediaStreamIdString(), mediaDescription->mediaStreamId());
        mediaDescriptionObject->setString(mediaStreamTrackIdString(), mediaDescription->mediaStreamTrackId());

        RefPtr<InspectorObject> dtlsObject = InspectorObject::create();
        dtlsObject->setString(setupString(), mediaDescription->dtlsSetup());
        dtlsObject->setString(fingerprintHashFunctionString(), mediaDescription->dtlsFingerprintHashFunction());
        dtlsObject->setString(fingerprintString(), mediaDescription->dtlsFingerprint());
        mediaDescriptionObject->setObject(dtlsString(), dtlsObject);

        RefPtr<InspectorArray> ssrcsArray = InspectorArray::create();

        for (auto ssrc : mediaDescription->ssrcs())
            ssrcsArray->pushDouble(ssrc);
        mediaDescriptionObject->setArray(ssrcsString(), ssrcsArray);

        mediaDescriptionObject->setString(cnameString(), mediaDescription->cname());

        RefPtr<InspectorObject> iceObject = InspectorObject::create();
        iceObject->setString(ufragString(), mediaDescription->iceUfrag());
        iceObject->setString(passwordString(), mediaDescription->icePassword());

        RefPtr<InspectorArray> candidatesArray = InspectorArray::create();

        for (RefPtr<IceCandidate> candidate : mediaDescription->iceCandidates())
            candidatesArray->pushObject(createCandidateObject(*candidate));

        iceObject->setArray(candidatesString(), candidatesArray);
        mediaDescriptionObject->setObject(iceString(), iceObject);

        mediaDescriptionsArray->pushObject(mediaDescriptionObject);
    }
    object->setArray(mediaDescriptionsString(), mediaDescriptionsArray);

    return object->toJSONString();
}

static String iceCandidateToJSON(const IceCandidate& candidate)
{
    return createCandidateObject(candidate)->toJSONString();
}

SDPProcessor::Result SDPProcessor::generate(const MediaEndpointSessionConfiguration& configuration, String& outSdpString) const
{
    String sdpString;
    if (!callScript("generate", configurationToJSON(configuration), sdpString))
        return Result::InternalError;

    outSdpString = sdpString;
    return Result::Success;
}

SDPProcessor::Result SDPProcessor::parse(const String& sdp, RefPtr<MediaEndpointSessionConfiguration>& outConfiguration) const
{
    String scriptOutput;
    if (!callScript("parse", sdp, scriptOutput))
        return Result::InternalError;

    if (scriptOutput == "ParseError")
        return Result::ParseError;

    RefPtr<MediaEndpointSessionConfiguration> configuration = configurationFromJSON(scriptOutput);
    if (!configuration)
        return Result::InternalError;

    outConfiguration = configuration;
    return Result::Success;
}

SDPProcessor::Result SDPProcessor::generateCandidateLine(const IceCandidate& candidate, String& outCandidateLine) const
{
    String candidateLine;
    if (!callScript("generateCandidateLine", iceCandidateToJSON(candidate), candidateLine))
        return Result::InternalError;

    outCandidateLine = candidateLine;
    return Result::Success;
}

SDPProcessor::Result SDPProcessor::parseCandidateLine(const String& candidateLine, RefPtr<IceCandidate>& outCandidate) const
{
    String scriptOutput;
    if (!callScript("parseCandidateLine", candidateLine, scriptOutput))
        return Result::InternalError;

    if (scriptOutput == "ParseError")
        return Result::ParseError;

    RefPtr<IceCandidate> candidate = iceCandidateFromJSON(scriptOutput);
    if (!candidate)
        return Result::InternalError;

    outCandidate = candidate;
    return Result::Success;
}

bool SDPProcessor::callScript(const String& functionName, const String& argument, String& outResult) const
{
    if (!scriptExecutionContext())
        return false;

    Document* document = downcast<Document>(scriptExecutionContext());
    if (!document->frame())
        return false;

    if (!m_isolatedWorld)
        m_isolatedWorld = DOMWrapperWorld::create(JSDOMWindow::commonVM());

    ScriptController& scriptController = document->frame()->script();
    JSDOMGlobalObject* globalObject = JSC::jsCast<JSDOMGlobalObject*>(scriptController.globalObject(*m_isolatedWorld));
    JSC::ExecState* exec = globalObject->globalExec();
    JSC::JSLockHolder lock(exec);

    JSC::JSValue probeFunctionValue = globalObject->get(exec, JSC::Identifier::fromString(exec, "generate"));
    if (!probeFunctionValue.isFunction()) {
        URL scriptURL;
        scriptController.evaluateInWorld(ScriptSourceCode(SDPProcessorScriptResource::scriptString(), scriptURL), *m_isolatedWorld);
        if (exec->hadException()) {
            exec->clearException();
            return false;
        }
    }

    JSC::JSValue functionValue = globalObject->get(exec, JSC::Identifier::fromString(exec, functionName));
    if (!functionValue.isFunction())
        return false;

    JSC::JSObject* function = functionValue.toObject(exec);
    JSC::CallData callData;
    JSC::CallType callType = function->methodTable()->getCallData(function, callData);
    if (callType == JSC::CallType::None)
        return false;

    JSC::MarkedArgumentBuffer argList;
    argList.append(JSC::jsString(exec, argument));

    JSC::JSValue result = JSC::call(exec, function, callType, callData, globalObject, argList);
    if (exec->hadException()) {
        LOG_ERROR("SDPProcessor script threw in function %s", functionName.ascii().data());
        exec->clearException();
        return false;
    }

    if (!result.isString())
        return false;

    outResult = result.getString(exec);
    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

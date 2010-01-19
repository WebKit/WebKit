/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationServiceBridge.h"

#include "GeolocationServiceAndroid.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "WebViewCore.h"
#include <JNIHelp.h>

namespace WebCore {

using JSC::Bindings::getJNIEnv;

static const char* javaGeolocationServiceClassName = "android/webkit/GeolocationService";
enum javaGeolocationServiceClassMethods {
    GeolocationServiceMethodInit = 0,
    GeolocationServiceMethodStart,
    GeolocationServiceMethodStop,
    GeolocationServiceMethodSetEnableGps,
    GeolocationServiceMethodCount,
};
static jmethodID javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodCount];

static const JNINativeMethod javaGeolocationServiceClassNativeMethods[] = {
    { "nativeNewLocationAvailable", "(JLandroid/location/Location;)V",
        (void*) GeolocationServiceBridge::newLocationAvailable },
    { "nativeNewErrorAvailable", "(JLjava/lang/String;)V",
        (void*) GeolocationServiceBridge::newErrorAvailable }
};

static const char *javaLocationClassName = "android/location/Location";
enum javaLocationClassMethods {
    LocationMethodGetLatitude = 0,
    LocationMethodGetLongitude,
    LocationMethodHasAltitude,
    LocationMethodGetAltitude,
    LocationMethodHasAccuracy,
    LocationMethodGetAccuracy,
    LocationMethodHasBearing,
    LocationMethodGetBearing,
    LocationMethodHasSpeed,
    LocationMethodGetSpeed,
    LocationMethodGetTime,
    LocationMethodCount,
};
static jmethodID javaLocationClassMethodIDs[LocationMethodCount];

GeolocationServiceBridge::GeolocationServiceBridge(ListenerInterface* listener)
    : m_listener(listener)
    , m_javaGeolocationServiceObject(0)
{
    ASSERT(m_listener);
    startJavaImplementation();
}

GeolocationServiceBridge::~GeolocationServiceBridge()
{
    stop();
    stopJavaImplementation();
}

void GeolocationServiceBridge::start()
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodStart]);
}

void GeolocationServiceBridge::stop()
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodStop]);
}

void GeolocationServiceBridge::setEnableGps(bool enable)
{
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->CallVoidMethod(m_javaGeolocationServiceObject,
                                javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodSetEnableGps],
                                enable);
}

void GeolocationServiceBridge::newLocationAvailable(JNIEnv* env, jclass, jlong nativeObject, jobject location)
{
    ASSERT(nativeObject);
    ASSERT(location);
    GeolocationServiceBridge* object = reinterpret_cast<GeolocationServiceBridge*>(nativeObject);
    object->m_listener->newPositionAvailable(toGeoposition(env, location));
}

void GeolocationServiceBridge::newErrorAvailable(JNIEnv* env, jclass, jlong nativeObject, jstring message)
{
    GeolocationServiceBridge* object = reinterpret_cast<GeolocationServiceBridge*>(nativeObject);
    RefPtr<PositionError> error =
        PositionError::create(PositionError::POSITION_UNAVAILABLE, android::to_string(env, message));
    object->m_listener->newErrorAvailable(error.release());
}

PassRefPtr<Geoposition> GeolocationServiceBridge::toGeoposition(JNIEnv *env, const jobject &location)
{
    // Altitude is optional and may not be supplied.
    bool hasAltitude =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LocationMethodHasAltitude]);
    double Altitude =
        hasAltitude ?
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LocationMethodGetAltitude]) :
        0.0;
    // Accuracy is required, but is not supplied by the emulator.
    double Accuracy =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LocationMethodHasAccuracy]) ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LocationMethodGetAccuracy]) :
        0.0;
    // heading is optional and may not be supplied.
    bool hasHeading =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LocationMethodHasBearing]);
    double heading =
        hasHeading ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LocationMethodGetBearing]) :
        0.0;
    // speed is optional and may not be supplied.
    bool hasSpeed =
        env->CallBooleanMethod(location, javaLocationClassMethodIDs[LocationMethodHasSpeed]);
    double speed =
        hasSpeed ?
        env->CallFloatMethod(location, javaLocationClassMethodIDs[LocationMethodGetSpeed]) :
        0.0;

    RefPtr<Coordinates> newCoordinates = WebCore::Coordinates::create(
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LocationMethodGetLatitude]),
        env->CallDoubleMethod(location, javaLocationClassMethodIDs[LocationMethodGetLongitude]),
        hasAltitude, Altitude,
        Accuracy,
        false, 0.0,  // AltitudeAccuracy not provided.
        hasHeading, heading,
        hasSpeed, speed);

    return WebCore::Geoposition::create(
         newCoordinates.release(),
         env->CallLongMethod(location, javaLocationClassMethodIDs[LocationMethodGetTime]));
}

void GeolocationServiceBridge::startJavaImplementation()
{
    JNIEnv* env = getJNIEnv();

    // Get the Java GeolocationService class.
    jclass javaGeolocationServiceClass = env->FindClass(javaGeolocationServiceClassName);
    ASSERT(javaGeolocationServiceClass);

    // Set up the methods we wish to call on the Java GeolocationService class.
    javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodInit] =
            env->GetMethodID(javaGeolocationServiceClass, "<init>", "(J)V");
    javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodStart] =
            env->GetMethodID(javaGeolocationServiceClass, "start", "()V");
    javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodStop] =
            env->GetMethodID(javaGeolocationServiceClass, "stop", "()V");
    javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodSetEnableGps] =
            env->GetMethodID(javaGeolocationServiceClass, "setEnableGps", "(Z)V");

    // Create the Java GeolocationService object.
    jlong nativeObject = reinterpret_cast<jlong>(this);
    jobject object = env->NewObject(javaGeolocationServiceClass,
                                    javaGeolocationServiceClassMethodIDs[GeolocationServiceMethodInit],
                                    nativeObject);

    m_javaGeolocationServiceObject = getJNIEnv()->NewGlobalRef(object);
    ASSERT(m_javaGeolocationServiceObject);

    // Register to handle calls to native methods of the Java GeolocationService
    // object. We register once only.
    static int registered = jniRegisterNativeMethods(env,
                                                     javaGeolocationServiceClassName,
                                                     javaGeolocationServiceClassNativeMethods,
                                                     NELEM(javaGeolocationServiceClassNativeMethods));
    ASSERT(registered == JNI_OK);

    // Set up the methods we wish to call on the Java Location class.
    jclass javaLocationClass = env->FindClass(javaLocationClassName);
    ASSERT(javaLocationClass);
    javaLocationClassMethodIDs[LocationMethodGetLatitude] =
        env->GetMethodID(javaLocationClass, "getLatitude", "()D");
    javaLocationClassMethodIDs[LocationMethodGetLongitude] =
        env->GetMethodID(javaLocationClass, "getLongitude", "()D");
    javaLocationClassMethodIDs[LocationMethodHasAltitude] =
        env->GetMethodID(javaLocationClass, "hasAltitude", "()Z");
    javaLocationClassMethodIDs[LocationMethodGetAltitude] =
        env->GetMethodID(javaLocationClass, "getAltitude", "()D");
    javaLocationClassMethodIDs[LocationMethodHasAccuracy] =
        env->GetMethodID(javaLocationClass, "hasAccuracy", "()Z");
    javaLocationClassMethodIDs[LocationMethodGetAccuracy] =
        env->GetMethodID(javaLocationClass, "getAccuracy", "()F");
    javaLocationClassMethodIDs[LocationMethodHasBearing] =
        env->GetMethodID(javaLocationClass, "hasBearing", "()Z");
    javaLocationClassMethodIDs[LocationMethodGetBearing] =
        env->GetMethodID(javaLocationClass, "getBearing", "()F");
    javaLocationClassMethodIDs[LocationMethodHasSpeed] =
        env->GetMethodID(javaLocationClass, "hasSpeed", "()Z");
    javaLocationClassMethodIDs[LocationMethodGetSpeed] =
        env->GetMethodID(javaLocationClass, "getSpeed", "()F");
    javaLocationClassMethodIDs[LocationMethodGetTime] =
        env->GetMethodID(javaLocationClass, "getTime", "()J");
}

void GeolocationServiceBridge::stopJavaImplementation()
{
    // Called by GeolocationServiceAndroid on WebKit thread.
    ASSERT(m_javaGeolocationServiceObject);
    getJNIEnv()->DeleteGlobalRef(m_javaGeolocationServiceObject);
}

} // namespace WebCore

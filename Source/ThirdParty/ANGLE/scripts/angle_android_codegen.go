//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package angle_android_codegen

import (
    "android/soong/android"
    "android/soong/cc"
)

func init() {
    android.RegisterModuleType(
        "angle_android_codegen", angle_android_codegen_DefaultsFactory)
}

// Values passed in from Android.bp
type angle_android_codegen_Properties struct {
}

func angle_android_codegen_Defaults(g *angle_android_codegen_Properties) func(ctx android.LoadHookContext) {
    return func(ctx android.LoadHookContext) {

        // Structure to write out new values
        type props struct {
        }

        p := &props{}

        ctx.AppendProperties(p)
    }
}

func angle_android_codegen_DefaultsFactory() android.Module {
    module := cc.DefaultsFactory()
    props := &angle_android_codegen_Properties{}
    module.AddProperties(props)
    android.AddLoadHook(module, angle_android_codegen_Defaults(props))
    return module
}
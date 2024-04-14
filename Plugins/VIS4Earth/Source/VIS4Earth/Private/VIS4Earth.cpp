// Copyright Epic Games, Inc. All Rights Reserved.

#include "VIS4Earth.h"

#define LOCTEXT_NAMESPACE "FVIS4EarthModule"

void FVIS4EarthModule::StartupModule() {
    // This code will execute after your module is loaded into memory; the exact timing is specified
    // in the .uplugin file per-module
    auto shaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("VIS4Earth/shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/VIS4Earth"), shaderDir);
}

void FVIS4EarthModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.  For modules that
    // support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVIS4EarthModule, VIS4Earth)

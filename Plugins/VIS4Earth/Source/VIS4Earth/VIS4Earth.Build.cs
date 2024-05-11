// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VIS4Earth : ModuleRules
{
    public VIS4Earth(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "RHI",
                "RenderCore",
                "Core",
				// ... add other public dependencies that you statically link with here ...
				"UMG",
                "MeshDescription",
                "StaticMeshDescription",
                "MeshConversion",
                "CesiumRuntime"
            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );

        CppStandard = CppStandardVersion.Cpp20;
    }
}

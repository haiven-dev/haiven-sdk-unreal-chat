using UnrealBuildTool;

public class HaivenChatStarter : ModuleRules
{
	public HaivenChatStarter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"Haiven",
			"UMG",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Json",
			"Projects",
			"Slate",
			"SlateCore",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}

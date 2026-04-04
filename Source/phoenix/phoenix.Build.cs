using UnrealBuildTool;

public class phoenix : ModuleRules
{
	public phoenix(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"SlateCore",
			"CableComponent",
			"Niagara"
		});
	}
}

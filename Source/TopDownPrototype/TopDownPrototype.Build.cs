using UnrealBuildTool;

public class TopDownPrototype : ModuleRules
{
    public TopDownPrototype(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
        PrivateDependencyModuleNames.Add("AnimGraphRuntime");
    }
}

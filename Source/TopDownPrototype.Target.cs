using UnrealBuildTool;

public class TopDownPrototypeTarget : TargetRules
{
    public TopDownPrototypeTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("TopDownPrototype");
    }
}

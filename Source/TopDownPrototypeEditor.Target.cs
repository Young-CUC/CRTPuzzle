using UnrealBuildTool;

public class TopDownPrototypeEditorTarget : TargetRules
{
    public TopDownPrototypeEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("TopDownPrototype");
    }
}

using UnrealBuildTool;
using System.Collections.Generic;

public class EchoLineEditorTarget : TargetRules
{
	public EchoLineEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("EchoLine");
	}
}

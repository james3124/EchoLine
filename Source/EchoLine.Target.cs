using UnrealBuildTool;
using System.Collections.Generic;

public class EchoLineTarget : TargetRules
{
	public EchoLineTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("EchoLine");
	}
}

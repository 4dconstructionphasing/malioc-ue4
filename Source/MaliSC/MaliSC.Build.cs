/*
 * Copyright 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace UnrealBuildTool.Rules
{
    public class MaliSC : ModuleRules
    {
        public MaliSC(TargetInfo Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "InputCore",
                    "EditorStyle",
                    "Engine",
                    "MaterialEditor",
                    "ShaderCore",
                    "Slate",
                    "SlateCore",
                    "UnrealEd",
                    "CoreUObject",
                    "RHI",
                    "OpenGLDrv"
                }
            );
        }
    }
}
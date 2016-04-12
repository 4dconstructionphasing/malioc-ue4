# Copyright 2015 ARM Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"Running exhaustive Mali Offline Compiler plugin for Unreal Engine 4 tests. This may take a while..."

"Initialising shader cache"

# Run once with just the block command followed by a quit - this initialises the cache
../../../../Binaries/Win64/UE4Editor-Cmd.exe " -log -forcelogflush -stdout -AllowStdOutLogVerbosity -nosplash -nosound -unattended -execcmds=`"automation runtests MaliOC.BlockUntilAllShaderCompilationComplete`;quit`" "

"Running Tests"

# Run the actual tests this time and track the output
../../../../Binaries/Win64/UE4Editor-Cmd.exe " -log -forcelogflush -stdout -AllowStdOutLogVerbosity -nosplash -nosound -unattended -execcmds=`"automation runtests MaliOC.CompilationReport.MSM_Unlit`;quit`" " 2>&1 | Tee-Object -Variable UE4Log
$UE4ExitCode = $LASTEXITCODE

# We also have MaliOC.CompilationReport.MSM_DefaultLit tests which we don't run because they are very long running and they don't test anything that doesn't get tested with the MaliOC.CompilationReport.MSM_Unlit tests.

$Errors = @($UE4Log | Select-String -pattern "Automation Test Failed" -context 5,0)
$NumErrors = @($Errors).Count

$Successes = @($UE4Log | Select-String -pattern "AutomationTestingLog: Info ...Automation Test Succeeded")
$NumSuccesses = @($Successes).Count

# Hard code the number of tests, in case UE4 cleanly exits early
$NumTests=208

$HasError = 0

if ($NumErrors -gt 0)
{
    ""
    "$NumErrors test failure(s):"
    $Errors
    $HasError = 1
}
elseif ($NumSuccesses -ne $NumTests)
{
    ""
    "There were $NumSuccesses test successes. Expected $NumTests test successes."
    $HasError = 1
}

if ($UE4ExitCode -ne 0)
{
    ""
    "Error: Exit code $UE4ExitCode returned by Unreal Engine."
    $HasError = 1
}

if ($HasError -eq 0)
{
    ""
    "All Tests Passed."
    exit 0
}
else
{
    ""
    "Tests Failed."
    exit 1
}
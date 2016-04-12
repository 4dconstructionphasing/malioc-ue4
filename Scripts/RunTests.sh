#!/bin/bash
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

echo "Running exhaustive Mali Offline Compiler plugin for Unreal Engine 4 tests. This may take a while..."

if [ "$(uname)" = "Darwin" ];
then
    UE4Editor="../../../Binaries/Mac/UE4Editor.app/Contents/MacOS/UE4Editor"
else
    UE4Editor="../../../Binaries/Linux/UE4Editor"
fi

echo "Initialising shader cache"

# Run once with just the block command followed by a quit - this initialises the cache
UE4ShaderCacheLog=$($UE4Editor -log -forcelogflush -stdout -AllowStdOutLogVerbosity -nosplash -nosound -unattended -execcmds="automation runtests MaliOC.BlockUntilAllShaderCompilationComplete;quit" | tee /dev/tty)

# Make the exit code of pipes be equal to the exit code of any application that returns a non-0 exit code, else 0
set -o pipefail

echo "Running Tests"

# Run the actual tests this time and track the output
UE4Log=$($UE4Editor -log -forcelogflush -stdout -AllowStdOutLogVerbosity -nosplash -nosound -unattended -execcmds="automation runtests MaliOC.CompilationReport.MSM_Unlit;quit" | tee /dev/tty)

UE4ExitCode=$?

# We also have MaliOC.CompilationReport.MSM_DefaultLit tests which we don't run because they are very long running and they don't test anything that doesn't get tested with the MaliOC.CompilationReport.MSM_Unlit tests.

FailureString="Automation Test Failed"
SuccessString="AutomationTestingLog: Info ...Automation Test Succeeded"

NumErrors=$(echo "$UE4Log" | grep "$FailureString" -c)
NumSuccesses=$(echo "$UE4Log" | grep "$SuccessString" -c)

# Hard code the number of tests, in case UE4 cleanly exits early
if [ "$(uname)" = "Darwin" ];
then
    # Fewer tests on OSX as the Mali-T600_r3p0-00rel0 compiler is unsupported
    NumTests=202
else
    NumTests=208
fi

HasError=0

if [ "$NumErrors" -gt 0 ]
then
    echo ""
    echo "$NumErrors test failure(s)"
    echo "$UE4Log" | grep "$FailureString" -B 5
    HasError=1
elif [ "$NumSuccesses" -ne $NumTests ]
then
    echo ""
    echo "There were $NumSuccesses test successes. Expected $NumTests test successes."
    HasError=1
fi

if [ $UE4ExitCode -ne 0 ]
then
    echo ""
    echo "Error: Exit code $UE4ExitCode returned by Unreal Engine."
    HasError=1
fi

if [ $HasError -eq 0 ]
then
    echo ""
    echo "All Tests Passed."
    exit 0
else
    echo ""
    echo "Tests Failed"
    exit 1
fi

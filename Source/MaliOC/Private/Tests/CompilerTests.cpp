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

#include "../MaliOCPrivatePCH.h"
#include "../MaliOCCompilerManager.h"
#include "../MaliOCAsyncCompiler.h"
#include "../MaliOCAsyncReportGenerator.h"
#include "AutomationTest.h"
#include "ShaderCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaliOCBlockUntilAllShaderCompilationCompleteTest, "MaliOC.BlockUntilAllShaderCompilationComplete", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A fake test which blocks until all outstanding shader compilation is complete.
// We run this before the normal tests to ensure all the default shaders have been compiled, as this can interfere with our tests
bool FMaliOCBlockUntilAllShaderCompilationCompleteTest::RunTest(const FString& Parameters)
{
    GShaderCompilingManager->FinishAllCompilation();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaliOCCompilerManagerTest, "MaliOC.CompilerManager", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Test all the functionality of Compiler Manager wrapper which isn't already tested in the Async compiler tests
bool FMaliOCCompilerManagerTest::RunTest(const FString& Parameters)
{
    auto* cm = FCompilerManager::Get();

    TestNotNull(TEXT("FCompilerManager::Get() must return a valid pointer"), cm);

    if (cm == nullptr)
    {
        return false;
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaliOCCompilerLoadingTest, "MaliOC.CompilerLoading", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// This indirectly tests a lot of the functionality exposed by the compiler manager
// It doesn't make sense to test it twice
bool FMaliOCCompilerLoadingTest::RunTest(const FString& Parameters)
{
    if (FCompilerManager::Get() == nullptr)
    {
        return false;
    }

    const auto& cores = FAsyncCompiler::Get()->GetCores();

    TestTrue(TEXT("There must be at least one supported core"), cores.Num() > 0);
    for (const auto& core : cores)
    {
        TestTrue(TEXT("Each core must have at least one revision"), core->GetRevisions().Num() > 0);
        TestTrue(TEXT("Each core must have a valid name"), core->GetName().Len() > 0);

        for (const auto& rev : core->GetRevisions())
        {
            TestTrue(TEXT("Each revision must have at least one driver"), rev->GetDrivers().Num() > 0);
            TestTrue(TEXT("Each revision must have a valid name"), rev->GetName().Len() > 0);
            TestEqual(TEXT("Each revision must point back to its core"), &rev->GetCore(), &core.Get());

            for (const auto& dri : rev->GetDrivers())
            {
                TestTrue(TEXT("Each driver must have a valid name"), dri->GetName().Len() > 0);
                TestEqual(TEXT("Each driver must point back to its revision"), &dri->GetRevision(), &rev.Get());

                TestTrue(TEXT("Each driver must support at least GLES2"), dri->GetMaxAPI() >= 100);

                unsigned int maxVersion = FCompilerManager::Get()->_malicm_get_highest_api_version(dri->GetCompiler());
                TestEqual(TEXT("Each driver's compiler must be valid"), dri->GetMaxAPI(), maxVersion);

                for (const auto& pla : dri->GetPlatforms())
                {
                    TestTrue(TEXT("Each platform must have a valid name"), pla->GetName().Len() > 0);
                    TestEqual(TEXT("Each platform must point back to its revision"), &pla->GetDriver(), &dri.Get());
                }
            }
        }
    }

    return true;
}

struct FMaliOCAsyncReportGenerationParams
{
    int32 core;
    int32 rev;
    int32 dri;
    int32 plat;
    EMaterialShadingModel model;
};

// Remove spaces and periods - the test harness uses periods as delimiters
FString SanitiseTestString(const FString& string)
{
    return string.Replace(TEXT("."), TEXT("")).Replace(TEXT(" "), TEXT(""));
}

// Copied from Engine\Source\Runtime\Engine\Private\Materials\MaterialShader.cpp
// (The function is not exported so we can't directly use it)
FString GetShadingModelString(EMaterialShadingModel ShadingModel)
{
    FString ShadingModelName;
    switch (ShadingModel)
    {
    case MSM_Unlit:             ShadingModelName = TEXT("MSM_Unlit"); break;
    case MSM_DefaultLit:        ShadingModelName = TEXT("MSM_DefaultLit"); break;
    case MSM_Subsurface:        ShadingModelName = TEXT("MSM_Subsurface"); break;
    case MSM_PreintegratedSkin: ShadingModelName = TEXT("MSM_PreintegratedSkin"); break;
    case MSM_ClearCoat:         ShadingModelName = TEXT("MSM_ClearCoat"); break;
    case MSM_SubsurfaceProfile: ShadingModelName = TEXT("MSM_SubsurfaceProfile"); break;
    case MSM_TwoSidedFoliage:   ShadingModelName = TEXT("MSM_TwoSidedFoliage"); break;
    default: ShadingModelName = TEXT("Unknown"); break;
    }
    return ShadingModelName;
}

FString PrettyPrintCompilerTestName(const FMaliPlatform& plat, EMaterialShadingModel model)
{
    const auto& dri = plat.GetDriver();
    const auto& rev = dri.GetRevision();
    const auto& core = rev.GetCore();

    return FString::Printf(TEXT("%s.%s.%s.%s.%s"), *SanitiseTestString(GetShadingModelString(model)), *SanitiseTestString(core.GetName()), *SanitiseTestString(rev.GetName()), *SanitiseTestString(dri.GetName()), *SanitiseTestString(plat.GetName()));
}

void EncodeCompilationReportParamsAsStrings(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands, EMaterialShadingModel model)
{
    if (FAsyncCompiler::Get() == nullptr)
    {
        return;
    }

    const auto& cores = FAsyncCompiler::Get()->GetCores();
    for (int i = 0; i < cores.Num(); i++)
    {
        const auto& core = cores[i];
        const auto& revs = core->GetRevisions();
        for (int j = 0; j < revs.Num(); j++)
        {
            const auto& rev = revs[j];
            const auto& drivers = rev->GetDrivers();
            for (int k = 0; k < drivers.Num(); k++)
            {
                const auto& dri = drivers[k];
                const auto& platforms = dri->GetPlatforms();
                for (int l = 0; l < platforms.Num(); l++)
                {
                    const auto& plat = platforms[l];
                    OutBeautifiedNames.Add(PrettyPrintCompilerTestName(plat.Get(), model));

                    FMaliOCAsyncReportGenerationParams params;
                    params.core = i;
                    params.rev = j;
                    params.dri = k;
                    params.plat = l;
                    params.model = model;

                    OutTestCommands.Add(FString::FromBlob((uint8*)&params, sizeof(params)));
                }
            }
        }
    }
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMaliOCCompilationReportTest, "MaliOC.CompilationReport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMaliOCCompilationReportTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutPlatformStrings) const
{
    EncodeCompilationReportParamsAsStrings(OutBeautifiedNames, OutPlatformStrings, MSM_Unlit);
    EncodeCompilationReportParamsAsStrings(OutBeautifiedNames, OutPlatformStrings, MSM_DefaultLit);
}

struct FMaliOCCompilationReportTestData
{
    FMaliOCCompilationReportTest* test;
    TSharedRef<FAsyncReportGenerator> reportGenerator;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FMaliOCCompilationReportTest_WaitForCompilationToComplete, FMaliOCCompilationReportTestData, testData);

bool FMaliOCCompilationReportTest::RunTest(const FString& Parameters)
{
    if (FCompilerManager::Get() == nullptr)
    {
        return false;
    }

    FMaliOCAsyncReportGenerationParams params;
    bool success = FString::ToBlob(Parameters, (uint8*)&params, sizeof(params));

    TestTrue("Invalid parameters", success);

    if (!success)
    {
        return false;
    }

    const auto& platform = FAsyncCompiler::Get()->GetCores()[params.core]->GetRevisions()[params.rev]->GetDrivers()[params.dri]->GetPlatforms()[params.plat].Get();

    AddLogItem(FString::Printf(TEXT("Testing %s"), *PrettyPrintCompilerTestName(platform, params.model)));

    // Create a temporary material for testing
    UMaterial* Material = NewObject<UMaterial>();
    Material->SetShadingModel(params.model);
    // Enabled all the options for a proper stress test
    Material->bUsedWithSkeletalMesh = true;
    Material->bUsedWithEditorCompositing = true;
    Material->bUsedWithLandscape = true;
    Material->bUsedWithParticleSprites = true;
    Material->bUsedWithBeamTrails = true;
    Material->bUsedWithMeshParticles = true;
    Material->bUsedWithStaticLighting = true;
    Material->bUsedWithFluidSurfaces = true;
    Material->bUsedWithMorphTargets = true;
    Material->bUsedWithSplineMeshes = true;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->bUsedWithClothing = true;
    // Should speed up tests a little
    Material->CancelOutstandingCompilation();

    TSharedRef<FAsyncReportGenerator> reportGenerator = MakeShareable(new FAsyncReportGenerator(Material, platform));

    // Pass the report generator and this test object to the latent command
    FMaliOCCompilationReportTestData testData = { this, reportGenerator };

    // Checks each frame to see if compilation is complete, and when it is ensures output is as expected
    ADD_LATENT_AUTOMATION_COMMAND(FMaliOCCompilationReportTest_WaitForCompilationToComplete(testData));

    return true;
}

bool FMaliOCCompilationReportTest_WaitForCompilationToComplete::Update()
{
    if (testData.reportGenerator->GetProgress() != FAsyncReportGenerator::EProgress::COMPILATION_COMPLETE)
    {
        return false;
    }

    auto progress = testData.reportGenerator->GetMaliOCCompilationProgress();

    testData.test->TestTrue(TEXT("There must be at least one shader to compile"), progress.NumTotalShaders > 0);
    testData.test->TestEqual(TEXT("At completion, the number of compiled shaders must equal the total number of shaders"), progress.NumCompiledShaders, progress.NumTotalShaders);

    auto report = testData.reportGenerator->GetReport();

    testData.test->TestEqual(TEXT("We should have no errors after compiling the basic material"), report->ErrorList.Num(), 0);

    if (report->ErrorList.Num() == 0)
    {
        testData.test->TestTrue(TEXT("There must either be all Midgard reports or all Utgard reports"), (report->UtgardReports.Num() == 0) ^ (report->MidgardReports.Num() == 0));

        if (report->UtgardReports.Num() != 0)
        {
            testData.test->TestEqual(TEXT("We should have as many Utgard reports as there are compiled shaders"), report->UtgardReports.Num(), (int32)progress.NumTotalShaders);

            testData.test->TestNotEqual(TEXT("We should have some summary reports"), report->UtgardSummaryReports.Num(), 0);
        }
        else if (report->MidgardReports.Num() != 0)
        {
            testData.test->TestEqual(TEXT("We should have as many Midgard reports as there are compiled shaders"), report->MidgardReports.Num(), (int32)progress.NumTotalShaders);

            testData.test->TestNotEqual(TEXT("We should have some summary reports"), report->MidgardSummaryReports.Num(), 0);
        }
    }

    return true;
}

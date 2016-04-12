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

#include "MaliOCPrivatePCH.h"
#include "MaliOCAsyncReportGenerator.h"

FAsyncReportGenerator::FAsyncReportGenerator(UMaterialInterface* MaterialInterface, const FMaliPlatform& MaliPlatform) :
Platform(MaliPlatform),
Resource(TUniqueObj<FMaterialResource>(*(new FMaterialResource)))
{
    check(MaterialInterface != nullptr);

    UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);

    // Set the material resource's material to the one passed in
    if (MaterialInstance != nullptr)
    {
        Resource->SetMaterial(MaterialInstance->GetMaterial(), EMaterialQualityLevel::High, false, GetMaxSupportedFeatureLevel(Platform.GetPlatform()), MaterialInstance);
    }
    else
    {
        Resource->SetMaterial(MaterialInterface->GetMaterial(), EMaterialQualityLevel::High, false, GetMaxSupportedFeatureLevel(Platform.GetPlatform()));
    }

    // Begin shader cross compilation
    bool success = Resource->CacheShaders(Platform.GetPlatform(), false);

    if (!success)
    {
        bWasCompilationError = true;
        Progress = EProgress::COMPILATION_COMPLETE;
    }
}

void FAsyncReportGenerator::Tick(float DeltaTime)
{
    // Early return if the job is complete
    if (Progress == EProgress::COMPILATION_COMPLETE)
    {
        return;
    }

    // Cross compilation from HLSL to GLSL is in Progress
    if (Progress == EProgress::CROSS_COMPILATION_IN_PROGRESS)
    {
        if (!Resource->IsCompilationFinished())
        {
            return;
        }

        // Should be a no-op. Guarantees that the results are all in the correct place.
        Resource->FinishCompilation();

        auto ShaderMap = Resource->GetGameThreadShaderMap();

        // No output shader map means that there were some compilation errors pre cross-compilation
        // This usually happens when a feature that GLES doesn't support is used
        if (ShaderMap == nullptr)
        {
            // Sometimes, cross compilation will fail without any errors
            // This typically happens when lots of shader permutations (100+) are being cross compiled
            // Attempting compilation one more time typically fixes it
            if (Resource->GetCompileErrors().Num() == 0 && NumAttempts == 0)
            {
                Resource->CacheShaders(Platform.GetPlatform(), false);
                NumAttempts++;
                return;
            }
            else
            {
                bWasCompilationError = true;
                Progress = EProgress::COMPILATION_COMPLETE;
                return;
            }
        }

        // Start the async compile job
        check(!JobHandle.IsValid());
        JobHandle = FAsyncCompiler::Get()->AddJob(Resource->GetGameThreadShaderMap(), Platform);
        Progress = EProgress::MALIOC_COMPILATION_IN_PROGRESS;
    }

    if (Progress == EProgress::MALIOC_COMPILATION_IN_PROGRESS)
    {
        // Wait until the compile job is complete
        if (!JobHandle->IsCompilationFinished())
        {
            return;
        }
    }

    // In theory, we could do the report generation asynchronously here as well
    // In practice, it's fast enough* that we just do it lazily on the UI thread (it's just string manipulation)
    // *(At least on a machine that meets the recommended specifications)

    Progress = EProgress::COMPILATION_COMPLETE;
}

void FAsyncReportGenerator::FinishReportGeneration()
{
    if (Progress == EProgress::COMPILATION_COMPLETE)
    {
        return;
    }

    if (Progress == EProgress::CROSS_COMPILATION_IN_PROGRESS)
    {
        // Block until the shader map has finished compilation
        Resource->FinishCompilation();
        // Update the internal state machine
        Tick(0.0f);
        check(Progress == EProgress::MALIOC_COMPILATION_IN_PROGRESS);
    }

    if (Progress == EProgress::MALIOC_COMPILATION_IN_PROGRESS)
    {
        // Block until the offline shader compile job has finished
        FAsyncCompiler::Get()->FinishCompilation();
        // Update the internal state machine
        Tick(0.0f);
    }

    check(Progress == EProgress::COMPILATION_COMPLETE);

    return;
}

FAsyncReportGenerator::EProgress FAsyncReportGenerator::GetProgress() const
{
    return Progress;
}

FAsyncReportGenerator::FMaliOCCompilationProgress FAsyncReportGenerator::GetMaliOCCompilationProgress() const
{
    FMaliOCCompilationProgress ret;

    if (JobHandle.IsValid())
    {
        ret.NumCompiledShaders = JobHandle->GetNumCompiledShaders();
        ret.NumTotalShaders = JobHandle->GetTotalShaders();
    }
    return ret;
}

struct FMidgardBoundPipes
{
    FString ShortestBound;
    FString LongestBound;
};

/* Get the limiting pipe on Midgard for a render target */
FMidgardBoundPipes GetMidgardBoundPipes(const FMaliOCRawCompilerOutput::FMidgardOutput::FRenderTarget& RenderTarget)
{
    FMidgardBoundPipes bounds;

    const auto GetMidgardBoundPipe = [](uint32 a, uint32 ls, uint32 t) -> FString
    {
        uint32 max = FMath::Max3(a, ls, t);

        if (max == a)
        {
            return TEXT("Arithmetic");
        }
        if (max == ls)
        {
            return TEXT("Load/Store");
        }
        return TEXT("Texture");
    };

    bounds.ShortestBound = GetMidgardBoundPipe(RenderTarget.arithmetic_shortest_path, RenderTarget.load_store_shortest_path, RenderTarget.texture_shortest_path);

    bounds.LongestBound = GetMidgardBoundPipe(RenderTarget.arithmetic_longest_path, RenderTarget.load_store_longest_path, RenderTarget.texture_longest_path);

    return bounds;
}

/* Take the raw common output from the compiler and return a beautified array of strings for the user to read */
TArray<TSharedRef<FString>> GetDetailsFromCommonOutput(const FMaliOCRawCompilerOutput::FCommonOutput& CommonOutput)
{
    TArray<TSharedRef<FString>> details;

    if (CommonOutput.Frequency == EShaderFrequency::SF_Pixel)
    {
        details.Add(MakeShareable(new FString(TEXT("<Text.Bold>Fragment Shader</>"))));
    }
    else if (CommonOutput.Frequency == EShaderFrequency::SF_Vertex)
    {
        details.Add(MakeShareable(new FString(TEXT("<Text.Bold>Vertex Shader</>"))));
    }

    return details;
}

TSharedRef<FMaliOCReport> FAsyncReportGenerator::GetReport() const
{
    check(Progress == EProgress::COMPILATION_COMPLETE);

    if (CachedReport.IsValid())
    {
        return CachedReport.ToSharedRef();
    }

    CachedReport = MakeShareable(new FMaliOCReport);

    if (bWasCompilationError)
    {
        // We never got far enough to make a job handle
        // Write out all compilation errors

        TSharedRef<FMaliOCReport::FErrorReport> errorReport = MakeShareable(new FMaliOCReport::FErrorReport);
        errorReport->TitleName = TEXT("Cross Compilation Errors");

        const auto& compileErrors = Resource->GetCompileErrors();

        if (compileErrors.Num() == 0)
        {
            errorReport->Errors.Add(MakeShareable(new FString(TEXT("An unknown error occurred. Try again."))));
        }
        else
        {
            for (const auto& error : Resource->GetCompileErrors())
            {
                errorReport->Errors.Add(MakeShareable(new FString(error)));
            }
        }

        CachedReport->ErrorList.Add(errorReport);
    }
    else
    {
        check(JobHandle.IsValid());
        const auto& rawReport = *JobHandle->GetRawCompilerOutput();

        // Package up all errors
        for (const auto& rawError : rawReport.ErrorOutput)
        {
            TSharedRef<FMaliOCReport::FErrorReport> errorReport = MakeShareable(new FMaliOCReport::FErrorReport);
            errorReport->TitleName = rawError.CommonOutput.ShaderName;
            errorReport->Details = GetDetailsFromCommonOutput(rawError.CommonOutput);
            errorReport->SourceCode = rawError.CommonOutput.SourceCode;

            for (const auto& warning : rawError.CommonOutput.Warnings)
            {
                errorReport->Warnings.Add(MakeShareable(new FString(warning)));
            }

            for (const auto& error : rawError.Errors)
            {
                errorReport->Errors.Add(MakeShareable(new FString(error)));
            }

            CachedReport->ErrorList.Add(errorReport);
        }

        TMap<FName, FString> shaderTypeNamesAndDescriptions;

        // Get representative shader names and their descriptions so the widget generator can generate the summary
        Resource->GetRepresentativeShaderTypesAndDescriptions(shaderTypeNamesAndDescriptions);

        // Package up any Midgard output
        // Midgard output and Utgard output should be mutually exclusive
        for (const auto& output : rawReport.MidgardOutput)
        {
            TSharedRef<FMaliOCReport::FMidgardReport> report = MakeShareable(new FMaliOCReport::FMidgardReport);
            report->TitleName = output.CommonOutput.ShaderName;
            report->VertexFactoryName = output.CommonOutput.VertexFactoryName;
            report->Details = GetDetailsFromCommonOutput(output.CommonOutput);
            report->SourceCode = output.CommonOutput.SourceCode;

            for (const auto& warning : output.CommonOutput.Warnings)
            {
                report->Warnings.Add(MakeShareable(new FString(warning)));
            }

            // For each render target, package up all the stats in a 5*4 table
            for (const auto& rt : output.RenderTargets)
            {
                TSharedRef<FMaliOCReport::FMidgardReport::FRenderTarget> rtReport = MakeShareable(new FMaliOCReport::FMidgardReport::FRenderTarget);

                auto BoundPipes = GetMidgardBoundPipes(rt);

                rtReport->Index = rt.render_target;

                const auto AsText = [](const FString& fs) -> TSharedPtr < FText >
                {
                    return MakeShareable(new FText(FText::FromString(fs)));
                };

                // Header line
                rtReport->StatsTable[0] = MakeShareable(new FText());
                rtReport->StatsTable[1] = AsText(TEXT("A"));
                rtReport->StatsTable[2] = AsText(TEXT("L/S"));
                rtReport->StatsTable[3] = AsText(TEXT("T"));
                rtReport->StatsTable[4] = AsText(TEXT("Bound"));

                // Shortest Path
                rtReport->StatsTable[5] = AsText(TEXT("Shortest Path (Cycles)"));
                rtReport->StatsTable[6] = AsText(FString::Printf(TEXT("%.4g"), rt.arithmetic_shortest_path));
                rtReport->StatsTable[7] = AsText(FString::Printf(TEXT("%.4g"), rt.load_store_shortest_path));
                rtReport->StatsTable[8] = AsText(FString::Printf(TEXT("%.4g"), rt.texture_shortest_path));
                rtReport->StatsTable[9] = AsText(BoundPipes.ShortestBound);

                // Longest Path
                rtReport->StatsTable[10] = AsText(TEXT("Longest Path (Cycles)"));
                rtReport->StatsTable[11] = AsText(FString::Printf(TEXT("%.4g"), rt.arithmetic_longest_path));
                rtReport->StatsTable[12] = AsText(FString::Printf(TEXT("%.4g"), rt.load_store_longest_path));
                rtReport->StatsTable[13] = AsText(FString::Printf(TEXT("%.4g"), rt.texture_longest_path));
                rtReport->StatsTable[14] = AsText(BoundPipes.LongestBound);

                // Instructions Emitted line
                rtReport->StatsTable[15] = AsText(TEXT("Instructions Emitted"));
                rtReport->StatsTable[16] = AsText(FString::Printf(TEXT("%.4g"), rt.arithmetic_cycles));
                rtReport->StatsTable[17] = AsText(FString::Printf(TEXT("%.4g"), rt.load_store_cycles));
                rtReport->StatsTable[18] = AsText(FString::Printf(TEXT("%.4g"), rt.texture_cycles));
                rtReport->StatsTable[19] = MakeShareable(new FText());

                // Register and spilling info
                rtReport->ExtraDetails.Add(MakeShareable(new FString(FString::Printf(TEXT("%u work registers used"), rt.work_registers_used))));
                rtReport->ExtraDetails.Add(MakeShareable(new FString(FString::Printf(TEXT("%u uniform registers used"), rt.uniform_registers_used))));

                if (rt.spilling_used)
                {
                    rtReport->ExtraDetails.Add(MakeShareable(new FString(TEXT("<Text.Warning>Register spilling used</>"))));
                }
                else
                {
                    rtReport->ExtraDetails.Add(MakeShareable(new FString(TEXT("Register spilling not used"))));
                }

                report->RenderTargets.Add(rtReport);
            }

            CachedReport->MidgardReports.Add(report);

            const FString* shaderDescription = shaderTypeNamesAndDescriptions.Find(FName(*report->TitleName));

            if (shaderDescription != nullptr)
            {
                TSharedRef<FMaliOCReport::FMidgardReport> reportCopy = MakeShareable(new FMaliOCReport::FMidgardReport(*report));

                reportCopy->Details.Add(MakeShareable(new FString(FString::Printf(TEXT("<Text.Bold>%s</>"), *report->VertexFactoryName))));
                reportCopy->Details.Add(MakeShareable(new FString(FString::Printf(TEXT("<Text.Bold>%s</>"), **shaderDescription))));
                CachedReport->MidgardSummaryReports.Add(reportCopy);
            }
        }

        // Package up the Utgard output
        for (const auto& output : rawReport.UtgardOutput)
        {
            TSharedRef<FMaliOCReport::FUtgardReport> report = MakeShareable(new FMaliOCReport::FUtgardReport);
            report->TitleName = output.CommonOutput.ShaderName;
            report->VertexFactoryName = output.CommonOutput.VertexFactoryName;
            report->Details = GetDetailsFromCommonOutput(output.CommonOutput);
            report->SourceCode = output.CommonOutput.SourceCode;

            for (const auto& warning : output.CommonOutput.Warnings)
            {
                report->Warnings.Add(MakeShareable(new FString(warning)));
            }

            report->ExtraDetails.Add(MakeShareable(new FString(FString::Printf(TEXT("Number of instruction words emitted: %u"), output.n_instruction_words))));
            report->ExtraDetails.Add(MakeShareable(new FString(FString::Printf(TEXT("Number of cycles for shortest code path: %u"), output.min_number_of_cycles))));
            report->ExtraDetails.Add(MakeShareable(new FString(FString::Printf(TEXT("Number of cycles for longest code path: %u"), output.max_number_of_cycles))));

            CachedReport->UtgardReports.Add(report);

            const FString* shaderDescription = shaderTypeNamesAndDescriptions.Find(FName(*report->TitleName));

            if (shaderDescription != nullptr)
            {
                TSharedRef<FMaliOCReport::FUtgardReport> reportCopy = MakeShareable(new FMaliOCReport::FUtgardReport(*report));

                reportCopy->Details.Add(MakeShareable(new FString(FString::Printf(TEXT("<Text.Bold>%s</>"), *report->VertexFactoryName))));
                reportCopy->Details.Add(MakeShareable(new FString(FString::Printf(TEXT("<Text.Bold>%s</>"), **shaderDescription))));
                CachedReport->UtgardSummaryReports.Add(reportCopy);
            }
        }

        // Explain what A, L/S and T mean if we're showing Midgard output
        if (rawReport.MidgardOutput.Num() != 0)
        {
            CachedReport->ShaderSummaryStrings.Add(MakeShareable(new FString(TEXT("<Text.Bold>A = Arithmetic, L/S = Load/Store, T = Texture</>"))));
        }
        // Add the disclaimers
        CachedReport->ShaderSummaryStrings.Add(MakeShareable(new FString(TEXT("<Text.Bold>The cycle counts do not include possible stalls due to cache misses.</>"))));
        CachedReport->ShaderSummaryStrings.Add(MakeShareable(new FString(TEXT("<Text.Bold>Shaders with loops may return \" - 1\" for cycle counts if the number of cycles cannot be statically determined.</>"))));

        // Sort all the dumped statistics into alphabetical order
        // We don't sort the summary in order to maintain the same order as the inbuilt statistics
        const auto errorSorter = [](const TSharedRef<FMaliOCReport::FErrorReport>& a, const TSharedRef<FMaliOCReport::FErrorReport>& b) -> bool
        {
            return a->TitleName < b->TitleName;
        };
        CachedReport->ErrorList.Sort(errorSorter);

        const auto midgardSorter = [](const TSharedRef<FMaliOCReport::FMidgardReport>& a, const TSharedRef<FMaliOCReport::FMidgardReport>& b) -> bool
        {
            return a->TitleName < b->TitleName;
        };
        CachedReport->MidgardReports.Sort(midgardSorter);

        const auto utgardSorter = [](const TSharedRef<FMaliOCReport::FUtgardReport>& a, const TSharedRef<FMaliOCReport::FUtgardReport>& b) -> bool
        {
            return a->TitleName < b->TitleName;
        };
        CachedReport->UtgardReports.Sort(utgardSorter);
    }

    return CachedReport.ToSharedRef();
}

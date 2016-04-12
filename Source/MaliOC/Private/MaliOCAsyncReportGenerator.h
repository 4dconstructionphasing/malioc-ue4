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

#pragma once
#include "MaliOCPrivatePCH.h"
#include "MaliOCAsyncCompiler.h"

/** Report structure. Created from raw output in AsyncCompiler */
struct FMaliOCReport
{
    struct FErrorReport
    {
        FString TitleName;
        TArray<TSharedRef<FString>> Details;
        TArray<TSharedRef<FString>> Errors;
        TArray<TSharedRef<FString>> Warnings;
        FString SourceCode;
    };

    struct FMidgardReport
    {
        struct FRenderTarget
        {
            uint32 Index;
            TStaticArray<TSharedPtr<FText>, 20> StatsTable;
            TArray<TSharedRef<FString>> ExtraDetails;
        };

        FString TitleName;
        FString VertexFactoryName;
        TArray<TSharedRef<FString>> Details;
        TArray<TSharedRef<FRenderTarget>> RenderTargets;
        TArray<TSharedRef<FString>> Warnings;
        FString SourceCode;
    };

    struct FUtgardReport
    {
        FString TitleName;
        FString VertexFactoryName;
        TArray<TSharedRef<FString>> Details;
        TArray<TSharedRef<FString>> ExtraDetails;
        TArray<TSharedRef<FString>> Warnings;
        FString SourceCode;
    };

    TArray<TSharedRef<FErrorReport>> ErrorList;
    TArray<TSharedRef<FString>> ShaderSummaryStrings;
    TArray<TSharedRef<FMidgardReport>> MidgardSummaryReports;
    TArray<TSharedRef<FMidgardReport>> MidgardReports;
    TArray<TSharedRef<FUtgardReport>> UtgardSummaryReports;
    TArray<TSharedRef<FUtgardReport>> UtgardReports;
};

/** Compiles the given non-null Material Interface with the given Mali Platform asynchronously */
class FAsyncReportGenerator final : private FTickableEditorObject
{
public:

    /**
     * Creates a report generator which will asynchronously compile the material and generate a report
     * @param MaterialInterface the non-null material interface we want to get a compilation report for
     * @param Platform the Mali platform to compile for
     */
    FAsyncReportGenerator(UMaterialInterface* MaterialInterface, const FMaliPlatform& Platform);
    ~FAsyncReportGenerator() = default;
    FAsyncReportGenerator(const FAsyncReportGenerator&) = delete;
    FAsyncReportGenerator(FAsyncReportGenerator&&) = delete;
    FAsyncReportGenerator& operator=(const FAsyncReportGenerator&) = delete;
    FAsyncReportGenerator& operator=(FAsyncReportGenerator&&) = delete;

    /* Block until the report is ready */
    void FinishReportGeneration();

    enum class EProgress
    {
        CROSS_COMPILATION_IN_PROGRESS,
        MALIOC_COMPILATION_IN_PROGRESS,
        COMPILATION_COMPLETE
    };

    /* @return the current progress of compilation */
    EProgress GetProgress() const;

    struct FMaliOCCompilationProgress
    {
        uint32 NumCompiledShaders = 0;
        uint32 NumTotalShaders = 0;
    };

    /**
     * This should be called only when GetProgress() returns MALIOC_COMPILATION_IN_PROGRESS.
     * @return a structure containing the total shaders to be compiled, and the number of shaders we have already compiled
     */
    FMaliOCCompilationProgress GetMaliOCCompilationProgress() const;

    /* This is only valid to be called when GetProgress() returns COMPILATION_COMPLETE. Will assert otherwise.
     * @return the report generated after compilation has completed.
     */
    TSharedRef<FMaliOCReport> GetReport() const;

private:
    /** Platform we're compiling for */
    const FMaliPlatform& Platform;
    /** Material resource we're extracting shaders from */
    TUniqueObj<FMaterialResource> Resource;
    /** Whether there was a compilation error during shader cross compilation (i.e. not our fault) */
    bool bWasCompilationError = false;
    /** Current progress of compilation */
    EProgress Progress = EProgress::CROSS_COMPILATION_IN_PROGRESS;
    /** Handle to the async compilation job */
    TSharedPtr<const FCompileJobHandle> JobHandle = nullptr;
    /** Cached instance of the report we create once compilation is complete */
    mutable TSharedPtr<FMaliOCReport> CachedReport = nullptr;
    /** Number of attempts we've made for cross compilation. Used due to a bug where cross compilation fails without errors*/
    uint32 NumAttempts = 0;

    // FTickableEditorObject functions

    virtual bool IsTickable() const override
    {
        return true;
    }

    /** Report generator tick */
    virtual void Tick(float DeltaTime) override;

    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncReportGenerator, STATGROUP_Tickables);
    }
};

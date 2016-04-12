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
#include "MaliOCReportWidgetGenerator.h"
#include "MaliOCAsyncReportGenerator.h"
#include "SExpandableArea.h"
#include "MaliOCStyle.h"

/* Standard widget padding */
static const FMargin WidgetPadding(3.0f, 2.0f, 3.0f, 2.0f);

bool FReportWidgetGenerator::IsCompilationComplete() const
{
    return Generator->GetProgress() == FAsyncReportGenerator::EProgress::COMPILATION_COMPLETE;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION;
FReportWidgetGenerator::FReportWidgetGenerator(TSharedRef<FAsyncReportGenerator> ReportGenerator) :
Generator(ReportGenerator)
{
    // Construct the throbber widget
    ThrobberWidget = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .VAlign(VAlign_Center)
        .HAlign(HAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .Padding(WidgetPadding)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            .AutoHeight()
            [
                SNew(SThrobber)
                .NumPieces(7)
            ]
            + SVerticalBox::Slot()
                .Padding(WidgetPadding)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .AutoHeight()
                [
                    SAssignNew(ThrobberTextLine1, SRichTextBlock)
                    .TextStyle(FMaliOCStyle::Get(), "Text.Normal")
                    .Justification(ETextJustify::Type::Center)
                ]
            + SVerticalBox::Slot()
                .Padding(WidgetPadding)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .AutoHeight()
                [
                    SAssignNew(ThrobberTextLine2, SRichTextBlock)
                    .TextStyle(FMaliOCStyle::Get(), "Text.Normal")
                    .Justification(ETextJustify::Type::Center)
                ]
        ];
}

/* Convert a string array into a list of strings widget */
TSharedRef<SWidget> GenerateFStringListView(const TArray<TSharedRef<FString>>& StringArray)
{
    TSharedRef<SVerticalBox> verticalBox = SNew(SVerticalBox);
    for (const auto& string : StringArray)
    {
        verticalBox->AddSlot()
            .AutoHeight()
            [
                SNew(SRichTextBlock)
                .Text(FText::FromString(*string))
                .TextStyle(FMaliOCStyle::Get(), "Text.Normal")
                .DecoratorStyleSet(FMaliOCStyle::Get().Get())
                .AutoWrapText(true)
            ];
    }

    return verticalBox;
}

/* Add the source code box to any vertical box*/
void AddSourceCodeToVerticalBox(TSharedPtr<SVerticalBox>& VerticalBox, const FString& SourceCode)
{
    if (SourceCode.Len() > 0)
    {
        // Replace tabs with two spaces for display in the widget, as the rich text block doesn't support tabs
        FString spacedSource = SourceCode.Replace(TEXT("\t"), TEXT("  "));
        VerticalBox->AddSlot()
            .AutoHeight()
            [
                SNew(SSeparator)
            ];
        VerticalBox->AddSlot()
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(TEXT("Source Code")))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .InitiallyCollapsed(true)
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SNew(SRichTextBlock)
                    .Text(FText::FromString(spacedSource))
                    .TextStyle(FMaliOCStyle::Get(), "Text.Normal")
                    .AutoWrapText(true)
                ]
            ];
    }
}

/* Construct the error widget from an array of errors*/
TSharedRef<SWidget> ConstructErrorWidget(const TArray<TSharedRef<FMaliOCReport::FErrorReport>>& Errors)
{
    TSharedRef<SVerticalBox> errorBox = SNew(SVerticalBox);

    for (const auto& error : Errors)
    {
        TSharedPtr<SVerticalBox> errorWarningBox = nullptr;

        errorBox->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(error->TitleName))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .InitiallyCollapsed(false)
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SAssignNew(errorWarningBox, SVerticalBox)
                ]
            ];

        // Print the details of the shader (such as frequency)
        errorWarningBox->AddSlot()
            .AutoHeight()
            [
                GenerateFStringListView(error->Details)
            ];

        if (error->Errors.Num() > 0)
        {
            errorWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SSeparator)
                ];
            errorWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(TEXT("Errors")))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(false)
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        GenerateFStringListView(error->Errors)
                    ]
                ];
        }

        if (error->Warnings.Num() > 0)
        {
            errorWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SSeparator)
                ];
            errorWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(TEXT("Warnings")))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(false)
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        GenerateFStringListView(error->Warnings)
                    ]
                ];
        }

        AddSourceCodeToVerticalBox(errorWarningBox, error->SourceCode);
    }

    return errorBox;
}

/* Generate a Midgard stats table */
TSharedRef<SVerticalBox> GenerateMidgardStatsTable(const TSharedRef<FMaliOCReport::FMidgardReport::FRenderTarget>& RenderTarget)
{
    TSharedRef<SVerticalBox> rtBox = SNew(SVerticalBox);

    int index = 0;

    // Four rows, 5 columns
    for (int i = 0; i < 4; i++)
    {
        TSharedPtr<SHorizontalBox> curRow = nullptr;

        rtBox->AddSlot()
            .AutoHeight()
            [
                SAssignNew(curRow, SHorizontalBox)
            ];

        const float columnWidths[] = { 2.5f, 1.0f, 1.0f, 1.0f, 2.0f };
        const float widthScaleFactor = 50.0f;

        for (int j = 0; j < 5; j++)
        {
            curRow->AddSlot()
                .FillWidth(columnWidths[j])
                .MaxWidth(columnWidths[j] * widthScaleFactor)
                [
                    SNew(STextBlock)
                    .Text(*RenderTarget->StatsTable[index])
                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ];
            index++;
        }
    }

    rtBox->AddSlot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ];

    rtBox->AddSlot()
        .AutoHeight()
        [
            GenerateFStringListView(RenderTarget->ExtraDetails)
        ];

    return rtBox;
};

/* Make the Midgard dump widget (where we dump the statistics for Midgard compilation) */
TSharedRef<SWidget> ConstructMidgardDumpWidget(const TArray<TSharedRef<FMaliOCReport::FMidgardReport>>& Reports, bool DumpSourceCode)
{
    TSharedRef<SVerticalBox> midgardBox = SNew(SVerticalBox);

    for (const auto& report : Reports)
    {
        TSharedPtr<SVerticalBox> reportWarningBox = nullptr;

        midgardBox->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(report->TitleName))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .ToolTipText(FText::FromString(report->TitleName))
                .InitiallyCollapsed(false)
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SAssignNew(reportWarningBox, SVerticalBox)
                ]
            ];

        // Print the details of the shader (such as frequency)
        reportWarningBox->AddSlot()
            .AutoHeight()
            [
                GenerateFStringListView(report->Details)
            ];

        // If there's only one render target, don't make an expandable area
        if (report->RenderTargets.Num() == 1)
        {
            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SSeparator)
                ];

            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    GenerateMidgardStatsTable(report->RenderTargets[0])
                ];
        }
        else
        {
            for (const auto& rt : report->RenderTargets)
            {
                reportWarningBox->AddSlot()
                    .AutoHeight()
                    [
                        SNew(SSeparator)
                    ];

                reportWarningBox->AddSlot()
                    .AutoHeight()
                    [
                        SNew(SExpandableArea)
                        .AreaTitle(FText::FromString(FString::Printf(TEXT("Render Target %u"), rt->Index)))
                        .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                        .InitiallyCollapsed(false)
                        .Padding(WidgetPadding)
                        .BodyContent()
                        [
                            GenerateMidgardStatsTable(rt)
                        ]
                    ];
            }
        }

        // Dump the warnings
        if (report->Warnings.Num() > 0)
        {
            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SSeparator)
                ];
            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(TEXT("Warnings")))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(false)
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        GenerateFStringListView(report->Warnings)
                    ]
                ];
        }

        if (DumpSourceCode)
        {
            AddSourceCodeToVerticalBox(reportWarningBox, report->SourceCode);
        }
    }

    return midgardBox;
}

/* Make the Utgard dump widget (where we dump the statistics for Utgard compilation) */
TSharedRef<SWidget> ConstructUtgardDumpWidget(const TArray<TSharedRef<FMaliOCReport::FUtgardReport>>& Reports, bool DumpSourceCode)
{
    TSharedRef<SVerticalBox> utgardBox = SNew(SVerticalBox);

    for (const auto& report : Reports)
    {
        TSharedPtr<SVerticalBox> reportWarningBox = nullptr;

        utgardBox->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(report->TitleName))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .ToolTipText(FText::FromString(report->TitleName))
                .InitiallyCollapsed(false)
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SAssignNew(reportWarningBox, SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        GenerateFStringListView(report->Details)
                    ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SSeparator)
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            GenerateFStringListView(report->ExtraDetails)
                        ]
                ]
            ];

        // Dump the warnings
        if (report->Warnings.Num() > 0)
        {
            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SSeparator)
                ];
            reportWarningBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(TEXT("Warnings")))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(false)
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        GenerateFStringListView(report->Warnings)
                    ]
                ];
        }

        if (DumpSourceCode)
        {
            AddSourceCodeToVerticalBox(reportWarningBox, report->SourceCode);
        }
    }

    return utgardBox;
}

/* Create the top level report widget */
TSharedPtr<SWidget> ConstructReportWidget(const FAsyncReportGenerator& Generator)
{
    TSharedPtr<SVerticalBox> Widget = nullptr;

    TSharedPtr<SWidget> ReportWidget =
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SAssignNew(Widget, SVerticalBox)
        ];

    auto Report = Generator.GetReport();

    // First show any errors, if there are any
    if (Report->ErrorList.Num() > 0)
    {
        Widget->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(TEXT("Error Summary")))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    ConstructErrorWidget(Report->ErrorList)
                ]
            ];
    }

    // Next show Midgard reports if there are any
    if (Report->MidgardReports.Num() > 0)
    {
        Widget->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(TEXT("Statistics Summary")))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .Padding(WidgetPadding)
                    .AutoHeight()
                    [
                        GenerateFStringListView(Report->ShaderSummaryStrings)
                    ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            ConstructMidgardDumpWidget(Report->MidgardSummaryReports, false)
                        ]
                ]
            ];

        // Dump the rest of the shaders by vertex factory name
        TMap<FString, TArray<TSharedRef<FMaliOCReport::FMidgardReport>>> VertexFactoryNames;
        for (auto& report : Report->MidgardReports)
        {
            VertexFactoryNames.FindOrAdd(report->VertexFactoryName).Add(report);
        }

        for (const auto& name : VertexFactoryNames)
        {
            Widget->AddSlot()
                .Padding(WidgetPadding)
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(FString::Printf(TEXT("All %s"), *name.Key)))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(true)
                    .BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        ConstructMidgardDumpWidget(name.Value, true)
                    ]
                ];
        }
    }

    // Next show Utgard reports, if there are any. These should be mutually exclusive with Midgard reports
    if (Report->UtgardReports.Num() > 0)
    {
        Widget->AddSlot()
            .Padding(WidgetPadding)
            .AutoHeight()
            [
                SNew(SExpandableArea)
                .AreaTitle(FText::FromString(TEXT("Statistics Summary")))
                .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                .InitiallyCollapsed(false)
                .BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
                .Padding(WidgetPadding)
                .BodyContent()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .Padding(WidgetPadding)
                    .AutoHeight()
                    [
                        GenerateFStringListView(Report->ShaderSummaryStrings)
                    ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            ConstructUtgardDumpWidget(Report->UtgardSummaryReports, false)
                        ]
                ]
            ];

        // Dump the rest of the shaders by vertex factory name
        TMap<FString, TArray<TSharedRef<FMaliOCReport::FUtgardReport>>> VertexFactoryNames;
        for (auto& report : Report->UtgardReports)
        {
            VertexFactoryNames.FindOrAdd(report->VertexFactoryName).Add(report);
        }

        for (const auto& name : VertexFactoryNames)
        {
            Widget->AddSlot()
                .Padding(WidgetPadding)
                .AutoHeight()
                [
                    SNew(SExpandableArea)
                    .AreaTitle(FText::FromString(FString::Printf(TEXT("All %s"), *name.Key)))
                    .AreaTitleFont(FEditorStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
                    .InitiallyCollapsed(true)
                    .BorderBackgroundColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
                    .Padding(WidgetPadding)
                    .BodyContent()
                    [
                        ConstructUtgardDumpWidget(name.Value, true)
                    ]
                ];
        }
    }

    return ReportWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION;

TSharedRef<SWidget> FReportWidgetGenerator::GetWidget()
{
    auto progress = Generator->GetProgress();

    if (progress == FAsyncReportGenerator::EProgress::COMPILATION_COMPLETE)
    {
        if (!CachedReportWidget.IsValid())
        {
            // Make the widget once then cache it
            CachedReportWidget = ConstructReportWidget(Generator.Get());
        }

        return CachedReportWidget.ToSharedRef();
    }
    else
    {
        // Set the throbber text depending on how far through compilation we are
        if (progress == FAsyncReportGenerator::EProgress::CROSS_COMPILATION_IN_PROGRESS)
        {
            ThrobberTextLine1->SetText(FText::FromString(TEXT("Compiling HLSL to GLSL")));
            ThrobberTextLine2->SetText(FText());
        }
        else
        {
            check(progress == FAsyncReportGenerator::EProgress::MALIOC_COMPILATION_IN_PROGRESS);

            const auto oscProgress = Generator->GetMaliOCCompilationProgress();
            ThrobberTextLine1->SetText(FText::FromString(TEXT("Compiling Shaders")));
            ThrobberTextLine2->SetText(FText::FromString(FString::Printf(TEXT("%u / %u"), oscProgress.NumCompiledShaders, oscProgress.NumTotalShaders)));
        }

        return ThrobberWidget.ToSharedRef();
    }
}

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
#include "MaliSCPrivatePCH.h"
#include "MaliSCAsyncReportGenerator.h"
#include "SThrobber.h"

/** Generates a report widget using the output from a report generator */
class FReportWidgetGenerator : public TSharedFromThis < FReportWidgetGenerator >
{
public:
    /** @return a loading widget with compilation progress if the report is not yet finished, else the full report widget */
    TSharedRef<SWidget> GetWidget();

    /** @return true if compilation has completed */
    bool IsCompilationComplete() const;

    /**
     * Construct a report widget generator. This object wraps up the report generator and creates a widget based on the current report creation progress.
     * If compilation is in progress, the returned widget will be a throbber, else it will be the full report widget
     * @param ReportGenerator the report generator that we want to generate the widget for
     */
    FReportWidgetGenerator(TSharedRef<FAsyncReportGenerator> ReportGenerator);
    ~FReportWidgetGenerator() = default;
    FReportWidgetGenerator(const FReportWidgetGenerator&) = delete;
    FReportWidgetGenerator(FReportWidgetGenerator&&) = delete;
    FReportWidgetGenerator& operator=(const FReportWidgetGenerator&) = delete;
    FReportWidgetGenerator& operator=(FReportWidgetGenerator&&) = delete;

private:
    /** Report generator we use to make the widget */
    TSharedRef<FAsyncReportGenerator> Generator;

    /** Throbber we show while compilation is in progress */
    TSharedPtr<SWidget> ThrobberWidget = nullptr;
    /** First line of text that accompanies the throbber */
    TSharedPtr<SRichTextBlock> ThrobberTextLine1 = nullptr;
    /** Second line of text that accompanies the throbber */
    TSharedPtr<SRichTextBlock> ThrobberTextLine2 = nullptr;

    /** Cached report widget we return after generation is complete */
    TSharedPtr<SWidget> CachedReportWidget = nullptr;
};

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

class FMaliSCStyle final
{
public:

    /** Create the style set singleton and register the style set with Slate */
    static void Initialize();
    /** Deregister the style set with Slate and destroy the singleton */
    static void Deinitialize();
    /** @return the style set. Must have initialized the singleton first */
    static TSharedPtr<ISlateStyle> Get();

    /** @return the normal font style for widgets */
    static FSlateFontInfo GetNormalFontStyle();

    /** @return the bold font style for widgets */
    static FSlateFontInfo GetBoldFontStyle();

private:

    static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);
    static TSharedPtr<FSlateStyleSet> StyleSet;

    FMaliSCStyle() = default;
    ~FMaliSCStyle() = default;
    FMaliSCStyle(const FMaliSCStyle&) = delete;
    FMaliSCStyle(FMaliSCStyle&&) = delete;
    FMaliSCStyle& operator=(const FMaliSCStyle&) = delete;
    FMaliSCStyle& operator=(FMaliSCStyle&&) = delete;
};

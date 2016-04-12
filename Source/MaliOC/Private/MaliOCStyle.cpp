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
#include "MaliOCStyle.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FMaliOCStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

FString FMaliOCStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
    static FString ContentDir = FPaths::EnginePluginsDir() / TEXT("Editor/MaliOC/Content");
    return (ContentDir / RelativePath) + Extension;
}

TSharedPtr<FSlateStyleSet> FMaliOCStyle::StyleSet = nullptr;

TSharedPtr< class ISlateStyle > FMaliOCStyle::Get()
{
    return StyleSet;
}

FSlateFontInfo FMaliOCStyle::GetNormalFontStyle()
{
    return FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
}

FSlateFontInfo FMaliOCStyle::GetBoldFontStyle()
{
    return FEditorStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont"));
}

void FMaliOCStyle::Initialize()
{
    // Make sure we aren't double initializing
    check(!StyleSet.IsValid());

    StyleSet = MakeShareable(new FSlateStyleSet("MaliOCStyle"));

    StyleSet->Set("MaliOC.OpenMaliOCPanel", new IMAGE_BRUSH(TEXT("icon40"), FVector2D(40.0f, 40.0f)));
    StyleSet->Set("MaliOC.OpenMaliOCPanel.Small", new IMAGE_BRUSH(TEXT("icon20"), FVector2D(20.0f, 20.0f)));
    StyleSet->Set("MaliOC.MaliOCIcon16", new IMAGE_BRUSH(TEXT("icon16"), FVector2D(16.0f, 16.0f)));

    const FTextBlockStyle NormalText = FTextBlockStyle()
        .SetFont(GetNormalFontStyle())
        .SetColorAndOpacity(FSlateColor::UseForeground())
        .SetShadowOffset(FVector2D::ZeroVector)
        .SetShadowColorAndOpacity(FLinearColor::Black)
        .SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f));
    StyleSet->Set("Text.Normal", NormalText);

    StyleSet->Set("Text.Bold", FTextBlockStyle(NormalText)
        .SetFont(GetBoldFontStyle())
        .SetShadowColorAndOpacity(FLinearColor::Black)
        .SetShadowOffset(FVector2D(1.0f, 1.0f))
        );

    StyleSet->Set("Text.Warning", FTextBlockStyle(NormalText)
        .SetFont(GetBoldFontStyle())
        .SetColorAndOpacity(FLinearColor(FColor(0xffec3b3b)))
        );

    FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FMaliOCStyle::Deinitialize()
{
    if (StyleSet.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
        StyleSet.Reset();
    }
}

#undef IMAGE_BRUSH

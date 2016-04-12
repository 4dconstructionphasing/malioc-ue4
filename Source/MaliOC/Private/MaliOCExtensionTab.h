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

/** Pure virtual interface for tab widget generators */
class ITabGenerator
{
public:
    virtual TSharedRef<SWidget> GetExtensionTab() = 0;

    virtual ~ITabGenerator() = default;
    ITabGenerator(const ITabGenerator&) = delete;
    ITabGenerator(ITabGenerator&&) = delete;
    ITabGenerator& operator=(const ITabGenerator&) = delete;
    ITabGenerator& operator=(ITabGenerator&&) = delete;

protected:
    ITabGenerator() = default;
};

class FMaterialEditorTabGenerator final
{
public:
    /**
    * @param Editor The Material Editor to create the generator for
    * @return a tab generator for a Material Editor or a Material Instance Editor
    */
    static TSharedRef<ITabGenerator> create(TSharedRef<IMaterialEditor> Editor);
};

class FMaterialFunctionEditorTabGenerator final
{
public:
    /**
     * @return a tab generator for a Material Function Editor
     */
    static TSharedRef<ITabGenerator> create();
};

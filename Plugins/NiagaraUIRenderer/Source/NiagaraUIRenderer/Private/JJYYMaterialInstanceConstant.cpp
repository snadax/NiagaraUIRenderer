// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "JJYYMaterialInstanceConstant.h"

UJJYYMaterialInstanceConstant::UJJYYMaterialInstanceConstant(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UJJYYMaterialInstanceConstant::CheckMaterialUsage_Concurrent(const EMaterialUsage Usage) const
{
    return true;
}


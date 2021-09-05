// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Classes/Materials/MaterialInstanceConstant.h"
#include "JJYYMaterialInstanceConstant.generated.h"


/**
 * JJYY Material Instances may be used to UI and Niagara.
 */

UCLASS(MinimalAPI)
class UJJYYMaterialInstanceConstant : public UMaterialInstanceConstant
{
	GENERATED_UCLASS_BODY()
	
public:
    bool CheckMaterialUsage_Concurrent(const EMaterialUsage Usage) const override;

};

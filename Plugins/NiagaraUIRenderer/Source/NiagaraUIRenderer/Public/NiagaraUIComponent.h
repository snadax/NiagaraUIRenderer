// Copyright 2021 - Michal Smoleň

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "NiagaraWidgetProperties.h"
#include "Slate/WidgetTransform.h"

#include "NiagaraUIComponent.generated.h"

class SNiagaraUISystemWidget;


/**
 * 
 */
UCLASS()
class NIAGARAUIRENDERER_API UNiagaraUIComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:
    void SetTransformationForUIRendering(const FTransform& Transform);

	void RenderUI(SNiagaraUISystemWidget* NiagaraWidget, const FSlateLayoutTransform& SlateLayoutTransform, const FTransform& ComponentTransform, const FNiagaraWidgetProperties* WidgetProperties);

	void AddSpriteRendererData(SNiagaraUISystemWidget* NiagaraWidget, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst,
								class UNiagaraSpriteRendererProperties* SpriteRenderer, const FSlateLayoutTransform& SlateLayoutTransform, const FTransform& ComponentTransform, const FNiagaraWidgetProperties* WidgetProperties);

	void AddRibbonRendererData(SNiagaraUISystemWidget* NiagaraWidget, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst,
                                class UNiagaraRibbonRendererProperties* RibbonRenderer, const FSlateLayoutTransform& SlateLayoutTransform, const FTransform& ComponentTransform, const FNiagaraWidgetProperties* WidgetProperties);

    void AddMeshRendererData(SNiagaraUISystemWidget* NiagaraWidget, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst,
        class UNiagaraMeshRendererProperties* MeshRenderer, const FSlateLayoutTransform& SlateLayoutTransform, const FTransform& ComponentTransform, const FNiagaraWidgetProperties* WidgetProperties);

	
private:
	bool ShouldActivateParticle = false;
	float WidgetAngleRad = 0.f;
};

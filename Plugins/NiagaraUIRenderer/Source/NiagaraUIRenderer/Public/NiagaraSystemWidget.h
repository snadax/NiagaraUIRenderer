// Copyright 2021 - Michal Smoleň

#pragma once

#include "CoreMinimal.h"
#include "NiagaraWidgetProperties.h"
#include "Runtime/UMG/Public/Components/Widget.h"
#include "Runtime/UMG/Public/Slate/SlateVectorArtData.h"
#include "SNiagaraUISystemWidget.h"
#include "NiagaraSystemWidget.generated.h"

class SNiagaraUISystemWidget;
class UMaterialInterface;

USTRUCT()
struct FSlateMeshData
{
    GENERATED_BODY()

	UPROPERTY()
		FName MeshPackageName;
    UPROPERTY()
        TArray<FVector2D> Vertex;
    UPROPERTY()
        TArray<FColor> VertexColor;
    UPROPERTY()
        TArray<FVector2D> UV;
    UPROPERTY()
        TArray<uint32> Index;

};


/**
 The Niagara System Widget allows to render niagara particle system directly into the UI. Only sprite and ribbon CPU particles are supported.
 */
UCLASS()
class NIAGARAUIRENDERER_API UNiagaraSystemWidget : public UWidget
{
	GENERATED_BODY()
		
public:
	UNiagaraSystemWidget(const FObjectInitializer& ObjectInitializer);
	
	virtual TSharedRef<SWidget> RebuildWidget() override;
	
	virtual void SynchronizeProperties() override;
	
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

private:
	void InitializeNiagaraUI();

public:
	// Activate Niagara System with option to reset the simulation
	UFUNCTION(BlueprintCallable, Category = "Niagara UI Renderer")
	void ActivateSystem(bool Reset);

	// Deactivate Niagara System
	UFUNCTION(BlueprintCallable, Category = "Niagara UI Renderer")
    void DeactivateSystem();

	// Return Niagara Component reference for the particle system.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Niagara UI Renderer")
    class UNiagaraUIComponent* GetNiagaraComponent();

	// Updates the Niagara System reference. This will cause the particle system to reset
	UFUNCTION(BlueprintCallable, Category = "Niagara UI Renderer")
	void UpdateNiagaraSystemReference(class UNiagaraSystem* NewNiagaraSystem);

	// Updates Tick When Paused - Should be this particle system updated even when the game is paused? Note that this will reset the particle simulation
	UFUNCTION(BlueprintCallable, Category = "Niagara UI Renderer")
	void UpdateTickWhenPaused(bool NewTickWhenPaused);

public:
	// Reference to the niagara system asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara UI Renderer", DisplayName = "Niagara System", BlueprintSetter = UpdateNiagaraSystemReference)
	class UNiagaraSystem* NiagaraSystemReference;

	// Should be this particle system automatically activated?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara UI Renderer")
	bool AutoActivate = true;

	// Should be this particle system updated even when the game is paused?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara UI Renderer", BlueprintSetter = UpdateTickWhenPaused)
	bool TickWhenPaused = false;

	// Scale particles based on their position in Y-axis (towards the camera)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara UI Renderer", AdvancedDisplay)
	bool FakeDepthScale = false;

	// Fake distance from camera if the particle is at 0 0 0 - Particles will be getting bigger quicker the lower this number is
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara UI Renderer", AdvancedDisplay, meta = (EditCondition = "FakeDepthScale"))
	float FakeDepthScaleDistance = 1000.f;

	// Show debug particle system we're rendering in the game world. It'll be near 0 0 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara UI Renderer", AdvancedDisplay)
	bool ShowDebugSystemInWorld = false;

	// Disable warnings for this Widget
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara UI Renderer", AdvancedDisplay)
	bool DisableWarnings = false;

    UPROPERTY()
	mutable TArray<FSlateMeshData> MeshData;

private:
	TSharedPtr<SNiagaraUISystemWidget> NiagaraSlateWidget;

	UPROPERTY()
	class UNiagaraUIComponent* NiagaraComponent;
};

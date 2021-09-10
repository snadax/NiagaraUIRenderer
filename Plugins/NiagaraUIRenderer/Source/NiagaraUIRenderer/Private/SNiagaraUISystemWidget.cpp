// Copyright 2021 - Michal Smoleň

#include "SNiagaraUISystemWidget.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraWidgetProperties.h"
#include "NiagaraUIComponent.h"

TMap<UMaterialInterface*, TSharedPtr<FSlateMaterialBrush>> SNiagaraUISystemWidget::MaterialBrushMap;

void SNiagaraUISystemWidget::Construct(const FArguments& Args)
{
}

SNiagaraUISystemWidget::~SNiagaraUISystemWidget()
{
    ClearRenderData();
}

int32 SNiagaraUISystemWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    if (!NiagaraComponent.IsValid())
        return LayerId;
    UNiagaraUIComponent* NiagaraUIComponent = NiagaraComponent.Get();
    const FSlateLayoutTransform& SlateLayoutTransform = AllottedGeometry.GetAccumulatedLayoutTransform();
    const FSlateRenderTransform& SlateRenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
    
    FTransform2D FixedCenterTransform(AllottedGeometry.Size / 2); 
    FixedCenterTransform = FixedCenterTransform.Concatenate(SlateRenderTransform);
    NiagaraUIComponent->SetTransformationForUIRendering(FixedCenterTransform);

    NiagaraUIComponent->RenderUI(const_cast<SNiagaraUISystemWidget*>(this), SlateLayoutTransform, FixedCenterTransform, &WidgetProperties);

    return SMeshWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SNiagaraUISystemWidget::AddRenderData(FSlateVertex** OutVertexData, SlateIndex** OutIndexData, UMaterialInterface* Material, int32 NumVertexData, int32 NumIndexData)
{
    if (NumVertexData < 1 || NumIndexData < 1)
        return;
    
    FRenderData& NewRenderData = RenderData[RenderData.Add(FRenderData())];

    NewRenderData.VertexData.AddUninitialized(NumVertexData);
    *OutVertexData = &NewRenderData.VertexData[0];
    
    NewRenderData.IndexData.AddUninitialized(NumIndexData);
    *OutIndexData = &NewRenderData.IndexData[0];

    if (Material)
    {
        NewRenderData.Brush = CreateSlateMaterialBrush(Material);
        NewRenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*NewRenderData.Brush);
    }
    AddRenderRun(RenderData.Num()-1,0,1);
    FSlateInstanceBufferData InstanceBuffer;
    InstanceBuffer.Add(FVector4());
    UpdatePerInstanceBuffer(RenderData.Num() - 1, InstanceBuffer);

}


int32 SNiagaraUISystemWidget::AddRenderDataWithInstance(FSlateVertex** OutVertexData, SlateIndex** OutIndexData, UMaterialInterface* Material, int32 NumVertexData, int32 NumIndexData)
{
    if (NumVertexData < 1 || NumIndexData < 1)
        return -1;

    int32 RenderDataIndex = RenderData.Add(FRenderData());
    FRenderData& NewRenderData = RenderData[RenderDataIndex];

    NewRenderData.VertexData.AddUninitialized(NumVertexData);
    *OutVertexData = &NewRenderData.VertexData[0];

    NewRenderData.IndexData.AddUninitialized(NumIndexData);
    *OutIndexData = &NewRenderData.IndexData[0];

    if (Material)
    {
        NewRenderData.Brush = CreateSlateMaterialBrush(Material);
        NewRenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*NewRenderData.Brush);
    }
    return RenderDataIndex;
}

void SNiagaraUISystemWidget::ClearRenderData()
{
    RenderData.Empty();
}

TSharedPtr<FSlateMaterialBrush> SNiagaraUISystemWidget::CreateSlateMaterialBrush(UMaterialInterface* Material)
{
    UMaterialInterface* MaterialToUse = Material;
    if (MaterialBrushMap.Contains(MaterialToUse))
    {
        const auto MapElement = MaterialBrushMap.Find(MaterialToUse);

        if (MapElement->IsValid() && MapElement->Get()->GetResourceObject()->IsValidLowLevel() && MapElement->Get()->GetResourceObject()->IsA<UMaterialInterface>())
        {
            return *MapElement;
        }
    }
    
    const auto MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(MaterialToUse, GetTransientPackage());
    TSharedPtr<FSlateMaterialBrush> NewElement = MakeShareable(new FSlateMaterialBrush(*MaterialInstanceDynamic, FVector2D(1.f, 1.f)));

    MaterialBrushMap.Add(MaterialToUse, NewElement);
    return NewElement;
}

void SNiagaraUISystemWidget::CheckForInvalidBrushes()
{
    TArray<UMaterialInterface*> RemoveMaterials;
    for (const auto& Brush : MaterialBrushMap)
    {
        if (Brush.Value.GetSharedReferenceCount() <= 1)
        {
            Brush.Value->GetResourceObject()->RemoveFromRoot();
            RemoveMaterials.Add(Brush.Key);
        }
    }

    for (const auto MaterialToRemove : RemoveMaterials)
    {
        MaterialBrushMap.Remove(MaterialToRemove);
    }
}

void SNiagaraUISystemWidget::SetNiagaraComponentReference(TWeakObjectPtr<UNiagaraUIComponent> NiagaraComponentIn, FNiagaraWidgetProperties Properties)
{
    if (!ensure(NiagaraComponentIn != nullptr))
        return;

    WidgetProperties = Properties;

    NiagaraComponent = NiagaraComponentIn;
}
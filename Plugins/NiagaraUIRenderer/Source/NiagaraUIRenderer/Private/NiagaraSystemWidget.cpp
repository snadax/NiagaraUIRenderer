// Copyright 2021 - Michal Smoleň

#include "NiagaraSystemWidget.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraUIComponent.h"
#include "NiagaraMeshRendererProperties.h"

UNiagaraSystemWidget::UNiagaraSystemWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsVolatile = true;
}

TSharedRef<SWidget> UNiagaraSystemWidget::RebuildWidget()
{
	NiagaraSlateWidget = SNew(SNiagaraUISystemWidget);

	InitializeNiagaraUI();

	return NiagaraSlateWidget.ToSharedRef();
}

void UNiagaraSystemWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!NiagaraSlateWidget.IsValid())
	{
		return;
	}

	if (!NiagaraComponent)
		InitializeNiagaraUI();
	
}

void UNiagaraSystemWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	NiagaraSlateWidget.Reset();

	if (NiagaraComponent)
		NiagaraComponent->UnregisterComponent();
}

#if WITH_EDITOR
const FText UNiagaraSystemWidget::GetPaletteCategory()
{
	return NSLOCTEXT("NiagaraUIRenderer", "Palette Category", "Niagara");
}

void UNiagaraSystemWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, NiagaraSystemReference)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, AutoActivate)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, FakeDepthScale)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, FakeDepthScaleDistance))
		{
			InitializeNiagaraUI();
		}
	}
}

static void StaticMeshToSlateRenderData(UStaticMesh& DataSource, TArray<FSlateMeshVertex>& OutSlateVerts, TArray<uint32>& OutIndexes, FVector2D& OutExtentMin, FVector2D& OutExtentMax)
{
    OutExtentMin = FVector2D(FLT_MAX, FLT_MAX);
    OutExtentMax = FVector2D(-FLT_MAX, -FLT_MAX);

    const FStaticMeshLODResources& LOD = DataSource.RenderData->LODResources[0];
    const int32 NumSections = LOD.Sections.Num();
    if (NumSections > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("StaticMesh %s has %d sections. SMeshWidget expects a static mesh with 1 section."), *DataSource.GetName(), NumSections);
    }
    else
    {
        // Populate Vertex Data
        {
            const uint32 NumVerts = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
            OutSlateVerts.Empty();
            OutSlateVerts.Reserve(NumVerts);

            static const int32 MAX_SUPPORTED_UV_SETS = 6;
            const int32 TexCoordsPerVertex = LOD.GetNumTexCoords();
            if (TexCoordsPerVertex > MAX_SUPPORTED_UV_SETS)
            {
                UE_LOG(LogTemp, Warning, TEXT("[%s] has %d UV sets; slate vertex data supports at most %d"), *DataSource.GetName(), TexCoordsPerVertex, MAX_SUPPORTED_UV_SETS);
            }

            for (uint32 i = 0; i < NumVerts; ++i)
            {
                // Copy Position
                const FVector& Position = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(i);
                OutExtentMin.X = FMath::Min(Position.X, OutExtentMin.X);
                OutExtentMin.Y = FMath::Min(Position.Y, OutExtentMin.Y);
                OutExtentMax.X = FMath::Max(Position.X, OutExtentMax.X);
                OutExtentMax.Y = FMath::Max(Position.Y, OutExtentMax.Y);

                // Copy Color
                FColor Color = (LOD.VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0) ? LOD.VertexBuffers.ColorVertexBuffer.VertexColor(i) : FColor::White;

                // Copy all the UVs that we have, and as many as we can fit.
                const FVector2D& UV0 = (TexCoordsPerVertex > 0) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 0) : FVector2D(1, 1);

                const FVector2D& UV1 = (TexCoordsPerVertex > 1) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 1) : FVector2D(1, 1);

                const FVector2D& UV2 = (TexCoordsPerVertex > 2) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 2) : FVector2D(1, 1);

                const FVector2D& UV3 = (TexCoordsPerVertex > 3) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 3) : FVector2D(1, 1);

                const FVector2D& UV4 = (TexCoordsPerVertex > 4) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 4) : FVector2D(1, 1);

                const FVector2D& UV5 = (TexCoordsPerVertex > 5) ? LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 5) : FVector2D(1, 1);

                OutSlateVerts.Add(FSlateMeshVertex(
                    FVector2D(Position.X, Position.Y),
                    Color,
                    UV0,
                    UV1,
                    UV2,
                    UV3,
                    UV4,
                    UV5
                ));
            }
        }

        // Populate Index data
        {
            FIndexArrayView SourceIndexes = LOD.IndexBuffer.GetArrayView();
            const int32 NumIndexes = SourceIndexes.Num();
            OutIndexes.Empty();
            OutIndexes.Reserve(NumIndexes);
            for (int32 i = 0; i < NumIndexes; ++i)
            {
                OutIndexes.Add(SourceIndexes[i]);
            }


            // Sort the index buffer such that verts are drawn in Z-order.
            // Assume that all triangles are coplanar with Z == SomeValue.
            ensure(NumIndexes % 3 == 0);
            for (int32 a = 0; a < NumIndexes; a += 3)
            {
                for (int32 b = 0; b < NumIndexes; b += 3)
                {
                    const float VertADepth = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(OutIndexes[a]).Z;
                    const float VertBDepth = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(OutIndexes[b]).Z;
                    if (VertADepth < VertBDepth)
                    {
                        // Swap the order in which triangles will be drawn
                        Swap(OutIndexes[a + 0], OutIndexes[b + 0]);
                        Swap(OutIndexes[a + 1], OutIndexes[b + 1]);
                        Swap(OutIndexes[a + 2], OutIndexes[b + 2]);
                    }
                }
            }
        }
    }
}

void UNiagaraSystemWidget::ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const
{
    Super::ValidateCompiledDefaults(CompileLog);

    MeshData.Empty();
    for (auto i : NiagaraSystemReference->GetEmitterHandles())
    {
        for (auto j : i.GetInstance()->GetRenderers())
        {
            if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(j))
            {
                if (MeshRenderer->ParticleMesh)
                {
                    TArray<FSlateMeshVertex> SlateVerts;
                    TArray<uint32> Indexes;
                    FVector2D ExtentMin;
                    FVector2D ExtentMax;
                    StaticMeshToSlateRenderData(*MeshRenderer->ParticleMesh, SlateVerts, Indexes, ExtentMin, ExtentMax);
                    FSlateMeshData Mesh;
                    Mesh.MeshPackageName = MeshRenderer->ParticleMesh->GetPackage()->GetFName();
                    Mesh.Vertex.Empty(SlateVerts.Num());
                    Mesh.VertexColor.Empty(SlateVerts.Num());
                    Mesh.UV.Empty(SlateVerts.Num());
                    for (auto k : SlateVerts)
                    {
                        Mesh.Vertex.Add(k.Position);
                        Mesh.VertexColor.Add(k.Color);
                        Mesh.UV.Add(k.UV0);
                    }
                    Mesh.Index = Indexes;
                    MeshData.Add(Mesh);
                }
                
            }
        }
    }
        
}
#endif

void UNiagaraSystemWidget::InitializeNiagaraUI()
{
	if (UWorld* World = GetWorld())
	{
		if (!World->PersistentLevel)
			return;

		if (!NiagaraComponent)
		{
			NiagaraComponent = NewObject<UNiagaraUIComponent>(this);
            NiagaraComponent->SetAutoActivate(AutoActivate);
            NiagaraComponent->SetHiddenInGame(!ShowDebugSystemInWorld);
            NiagaraComponent->RegisterComponentWithWorld(World);
            NiagaraComponent->SetAsset(NiagaraSystemReference);
            NiagaraComponent->SetAutoDestroy(false);
           
            if (TickWhenPaused)
            {
				NiagaraComponent->PrimaryComponentTick.bTickEvenWhenPaused = true;
				NiagaraComponent->SetForceSolo(true);
            }
		}
		NiagaraSlateWidget->SetNiagaraComponentReference(NiagaraComponent, FNiagaraWidgetProperties(AutoActivate, ShowDebugSystemInWorld, FakeDepthScale, FakeDepthScaleDistance));
	}
}

void UNiagaraSystemWidget::ActivateSystem(bool Reset)
{
	if (NiagaraComponent)
		NiagaraComponent->Activate(Reset);
}

void UNiagaraSystemWidget::DeactivateSystem()
{
	if (NiagaraComponent)
		NiagaraComponent->Deactivate();
}

UNiagaraUIComponent* UNiagaraSystemWidget::GetNiagaraComponent()
{
	return NiagaraComponent;
}

void UNiagaraSystemWidget::UpdateNiagaraSystemReference(UNiagaraSystem* NewNiagaraSystem)
{
	NiagaraSystemReference = NewNiagaraSystem;

	if (NiagaraComponent)
	{
		NiagaraComponent->SetAsset(NewNiagaraSystem);
		NiagaraComponent->ResetSystem();
	}
}

void UNiagaraSystemWidget::UpdateTickWhenPaused(bool NewTickWhenPaused)
{
	TickWhenPaused = NewTickWhenPaused;

	if (NiagaraComponent)
	{
		NiagaraComponent->SetTickableWhenPaused(NewTickWhenPaused);
		NiagaraComponent->SetForceSolo(NewTickWhenPaused);
		NiagaraComponent->ResetSystem();
	}
}

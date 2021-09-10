// Copyright 2021 - Michal Smoleň

#include "NiagaraUIComponent.h"
#include "Stats/Stats.h"
#include "NiagaraRenderer.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Slate/SlateVectorArtInstanceData.h"
#include "SNiagaraUISystemWidget.h"


DECLARE_STATS_GROUP(TEXT("NiagaraUI"), STATGROUP_NiagaraUI, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Generate Sprite Data"), STAT_GenerateSpriteData, STATGROUP_NiagaraUI);
DECLARE_CYCLE_STAT(TEXT("Generate Ribbon Data"), STAT_GenerateRibbonData, STATGROUP_NiagaraUI);

//PRAGMA_DISABLE_OPTIMIZATION

void UNiagaraUIComponent::SetTransformationForUIRendering(FVector2D Location, FVector2D Scale, float Angle)
{
	const FVector NewLocation(Location.X, 0.f, -Location.Y);
	const FVector NewScale(Scale.X, 1.f, Scale.Y);
	const FRotator NewRotation(FMath::RadiansToDegrees(Angle), 0.f, 0.f);;
	WidgetAngleRad = Angle;
    SetRelativeTransform(FTransform(NewRotation, NewLocation, NewScale));

	if (bAutoActivate)
	{
		ActivateSystem();
		bAutoActivate = false;
	}
}

void UNiagaraUIComponent::SetTransformationForUIRendering(const FSlateRenderTransform& SlateRenderTransform)
{
    float A, B, C, D;
	SlateRenderTransform.GetMatrix().GetMatrix(A, B, C, D);
    FVector2D T = SlateRenderTransform.GetTranslation();
    FMatrix M3 = FMatrix(
        FPlane(A, 0.0f, -B, 0.0f),
        FPlane(0.0f, 1.0f, 0.0f, 0.0f),
        FPlane(-C, 0.0f, D, 0.0f),
        FPlane(T.X, 0.0f, -T.Y, 1.0f)
    );

    SetRelativeTransform(FTransform(M3));

    if (bAutoActivate)
    {
        ActivateSystem();
        bAutoActivate = false;
    }
}

struct FNiagaraRendererEntry
{
	FNiagaraRendererEntry(UNiagaraRendererProperties* PropertiesIn, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstIn, UNiagaraEmitter* EmitterIn)
		: RendererProperties(PropertiesIn), EmitterInstance(EmitterInstIn), Emitter(EmitterIn) {}
	UNiagaraRendererProperties* RendererProperties;
	TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstance;
	UNiagaraEmitter* Emitter;
};

void UNiagaraUIComponent::RenderUI(SNiagaraUISystemWidget* NiagaraWidget, const FSlateLayoutTransform& SlateLayoutTransform, const FSlateRenderTransform& SlateRenderTransform, const FNiagaraWidgetProperties* WidgetProperties)
{
	if (!IsActive())
		return;

	if (!GetSystemInstance())
		return;

    NiagaraWidget->ClearRenderData();
	NiagaraWidget->ClearRuns(1);
	TArray<FNiagaraRendererEntry> Renderers;

	for(TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst : GetSystemInstance()->GetEmitters())
	{
		if (UNiagaraEmitter* Emitter = EmitterInst->GetCachedEmitter())
		{
			TArray<UNiagaraRendererProperties*> Properties = Emitter->GetRenderers();

			for (UNiagaraRendererProperties* Property : Properties)
			{
				FNiagaraRendererEntry NewEntry(Property, EmitterInst, Emitter);
                Renderers.Add(NewEntry);
			}
		}
	}

	Algo::Sort(Renderers, [] (FNiagaraRendererEntry& FirstElement, FNiagaraRendererEntry& SecondElement) {return FirstElement.RendererProperties->SortOrderHint < SecondElement.RendererProperties->SortOrderHint;});
			
	for (FNiagaraRendererEntry Renderer : Renderers)
	{
		if (Renderer.RendererProperties && Renderer.RendererProperties->GetIsEnabled() && Renderer.RendererProperties->IsSimTargetSupported(Renderer.Emitter->SimTarget))
		{
			if (Renderer.Emitter->SimTarget == ENiagaraSimTarget::CPUSim)
			{
				if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer.RendererProperties))
				{
					AddSpriteRendererData(NiagaraWidget, Renderer.EmitterInstance, SpriteRenderer, SlateLayoutTransform, SlateRenderTransform, WidgetProperties);
				}
				else if (UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Renderer.RendererProperties))
				{
					AddRibbonRendererData(NiagaraWidget, Renderer.EmitterInstance, RibbonRenderer, SlateLayoutTransform, SlateRenderTransform, WidgetProperties);
				}
			}
		}
	}
}

FORCEINLINE FVector2D FastRotate(const FVector2D Vector, float Sin, float Cos)
{
	return FVector2D(Cos * Vector.X - Sin * Vector.Y,
                     Sin * Vector.X + Cos * Vector.Y);
}

template<int32 Component, int32 ByteIndex>
static void PackUint8IntoByte(FVector4& Data, uint8 InValue)
{
    static const uint32 Mask = ~(0xFF << ByteIndex * 8);
    uint32* CurrentData = (uint32*)(&Data[Component]);
    *CurrentData = (*CurrentData) & Mask;
    *CurrentData = (*CurrentData) | (InValue << ByteIndex * 8);
}

template<int32 Component, int32 ByteIndex>
static void PackUint16IntoByte(FVector4& Data, uint16 InValue)
{
    static const uint32 Mask = ~(0xFFFF << ByteIndex * 16);
    uint32* CurrentData = (uint32*)(&Data[Component]);
    *CurrentData = (*CurrentData) & Mask;
    *CurrentData = (*CurrentData) | (InValue << ByteIndex * 16);
}

static void InitPack(FVector4& Datas)
{
    PackUint8IntoByte<0, 3>(Datas, 2);
    PackUint8IntoByte<1, 3>(Datas, 2);
    PackUint8IntoByte<2, 3>(Datas, 2);
    PackUint8IntoByte<3, 3>(Datas, 2);

}

static void PackColor(FVector4& Datas,FColor Color)
{
    PackUint8IntoByte<2, 0>(Datas, Color.R);
    PackUint8IntoByte<2, 1>(Datas, Color.G);
    PackUint8IntoByte<2, 2>(Datas, Color.B);
    PackUint8IntoByte<2, 3>(Datas, ((Color.A >> 2) << 2) + 2);
}

static void PackPosition(FVector4& Datas, FVector2D Pos)
{
	Pos = Pos.ClampAxes(0.f,8191.f);

    PackUint16IntoByte<0, 0>(Datas, uint16(Pos.X * 8.0f));
    PackUint16IntoByte<1, 0>(Datas, uint16(Pos.Y * 8.0f));
}

static void PackScale(FVector4& Datas, FVector2D Scale)
{
	Scale = Scale.ClampAxes(0.f,127.f);

	uint16 SizeX = uint16(Scale.X * 128.0f);
    PackUint8IntoByte<0, 2>(Datas, (SizeX & 0xFF00) >> 8);
    PackUint8IntoByte<0, 3>(Datas, (((SizeX & 0x00FF) >> 2) << 2) + 2);

    uint16 SizeY = uint16(Scale.Y * 128.0f);
    PackUint8IntoByte<1, 2>(Datas, (SizeY & 0xFF00) >> 8);
    PackUint8IntoByte<1, 3>(Datas, (((SizeY & 0x00FF) >> 2) << 2) + 2);
}

static void PackRotation(FVector4& Datas, float rot)
{
	float Angle = FMath::Fmod(rot, 360.f);
    if (Angle < 0.f) Angle += 360.f;
    uint16 urot = uint16(Angle * 32.0f);
    PackUint8IntoByte<3, 2>(Datas, (urot & 0xFF00) >> 8);
    PackUint8IntoByte<3, 3>(Datas, (((urot & 0x00FF) >> 2) << 2) + 2);
}

static void PackSubImage(FVector4& Datas, uint8 index,uint8 sizeX,uint8 sizeY)
{
    PackUint8IntoByte<3, 0>(Datas, index);
    PackUint8IntoByte<3, 1>(Datas, sizeY * 16 + sizeX);
}

void UNiagaraUIComponent::AddSpriteRendererData(SNiagaraUISystemWidget* NiagaraWidget, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst, UNiagaraSpriteRendererProperties* SpriteRenderer, const FSlateLayoutTransform& SlateLayoutTransform, const FSlateRenderTransform& SlateRenderTransform, const FNiagaraWidgetProperties* WidgetProperties)
{
    {
        SCOPE_CYCLE_COUNTER(STAT_GenerateSpriteData);

        FNiagaraDataSet& DataSet = EmitterInst->GetData();
        FNiagaraDataBuffer& ParticleData = DataSet.GetCurrentDataChecked();
        const int32 ParticleCount = ParticleData.GetNumInstances();

        if (ParticleCount < 1)
            return;

		//Add a box mesh
		
        FSlateVertex* VertexData;
        SlateIndex* IndexData;

        FSlateBrush Brush;

        UMaterialInterface* SpriteMaterial = SpriteRenderer->Material;
        uint32 RenderDataIndex = NiagaraWidget->AddRenderDataWithInstance(&VertexData, &IndexData, SpriteMaterial, 4, 6);

        VertexData[0].Position = FVector2D(-10, -10);
        VertexData[0].Color = FColor(255, 0, 0, 255);
        VertexData[0].TexCoords[0] = 0;
        VertexData[0].TexCoords[1] = 0;
        VertexData[0].TexCoords[2] = 0;
        VertexData[0].TexCoords[3] = 0;

        VertexData[1].Position = FVector2D(10, -10);
        VertexData[1].Color = FColor(255, 0, 0, 255);
        VertexData[1].TexCoords[0] = 1;
        VertexData[1].TexCoords[1] = 0;
        VertexData[1].TexCoords[2] = 0;
        VertexData[1].TexCoords[3] = 0;

        VertexData[2].Position = FVector2D(10, 10);
        VertexData[2].Color = FColor(255, 0, 0, 255);
        VertexData[2].TexCoords[0] = 1;
        VertexData[2].TexCoords[1] = 1;
        VertexData[2].TexCoords[2] = 0;
        VertexData[2].TexCoords[3] = 0;

        VertexData[3].Position = FVector2D(-10, 10);
        VertexData[3].Color = FColor(255, 0, 0, 255);
        VertexData[3].TexCoords[0] = 0;
        VertexData[3].TexCoords[1] = 1;
        VertexData[3].TexCoords[2] = 0;
        VertexData[3].TexCoords[3] = 0;

        IndexData[0] = 0;
        IndexData[1] = 1;
        IndexData[2] = 2;

        IndexData[3] = 0;
        IndexData[4] = 2;
        IndexData[5] = 3;
		
		
        const auto PositionData = FNiagaraDataSetAccessor<FVector>::CreateReader(DataSet, SpriteRenderer->PositionBinding.GetDataSetBindableVariable().GetName());
        const auto ColorData = FNiagaraDataSetAccessor<FLinearColor>::CreateReader(DataSet, SpriteRenderer->ColorBinding.GetDataSetBindableVariable().GetName());
        const auto VelocityData = FNiagaraDataSetAccessor<FVector>::CreateReader(DataSet, SpriteRenderer->VelocityBinding.GetDataSetBindableVariable().GetName());
        const auto SizeData = FNiagaraDataSetAccessor<FVector2D>::CreateReader(DataSet, SpriteRenderer->SpriteSizeBinding.GetDataSetBindableVariable().GetName());
        const auto RotationData = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, SpriteRenderer->SpriteRotationBinding.GetDataSetBindableVariable().GetName());
        const auto SubImageData = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, SpriteRenderer->SubImageIndexBinding.GetDataSetBindableVariable().GetName());
        const auto DynamicMaterialData = FNiagaraDataSetAccessor<FVector4>::CreateReader(DataSet, SpriteRenderer->DynamicMaterialBinding.GetDataSetBindableVariable().GetName());

        auto GetParticlePosition2D = [&PositionData](int32 Index)
        {
            const FVector Position3D = PositionData.GetSafe(Index, FVector::ZeroVector);
            return FVector2D(Position3D.X, -Position3D.Z);
        };

        auto GetParticleDepth = [&PositionData](int32 Index)
        {
            return PositionData.GetSafe(Index, FVector::ZeroVector).Y;
        };

        auto GetParticleColor = [&ColorData](int32 Index)
        {
            return ColorData.GetSafe(Index, FLinearColor::White);
        };

        auto GetParticleVelocity2D = [&VelocityData](int32 Index)
        {
            const FVector Velocity3D = VelocityData.GetSafe(Index, FVector::ZeroVector);
            return FVector2D(Velocity3D.X, Velocity3D.Z);
        };

        auto GetParticleSize = [&SizeData](int32 Index)
        {
            return SizeData.GetSafe(Index, FVector2D::ZeroVector);
        };

        auto GetParticleRotation = [&RotationData](int32 Index)
        {
            return RotationData.GetSafe(Index, 0.f);
        };

        auto GetParticleSubImage = [&SubImageData](int32 Index)
        {
            return SubImageData.GetSafe(Index, 0.f);
        };

        auto GetDynamicMaterialData = [&DynamicMaterialData](int32 Index)
        {
            return DynamicMaterialData.GetSafe(Index, FVector4(0.f, 0.f, 0.f, 0.f));
        };

       
        bool LocalSpace = EmitterInst->GetCachedEmitter()->bLocalSpace;

        const float FakeDepthScaler = 1 / WidgetProperties->FakeDepthScaleDistance;

        FVector2D SubImageSize = SpriteRenderer->SubImageSize;

        FSlateInstanceBufferData InstanceData;

        for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
        {

            FVector2D ParticlePosition = GetParticlePosition2D(ParticleIndex);
            FVector2D ParticleSize = GetParticleSize(ParticleIndex);
            FVector2D ParticleScale = ParticleSize * 0.05 * SlateLayoutTransform.GetScale();
			          
            if (WidgetProperties->FakeDepthScale)
            {
                const float ParticleDepth = (-GetParticleDepth(ParticleIndex) + WidgetProperties->FakeDepthScaleDistance) * FakeDepthScaler;
                ParticleSize *= ParticleDepth;
            }

            const FColor ParticleColor = GetParticleColor(ParticleIndex).ToFColor(true);

			float ParticleRotation = 0.0;           

            if (SpriteRenderer->Alignment == ENiagaraSpriteAlignment::VelocityAligned)
            {
                const FVector2D ParticleVelocity = GetParticleVelocity2D(ParticleIndex);
                float Ang1 = FMath::Atan2(ParticleVelocity.X, ParticleVelocity.Y);
                float Ang = FMath::RadiansToDegrees(Ang1);				                
				ParticleRotation = Ang;
            }
            else
            {
				float Ang = GetParticleRotation(ParticleIndex);
                ParticleRotation = Ang;
            }
            
            float ParticleSubImage = 0;
            int Row = 1;
            int Column = 1;

            if (SubImageSize != FVector2D(1.f, 1.f))
            {
                ParticleSubImage = GetParticleSubImage(ParticleIndex);
                Row = (int)FMath::Floor(ParticleSubImage / SubImageSize.X) % (int)SubImageSize.Y;
                Column = (int)(ParticleSubImage) % (int)(SubImageSize.X);
            }
			
			if (LocalSpace)
			{
				ParticlePosition = SlateRenderTransform.TransformPoint(ParticlePosition);
				FVector2D RV = SlateRenderTransform.TransformVector(FVector2D(1.f,0.f));
                float ParentRotRad = FMath::Atan2(RV.Y, RV.X);
				ParticleRotation += FMath::RadiansToDegrees(ParentRotRad);
			}

            FSlateVectorArtInstanceData ArtInstData;
            FVector4& data = ArtInstData.GetData();
            InitPack(data);
            PackPosition(data, ParticlePosition);
            PackRotation(data, ParticleRotation);
            PackColor(data, ParticleColor);
            PackScale(data, ParticleScale);
            PackSubImage(data, ParticleSubImage, Column, Row);
            InstanceData.Add(ArtInstData.GetData());
            
        }
        NiagaraWidget->AddRenderRun(RenderDataIndex, 0, InstanceData.Num());
        NiagaraWidget->UpdatePerInstanceBuffer(RenderDataIndex, InstanceData);
    }
   

}

void UNiagaraUIComponent::AddRibbonRendererData(SNiagaraUISystemWidget* NiagaraWidget, TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst, UNiagaraRibbonRendererProperties* RibbonRenderer, const FSlateLayoutTransform& SlateLayoutTransform, const FSlateRenderTransform& SlateRenderTransform, const FNiagaraWidgetProperties* WidgetProperties)
{
	SCOPE_CYCLE_COUNTER(STAT_GenerateRibbonData);
	
	FNiagaraDataSet& DataSet = EmitterInst->GetData();
	FNiagaraDataBuffer& ParticleData = DataSet.GetCurrentDataChecked();
	const int32 ParticleCount = ParticleData.GetNumInstances();

	if (ParticleCount < 2)
		return;
	


	const auto SortKeyReader = RibbonRenderer->SortKeyDataSetAccessor.GetReader(DataSet);

	const auto PositionData		= RibbonRenderer->PositionDataSetAccessor.GetReader(DataSet);
	const auto ColorData		= FNiagaraDataSetAccessor<FLinearColor>::CreateReader(DataSet, RibbonRenderer->ColorBinding.GetDataSetBindableVariable().GetName());
	const auto RibbonWidthData	= RibbonRenderer->SizeDataSetAccessor.GetReader(DataSet);
	
	const auto RibbonFullIDData = RibbonRenderer->RibbonFullIDDataSetAccessor.GetReader(DataSet);

    const bool LocalSpace = EmitterInst->GetCachedEmitter()->bLocalSpace;
    const bool FullIDs = RibbonFullIDData.IsValid();
    const bool MultiRibbons = FullIDs;

	auto GetParticlePosition2D = [&LocalSpace ,&SlateRenderTransform ,&PositionData](int32 Index)
	{
		const FVector Position3D = PositionData.GetSafe(Index, FVector::ZeroVector);
		return LocalSpace ? SlateRenderTransform.TransformPoint(FVector2D(Position3D.X, -Position3D.Z)) : FVector2D(Position3D.X, -Position3D.Z);
	};	

	auto GetParticleColor = [&ColorData](int32 Index)
	{
		return ColorData.GetSafe(Index, FLinearColor::White);
	};
	
	auto GetParticleWidth = [&SlateLayoutTransform ,&RibbonWidthData](int32 Index)
	{
		
		return RibbonWidthData.GetSafe(Index, 0.f) * SlateLayoutTransform.GetScale();
	};

	

	auto AddRibbonVerts = [&](TArray<int32>& RibbonIndices)
	{
		const int32 numParticlesInRibbon = RibbonIndices.Num();
		if (numParticlesInRibbon < 3)
			return;
		
		FSlateVertex* VertexData;	
		SlateIndex* IndexData;
	
		FSlateBrush Brush;
		UMaterialInterface* SpriteMaterial = RibbonRenderer->Material;
		
		NiagaraWidget->AddRenderData(&VertexData, &IndexData, SpriteMaterial, (numParticlesInRibbon - 1) * 2, (numParticlesInRibbon - 2) * 6);
		
		int32 CurrentVertexIndex = 0;
		int32 CurrentIndexIndex = 0;
		
			
		const int32 StartDataIndex = RibbonIndices[0];

		float TotalDistance = 0.0f;

		FVector2D LastPosition = GetParticlePosition2D(StartDataIndex);
		FVector2D CurrentPosition = FVector2D::ZeroVector;
		float CurrentWidth = 0.f;
		FVector2D LastToCurrentVector = FVector2D::ZeroVector;
		float LastToCurrentSize = 0.f;
		float LastU0 = 0.f;
		float LastU1 = 0.f;
		
		FVector2D LastParticleUIPosition = LastPosition;
		
		int32 CurrentIndex = 1;
		int32 CurrentDataIndex = RibbonIndices[CurrentIndex];
		
		CurrentPosition = GetParticlePosition2D(CurrentDataIndex);

		LastToCurrentVector = CurrentPosition - LastPosition;
		LastToCurrentSize = LastToCurrentVector.Size();
		
		// Normalize LastToCurrVec
		LastToCurrentVector *= 1.f / LastToCurrentSize;
		

		const FColor InitialColor = GetParticleColor(StartDataIndex).ToFColor(true);
		const float InitialWidth = GetParticleWidth(StartDataIndex);
		
		FVector2D InitialPositionArray[2];
		InitialPositionArray[0] = LastToCurrentVector.GetRotated(90.f) * InitialWidth * 0.5f;
		InitialPositionArray[1] = -InitialPositionArray[0];
		
		for (int i = 0; i < 2; ++i)
		{
			VertexData[CurrentVertexIndex + i].Position = InitialPositionArray[i] +  LastParticleUIPosition;
			VertexData[CurrentVertexIndex + i].Color = InitialColor;
			VertexData[CurrentVertexIndex + i].TexCoords[0] = i;
			VertexData[CurrentVertexIndex + i].TexCoords[1] = 0;
		}

		CurrentVertexIndex += 2;

		int32 NextIndex = CurrentIndex + 1;
		
		while (NextIndex < numParticlesInRibbon)
		{
			const int32 NextDataIndex = RibbonIndices[NextIndex];
			const FVector2D NextPosition = GetParticlePosition2D(NextDataIndex);	
			FVector2D CurrentToNextVector = NextPosition - CurrentPosition;
			const float CurrentToNextSize = CurrentToNextVector.Size();		
			CurrentWidth = GetParticleWidth(CurrentDataIndex);
			FColor CurrentColor = GetParticleColor(CurrentDataIndex).ToFColor(true);

			// Normalize CurrToNextVec
			CurrentToNextVector *= 1.f / CurrentToNextSize;
			
			const FVector2D CurrentTangent = (LastToCurrentVector + CurrentToNextVector).GetSafeNormal();
			
			TotalDistance += LastToCurrentSize;

			FVector2D CurrentPositionArray[2];
			CurrentPositionArray[0] = CurrentTangent.GetRotated(90.f) * CurrentWidth * 0.5f;
			CurrentPositionArray[1] = -CurrentPositionArray[0];

			FVector2D CurrentParticleUIPosition = CurrentPosition;
			
			float CurrentU0 = 0.f;
		
			if (RibbonRenderer->UV0Settings.DistributionMode == ENiagaraRibbonUVDistributionMode::TiledOverRibbonLength)
			{
				CurrentU0 = LastU0 + LastToCurrentSize / RibbonRenderer->UV0Settings.TilingLength;
			}
			else
			{
				CurrentU0 = (float)CurrentIndex / (float)numParticlesInRibbon;
			}
			
			float CurrentU1 = 0.f;
		
			if (RibbonRenderer->UV1Settings.DistributionMode == ENiagaraRibbonUVDistributionMode::TiledOverRibbonLength)
			{
				CurrentU1 = LastU1 + LastToCurrentSize / RibbonRenderer->UV1Settings.TilingLength;
			}
			else
			{
				CurrentU1 = (float)CurrentIndex / (float)numParticlesInRibbon;
			}

			FVector2D TextureCoordinates0[2];
			TextureCoordinates0[0] = FVector2D(CurrentU0, 1.f);
			TextureCoordinates0[1] = FVector2D(CurrentU0, 0.f);
			
            FVector2D TextureCoordinates1[2];
			TextureCoordinates1[0] = FVector2D(CurrentU1, 1.f);
			TextureCoordinates1[1] = FVector2D(CurrentU1, 0.f);
			
			for (int i = 0; i < 2; ++i)
			{
				VertexData[CurrentVertexIndex + i].Position = CurrentPositionArray[i] + CurrentParticleUIPosition;
				VertexData[CurrentVertexIndex + i].Color = CurrentColor;
				VertexData[CurrentVertexIndex + i].TexCoords[0] = TextureCoordinates0[i].X;
				VertexData[CurrentVertexIndex + i].TexCoords[1] = TextureCoordinates0[i].Y;
				VertexData[CurrentVertexIndex + i].TexCoords[2] = TextureCoordinates1[i].X;
				VertexData[CurrentVertexIndex + i].TexCoords[3] = TextureCoordinates1[i].Y;
			}
			
			IndexData[CurrentIndexIndex] = CurrentVertexIndex - 2;
			IndexData[CurrentIndexIndex + 1] = CurrentVertexIndex - 1;
			IndexData[CurrentIndexIndex + 2] = CurrentVertexIndex;
		
			IndexData[CurrentIndexIndex + 3] = CurrentVertexIndex - 1;
			IndexData[CurrentIndexIndex + 4] = CurrentVertexIndex;
			IndexData[CurrentIndexIndex + 5] = CurrentVertexIndex + 1;
			

			CurrentVertexIndex += 2;
			CurrentIndexIndex += 6;
			
			CurrentIndex = NextIndex;
			CurrentDataIndex = NextDataIndex;
			LastPosition = CurrentPosition;
			LastParticleUIPosition = CurrentParticleUIPosition;
			CurrentPosition = NextPosition;
			LastToCurrentVector = CurrentToNextVector;
			LastToCurrentSize = CurrentToNextSize;
			LastU0 = CurrentU0;

			++NextIndex;
		}
	};

	if (!MultiRibbons)
	{
		TArray<int32> SortedIndices;
		for (int32 i = 0; i < ParticleCount; ++i)
		{
			SortedIndices.Add(i);
		}

		SortedIndices.Sort([&SortKeyReader](const int32& A, const int32& B) {	return (SortKeyReader[A] < SortKeyReader[B]); });

		AddRibbonVerts(SortedIndices);
	}
	else
	{
		if (FullIDs)
		{
			TMap<FNiagaraID, TArray<int32>> MultiRibbonSortedIndices;

			for (int32 i = 0; i < ParticleCount; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonFullIDData[i]);
				Indices.Add(i);
			}

			// Sort the ribbons by ID so that the draw order stays consistent.
			MultiRibbonSortedIndices.KeySort(TLess<FNiagaraID>());

			for (TPair<FNiagaraID, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				TArray<int32>& SortedIndices = Pair.Value;
				SortedIndices.Sort([&SortKeyReader](const int32& A, const int32& B) {	return (SortKeyReader[A] < SortKeyReader[B]); });
				AddRibbonVerts(SortedIndices);
			};
		}
	}

}

//PRAGMA_ENABLE_OPTIMIZATION

// Copyright (c) 2021 Michal Smoleň 


#include "NiagaraWidgetDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemWidget.h"
#include "NiagaraUIComponent.h"
#include "NiagaraUIRendererEditorStyle.h"
#include "Widgets/Text/SMultiLineEditableText.h"

#define LOCTEXT_NAMESPACE "NiagaraWidgetDetailCustomization"

void FNiagaraWidgetDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& NiagaraCategory = DetailBuilder.EditCategory("Niagara UI Renderer", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	
	TArray<TWeakObjectPtr<UObject>> SelectedObjects = DetailBuilder.GetSelectedObjects();

	TArray<UNiagaraSystemWidget*> SelectedNiagaraWidgets;

	for (TWeakObjectPtr<UObject> Reference : SelectedObjects)
	{
		if (UNiagaraSystemWidget* NiagaraWidget = Cast<UNiagaraSystemWidget>(Reference.Get()))
			SelectedNiagaraWidgets.Add(NiagaraWidget);
	}

	if (SelectedNiagaraWidgets.Num() != 1)
		return;	

	CachedNiagaraWidget = SelectedNiagaraWidgets[0];

	RegisterPropertyChanged(DetailBuilder);

	if (CachedNiagaraWidget->DisableWarnings || !CachedNiagaraWidget->NiagaraSystemReference)
		return;

	WarningMessages.Empty();
	ShowAutoPopulate = false;
	
	CheckWarnings();

	if (WarningMessages.Num() > 0)
		DisplayWarningBox(DetailBuilder);
}

void FNiagaraWidgetDetailCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{	
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

TSharedRef<IDetailCustomization> FNiagaraWidgetDetailCustomization::MakeInstance()
{
	return MakeShareable(new FNiagaraWidgetDetailCustomization);
}

void FNiagaraWidgetDetailCustomization::RegisterPropertyChanged(IDetailLayoutBuilder& DetailBuilder)
{
	RegisterPropertyChangedFullRefresh(DetailBuilder, GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, NiagaraSystemReference));
	RegisterPropertyChangedFullRefresh(DetailBuilder, GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, DisableWarnings));

	if (CachedNiagaraWidget->NiagaraSystemReference)
	{
		CachedNiagaraWidget->NiagaraSystemReference->OnSystemCompiled().AddSP(this, &FNiagaraWidgetDetailCustomization::OnNiagaraSystemChanged);
		CachedNiagaraWidget->NiagaraSystemReference->OnSystemPostEditChange().AddSP(this, &FNiagaraWidgetDetailCustomization::OnNiagaraSystemChanged);
	}
}

void FNiagaraWidgetDetailCustomization::RegisterPropertyChangedFullRefresh(IDetailLayoutBuilder& DetailBuilder, const FName Property, bool RegisterChildProperty)
{
	const TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(Property);
	PropertyHandle.Get()->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNiagaraWidgetDetailCustomization::ForceRefreshDetailPanel));	
	PropertyHandle.Get()->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNiagaraWidgetDetailCustomization::ForceRefreshDetailPanel));
}

void FNiagaraWidgetDetailCustomization::CheckWarnings()
{
	if (!CachedNiagaraWidget.IsValid())
		return;

	UNiagaraUIComponent* NiagaraComponent = CachedNiagaraWidget->GetNiagaraComponent();

	if (!NiagaraComponent || !NiagaraComponent->GetSystemInstance())
		return;

	TArray<FString> GPUWarningEmitterNames;
	TArray<FString> DomainWarningEmitterNames;
	TArray<FString> DomainWarningRendererNames;
	TArray<FString> DomainWarningMaterialNames;

	for(TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst : NiagaraComponent->GetSystemInstance()->GetEmitters())
	{
		if (UNiagaraEmitter* Emitter = EmitterInst->GetCachedEmitter())
		{
			if (Emitter->SimTarget != ENiagaraSimTarget::CPUSim)
			{
				GPUWarningEmitterNames.Add(Emitter->GetName());
			}
			else
			{
				TArray<UNiagaraRendererProperties*> Properties = Emitter->GetRenderers();

				for (UNiagaraRendererProperties* Property : Properties)
				{
					UMaterialInterface* RendererMaterial = nullptr;
					
					if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Property))
					{
						RendererMaterial = SpriteRenderer->Material;
					}
					else if (UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Property))
					{
						RendererMaterial = RibbonRenderer->Material;
					}
				}
			}
		}
	}

	if (GPUWarningEmitterNames.Num() > 0)
	{
		FString EmitterNames;

		for (int i = 0; i < GPUWarningEmitterNames.Num(); ++i)
		{
			if (i != 0)
				EmitterNames.Append(TEXT(", "));

			EmitterNames.Append(TEXT("'") + GPUWarningEmitterNames[i] + TEXT("'"));
		}
		
		AddWarning(FString::Printf(TEXT("Incompatible sim target set in the %s emitter in the '%s' Niagara System. Only the 'CPUSim' sim target is supported. GPU emitters will NOT be rendered"), *EmitterNames, *CachedNiagaraWidget->NiagaraSystemReference->GetName()));
	}

	if (DomainWarningEmitterNames.Num() > 0)
	{
		FString RendererNames;

		for (int i = 0; i < DomainWarningEmitterNames.Num(); ++i)
		{
			if (i != 0)
				RendererNames.Append(TEXT(", "));

			RendererNames.Append(TEXT("'") + DomainWarningMaterialNames[i] + TEXT("' material in the '") + DomainWarningRendererNames[i] + TEXT("' emitter in the '") + DomainWarningEmitterNames[i] + TEXT("' emitter"));
		}
		
		AddWarning(FString::Printf(TEXT("Incompatible material domain set on the %s in the '%s' niagara system. Please use 'Material Remap List' in the widget properties to remap those materials to ones with the 'User Interface' material domain")
					, *RendererNames, *CachedNiagaraWidget->NiagaraSystemReference->GetName()));
	}
}

void FNiagaraWidgetDetailCustomization::DisplayWarningBox(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& NiagaraCategory = DetailBuilder.EditCategory("Niagara UI Renderer");
	TSharedPtr<IPropertyHandle> NiagaraSystemPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraSystemWidget, AutoActivate));

	FDetailWidgetRow& WarningRow = NiagaraCategory.AddCustomRow(FText(), false)
		.WholeRowContent()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(FMargin(0.f, 10.f, 15.f, 10.f))
			[
				SNew(SVerticalBox)
				
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FNiagaraUIRendererEditorStyle::GetBrush("NiagaraUIRendererEditorStyle.WarningBox.Top"))
					.BorderBackgroundColor(FLinearColor(FNiagaraUIRendererEditorStyle::WarningBoxComplementaryColor))
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.Padding(FMargin(4.f, 4.f))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FNiagaraUIRendererEditorStyle::GetBrush("NiagaraUIRendererEditorStyle.WarningBox.WarningIcon"))
						]
					]
				]

				+SVerticalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FNiagaraUIRendererEditorStyle::GetBrush("NiagaraUIRendererEditorStyle.WarningBox.Bottom"))
					.BorderBackgroundColor(FLinearColor(FColor(40,40, 40)))
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10.f, 15.f, 10.f, 10.f)
						[
							SNew(SMultiLineEditableText)
							.AutoWrapText(true)
							.Text(BuildWarningMessage())
							.IsReadOnly(true)
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Bottom)
						[
							SNew(SHorizontalBox)

							// Refresh button, shouldn't be necessary anymore
							/*
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.FillWidth(1.f)
							.Padding(5.f, 0.f)
							[
								SNew(SButton)
								.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
								.ForegroundColor(FColor::White)
								.OnPressed_Raw(this, &FNiagaraWidgetDetailCustomization::ForceRefreshDetailPanel)
								[
									SNew(SImage)
									.Image(FNiagaraUIRendererEditorStyle::GetBrush("NiagaraUIRendererEditorStyle.WarningBox.Refresh"))
								]
							]
							*/
							
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							//.AutoWidth()
							.FillWidth(1.f)
							[
								SNew(SButton)
								.ButtonStyle(FNiagaraUIRendererEditorStyle::Get(), "NiagaraUIRendererEditorStyle.WarningBox.AutoPopulateButton")
								.Text(LOCTEXT("NiagaraUIAutoPopulate", "Populate Remap List"))
								.TextStyle(FNiagaraUIRendererEditorStyle::Get(), "NiagaraUIRendererEditorStyle.IgnoreButtonText")
								.ForegroundColor(FColor::White)
								.ContentPadding(FMargin(8.f, 5.f))
								.OnPressed_Raw(this, &FNiagaraWidgetDetailCustomization::OnAutoPopulatePressed)
								.Visibility(ShowAutoPopulate ? EVisibility::Visible : EVisibility::Hidden)
								.IsEnabled(ShowAutoPopulate)
							]

							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								SNew(SButton)
								.ButtonStyle(FNiagaraUIRendererEditorStyle::Get(), ShowAutoPopulate ? "NiagaraUIRendererEditorStyle.WarningBox.IgnoreButtonFix" : "NiagaraUIRendererEditorStyle.WarningBox.IgnoreButton")
								.Text(LOCTEXT("NiagaraUIIgnoreWarnings", "Disable Warnings"))
								.TextStyle(FNiagaraUIRendererEditorStyle::Get(), "NiagaraUIRendererEditorStyle.IgnoreButtonText")
								.ForegroundColor(FColor::White)
								.ContentPadding(FMargin(5.f))
								.OnPressed_Raw(this, &FNiagaraWidgetDetailCustomization::OnIgnoreWarningsPressed)
							]
						]
					]
				]
			]
		];
}


void FNiagaraWidgetDetailCustomization::OnIgnoreWarningsPressed()
{
	CachedNiagaraWidget->DisableWarnings = true;
	ForceRefreshDetailPanel();
}

void FNiagaraWidgetDetailCustomization::OnAutoPopulatePressed()
{
	if (!CachedNiagaraWidget.IsValid())
		return;

	UNiagaraUIComponent* NiagaraComponent = CachedNiagaraWidget->GetNiagaraComponent();

	if (!NiagaraComponent || !NiagaraComponent->GetSystemInstance())
		return;

	for(TSharedRef<const FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInst : NiagaraComponent->GetSystemInstance()->GetEmitters())
	{
		if (UNiagaraEmitter* Emitter = EmitterInst->GetCachedEmitter())
		{
			if (Emitter->SimTarget == ENiagaraSimTarget::CPUSim)
			{
				TArray<UNiagaraRendererProperties*> Properties = Emitter->GetRenderers();

				for (UNiagaraRendererProperties* Property : Properties)
				{
					UMaterialInterface* RendererMaterial = nullptr;
					
					if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Property))
					{
						RendererMaterial = SpriteRenderer->Material;
					}
					else if (UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Property))
					{
						RendererMaterial = RibbonRenderer->Material;
					}
				}
			}
		}
	}

	ForceRefreshDetailPanel();
}

void FNiagaraWidgetDetailCustomization::OnNiagaraSystemChanged(UNiagaraSystem* System)
{
	ForceRefreshDetailPanel();
}

void FNiagaraWidgetDetailCustomization::ForceRefreshDetailPanel()
{
	if (IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get())
		DetailBuilder->ForceRefreshDetails();
}

void FNiagaraWidgetDetailCustomization::AddWarning(FString WarningMessage)
{
	WarningMessages.Add(WarningMessage);
}

FText FNiagaraWidgetDetailCustomization::BuildWarningMessage()
{
	FString FinalMessage;

	for (int i = 0; i < WarningMessages.Num(); ++i)
	{
		if (i != 0)
			FinalMessage.Append(TEXT("\n\n"));

		FinalMessage.Append(TEXT("- "));
		FinalMessage.Append(WarningMessages[i]);
	}

	return FText::FromString(FinalMessage);
}

#undef LOCTEXT_NAMESPACE

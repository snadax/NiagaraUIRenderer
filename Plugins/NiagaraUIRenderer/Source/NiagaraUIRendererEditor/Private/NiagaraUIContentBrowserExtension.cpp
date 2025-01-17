// Copyright (c) 2021 Michal Smoleň 


#include "NiagaraUIContentBrowserExtension.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "NiagaraUIRendererEditorStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "IContentBrowserSingleton.h"
#include "AssetTypeActions_JJYYMaterialInstanceConstant.h"


#define LOCTEXT_NAMESPACE "NiagaraUIRenderer"


static FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
static FDelegateHandle ContentBrowserExtenderDelegateHandle;


struct FContentBrowserSelectedAssetExtensionBase
{
public:
	TArray<struct FAssetData> SelectedAssets;

public:
	virtual void Execute() {}
	virtual ~FContentBrowserSelectedAssetExtensionBase() {}
};

#include "IAssetTools.h"
#include "AssetToolsModule.h"

struct FCreateNiagaraUIMaterialsExtension : public FContentBrowserSelectedAssetExtensionBase
{
	void CreateNiagaraUIMaterials(TArray<UMaterial*>& Materials)
	{		
        FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        const FString DefaultSuffix = TEXT("_JJYYInst");

        
		
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Materials.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt);
			if (Object && Object->MaterialDomain == EMaterialDomain::MD_UI)
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
                AssetToolsModule.Get().CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UJJYYMaterialInstanceConstantFactory* Factory = NewObject<UJJYYMaterialInstanceConstantFactory>();
				Factory->InitialParent = Object;

				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UJJYYMaterialInstanceConstant::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}
        if (ObjectsToSync.Num() > 0)
        {
            ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
        }
	}

	virtual void Execute() override
	{
		TArray<UMaterial*> Materials;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (UMaterial* Material = Cast<UMaterial>(AssetData.GetAsset()))
			{
				Materials.Add(Material);
			}
		}

		CreateNiagaraUIMaterials(Materials);
	}
};

class FNiagaraUIContentBrowserExtension_Impl
{
public:
	static void ExecuteSelectedContentFunctor(TSharedPtr<FContentBrowserSelectedAssetExtensionBase> SelectedAssetFunctor)
	{
		SelectedAssetFunctor->Execute();
	}

	static void CreateNiagaraUIFunctions(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		TArray<UMaterial*> Materials;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (UMaterial* Material = Cast<UMaterial>(AssetData.GetAsset()))
			{
				Materials.Add(Material);
			}
		}

		TSharedPtr<FCreateNiagaraUIMaterialsExtension> NiagaraUIMaterialsFunctor = MakeShareable(new FCreateNiagaraUIMaterialsExtension());
		NiagaraUIMaterialsFunctor->SelectedAssets = SelectedAssets;

		FUIAction ActionCreateNiagaraUIMaterial(FExecuteAction::CreateStatic(&FNiagaraUIContentBrowserExtension_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(NiagaraUIMaterialsFunctor)));
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NiagaraUIRenderer_CreateUIMaterial", "Create Niagara UI Material"),
			LOCTEXT("NiagaraUIRenderer_CreateUIMaterialTooltip", "Creates a new version of this material that is suitable to be used for the Niagara UI Particles. Be aware that manual inspection of this material is recommended."),
			FSlateIcon(FNiagaraUIRendererEditorStyle::GetStyleSetName(), "NiagaraUIRendererEditorStyle.ParticleIcon"),
			ActionCreateNiagaraUIMaterial,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		bool AnyMaterials = false;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			AnyMaterials = AnyMaterials || (Asset.AssetClass == UMaterial::StaticClass()->GetFName());
		}

		if (AnyMaterials)
		{
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FNiagaraUIContentBrowserExtension_Impl::CreateNiagaraUIFunctions, SelectedAssets));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	}
};

void FNiagaraUIContentBrowserExtension::InstallHooks()
{
	ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FNiagaraUIContentBrowserExtension_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FNiagaraUIContentBrowserExtension_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FNiagaraUIContentBrowserExtension::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FNiagaraUIContentBrowserExtension_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate){ return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

#undef LOCTEXT_NAMESPACE
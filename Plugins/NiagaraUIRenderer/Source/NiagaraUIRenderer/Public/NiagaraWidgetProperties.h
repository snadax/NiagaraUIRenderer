// Copyright 2021 - Michal Smoleň

#pragma once

class UMaterialInterface;

struct FNiagaraWidgetProperties
{
	FNiagaraWidgetProperties();
	FNiagaraWidgetProperties(bool inAutoActivate, bool inShowDebugSystem, bool inFakeDepthScale, float inFakeDepthDistance)
        : AutoActivate(inAutoActivate), ShowDebugSystemInWorld(inShowDebugSystem), FakeDepthScale(inFakeDepthScale), FakeDepthScaleDistance(inFakeDepthDistance) {}
	
	bool AutoActivate = true;
	bool ShowDebugSystemInWorld = false;
	bool FakeDepthScale = false;
	float FakeDepthScaleDistance = 1000.f;
};

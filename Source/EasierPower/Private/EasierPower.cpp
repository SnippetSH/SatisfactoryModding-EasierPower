// Copyright Epic Games, Inc. All Rights Reserved.

#include "EasierPower.h"

#include "KismetAnimationLibrary.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturerVariablePower.h"
#include "Kismet/KismetMathLibrary.h"
#include "Patching/NativeHookManager.h"

#define LOCTEXT_NAMESPACE "FEasierPowerModule"

namespace MySpace {
	constexpr float SLOOP_FACTOR = 1.618f;

	/*make 1.0~2.5 Overclock to 1.0~2.0*/
	float CompressOC(float oc) {
		if (oc >= 1.0f) {
			float tmp_oc = oc - 1.0f;
			tmp_oc = tmp_oc / 1.5f; //tmp_oc range is 0.0 ~ 1.0.
			tmp_oc += 1.0f;

			oc = tmp_oc;
		}

		return oc;
	}

	float PowerCurve(float p) {
		if (p < 0.4f) {
			// 0~4%: 0 → 0.95 선형 상승
			return (p / 0.4f) * 0.95f;
		}
		else if (p < 0.6f) {
			// 40~60%: 0.95 중심으로 sin 한 주기 (90~100 출렁)
			float t = (p - 0.40f) / 0.20f;          // 구간 진행도 0~1
			return 0.95f + 0.05f * FMath::Sin(t * 2.0f * PI);
			// t=0 → 0.95, t=0.25 → 1.0, t=0.5 → 0.95, t=0.75 → 0.90, t=1 → 0.95
		}
		else {
			// 60~100%: 0.95 → 0 선형 하강
			return 0.95f - ((p - 0.6f) / 0.4f) * 0.95f;
		}
	}
}

void FEasierPowerModule::StartupModule()
{
	if (!WITH_EDITOR) {
		SUBSCRIBE_METHOD(AFGBuildableFactory::GetProducingPowerConsumption, [](auto& Scope, const AFGBuildableFactory* Self) {
			if (!Self->IsProducing()) return;

			float sloop = FMath::Pow(Self->GetCurrentProductionBoost(), MySpace::SLOOP_FACTOR);

			if (auto variable_pm = Cast<AFGBuildableManufacturerVariablePower>(Self)) {
				auto recipe = variable_pm->GetCurrentRecipe().GetDefaultObject();
				if (!recipe) return;

				float oc = MySpace::CompressOC(variable_pm->GetCurrentPotential());

				auto min_consume = recipe->GetPowerConsumptionConstant() * oc;
				auto max_consume = (recipe->GetPowerConsumptionConstant() + recipe->GetPowerConsumptionFactor()) * oc;

				float progress = Self->GetProductionProgress();
				float curve_val = MySpace::PowerCurve(progress);
				auto current_usage = min_consume + (max_consume - min_consume) * curve_val;

				Scope.Override(FMath::Max(0.1f, current_usage * sloop));
			}
			else {
				float oc = MySpace::CompressOC(Self->GetCurrentPotential());

				Scope.Override(Self->GetDefaultProducingPowerConsumption() * oc * sloop);
			}
			});

		SUBSCRIBE_METHOD(AFGBuildableManufacturerVariablePower::GetMinPowerConsumption, [](auto& scope, const AFGBuildableManufacturerVariablePower* self) {
			auto recipe = self->GetCurrentRecipe().GetDefaultObject();
			if (!recipe) return;

			float oc = MySpace::CompressOC(self->GetCurrentPotential());

			float sloop = FMath::Pow(self->GetCurrentProductionBoost(), MySpace::SLOOP_FACTOR);
			auto output = recipe->GetPowerConsumptionConstant() * oc * sloop;
			scope.Override(output);
			});

		SUBSCRIBE_METHOD(AFGBuildableManufacturerVariablePower::GetMaxPowerConsumption, [](auto& scope, const AFGBuildableManufacturerVariablePower* self) {
			auto recipe = self->GetCurrentRecipe().GetDefaultObject();
			if (!recipe) return;

			float oc = MySpace::CompressOC(self->GetCurrentPotential());

			float sloop = FMath::Pow(self->GetCurrentProductionBoost(), MySpace::SLOOP_FACTOR);
			auto output = (recipe->GetPowerConsumptionConstant() + recipe->GetPowerConsumptionFactor()) * oc * sloop;
			scope.Override(output);
			});
	}
}

void FEasierPowerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEasierPowerModule, EasierPower)
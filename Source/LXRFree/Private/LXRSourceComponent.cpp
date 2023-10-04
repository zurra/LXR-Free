/*
 *MIT License*

Copyright (c) 2023 Clusterfact Games

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "LXRSourceComponent.h"
#include "LXRFunctionLibrary.h"
#include "LXRSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
ULXRSourceComponent::ULXRSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	if (!bAlwaysRelevant)
	{
		PrimaryComponentTick.bCanEverTick = true;
		PrimaryComponentTick.bStartWithTickEnabled = true;
		SetComponentTickInterval(1.0f);
	}

	// ...
}

void ULXRSourceComponent::TickComponent(float DeltaTime, ELevelTick Tick, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, Tick, ThisTickFunction);
	constexpr float Mpl = 2;
	const float SpeedPercent = FMath::Abs(FMath::Max(1.f, GetOwner()->GetVelocity().Size()) / 150 - 1) * Mpl;
	SetComponentTickInterval(FMath::Clamp(SpeedPercent, 0.15f, Mpl));
}


bool ULXRSourceComponent::IsEnabled() const
{
	if (bDisable) return false;

	for (const ULightComponent* LightComponent : GetMyLightComponents())
	{
		if (IsLightComponentEnabled(LightComponent))
			return true;
	}


	return false;
}


bool ULXRSourceComponent::IsLightComponentEnabled(const ULightComponent* LightComponent) const
{
	return LightComponent->IsVisible();
}

TArray<AActor*> ULXRSourceComponent::GetIgnoreVisibilityActors_Implementation()
{
	return IgnoreVisibilityActors;
}


FLinearColor ULXRSourceComponent::GetLightComponentColor(const ULightComponent& LightComponent)
{
	FLinearColor LightColor;
	if (LightComponent.bUseTemperature)
	{
		LightColor = FLinearColor::MakeFromColorTemperature(LightComponent.Temperature);
		LightColor *= LightComponent.GetLightColor();
	}
	else
		LightColor = LightComponent.GetLightColor();

	return LightColor;
}


FLinearColor ULXRSourceComponent::GetCombinedColors()
{
	TArray<FLinearColor> CombinedLightColors;
	for (auto It = GetMyLightComponents().CreateConstIterator(); It; ++It)
	{
		const ULightComponent* LightComponent = *It;
		FLinearColor LightColor;
		LightColor = GetLightComponentColor(*LightComponent);
		CombinedLightColors.Add(LightColor);
	}

	return ULXRFunctionLibrary::GetLinearColorArrayAverage(CombinedLightColors);
}

FLinearColor ULXRSourceComponent::GetCombinedColorsByComponents(const TArray<ULightComponent*>& LightComponents)
{
	TArray<FLinearColor> CombinedLightColors;
	for (const ULightComponent* const LightComponent : LightComponents)
	{
		FLinearColor LightColor;
		LightColor = GetLightComponentColor(*LightComponent);
		CombinedLightColors.Add(LightColor);
	}

	return ULXRFunctionLibrary::GetLinearColorArrayAverage(CombinedLightColors);
}

FLinearColor ULXRSourceComponent::GetCombinedColorsByComponentIndices(const TArray<int>& Indices)
{
	TArray<FLinearColor> CombinedLightColors;
	for (const int Idx : Indices)
	{
		FLinearColor LightColor;
		LightColor = GetLightComponentColor(*GetMyLightComponents()[Idx]);
		CombinedLightColors.Add(LightColor);
	}

	return ULXRFunctionLibrary::GetLinearColorArrayAverage(CombinedLightColors);
}


// Called when the game starts
void ULXRSourceComponent::BeginPlay()
{
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypeQueries;
	ObjectTypeQueries.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	UKismetSystemLibrary::SphereOverlapActors(this, GetOwner()->GetActorLocation(), 30.f, ObjectTypeQueries,NULL, {}, MyOverlappingActors);
	FindMyLightComponents();

	for (const auto Component : MyLightComponents)
	{
		if (!bAlwaysRelevant)
			bAlwaysRelevant = Component->IsA(UDirectionalLightComponent::StaticClass()) ? true : bAlwaysRelevant;
	}

	RegisterLight();

	Super::BeginPlay();
}

void ULXRSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeRegisterLight();
	Super::EndPlay(EndPlayReason);
}

void ULXRSourceComponent::DestroyComponent(bool bPromoteChildren)
{
	DeRegisterLight();
	Super::DestroyComponent(bPromoteChildren);
}


void ULXRSourceComponent::RegisterLight()
{
	ULXRSubsystem* LightDetectionSubsystem = GetOwner()->GetWorld()->GetSubsystem<ULXRSubsystem>();
	if (IsValid(LightDetectionSubsystem))
		LightDetectionSubsystem->RegisterLight(Cast<AActor>(GetOwner()));

}


void ULXRSourceComponent::DeRegisterLight() const
{
	ULXRSubsystem* LightDetectionSubsystem = GetOwner()->GetWorld()->GetSubsystem<ULXRSubsystem>();
	if (IsValid(LightDetectionSubsystem))
		LightDetectionSubsystem->UnregisterLight(GetOwner());
}

TArray<AActor*>& ULXRSourceComponent::GetMyOverlappingActors()
{
	return MyOverlappingActors;
}

TArray<ULightComponent*> ULXRSourceComponent::GetMyLightComponents() const
{
	return MyLightComponents;
}

void ULXRSourceComponent::FindMyLightComponents()
{
	GetOwner()->GetComponents<ULightComponent>(MyLightComponents);
	for (FComponentReference ExcludedLightComponent : ExcludedLights)
	{
		for (int i = MyLightComponents.Num() - 1; i >= 0; --i)
		{
			if (MyLightComponents[i] == ExcludedLightComponent.GetComponent(GetOwner()))
			{
				MyLightComponents.RemoveAt(i);
			}
		}
	}
}

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

#pragma once

#include "CoreMinimal.h"
#include "LXRSubsystem.h"
#include "Components/ActorComponent.h"
#include "LXRSourceComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LXRFREE_API ULXRSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULXRSourceComponent();

	//Should LightDetection component show debug about this light source actor.
	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category="LXR|Source|Debug", meta=(AdvancedDisplay))
	bool bDrawDebug = false;
	
	//Disables detection from all other non-solo LXR Sources
	UPROPERTY(EditAnywhere, Category="LXR|Source|Debug", meta=(AdvancedDisplay))
	bool bSolo = false;
	
	//Disables component
	UPROPERTY(EditAnywhere, Category="LXR|Source|Debug", meta=(AdvancedDisplay))
	bool bDisable = false;

	
	//Should this light source actor be always relevant to Light Detection.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	bool bAlwaysRelevant = false;

	//Add detected actors to list. DetectionComponent bAddToSourceWhenDetected needs to be true.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	bool bAddDetected = false;

	//Attenuation multiplier to be relevant.
	//Light is relevant if Actor is closer than Attenuation * AttenuationMultiplierToBeRelevant.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	float AttenuationMultiplierToBeRelevant = 1;

	//LXR multiplier.
	//Multiplier to add to final LXR.
	//Added to all LightComponents unless overriden with LightLXRMultipliers variable.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	float LXRMultiplier = 1;

	//LXR color multiplier.
	//Multiplier to add to final LXR color.
	//Added to all LightComponents unless overriden with LightLXRColorMultipliers variable.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	float LXRColorMultiplier = 1;

	//LXR Intensity multiplier per LightComponent.
	//Overrides LXR Multiplier for light contained in array.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	TArray<FLightSourceData> LightLXRMultipliers;

	//LXR Color multiplier per LightComponent.
	// Overrides LXR Color Multiplier for light contained in array.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Source")
	TArray<FLightSourceData> LightLXRColorMultipliers;

	//List of LightComponents to exclude from LightDetection, Only used when GetMyLightComponentsMethodToUse is set to Class.
	UPROPERTY(EditAnywhere, Category="LXR|Source", meta=(UseComponentPicker, AllowedClasses="LightComponent", EditCondition = "GetMyLightComponentsMethodToUse == EMethodToUse::Class"))
	TArray<FComponentReference> ExcludedLights;
	
	//List of Detected actors.
	UPROPERTY(BlueprintReadWrite, Category="LXR|Source")
	TArray<AActor*> DetectedActors;

	//List of actors to ignore when checking visibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LXR|Source")
	TArray<AActor*> IgnoreVisibilityActors;

	//IF owner does not implement ILightSource::IsEnabled, then use this function to determine if light source is enabled.
	UFUNCTION(BlueprintPure, Category="LXR|Source")
	bool IsEnabled() const;

	//IF owner does not implement ILightSource::IsLightComponentEnabled, then use this function to determine if light component is enabled.
	UFUNCTION(BlueprintPure, Category="LXR|Source")
	bool IsLightComponentEnabled(const ULightComponent* LightComponent) const;
	
	UFUNCTION(BlueprintPure, Category="LXR|Source")
	FLinearColor GetCombinedColors();
	
	UFUNCTION(BlueprintPure, Category="LXR|Source")
	FLinearColor GetCombinedColorsByComponents(const TArray<ULightComponent*>& LightComponents);
	
	UFUNCTION(BlueprintPure, Category="LXR|Source")
	FLinearColor GetCombinedColorsByComponentIndices(const TArray<int>& Indices);

	//Get actors to ignore when checking visibility.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="LXR|Source")
	TArray<AActor*> GetIgnoreVisibilityActors();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DestroyComponent(bool bPromoteChildren) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FLinearColor GetLightComponentColor(const ULightComponent& LightComponent);

public:
	void RegisterLight();
	void DeRegisterLight() const;
	TArray<ULightComponent*> GetMyLightComponents() const;
	TArray<AActor*>& GetMyOverlappingActors();


protected:
	UPROPERTY(BlueprintReadOnly, Category="LXR|Source")
	TArray<ULightComponent*> MyLightComponents;

	UPROPERTY()
	TArray<AActor*> MyOverlappingActors;

	void FindMyLightComponents();

};

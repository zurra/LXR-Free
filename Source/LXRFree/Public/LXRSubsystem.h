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
#include "Subsystems/WorldSubsystem.h"
#include  "LXRFree.h"
#include "LXRSubsystem.generated.h"

DECLARE_EVENT_OneParam(ULightDetectionSubsystem, FOnLightAdded, AActor*);

DECLARE_EVENT_OneParam(ULightDetectionSubsystem, FOnLightRemoved, AActor*);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Traces in second (Sync)"), STAT_TRACESSYNC, STATGROUP_LXR);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Traces in second (Multithread)"), STAT_TRACESMULTITHREAD, STATGROUP_LXR);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Tasks in second (Multithread)"), STAT_THREADS, STATGROUP_LXR);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("LightSense TraceTarget Traces"), STAT_TRACELIGHTSENSETARGETS, STATGROUP_LXR);

DECLARE_DWORD_COUNTER_STAT(TEXT("Relevant Lights"), STAT_RELEVANTLIGHTS, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("Passed Relevant Lights"), STAT_PASSEDRELEVANTLIGHTS, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("All Lights"), STAT_ALLLIGHTS, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("Smart Near"), STAT_SMARTNEAR, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("Smart Mid"), STAT_SMARTMID, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("Smart Far"), STAT_SMARTFAR, STATGROUP_LXR);
DECLARE_DWORD_COUNTER_STAT(TEXT("Light Sense"), STAT_LIGHTSENSE, STATGROUP_LXR);

DECLARE_CYCLE_STAT(TEXT("Relevant Check"), STAT_RelevantCheck, STATGROUP_LXR);
DECLARE_CYCLE_STAT(TEXT("Relevancy Check"), STAT_RelevancyCheck, STATGROUP_LXR);
DECLARE_CYCLE_STAT(TEXT("Get Combined Datas"), STAT_GetCombinedDatas, STATGROUP_LXR);
DECLARE_CYCLE_STAT(TEXT("Light Sense Check"), STAT_LightSenseCheck, STATGROUP_LXR);


USTRUCT(BlueprintType)
struct LXRFREE_API FLightSourceData
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere,Category="LXR|Source", meta=(UseComponentPicker,AllowedClasses="LightComponent"))
	FComponentReference  LightComponent;

	UPROPERTY(EditAnywhere,Category="LXR|Source")
	float LightData;
};

USTRUCT(BlueprintType)
struct LXRFREE_API FLightSourcePassedData
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere,Category="LXR|Source", meta=(UseComponentPicker,AllowedClasses="LightComponent"))
	TWeakObjectPtr<AActor>  LightSourceActor;

	UPROPERTY(EditAnywhere,Category="LXR|Source")
	TArray<int> PassedComponents;
};

/**
 * 
 */
UCLASS()
class LXRFREE_API ULXRSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	FOnLightAdded OnLightAdded;
	FOnLightRemoved OnLightRemoved;

	//Registers new light source for LXR
	UFUNCTION(BlueprintCallable,Category="LXR")
	void RegisterLight(AActor* LightSource);
	
	//Removes light source from LXR
	UFUNCTION(BlueprintCallable,Category="LXR")
	void UnregisterLight(AActor* LightSource);

	const TArray<TWeakObjectPtr<AActor>>& GetAllLights() const;

	bool bSoloFound;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> LightSources;


};


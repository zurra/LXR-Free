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
#include "Components/ActorComponent.h"
#include "CollisionQueryParams.h"
#include "LXRDetectionComponent.generated.h"

class ULXRSubsystem;
class ULXRSourceComponent;

// DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLightCheckChanged, int, PassedCount, ULXRSourceComponent*, LightSourceComponent);

UENUM()
enum class ELightArrayType
{
	All,
	SmartFar,
	SmartMid,
	SmartNear,
	Relevant
};

UENUM()
enum class ETraceTarget
{
	None UMETA(HIDDEN),
	// Owner Location.
	ActorLocation UMETA(DisplayName = "Actor Location"),
	// Bones or sockets to use from skeletal mesh.
	Sockets UMETA(DisplayName = "SkeletalMesh Sockets"),
	// TargetVectors property to use as targets.
	VectorArray UMETA(DisplayName = "Vector Array"),
	// Four vectors generated from actor bounds. (Approximate of Toes, Head, Sides)  
	ActorBounds UMETA(DisplayName = "Actor Bounds"),
};


UENUM()
enum class ERelevancyCheckType
{
	//Use property RelevancyCheckRate as check rate.
	Fixed UMETA(DisplayName = "Fixed"),

	// Calculate rate from distance to light.
	// Lights will be categorized in three different category:
	// Near: Distance to light is less than RelevancySmartDistanceMin, light will be checked 4 times in second.
	// Mid: Distance to light is between RelevancySmartDistanceMin and RelevancySmartDistanceMax, light will be checked twice in second.
	// Far: Distance to light is more than RelevancySmartDistanceMax, light will be checked every second.
	Smart UMETA(DisplayName = "Smart"),
};

UENUM(BlueprintType)
enum class ERelevantTraceType : uint8
{
	// Use Synchronous LineTrace for relevant light visibility checks
	Sync UMETA(DisplayName = "Synchronous LineTrace"),
};

/*Component for detecting light emitted by actors with LXRLightSource component. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LXRFREE_API ULXRDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULXRDetectionComponent();

	//Render debug shapes, also needs to be enabled on Light Source Component.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Debug")
	bool bDrawDebug = false;

	//Print debug messages, if bDrawDebug is enabled on Light Source Component.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Debug")
	bool bPrintDebug = false;

	//Debug Render Relevant lights & Passed lights 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Debug")
	bool bDebugRelevantAndPassed = false;

	UPROPERTY(EditAnywhere, Category="LXR|Detection|Debug")
	float DebugDrawTime = 0.1f;

	//Draw debug sphere for each VectorArray position.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Debug")
	bool bDebugVectorArray = false;

	//Bones or sockets to use as detection targets 
	UPROPERTY(EditAnywhere, Category="LXR")
	TArray<FName> TargetSockets;

	//Detection targets in local space to use for VectorArray type.
	UPROPERTY(EditAnywhere, Category="LXR")
	TArray<FVector> TargetVectors;

	//This actor can be added to source component DetectedActors list.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR")
	bool bAddToSourceWhenDetected = false;

	//Gets illuminated targets and their LXR value.
	// Causes some extra calculations.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR")
	bool bGetIlluminatedTargets = false;

	//Method to use for checking if light is relevant
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy")
	ERelevancyCheckType RelevancyCheckType = ERelevancyCheckType::Smart;

	//Min distance after which check rate will be reduced.  
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType == ERelevancyCheckType::Smart"))
	float RelevancySmartDistanceMin = 1000;

	//Max distance after which check rate will be greatly reduced.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType == ERelevancyCheckType::Smart"))
	float RelevancySmartDistanceMax = 6000;
	
    //Max distance to check if Directional Light sees owner.
    UPROPERTY(EditAnywhere, Category="LXR|Detection|Directional Light")
    float DirectionalLightTraceDistance = 10000;

	//Divider for Smart check rate.
	// Near check rate is 5/RelevancySmartCheckRateDivider in second
	// Mid: light will be 2/RelevancySmartCheckRateDivider twice in second.
	// Far: light will be 1/RelevancySmartCheckRateDivider every second.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType == ERelevancyCheckType::Smart", ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10"))
	float RelevancySmartCheckRateDivider = 1.f;

	//Rate for checking for new relevant lights.  
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType == ERelevancyCheckType::Fixed || RelevancyCheckType == ERelevancyCheckType::Octree"))
	float RelevancyCheckRate = 0.05f;

	//Bounds size to check for nearby lights.  
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType == ERelevancyCheckType::Octree", ClampMin = "1000", ClampMax = "10000", UIMin = "1000", UIMax = "10000"))
	int RelevancyOctreeCheckBoundsSize = 2000;

	//How many lights we check for relevancy per check.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Relevancy")
	int RelevancyLightBatchCount = 100;

	//Use location difference for relevancy checks.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, EditCondition = "RelevancyCheckType != ERelevancyCheckType::Octree"))
	bool bUseLocationChange = false;

	//Difference from last relevancy check.  
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta=(HideEditConditionToggle, ClampMin = "100", ClampMax = "1000", UIMin = "100", UIMax = "1000", EditCondition = "bUseDistanceChange == true"))
	int RelevancyLocationThreshold = 200;

	//Target type to use for relevancy detection.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy")
	ETraceTarget RelevancyTargetType = ETraceTarget::ActorLocation;

	//Percentage of how many targets needs to pass relevancy check for light to be relevant. 
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevancy", meta = (DisplayName="Passed Targets Required", ClampMin = "0.1", ClampMax = "1", UIMin = "0.1", UIMax = "1", HideEditConditionToggle, EditCondition = "RelevancyTargetType != ETraceTarget::ActorLocation"))
	float TargetsRequired = 0.5f;

	//Target type to use for detection.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevant")
	ETraceTarget RelevantTargetType = ETraceTarget::ActorBounds;

	//Rate for checking if relevant light illuminates Actor.
	// Used for Sync, ParallelFor, Multithread RelevantTraceTypes
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevant")
	float RelevantLightCheckRate = 0.01;

	//How many relevant lights we process per check.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="LXR|Detection|Relevant")
	int RelevantLightBatchCount = 50;

	//Trace channel to use for traces.
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevant")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	//Percentage of how many traces needs to be passed (not hit) to be lit. 
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevant", meta=(DisplayName="Passed Targets Required", ClampMin = "0.1", ClampMax = "1", UIMin = "0.1", UIMax = "1", HideEditConditionToggle, EditCondition = "RelevantTargetType != ETraceTarget::ActorLocation"))
	float TracesRequired = 0.5f;

	//How many  times in row relevant check must fail to remove light from relevant list. 
	UPROPERTY(EditAnywhere, Category="LXR|Detection|Relevant")
	float MaxConsecutiveFails = 5;

	// UPROPERTY(BlueprintAssignable, Category="LXR|Detection|Relevant")
	// FOnLightCheckChanged OnLightCheckChanged;

	UPROPERTY(BlueprintReadOnly, Category="LXR|Detection|Passed")
	FLinearColor CombinedLXRColor;
	UPROPERTY(BlueprintReadOnly, Category="LXR|Detection|Passed")
	float CombinedLXRIntensity;

	//List of actors to ignore when checking visibility.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LXR|Detection")
	TArray<AActor*> IgnoreVisibilityActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LXR|Detection")
	TMap<int, FLinearColor> IlluminatedTargets;

	UFUNCTION(BlueprintPure, Category="LXR|Detection|TraceTargets")
	TArray<FVector> GetRelevantTraceTypeTargets() const;

	UFUNCTION(BlueprintPure, Category="LXR|Detection|PassedLights")
	TArray<AActor*> GetPassedLights() const;
	
	UFUNCTION(BlueprintPure, Category="LXR|Detection|PassedLights")
	TArray<ULightComponent*> GetPassedLightComponents(AActor* LightSourceOwner) ;

	TArray<FVector> GetTraceTargets(const bool& bIsRelevant, const ETraceTarget TargetOverride = ETraceTarget::None) const;

	bool GetIsRelevant(const ULXRSourceComponent& LightSourceComponent) const;

private:
	friend class ULXRAISightDetectionComponent;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void GetLightSystemLights();

	void AddLight(AActor* LightSource);
	void RemoveLight(AActor* LightSource);
	void CheckAllLightForRelevancy();
	void CheckRelevantLights();
	void IncreaseFailCount(const TWeakObjectPtr<AActor>& LightSourceOwner);
	void IncreaseFailCountIfNotAlwaysRelevantLightFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner);
	void CheckAndRemoveIfLightNotRelevant(const TWeakObjectPtr<AActor>& LightSourceOwner, bool IsFromThread = false);
	void CheckAndRemoveIfLightNotRelevantFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner);

	void AddNewLights();
	void RemoveNonRelevantLights();
	void AddLightToNewRelevantList(const TWeakObjectPtr<AActor>& LightSourceOwner);
	void AddNewRelevantLights();
	void RemoveRedundantLights();
	void RemoveAllStaleLights();
	void RemoveStaleLightsByLightArrayType(ELightArrayType LightArrayType);
	void LightPassed(const TWeakObjectPtr<AActor>& LightSourceOwner, const TArray<int>& PassedComponents);
	void LightPassedFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner, const TArray<int>& PassedComponents);
	void RemovePassedLight(const TWeakObjectPtr<AActor>& LightSourceOwner);
	void ChangeSmartLightArray(const ELightArrayType& From, const ELightArrayType& To, const TWeakObjectPtr<AActor>& LightSourceOwner);
	void GetLXR();

	bool CheckDirectionalLight(const ULightComponent& LightSourceComponent, const FVector& Start) const;
	bool CheckDistance(const ULXRSourceComponent& LightSourceComponent, const ULightComponent& LightComponent, const FVector& Start, const FVector& End) const;
	bool CheckDistance(const ULXRSourceComponent& LightSourceComponent) const;
	bool CheckAttenuation(const ULightComponent& LightComponent, const FVector& Start, const FVector& End, bool IsRect) const;
	bool CheckDirection(const ULightComponent& LightComponent, const FVector& Start, const FVector& End) const;
	bool CheckVisibility(const TArray<ULightComponent*>& LightComponents, const TArray<int>& PassedComponents, TArray<int>& PassedTargets, bool IsLightSenseCheck = false);
	bool CheckIfInsideSpotOrRect(const ULightComponent& LightComponent, const FVector& Start, const FVector& End, bool IsSpot) const;
	bool CheckIsLightRelevant(const ULXRSourceComponent& LightSourceComponent, TArray<int>& PassedComponents, TArray<int>& PassedTargets, bool IsLightSenseCheck = false, bool IsFromThread = false) const;

	void GetNextBatchByLightArrayType(TArray<TWeakObjectPtr<AActor>>& OutLightBatch, ELightArrayType LightArrayType);
	void GetNextRelevantCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& OutLightBatch);

	void ProcessRelevantCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& LightBatch, bool IsLightSenseCheck = false);
	void ProcessRelevancyCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& LightBatch, ELightArrayType LightArrayType);

	void DoRelevantCheckOnSourceActor(const TWeakObjectPtr<AActor>& LightSourceComponentOwner, bool IsFromThread, bool IsLightSenseCheck = false);

	void AddToSmartArrayBySmartArrayType(ELightArrayType LightArrayType, AActor& LightSourceActor);

	int GetCurrentLightArrayIndexByLightArrayType(const ELightArrayType LightArrayType) const;
	void SetCurrentLightArrayIndexByLightArrayType(int InIndex, const ELightArrayType LightArrayType);

	TArray<TWeakObjectPtr<AActor>>& GetLightArrayByLightArrayType(ELightArrayType LightArrayType);

	ELightArrayType GetSmartArrayTypeForLight(const FVector& Start, const FVector& End) const;
	ELightArrayType GetSmartArrayTypeForLightFromSqrDistance(const float& SqrDist) const;

	FCollisionQueryParams GetCollisionQueryParams(const TArray<AActor*>& ActorsToIgnore) const;

	ULXRSourceComponent* GetCurrentLightSourceComponentByType(const ELightArrayType LightArrayType) const;
	ULightComponent* GetCurrentLightComponentByType(const ELightArrayType LightArrayType) const;
	ULXRSourceComponent* GetLightSourceComponentByTypeAndIndex(const ELightArrayType LightArrayType, int Index) const;

	UPROPERTY()
	ULXRSubsystem* LXRSubsystem;

	bool bStop = false;
	bool bUpdateOctreeLights = false;
	bool LastFrameDrawDebug = false;

	int SmartFarLightIndex = 0;
	int SmartMidLightIndex = 0;
	int SmartNearLightIndex = 0;
	int RelevancyLightIndex = 0;
	int RelevantLightIndex = 0;
	int AllLightSourceLightActorComponentIndex = 0;
	int RelevantLightSourceActorLightComponentIndex = 0;

	float NearSmartTimer = 0;
	float MidSmartTimer = 0;
	float FarSmartTimer = 0;
	float StatResetTimer = 0;
	float LightSenseTimer = 0;
	float GetCombinedDatasTimer = 0;

	mutable FRWLock RelevantDataLockObject;

	FTimerHandle CheckAllLightsTimerHandle;
	FTimerHandle CheckRelevantLightsTimerHandle;
	FBoxCenterAndExtent OctreeBoundsTestObject;

	FVector LastRelevancyUpdateLocation;

	TArray<FLinearColor> CombinedLightColors;
	TMap<int, TArray<FLinearColor>> TargetsCombinedLightColors;

	TMap<int, FLinearColor> TargetsCombinedLXRColor;
	TMap<int, float> TargetsCombinedLXRIntensity;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RelevantLights;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RelevantLightsPassed;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> NewRelevantLightsToAdd;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RelevantLightsToRemove;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> AllLights;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartNearLights;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartMidLights;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartFarLights;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> NewAllLightsToAdd;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> LightsToRemove;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ErrorAlreadyThrownFromActor;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartFarLightsToRemove;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartMidLightsToRemove;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartNearLightsToRemove;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartFarLightsToAdd;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartMidLightsToAdd;
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SmartNearLightsToAdd;

	TMap<TWeakObjectPtr<AActor>, TArray<int>> LightsPassedComponents;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, int> RelevantLightsFailCounts;

	UPROPERTY()
	USkeletalMeshComponent* SkeletalMeshComponent;
};

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

#include "LXRDetectionComponent.h"
#include  "LXRFree.h"
#include "LXRSourceComponent.h"
#include "LXRSubsystem.h"
#include "Components/LocalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "engine/World.h"
#include "DrawDebugHelpers.h"
#include "LXRFunctionLibrary.h"

// Sets default values for this component's properties
ULXRDetectionComponent::ULXRDetectionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void ULXRDetectionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);

	if (RelevancyTargetType == ETraceTarget::Sockets || RelevantTargetType == ETraceTarget::Sockets)
	{
		if (TargetSockets.Num() > 0)
		{
			if (SkeletalMeshComponent == NULL)
			{
				UActorComponent* ActorComponent = GetOwner()->GetComponentByClass(USkeletalMeshComponent::StaticClass());
				if (ActorComponent)

					SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorComponent);
			}
			ensureMsgf(SkeletalMeshComponent, TEXT("SkeletalMeshComponent does not exists on %s"), *GetOwner()->GetName());
		}
	}

	SetComponentTickInterval(RelevantLightCheckRate);

	//Lazy "late begin play"
	FTimerHandle Temp;
	LXRSubsystem = GetOwner()->GetWorld()->GetSubsystem<ULXRSubsystem>();
	GetWorld()->GetTimerManager().SetTimer(Temp, FTimerDelegate::CreateLambda([&]
	{
		GetLightSystemLights();
		LXRSubsystem->OnLightAdded.AddUObject(this, &ULXRDetectionComponent::AddLight);
		LXRSubsystem->OnLightRemoved.AddUObject(this, &ULXRDetectionComponent::RemoveLight);

		for (int i = 0; i < AllLights.Num(); ++i)
		{
			if (Cast<ULXRSourceComponent>(AllLights[i]->GetComponentByClass(ULXRSourceComponent::StaticClass()))->bAlwaysRelevant)
			{
				RelevantLights.Add(AllLights[i]);
			}
		}

		switch (RelevancyCheckType)
		{
			case ERelevancyCheckType::Fixed:
				{
					CheckAllLightForRelevancy();
					break;
				}
			case ERelevancyCheckType::Smart:
				{
					for (int i = 0; i < AllLights.Num(); ++i)
					{
						const ELightArrayType SmartArrayType = GetSmartArrayTypeForLightFromSqrDistance(FVector::DistSquared(AllLights[i].Get()->GetActorLocation(), GetOwner()->GetActorLocation()));
						AddToSmartArrayBySmartArrayType(SmartArrayType, *AllLights[i]);
					}
					break;
				}
			default: ;
		}

		SetComponentTickEnabled(true);
	}), 0.1f, false);

	GetWorld()->GetTimerManager().SetTimer(CheckAllLightsTimerHandle, FTimerDelegate::CreateUObject(this, &ULXRDetectionComponent::CheckAllLightForRelevancy), RelevancyCheckRate, true, 0.15);

#if UE_ENABLE_DEBUG_DRAWING
	if (bDebugVectorArray)
	{
		for (FVector TargetVector : TargetVectors)
		{
			DrawDebugSphere(GetWorld(), GetOwner()->GetActorTransform().TransformPosition(TargetVector), 10, 12, FColor::Green, false, 5.f);
		}
	}
#endif
	if (bGetIlluminatedTargets)
	{
		TArray<FVector> TraceTargets = GetTraceTargets(true);
		for (int i = 0; i < TraceTargets.Num(); ++i)
		{
			IlluminatedTargets.Add(i);
			TargetsCombinedLXRIntensity.Add(i);
			TargetsCombinedLXRColor.Add(i);
			TargetsCombinedLightColors.Add(i);
		}
	}
}

void ULXRDetectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LXRSubsystem->OnLightAdded.RemoveAll(this);
	LXRSubsystem->OnLightRemoved.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ULXRDetectionComponent::TickComponent(float DeltaTime, ELevelTick Tick, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, Tick, ThisTickFunction);

	if (LXRSubsystem->bSoloFound)
		GEngine->AddOnScreenDebugMessage(50, GetComponentTickInterval(), FColor::Red, FString::Printf(TEXT("SOLO LIGHT DETECTED! \n ONLY SOLO LIGHTS WILL WORK WITH LXR")));

	CheckRelevantLights();
	LastFrameDrawDebug = bDrawDebug;
}

ELightArrayType ULXRDetectionComponent::GetSmartArrayTypeForLight(const FVector& Start, const FVector& End) const
{
	const float DistSqr = FVector::DistSquared(Start, End);
	return GetSmartArrayTypeForLightFromSqrDistance(DistSqr);
}

void ULXRDetectionComponent::AddToSmartArrayBySmartArrayType(ELightArrayType LightArrayType, AActor& LightSourceActor)
{
	switch (LightArrayType)
	{
		case ELightArrayType::SmartFar:
			SmartFarLightsToAdd.AddUnique(&LightSourceActor);
			break;
		case ELightArrayType::SmartMid:
			SmartMidLightsToAdd.AddUnique(&LightSourceActor);
			break;
		case ELightArrayType::SmartNear:
			SmartNearLightsToAdd.AddUnique(&LightSourceActor);
			break;
		default: ;
	}
}

int ULXRDetectionComponent::GetCurrentLightArrayIndexByLightArrayType(const ELightArrayType LightArrayType) const
{
	switch (LightArrayType)
	{
		case ELightArrayType::All:
			return RelevancyLightIndex;

		case ELightArrayType::Relevant:
			return RelevantLightIndex;

		case ELightArrayType::SmartFar:
			return SmartFarLightIndex;

		case ELightArrayType::SmartMid:
			return SmartMidLightIndex;

		case ELightArrayType::SmartNear:
			return SmartNearLightIndex;

		default: ;
	}
	return -1;
}

void ULXRDetectionComponent::SetCurrentLightArrayIndexByLightArrayType(int InIndex, const ELightArrayType LightArrayType)
{
	switch (LightArrayType)
	{
		case ELightArrayType::All:
			RelevancyLightIndex = InIndex;
			break;

		case ELightArrayType::Relevant:
			RelevantLightIndex = InIndex;
			break;

		case ELightArrayType::SmartFar:
			SmartFarLightIndex = InIndex;
			break;

		case ELightArrayType::SmartMid:
			SmartMidLightIndex = InIndex;
			break;

		case ELightArrayType::SmartNear:
			SmartNearLightIndex = InIndex;
			break;

		default: ;
	}
}

ELightArrayType ULXRDetectionComponent::GetSmartArrayTypeForLightFromSqrDistance(const float& SqrDist) const
{
	if (SqrDist > RelevancySmartDistanceMax * RelevancySmartDistanceMax)
		return ELightArrayType::SmartFar;
	if (SqrDist > RelevancySmartDistanceMin * RelevancySmartDistanceMin && SqrDist < RelevancySmartDistanceMax * RelevancySmartDistanceMax)
		return ELightArrayType::SmartMid;

	return ELightArrayType::SmartNear;
}

FCollisionQueryParams ULXRDetectionComponent::GetCollisionQueryParams(const TArray<AActor*>& ActorsToIgnore) const
{
	FCollisionQueryParams Params(GetOwner()->GetFName(), SCENE_QUERY_STAT_ONLY(KismetTraceUtils), false);

	Params.AddIgnoredActors(ActorsToIgnore);
	return Params;
}


void ULXRDetectionComponent::ChangeSmartLightArray(const ELightArrayType& From, const ELightArrayType& To, const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	if (From == To) return;
	switch (To)
	{
		case ELightArrayType::SmartFar:
			SmartFarLightsToAdd.AddUnique(LightSourceOwner);
			break;
		case ELightArrayType::SmartMid:
			SmartMidLightsToAdd.AddUnique(LightSourceOwner);
			break;
		case ELightArrayType::SmartNear:
			SmartNearLightsToAdd.AddUnique(LightSourceOwner);
			break;
		default: ;
	}
	switch (From)
	{
		case ELightArrayType::SmartFar:
			SmartFarLightsToRemove.AddUnique(LightSourceOwner);
			break;
		case ELightArrayType::SmartMid:
			SmartMidLightsToRemove.AddUnique(LightSourceOwner);
			break;
		case ELightArrayType::SmartNear:
			SmartNearLightsToRemove.AddUnique(LightSourceOwner);
			break;
		default: ;
	}
}

void ULXRDetectionComponent::CheckAllLightForRelevancy()
{
	if (bStop) return;

	RemoveRedundantLights();
	AddNewLights();

	if (!bUpdateOctreeLights)
	{
		if (bUseLocationChange)
		{
			if ((GetOwner()->GetActorLocation() - LastRelevancyUpdateLocation).Size() < RelevancyLocationThreshold)
				return;
		}
	}

	SCOPE_CYCLE_COUNTER(STAT_RelevancyCheck);

	TArray<int> StaleLightsIndexes;
	TArray<TWeakObjectPtr<AActor>> LightBatch;

	switch (RelevancyCheckType)
	{
		case ERelevancyCheckType::Fixed:
			{
				if (!AllLights.IsValidIndex(RelevancyLightIndex))
					RelevancyLightIndex = 0;

				GetNextBatchByLightArrayType(LightBatch, ELightArrayType::All);
				ProcessRelevancyCheckLightBatch(LightBatch, ELightArrayType::All);
				break;
			}

		case ERelevancyCheckType::Smart:
			{
				for (int i = SmartFarLightsToRemove.Num() - 1; i >= 0; --i)
				{
					SmartFarLights.RemoveSwap(SmartFarLightsToRemove[i]);
				}

				for (int i = SmartMidLightsToRemove.Num() - 1; i >= 0; --i)
				{
					SmartMidLights.RemoveSwap(SmartMidLightsToRemove[i]);
				}

				for (int i = SmartNearLightsToRemove.Num() - 1; i >= 0; --i)
				{
					SmartNearLights.RemoveSwap(SmartNearLightsToRemove[i]);
				}

				for (int i = SmartFarLightsToAdd.Num() - 1; i >= 0; --i)
				{
					SmartFarLights.Add(SmartFarLightsToAdd[i]);
				}

				for (int i = SmartMidLightsToAdd.Num() - 1; i >= 0; --i)
				{
					SmartMidLights.Add(SmartMidLightsToAdd[i]);
				}

				for (int i = SmartNearLightsToAdd.Num() - 1; i >= 0; --i)
				{
					SmartNearLights.Add(SmartNearLightsToAdd[i]);
				}

				SmartFarLightsToRemove.Empty();
				SmartMidLightsToRemove.Empty();
				SmartNearLightsToRemove.Empty();
				SmartFarLightsToAdd.Empty();
				SmartMidLightsToAdd.Empty();
				SmartNearLightsToAdd.Empty();

				NearSmartTimer += RelevancyCheckRate;
				NearSmartTimer += RelevancyCheckRate;
				MidSmartTimer += RelevancyCheckRate;
				FarSmartTimer += RelevancyCheckRate;

				if (FarSmartTimer > 1 / RelevancySmartCheckRateDivider)
				{
					if (SmartFarLights.Num() > 0)
					{
						GetNextBatchByLightArrayType(LightBatch, ELightArrayType::SmartFar);
						ProcessRelevancyCheckLightBatch(LightBatch, ELightArrayType::SmartFar);
					}
					FarSmartTimer = 0;
				}

				if (MidSmartTimer > 0.5 / RelevancySmartCheckRateDivider)
				{
					if (SmartMidLights.Num() > 0)
					{
						GetNextBatchByLightArrayType(LightBatch, ELightArrayType::SmartMid);
						ProcessRelevancyCheckLightBatch(LightBatch, ELightArrayType::SmartMid);
					}
					MidSmartTimer = 0;
				}


				if (NearSmartTimer > 0.2 / RelevancySmartCheckRateDivider)
				{
					if (SmartNearLights.Num() > 0)
					{
						GetNextBatchByLightArrayType(LightBatch, ELightArrayType::SmartNear);
						ProcessRelevancyCheckLightBatch(LightBatch, ELightArrayType::SmartNear);
					}
					NearSmartTimer = 0;
				}

				break;
			}

		default: ;
	}

	LastRelevancyUpdateLocation = GetOwner()->GetActorLocation();

	SET_DWORD_STAT(STAT_ALLLIGHTS, AllLights.Num());
	SET_DWORD_STAT(STAT_SMARTNEAR, SmartNearLights.Num());
	SET_DWORD_STAT(STAT_SMARTMID, SmartMidLights.Num());
	SET_DWORD_STAT(STAT_SMARTFAR, SmartFarLights.Num());
}

void ULXRDetectionComponent::AddLightToNewRelevantList(const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	NewRelevantLightsToAdd.AddUnique(LightSourceOwner);
	if (bPrintDebug)
		UE_LOG(LogLightSystem, Warning, TEXT("Added relevant light %s to %s"), *LightSourceOwner->GetName(), *GetOwner()->GetName());
}

void ULXRDetectionComponent::ProcessRelevancyCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& LightBatch, ELightArrayType LightArrayType)
{
	switch (RelevancyCheckType)
	{
		case ERelevancyCheckType::Fixed:

			for (int i = 0; i < LightBatch.Num(); ++i)
			{
				const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightBatch[i].Get()->GetComponentByClass(ULXRSourceComponent::StaticClass()));
				if (IsValid(LightSourceComponent))
				{
					if (!RelevantLights.Contains(LightSourceComponent->GetOwner()))
					{
						bool bIsRelevant = LightSourceComponent->bAlwaysRelevant;
						if (!bIsRelevant)
						{
							bIsRelevant = CheckDistance(*LightSourceComponent);
						}

						if (bIsRelevant)
						{
							AddLightToNewRelevantList(LightSourceComponent->GetOwner());
						}
					}
				}
			}
			break;

		case ERelevancyCheckType::Smart:
			{
				switch (LightArrayType)
				{
					case ELightArrayType::SmartFar:

						for (int i = 0; i < LightBatch.Num(); ++i)
						{
							const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightBatch[i].Get()->GetComponentByClass(ULXRSourceComponent::StaticClass()));
							if (IsValid(LightSourceComponent))
							{
#if UE_ENABLE_DEBUG_DRAWING
								if (bDrawDebug && LightSourceComponent->bDrawDebug)
								{
									constexpr float Radius = 30;
									constexpr float Thickness = 0;
									const FColor Color = FColor::Magenta;
									DrawDebugSphere(GetWorld(), LightSourceComponent->GetOwner()->GetActorLocation(), Radius, FMath::Clamp<int32>(Radius / 4.f, 8, 32), Color, false, 1 / RelevancySmartCheckRateDivider, 0, Thickness);
								}
#endif


								const float DistSqr = FVector::DistSquared(LightSourceComponent->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation());
								if (DistSqr < RelevancySmartDistanceMax * RelevancySmartDistanceMax)
								{
									const ELightArrayType NewType = GetSmartArrayTypeForLightFromSqrDistance(DistSqr);
									ChangeSmartLightArray(ELightArrayType::SmartFar, NewType, LightSourceComponent->GetOwner());
								}
							}
						}
						break;


					case ELightArrayType::SmartMid:

						for (int i = 0; i < LightBatch.Num(); ++i)
						{
							const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightBatch[i].Get()->GetComponentByClass(ULXRSourceComponent::StaticClass()));
							if (IsValid(LightSourceComponent))
							{
#if UE_ENABLE_DEBUG_DRAWING
								if (bDrawDebug && LightSourceComponent->bDrawDebug)
								{
									const float Radius = 30;
									const float Thickness = 0;
									const FColor Color = FColor::Yellow;
									DrawDebugSphere(GetWorld(), LightSourceComponent->GetOwner()->GetActorLocation(), Radius, FMath::Clamp<int32>(Radius / 4.f, 8, 32), Color, false, 0.5 / RelevancySmartCheckRateDivider, 0, Thickness);
								}
#endif
								const float DistSqr = FVector::DistSquared(LightSourceComponent->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation());
								const ELightArrayType NewType = GetSmartArrayTypeForLightFromSqrDistance(DistSqr);

								if (NewType != ELightArrayType::SmartMid)
									ChangeSmartLightArray(ELightArrayType::SmartMid, NewType, LightSourceComponent->GetOwner());

								if (NewType > ELightArrayType::SmartMid || NewType == ELightArrayType::SmartMid)
								{
									if (DistSqr < RelevancySmartDistanceMax * RelevancySmartDistanceMax)
									{
										if (!RelevantLights.Contains(LightSourceComponent->GetOwner()))
										{
											bool bIsRelevant = LightSourceComponent->bAlwaysRelevant;

											if (!bIsRelevant)
											{
												TArray<int> PassedComponents;
												bIsRelevant = CheckIsLightRelevant(*LightSourceComponent, PassedComponents, PassedComponents, false, false);
											}

											if (bIsRelevant)
											{
												SmartMidLightsToRemove.AddUnique(LightSourceComponent->GetOwner());
												AddLightToNewRelevantList(LightSourceComponent->GetOwner());
											}
										}
									}
								}
							}
						}
						break;

					case ELightArrayType::SmartNear:

						for (int i = 0; i < LightBatch.Num(); ++i)
						{
							const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightBatch[i].Get()->GetComponentByClass(ULXRSourceComponent::StaticClass()));
							if (IsValid(LightSourceComponent))
							{
#if UE_ENABLE_DEBUG_DRAWING
								if (bDrawDebug && LightSourceComponent->bDrawDebug)
								{
									const float Radius = 30;
									const float Thickness = 0;
									const FColor Color = FColor::Cyan;
									DrawDebugSphere(GetWorld(), LightSourceComponent->GetOwner()->GetActorLocation(), Radius, FMath::Clamp<int32>(Radius / 4.f, 8, 32), Color, false, 0.2 / RelevancySmartCheckRateDivider, 0, Thickness);
								}
#endif
								const float DistSqr = FVector::DistSquared(LightSourceComponent->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation());
								const ELightArrayType NewType = GetSmartArrayTypeForLightFromSqrDistance(DistSqr);

								if (NewType != ELightArrayType::SmartNear)
								{
									ChangeSmartLightArray(ELightArrayType::SmartNear, NewType, LightSourceComponent->GetOwner());
									continue;
								}

								if (!RelevantLights.Contains(LightSourceComponent->GetOwner()))
								{
									bool bIsRelevant = LightSourceComponent->bAlwaysRelevant;
									if (!bIsRelevant)
									{
										TArray<int> PassedComponents;
										bIsRelevant = CheckIsLightRelevant(*LightSourceComponent, PassedComponents, PassedComponents, false, false);
									}

									if (bIsRelevant)
									{
										SmartNearLightsToRemove.AddUnique(LightSourceComponent->GetOwner());
										AddLightToNewRelevantList(LightSourceComponent->GetOwner());
									}
								}
							}
						}
						break;
					default: ;
				}
			}
		default: ;
	}
}

void ULXRDetectionComponent::DoRelevantCheckOnSourceActor(const TWeakObjectPtr<AActor>& LightSourceComponentOwner, bool IsFromThread, bool IsLightSenseCheck)
{
	if (!LightSourceComponentOwner.IsValid())
	{
		return;
	}

	if (GetOwner() == LightSourceComponentOwner)
		return;

	const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightSourceComponentOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()));
	const TArray<ULightComponent*> LightComponents = LightSourceComponent->GetMyLightComponents();
	const bool IsLightSourceEnabled = LightSourceComponent->IsEnabled();
	if (LXRSubsystem->bSoloFound)
	{
		if (!LightSourceComponent->bSolo)
			return;
	}

	bool IsRelevant = false;
	TArray<int> PassedComponents;
	TArray<int> PassedTargets;

	if (IsLightSourceEnabled)
	{
		IsRelevant = CheckIsLightRelevant(*LightSourceComponent, PassedComponents, PassedTargets, IsLightSenseCheck, IsFromThread);
	}

	// const bool ShouldCheckVisibility = IsRelevant || UpdateMemory;

	if (IsRelevant)
	{
		// if (!IsRelevant)
		// {
		// 	for (int i = 0; i < LightComponents.Num(); ++i)
		// 	{
		// 		PassedComponents.AddUnique(i);
		// 	}
		// }

		if (CheckVisibility(LightComponents, PassedComponents, PassedTargets, IsLightSenseCheck))
		{
			if (IsLightSourceEnabled)
			{
				LightPassed(LightSourceComponentOwner, PassedComponents);
			}
		}
		else
		{
			PassedComponents.Empty();
		}
	}

	if (PassedComponents.Num() == 0)
	{
		if (LightSourceComponent->bAlwaysRelevant)
		{
			RemovePassedLight(LightSourceComponentOwner);
		}
		else
		{
			IsFromThread ? IncreaseFailCountIfNotAlwaysRelevantLightFromThread(LightSourceComponentOwner) : IncreaseFailCount(LightSourceComponentOwner);

			if (RelevantLightsFailCounts[LightSourceComponentOwner] > MaxConsecutiveFails)
			{
				IsFromThread ? CheckAndRemoveIfLightNotRelevantFromThread(LightSourceComponentOwner) : CheckAndRemoveIfLightNotRelevant(LightSourceComponentOwner);
			}
		}
	}
}

void ULXRDetectionComponent::GetLXR()
{
	SCOPE_CYCLE_COUNTER(STAT_GetCombinedDatas);
	CombinedLightColors.Empty();
	CombinedLXRColor = FLinearColor::Black;
	CombinedLXRIntensity = 0;
	TArray<FVector> TraceTargets;
	// TraceTargets.Add(GetOwner()->GetActorLocation());
	TraceTargets = GetTraceTargets(true);
	if (bGetIlluminatedTargets)
	{
		// TraceTargets = GetTraceTargets(true);

		for (auto It = IlluminatedTargets.CreateConstIterator(); It; ++It)
		{
			TargetsCombinedLXRColor[It.Key()] = FLinearColor::Black;
			TargetsCombinedLXRIntensity[It.Key()] = 0;
			TargetsCombinedLightColors[It.Key()].Empty();
		}
	}
#if UE_ENABLE_DEBUG_DRAWING
	if (bDrawDebug)
	{
		for (auto TraceTarget : TraceTargets)
		{
			DrawDebugBox(GetWorld(), TraceTarget, FVector(5), FColor::Green, false, 0.1f);
		}
	}
#endif


	for (int i = RelevantLightsPassed.Num() - 1; i >= 0; --i)
	{
		TWeakObjectPtr<AActor> LightActor = RelevantLightsPassed[i];
		if (IsValid(LightActor.Get()))
		{
			const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightActor->GetComponentByClass(ULXRSourceComponent::StaticClass()));
			TArray<ULightComponent*> LightComponents = LightSourceComponent->GetMyLightComponents();

			TArray<int> PassedComps = LightsPassedComponents[LightActor];

			for (const int CompIndex : PassedComps)
			{
				float Multiplier = 1;
				float ColorMultiplier = 1;
				if (LightSourceComponent->LightLXRMultipliers.Num() == 0)
				{
					Multiplier = LightSourceComponent->LXRMultiplier;
				}
				else
				{
					for (int l = 0; l < LightSourceComponent->LightLXRMultipliers.Num(); ++l)
					{
						if (LightSourceComponent->LightLXRMultipliers[l].LightComponent.GetComponent(LightSourceComponent->GetOwner()) == LightComponents[CompIndex])
						{
							Multiplier = LightSourceComponent->LightLXRMultipliers[l].LightData;
							break;
						}
					}
				}

				if (LightSourceComponent->LightLXRColorMultipliers.Num() == 0)
				{
					ColorMultiplier = LightSourceComponent->LXRColorMultiplier;
				}
				else
				{
					for (int l = 0; l < LightSourceComponent->LightLXRColorMultipliers.Num(); ++l)
					{
						if (LightSourceComponent->LightLXRColorMultipliers[l].LightComponent.GetComponent(LightSourceComponent->GetOwner()) == LightComponents[CompIndex])
						{
							ColorMultiplier = LightSourceComponent->LightLXRColorMultipliers[l].LightData;
							break;
						}
					}
				}

				ULightComponent* Target = LightComponents[CompIndex];
				FLinearColor LightColor;

				// Target->LightColor.ComputeAndFixedColorAndIntensity
				if (Target->bUseTemperature)
				{
					LightColor = FLinearColor::MakeFromColorTemperature(Target->Temperature);
					LightColor *= Target->GetLightColor();
				}
				else
					LightColor = Target->GetLightColor();

				// float MaxComponent = FMath::Max(DELTA, FMath::Max(LightColor.R, FMath::Max(LightColor.G, LightColor.B)));
				// LightColor = (LightColor.GetClamped(0, MaxComponent) / MaxComponent).ToFColor(true);
				// float Intensity = FMath::Min(Target->Intensity, 100.f);

				float Percent = 0;
				float Candelas = 0;

				for (int j = 0; j < TraceTargets.Num(); ++j)
				{
					if (j > 0)
					{
						if (!bGetIlluminatedTargets)
						{
							break;
						}
					}

					const float Distance = FVector::Distance(TraceTargets[j], Target->GetComponentLocation());
					const float DistSquared = Distance * Distance;

					if (Cast<UDirectionalLightComponent>(Target))
					{
						Candelas = Cast<ULightComponent>(Target)->Intensity * DistSquared;
						Percent = 1;
					}
					else
					{
						const float BeforeConversionIntensity = Cast<ULightComponent>(Target)->Intensity;
						const ELightUnits IntensityUnits = Cast<ULocalLightComponent>(Target)->IntensityUnits;
						float CosHalfConeAngle;
						if (Cast<USpotLightComponent>(Target))
							CosHalfConeAngle = Cast<USpotLightComponent>(Target)->GetCosHalfConeAngle();
						else
							CosHalfConeAngle = -1;

						Candelas = BeforeConversionIntensity * Cast<ULocalLightComponent>(Target)->GetUnitsConversionFactor(IntensityUnits, ELightUnits::Candelas, CosHalfConeAngle);
					}


					float AfterDivideIntensity = Candelas / DistSquared;

					float Attenuation;
					// LightColor.GetClamped(0, 1);
					if (Cast<USpotLightComponent>(Target))
					{
						FVector SpotForward = Target->GetForwardVector();
						const float OuterConeAngle = Cast<USpotLightComponent>(Target)->OuterConeAngle;
						const FVector DirectionToTarget = (TraceTargets[j] - Target->GetComponentLocation()).GetSafeNormal();

						const float Dot = FVector::DotProduct(SpotForward, DirectionToTarget);
						const float Angle = FMath::RadiansToDegrees(acosf(Dot));
						Percent = FMath::Abs(Angle / OuterConeAngle - 1) * 1.5f;

						Attenuation = Cast<USpotLightComponent>(Target)->AttenuationRadius;
						Percent *= FMath::Abs(Distance / Attenuation - 1);
					}
					else if (Cast<URectLightComponent>(Target) || Cast<UPointLightComponent>(Target))
					{
						Attenuation = Cast<ULocalLightComponent>(Target)->AttenuationRadius;
						Percent = FMath::Abs(Distance / Attenuation - 1);
						// if(Cast<URectLightComponent>(Target))
						// {
						// UE_LOG(LogLightSystem, Warning, TEXT("%f" ), Percent);
						// UE_LOG(LogLightSystem, Warning, TEXT("%f - %s - %f" ), CombinedLightAttenuation, *CombinedLightColor.ToString(), CombinedLightIntensity);
						// }
						// Percent = 1;
					}


					// float Intensity = Target->ComputeLightBrightness();

					LightColor *= Percent;
					AfterDivideIntensity *= Percent;

					// CombinedLightColor += LightColor * ColorMultiplier;
					LightColor *= ColorMultiplier;

					if (j == 0)
					{
						CombinedLightColors.Add(LightColor);
						CombinedLXRIntensity += FMath::Min(AfterDivideIntensity * 1500.f, 1.f) * Multiplier;
					}
					else
					{
						TargetsCombinedLightColors[j].Add(LightColor);
						TargetsCombinedLXRIntensity[j] += FMath::Min(AfterDivideIntensity * 1500.f, 1.f) * Multiplier;
					}
				}


				// if (bDrawDebug)
				// {
				// 	TArray<FStringFormatArg> Args;
				// 	Args.Add(FStringFormatArg(ULXRFunctionLibrary::GetFloatAsStringWithPrecision(LightColor.R, 2)));
				// 	Args.Add(FStringFormatArg(ULXRFunctionLibrary::GetFloatAsStringWithPrecision(LightColor.G, 2)));
				// 	Args.Add(FStringFormatArg(ULXRFunctionLibrary::GetFloatAsStringWithPrecision(LightColor.B, 2)));
				//
				// 	FString Colors = FString::Format(TEXT("R:{0}, G:{1}, B:{2}"), Args);
				// 	FVector loc = GetOwner()->GetActorLocation() + GetOwner()->GetActorRightVector() * 50 + (FVector::UpVector * 100);
				// 	CombinedLXRColor = ULXRFunctionLibrary::GetLinearColorArrayAverage(CombinedLightColors);
				// 	DrawDebugPoint(GetWorld(), loc, 15.f, LightColor.ToFColor(true), false, 0.1f);
				// 	DrawDebugString(GetWorld(), LightActor->GetActorLocation() + FVector::UpVector * 100, Colors, 0, LightColor.ToFColor(false), 0.1f);
				// 	DrawDebugString(GetWorld(), LightActor->GetActorLocation() + FVector::UpVector * 50, ULXRFunctionLibrary::GetFloatAsStringWithPrecision(Percent, 2), nullptr, FColor::Red, 0.1f);
				// }

				// UE_LOG(LogLightSystem, Warning, TEXT("D:%f - A:%f - P:%f - AP:%f" ), Distance, Attenuation, Percent);
			}
		}
	}
	if (bGetIlluminatedTargets)
	{
		for (int i = 0; i < IlluminatedTargets.Num(); ++i)
		{
			if (TargetsCombinedLightColors.Contains(i))
			{
				FLinearColor TargetLXR = ULXRFunctionLibrary::GetLinearColorArrayAverage(TargetsCombinedLightColors[i]);
				TargetLXR.A = TargetsCombinedLXRIntensity[i];
				IlluminatedTargets[i] = TargetLXR;
			}
		}
	}

	CombinedLXRColor = ULXRFunctionLibrary::GetLinearColorArrayAverage(CombinedLightColors);
#if UE_ENABLE_DEBUG_DRAWING
	if (bDrawDebug)
	{
		FVector loc = GetOwner()->GetActorLocation() + GetOwner()->GetActorRightVector() * -50 + (FVector::UpVector * 100);
		DrawDebugPoint(GetWorld(), loc, 15.f, CombinedLXRColor.ToFColor(true), false, 0.1f);
	}
#endif


	//UE_LOG(LogLightSystem, Warning, TEXT("%f - %s - %f" ), CombinedLightAttenuation, *CombinedLightColor.ToString(), CombinedLightIntensity);
	// DrawDebugSphere(GetWorld(), GetOwner()->GetActorLocation(), 100, 20, CombinedLightColor.ToFColor(false), false, RelevantTraceType == ERelevantTraceType::Async ? GetWorld()->DeltaTimeSeconds : GetComponentTickInterval(), 0, 1);
}

void ULXRDetectionComponent::ProcessRelevantCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& LightBatch, bool IsLightSenseCheck)
{
	for (TWeakObjectPtr<AActor> LightSourceComponentOwner : LightBatch)
	{
		DoRelevantCheckOnSourceActor(LightSourceComponentOwner, false, IsLightSenseCheck);
		// if (!LightSourceComponentOwner.IsValid())
		// {
		// 	continue;
		// }
		//
		// if (GetOwner() == LightSourceComponentOwner)
		// 	continue;
		//
		// const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightSourceComponentOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()));
		// const TArray<ULightComponent*> LightComponents = LightSourceComponent->GetMyLightComponents();
		// const bool IsLightSourceEnabled = LightSourceComponent->IsEnabled();
		// const bool UpdateMemory = IsLightMemoryEnabled && LightSourceComponent->bIsMemorizable;
		//
		// bool IsRelevant = false;
		// TArray<int> PassedComponents;
		// TArray<int> PassedTargets;
		//
		// if (IsLightSourceEnabled)
		// {
		// 	IsRelevant = CheckIsLightRelevant(*LightSourceComponent, PassedComponents, PassedTargets, IsLightSenseCheck);
		// }
		//
		// const bool ShouldCheckVisibility = IsRelevant || UpdateMemory;
		//
		// if (ShouldCheckVisibility)
		// {
		// 	if (!IsRelevant)
		// 	{
		// 		//must be memory chek then. At this point PassedComponents must be empty... but Memory Check needs to check those soooo....
		// 		//hopefully this does not break anything..
		// 		for (int i = 0; i < LightComponents.Num(); ++i)
		// 		{
		// 			PassedComponents.AddUnique(i);
		// 		}
		// 	}
		//
		// 	if (CheckVisibility(LightComponents, PassedComponents, PassedTargets, IsLightSenseCheck))
		// 	{
		// 		if (IsLightSenseCheck)
		// 		{
		// 			SenseComponent->AddSensedLight(LightSourceComponentOwner, PassedComponents, PassedTargets);
		// 			if (bDebugSensing)
		// 			{
		// 				// DrawDebugDirectionalArrow(GetWorld(), GetOwner()->GetActorLocation(), LightSourceComponent->GetOwner()->GetActorLocation(), 15, FColor::White, false, 2.5f, 0, 5);
		// 				for (const int ChosenTraceTargetIdx : SenseComponent->SensedLightPassedData[LightSourceComponentOwner].Value)
		// 				{
		// 					if (!SenseComponent->AllGeneratedTraceTargets.IsValidIndex(ChosenTraceTargetIdx))
		// 					{
		// 						break;
		// 					}
		// 					FVector ChosenTarget = SenseComponent->AllGeneratedTraceTargets[ChosenTraceTargetIdx];
		//
		// 					DrawDebugDirectionalArrow(GetWorld(), ChosenTarget, LightSourceComponent->GetOwner()->GetActorLocation(), 5, FColor::Green, false, SenseComponent->CheckRate, 0, 0);
		// 					DrawDebugPoint(GetWorld(), ChosenTarget, 5, FColor::Yellow, false, SenseComponent->CheckRate);
		//
		// 					for (const int CompIdx : PassedComponents)
		// 					{
		// 						const ULightComponent* Target = LightSourceComponent->GetMyLightComponents()[CompIdx];
		// 						FLinearColor LightColor;
		//
		// 						// Target->LightColor.ComputeAndFixedColorAndIntensity
		// 						if (Target->bUseTemperature)
		// 						{
		// 							LightColor = FLinearColor::MakeFromColorTemperature(Target->Temperature);
		// 							LightColor *= Target->GetLightColor();
		// 						}
		// 						else
		// 							LightColor = Target->GetLightColor();
		//
		// 						DrawDebugDirectionalArrow(GetWorld(), LightSourceComponent->GetOwner()->GetActorLocation(), LightSourceComponent->GetOwner()->GetActorLocation() + FVector::UpVector * 250, 50, LightColor.ToFColor(true), false, SenseComponent->CheckRate * 2, 0, 5);
		// 					}
		// 				}
		// 			}
		// 		}
		// 		else
		// 		{
		// 			LightPassed(LightSourceComponentOwner, PassedComponents);
		// 		}
		//
		// 		if (UpdateMemory)
		// 		{
		// 			MemoryComponent->AddOrUpdateLightState(LightSourceComponentOwner);
		// 		}
		// 	}
		// 	else
		// 	{
		// 		PassedComponents.Empty();
		// 	}
		// }
		// else if (IsLightSenseCheck)
		// {
		// 	SenseComponent->RemoveSensedLight(LightSourceComponentOwner);
		// 	continue;
		// }
		//
		// if (PassedComponents.Num() == 0)
		// {
		// 	if (LightSourceComponent->bAlwaysRelevant)
		// 	{
		// 		RemovePassedLight(LightSourceComponentOwner);
		// 	}
		// 	else
		// 	{
		// 		IncreaseFailCount(LightSourceComponentOwner);
		// 		if (RelevantLightsFailCounts[TWeakObjectPtr<AActor>(LightSourceComponentOwner)] > MaxConsecutiveFails)
		// 		{
		// 			CheckAndRemoveIfLightNotRelevant(TWeakObjectPtr<AActor>(LightSourceComponentOwner));
		// 		}
		// 	}
		// }
	}
}

void ULXRDetectionComponent::CheckRelevantLights()
{
	if (bStop) return;

	StatResetTimer += GetWorld()->DeltaTimeSeconds;
	GetCombinedDatasTimer += GetWorld()->DeltaTimeSeconds;

	if (StatResetTimer > 1)
	{
		SET_DWORD_STAT(STAT_TRACESSYNC, 0);
		SET_DWORD_STAT(STAT_TRACESMULTITHREAD, 0);
		SET_DWORD_STAT(STAT_THREADS, 0);

		StatResetTimer = 0;
	}

	if (GetCombinedDatasTimer > 0.1f)
	{
		GetLXR();
		GetCombinedDatasTimer = 0;
	}

	SCOPE_CYCLE_COUNTER(STAT_RelevantCheck);

	if (bPrintDebug)
	{
		GEngine->AddOnScreenDebugMessage(97, GetWorld()->DeltaTimeSeconds, FColor::Green, FString::Printf(TEXT("Relevant Lights: %d"), RelevantLights.Num()));
		GEngine->AddOnScreenDebugMessage(98, GetWorld()->DeltaTimeSeconds, FColor::Green, FString::Printf(TEXT("Passed Lights: %d"), RelevantLightsPassed.Num()));
		GEngine->AddOnScreenDebugMessage(99, GetWorld()->DeltaTimeSeconds, FColor::Green, FString::Printf(TEXT("All Lights: %d"), AllLights.Num()));
	}

#if UE_ENABLE_DEBUG_DRAWING
	if (bDebugRelevantAndPassed)
	{
		for (auto RelevantLight : RelevantLights)
		{
			if (RelevantLight.IsValid())
			{
				DrawDebugBox(GetWorld(), RelevantLight->GetActorLocation(), FVector(25), FColor::Orange, false, GetComponentTickInterval());
				DrawDebugDirectionalArrow(GetWorld(), GetOwner()->GetActorLocation(), RelevantLight->GetActorLocation(), 150, FColor::Orange, false, GetComponentTickInterval(), 0, 0);
			}
		}
		for (auto PassedLight : RelevantLightsPassed)
		{
			if (PassedLight.IsValid())
			{
				DrawDebugBox(GetWorld(), PassedLight->GetActorLocation(), FVector(15), FColor::Cyan, false, GetComponentTickInterval());
				DrawDebugDirectionalArrow(GetWorld(), GetOwner()->GetActorLocation(), PassedLight->GetActorLocation(), 100, FColor::Cyan, false, GetComponentTickInterval(), 0, 0);
			}
		}
	}
#endif

	RemoveNonRelevantLights();
	AddNewRelevantLights();
	TArray<TWeakObjectPtr<AActor>> LightBatch;
	GetNextRelevantCheckLightBatch(LightBatch);

	ProcessRelevantCheckLightBatch(LightBatch);

	SET_DWORD_STAT(STAT_RELEVANTLIGHTS, RelevantLights.Num());
	SET_DWORD_STAT(STAT_PASSEDRELEVANTLIGHTS, RelevantLightsPassed.Num());
}


TArray<TWeakObjectPtr<AActor>>& ULXRDetectionComponent::GetLightArrayByLightArrayType(ELightArrayType LightArrayType)
{
	switch (LightArrayType)
	{
		case ELightArrayType::All:
			return AllLights;

		case ELightArrayType::Relevant:
			return RelevantLights;

		case ELightArrayType::SmartFar:
			return SmartFarLights;

		case ELightArrayType::SmartMid:
			return SmartMidLights;

		case ELightArrayType::SmartNear:
			return SmartNearLights;

		default: ;
	}

	return AllLights;
}

void ULXRDetectionComponent::GetNextBatchByLightArrayType(TArray<TWeakObjectPtr<AActor>>& OutLightBatch, ELightArrayType LightArrayType)
{
	const bool bIsRelevancyCheck = LightArrayType < ELightArrayType::Relevant;
	const int BatchCount = bIsRelevancyCheck ? RelevancyLightBatchCount : RelevantLightBatchCount;
	TArray<TWeakObjectPtr<AActor>>& Array = GetLightArrayByLightArrayType(LightArrayType);
	OutLightBatch.Empty();

	int Index = GetCurrentLightArrayIndexByLightArrayType(LightArrayType);
	// GEngine->AddOnScreenDebugMessage(1, GetWorld()->DeltaTimeSeconds, FColor::Red, FString::Printf(TEXT("Start")));
	if (!Array.IsValidIndex(Index))
		Index = Array.Num() - 1;

	if (Array.Num() == 0)
		Index = -1;

	const int MaxIteration = Array.Num() > BatchCount ? Array.Num() * 2 : Array.Num();
	int Iteration = 0;

	while (OutLightBatch.Num() != BatchCount && Iteration < MaxIteration)
	{
		if (Index == -1)
		{
			Index = Array.Num() - 1;
		}

		if (!Array[Index].IsValid())
		{
			Array.RemoveAtSwap(Index);
			Iteration++;
			Index--;
			continue;
		}

		if (LXRSubsystem->bSoloFound)
		{
			const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(Array[Index]->GetComponentByClass(ULXRSourceComponent::StaticClass()));
			if (LightSourceComponent->bSolo)
			{
				OutLightBatch.AddUnique(Array[Index]);
			}
		}
		else
		{
			OutLightBatch.AddUnique(Array[Index]);
		}

		Iteration++;
		Index--;
	}

	// if(LightArrayType==ELightArrayType::SmartMid || LightArrayType==ELightArrayType::SmartFar)
	// for (auto actor : OutLightBatch)
	// {
	// 	UE_LOG(LogLightSystem, Verbose, TEXT("In Batch %s"), *actor->GetName());
	// }
	SetCurrentLightArrayIndexByLightArrayType(Index, LightArrayType);
}

void ULXRDetectionComponent::GetNextRelevantCheckLightBatch(TArray<TWeakObjectPtr<AActor>>& OutLightBatch)
{
	if (!RelevantLights.IsValidIndex(RelevantLightIndex))
		RelevantLightIndex = RelevantLights.Num() - 1;

	if (RelevantLights.Num() == 0)
	{
		RelevantLightIndex = -1;
		return;
	}

	for (RelevantLightIndex; RelevantLightIndex >= 0; --RelevantLightIndex)
	{
		if (!RelevantLights[RelevantLightIndex].IsValid())
		{
			RelevantLights.RemoveAtSwap(RelevantLightIndex);
			continue;
		}

		if (RelevantLights[RelevantLightIndex] == GetOwner())
			continue;

		OutLightBatch.AddUnique(RelevantLights[RelevantLightIndex]);
		if (OutLightBatch.Num() == RelevantLightBatchCount)
			break;
	}


	if (OutLightBatch.Num() != RelevantLightBatchCount && RelevantLightIndex == -1 && RelevantLights.Num() < RelevantLightBatchCount)
	{
		for (RelevantLightIndex = RelevantLights.Num() - 1; RelevantLightIndex >= 0; --RelevantLightIndex)
		{
			if (RelevantLights[RelevantLightIndex].IsValid())
			{
				OutLightBatch.AddUnique(RelevantLights[RelevantLightIndex]);
				RelevantLightIndex--;
				if (OutLightBatch.Num() == RelevantLightBatchCount)
					break;
			}
		}
	}
}


bool ULXRDetectionComponent::CheckDirectionalLight(const ULightComponent& LightComponent, const FVector& Start) const
{
	const FVector DirectionalForwardInverse = LightComponent.GetForwardVector() * -1;
	const FVector End = Start + DirectionalForwardInverse.GetSafeNormal() * DirectionalLightTraceDistance;

	TArray<AActor*> ActorsToIgnore;

	ActorsToIgnore.Append(Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->GetMyOverlappingActors());
	ActorsToIgnore.Append(Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->GetIgnoreVisibilityActors());
	ActorsToIgnore.Append(IgnoreVisibilityActors);

	ActorsToIgnore.AddUnique(GetOwner());
	ActorsToIgnore.AddUnique(LightComponent.GetOwner());

	if (!GetWorld()->LineTraceTestByChannel(Start, End, TraceChannel, GetCollisionQueryParams(ActorsToIgnore)))
	{
#if UE_ENABLE_DEBUG_DRAWING
		if (bDrawDebug && Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->bDrawDebug)
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, DebugDrawTime, 0, 1);
#endif

		return true;
	}
#if UE_ENABLE_DEBUG_DRAWING
	if (bDrawDebug && Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->bDrawDebug)
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, DebugDrawTime, 0, 1);
#endif
	return false;
}

bool ULXRDetectionComponent::CheckDistance(const ULXRSourceComponent& LightSourceComponent) const
{
	float Attenuation = 0;
	const FVector Start = GetOwner()->GetActorLocation();
	const FVector End = LightSourceComponent.GetOwner()->GetActorLocation();

	TArray<ULightComponent*> SourceLightComponents = LightSourceComponent.GetMyLightComponents();

	for (ULightComponent* ItLightComponent : SourceLightComponents)
	{
		const bool bIsSpotLight = ItLightComponent->IsA(USpotLightComponent::StaticClass());
		const bool bIsPointLight = ItLightComponent->IsA(UPointLightComponent::StaticClass()) && !bIsSpotLight;
		const float newAttenuation = bIsSpotLight ? Cast<USpotLightComponent>(ItLightComponent)->AttenuationRadius : bIsPointLight ? Cast<UPointLightComponent>(ItLightComponent)->AttenuationRadius : Cast<URectLightComponent>(ItLightComponent)->AttenuationRadius;
		Attenuation = FMath::Max(newAttenuation, Attenuation);
	}
	const float DistanceToCheck = Attenuation * LightSourceComponent.AttenuationMultiplierToBeRelevant;
	const float DistanceSqr = FVector::DistSquared(Start, End);

	if (DistanceSqr < DistanceToCheck * DistanceToCheck)
		return true;
	return false;
}

bool ULXRDetectionComponent::CheckDistance(const ULXRSourceComponent& LightSourceComponent, const ULightComponent& LightComponent, const FVector& Start, const FVector& End) const
{
	const bool bIsSpotLight = LightComponent.IsA(USpotLightComponent::StaticClass());
	const bool bIsPointLight = LightComponent.IsA(UPointLightComponent::StaticClass()) && !bIsSpotLight;
	const float Attenuation = bIsSpotLight ? Cast<USpotLightComponent>(&LightComponent)->AttenuationRadius : bIsPointLight ? Cast<UPointLightComponent>(&LightComponent)->AttenuationRadius : Cast<URectLightComponent>(&LightComponent)->AttenuationRadius;

	const float DistanceToCheck = Attenuation * LightSourceComponent.AttenuationMultiplierToBeRelevant;
	const float DistanceSqr = FVector::DistSquared(Start, End);

	if (DistanceSqr < DistanceToCheck * DistanceToCheck)
		return true;
	return false;
}


void ULXRDetectionComponent::GetLightSystemLights()
{
	AllLights = LXRSubsystem->GetAllLights();
}

void ULXRDetectionComponent::AddLight(AActor* LightSource)
{
	if (LXRSubsystem)
		NewAllLightsToAdd.AddUnique(LightSource);
}

void ULXRDetectionComponent::RemoveLight(AActor* LightSource)
{
	LightsToRemove.AddUnique(LightSource);
}

bool ULXRDetectionComponent::GetIsRelevant(const ULXRSourceComponent& LightSourceComponent) const
{
	return RelevantLights.Contains(LightSourceComponent.GetOwner());
}


bool ULXRDetectionComponent::CheckIsLightRelevant(const ULXRSourceComponent& LightSourceComponent, TArray<int>& PassedComponents, TArray<int>& PassedTargets, bool IsLightSenseCheck, bool IsFromThread) const
{
	const bool bIsRelevantCheck = GetIsRelevant(LightSourceComponent);

	TArray<FVector> TraceTargets = GetTraceTargets(bIsRelevantCheck);

	TArray<ULightComponent*> LightComponents = LightSourceComponent.GetMyLightComponents();

	int PassedChecks = 0;
	int ComponentIdx = 0;
	bool Passed = false;

	const float RequiredChecksToPassAmount = bIsRelevantCheck ? TraceTargets.Num() * TracesRequired : TargetsRequired;

	for (ULightComponent* LightComponent : LightComponents)
	{
		bool DoCheck = true;

		DoCheck = LightSourceComponent.IsLightComponentEnabled(LightComponent);

		if (!DoCheck)
			continue;

		bool TargetPassed = false;
		for (const FVector TraceTarget : TraceTargets)
		{
			const FVector Start = TraceTarget;
			const FVector End = LightComponent->GetComponentLocation();

			const bool bIsDirectionalLight = LightComponent->IsA(UDirectionalLightComponent::StaticClass());
			const bool bCheckDirectionalLight = bIsDirectionalLight;

			if (bCheckDirectionalLight)
			{
				const bool DirectionalLightPassed = CheckDirectionalLight(*LightComponent, Start);
				if (DirectionalLightPassed)
					TargetPassed = true;
			}
			else
			{
				bool bCheckFailed = false;
				const bool bIsSpotLight = LightComponent->IsA(USpotLightComponent::StaticClass());
				const bool bIsPointLight = LightComponent->IsA(UPointLightComponent::StaticClass()) && !bIsSpotLight;
				const bool bIsRectLight = LightComponent->IsA(URectLightComponent::StaticClass());

				const bool bCheckDistance = !bIsDirectionalLight;
				const bool bCheckAttenuation = bIsSpotLight || bIsPointLight || bIsRectLight;
				const bool bCheckDirection = bIsSpotLight || bIsRectLight;
				const bool bCheckIfInsideSpotOrRect = bIsSpotLight || bIsRectLight;


				bool DistancePassed = false;
				bool DirectionPassed = false;
				bool AttenuationPassed = false;
				bool InsideSpotOrRectPassed = false;


				if (bCheckDistance)
				{
					DistancePassed = CheckDistance(LightSourceComponent, *LightComponent, Start, End);
					if (bPrintDebug && LightSourceComponent.bDrawDebug)
					{
						UE_LOG(LogLightSystem, Verbose, TEXT("Distance to source %s from %s is %f"), *LightSourceComponent.GetOwner()->GetName(), *GetOwner()->GetName(), FVector::Dist(Start,End));
					}
					if (!DistancePassed)
						bCheckFailed = true;
				}


				//if(!bComponentPassed)
				if (bCheckFailed && (!bDrawDebug || !bPrintDebug || !LightSourceComponent.bDrawDebug))
				{
					TargetPassed = false;
					continue;
				}

				if (bCheckDirection)
				{
					DirectionPassed = CheckDirection(*LightComponent, Start, End);
					// if (bDrawDebug && LightSourceComponent.bDrawDebug)
					// 	DrawDebugDirectionalArrow(GetWorld(), End, FMath::Lerp(Start, End, 0.5f), 500, DirectionPassed ? FColor::Magenta : FColor::Cyan, false, DebugDrawTime, 0, 5);

					if (!DirectionPassed)
						bCheckFailed = true;
				}

				//if(!bComponentPassed)
				if (bCheckFailed && (!bDrawDebug || !bPrintDebug || !LightSourceComponent.bDrawDebug))
				{
					TargetPassed = false;
					continue;
				}

				if (bCheckAttenuation)
				{
					AttenuationPassed = CheckAttenuation(*LightComponent, Start, End, bIsRectLight);
#if UE_ENABLE_DEBUG_DRAWING
					if (bDrawDebug && LightSourceComponent.bDrawDebug && !IsFromThread)
					{
						float Attenuation = 0;
						if (bIsSpotLight)
						{
							Attenuation = Cast<USpotLightComponent>(LightComponent)->AttenuationRadius;
							DrawDebugCone(GetWorld(), End, LightComponent->GetForwardVector(), Attenuation, FMath::DegreesToRadians(Cast<USpotLightComponent>(LightComponent)->OuterConeAngle), FMath::DegreesToRadians(Cast<USpotLightComponent>(LightComponent)->OuterConeAngle), FMath::Clamp<int32>(Cast<USpotLightComponent>(LightComponent)->AttenuationRadius / 4.f, 8, 32), AttenuationPassed ? FColor::Green : FColor::Red, false, DebugDrawTime);

							// Draw point light source shape
							// DrawWireCapsule(PDI, TransformNoScale.GetTranslation(), TransformNoScale.GetUnitAxis( EAxis::X ), TransformNoScale.GetUnitAxis( EAxis::Y ), TransformNoScale.GetUnitAxis( EAxis::Z ),
							// 				FColor(231, 239, 0, 255), SpotLightComp->SourceRadius, 0.5f * SpotLightComp->SourceLength + SpotLightComp->SourceRadius, 25, SDPG_World);

							// Draw outer light cone
							// DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->OuterConeAngle, 32, 8, 10, FColor(200, 255, 255), SDPG_World);
							// DrawDebugCone(GetWorld(), End, LightComponent->GetForwardVector(), Attenuation, FMath::DegreesToRadians(45.f), FMath::DegreesToRadians(45.f), FMath::Clamp<int32>(Cast<USpotLightComponent>(LightComponent)->AttenuationRadius / 4.f, 8, 32), AttenuationPassed ? FColor::Green : FColor::Red, false, DebugDrawTime);


							// Draw inner light cone (if non zero)
							// if(SpotLightComp->InnerConeAngle > KINDA_SMALL_NUMBER)
							// {
							// 	DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->InnerConeAngle, 32, 8, 10, FColor(150, 200, 255), SDPG_World);
							// }
						}
						else
						{
							if (bIsRectLight)
								Attenuation = Cast<URectLightComponent>(LightComponent)->AttenuationRadius;
							else if (bIsPointLight)
								Attenuation = Cast<UPointLightComponent>(LightComponent)->AttenuationRadius;
							DrawDebugSphere(GetWorld(), End, Attenuation, FMath::Clamp<int32>(Attenuation / 4.f, 8, 32), AttenuationPassed ? FColor::Cyan : FColor::Magenta, false, DebugDrawTime);
						}
					}
#endif

					if (!AttenuationPassed)
						bCheckFailed = true;
				}

				// if(!bComponentPassed)
				if (bCheckFailed && (!bDrawDebug || !bPrintDebug || !LightSourceComponent.bDrawDebug))
				{
					TargetPassed = false;
					continue;
				}

				if (bCheckIfInsideSpotOrRect)
				{
					InsideSpotOrRectPassed = CheckIfInsideSpotOrRect(*LightComponent, Start, End, bIsSpotLight);
					if (!InsideSpotOrRectPassed)
						bCheckFailed = true;
				}

				// if(!bComponentPassed)
				if (bCheckFailed && (!bDrawDebug || !bPrintDebug || !LightSourceComponent.bDrawDebug))
				{
					TargetPassed = false;
					continue;
				}

#if UE_ENABLE_DEBUG_DRAWING
				if (bCheckFailed && (bDrawDebug && bPrintDebug && LightSourceComponent.bDrawDebug))
				{
					FString StrDistancePassed = DistancePassed ? TEXT("") : TEXT(" Distance");
					FString StrDirectionPassed = DirectionPassed ? TEXT("") : TEXT(" Direction");
					FString StrAttenuationPassed = AttenuationPassed ? TEXT("") : TEXT(" Attenuation");
					FString StrInsideSpotOrRectPassed = InsideSpotOrRectPassed ? TEXT("") : TEXT(" InsideSpotOrRectPassed");

					FString FailedTests;
					if (bIsPointLight)
						FailedTests = StrDistancePassed + StrAttenuationPassed;
					if (bIsSpotLight || bIsRectLight)
						FailedTests = StrDistancePassed + StrDirectionPassed + StrAttenuationPassed + StrInsideSpotOrRectPassed;

					UE_LOG(LogLightSystem, Warning, TEXT("%s: %s fails checks %s"), *GetOwner()->GetName(), *LightSourceComponent.GetOwner()->GetName(), *FailedTests)
					TargetPassed = false;
					continue;
				}
#endif


				if (bIsPointLight)
				{
					if (AttenuationPassed && DistancePassed)
						TargetPassed = true;
				}
				if (bIsSpotLight)
				{
					if (AttenuationPassed && DirectionPassed && InsideSpotOrRectPassed && DistancePassed)
						TargetPassed = true;
				}

				if (bIsRectLight)
				{
					if (AttenuationPassed && DirectionPassed && InsideSpotOrRectPassed && DistancePassed)
						TargetPassed = true;
				}
			}
			if (TargetPassed)
			{
				PassedChecks++;
				PassedComponents.AddUnique(ComponentIdx);
			}
		}


		ComponentIdx++;

		if (PassedChecks >= RequiredChecksToPassAmount)
			Passed = true;
	}

	if (!Passed)
		PassedComponents.Empty();

	return Passed;
}

bool ULXRDetectionComponent::CheckVisibility(const TArray<ULightComponent*>& LightComponents, const TArray<int>& PassedComponents, TArray<int>& PassedTargets, bool IsLightSenseCheck)
{
	TArray<FVector> TraceTargets;
	TraceTargets = GetTraceTargets(true);

	int PassedChecks = 0;
	const float RequiredChecksToPassAmount = TraceTargets.Num() * TracesRequired;

	for (const auto ComponentIndex : PassedComponents)
	{
		const ULightComponent* LightComponent = LightComponents[ComponentIndex];

		for (int i = 0; i < TraceTargets.Num(); ++i)
		{
			const int ThisLoopPassedChecks = PassedChecks;
			FVector TraceTarget = TraceTargets[i];
			FVector Start = TraceTarget;
			FVector End;
			TArray<AActor*> ActorsToIgnore;
			ActorsToIgnore.Append(Cast<ULXRSourceComponent>(LightComponent->GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->GetMyOverlappingActors());
			ActorsToIgnore.Append(Cast<ULXRSourceComponent>(LightComponent->GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->GetIgnoreVisibilityActors());
			ActorsToIgnore.Append(IgnoreVisibilityActors);

			ActorsToIgnore.AddUnique(GetOwner());
			ActorsToIgnore.AddUnique(LightComponent->GetOwner());

			INC_DWORD_STAT(STAT_TRACESSYNC);
			if (LightComponent->IsA(UDirectionalLightComponent::StaticClass()))
			{
				FVector DirectionalForwardInverse = LightComponent->GetForwardVector() * -1;
				End = Start + DirectionalForwardInverse.GetSafeNormal() * 15000;
			}
			else
			{
				End = LightComponent->GetComponentLocation();
			}

			FCollisionQueryParams P = GetCollisionQueryParams(ActorsToIgnore);
			if (!GetWorld()->LineTraceTestByChannel(Start, End, TraceChannel, P))
			{
				PassedChecks++;
			}

#if UE_ENABLE_DEBUG_DRAWING
			else if (bDrawDebug && Cast<ULXRSourceComponent>(LightComponent->GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->bDrawDebug)
			{
				DrawDebugLine(GetWorld(), Start, End, ThisLoopPassedChecks == PassedChecks ? FColor::Red : FColor::Green, false, DebugDrawTime, 0, 1);
				DrawDebugSphere(GetWorld(), LightComponent->GetComponentLocation(), 15, 12, FColor::Green, false, DebugDrawTime);

				FHitResult result;
				if (GetWorld()->LineTraceSingleByChannel(result, Start, End, TraceChannel, P))
				{
					DrawDebugBox(GetWorld(), result.Location, FVector(10), FColor::Red, false, DebugDrawTime, 0, 2);
					if (bPrintDebug)
					{
						TWeakObjectPtr<AActor> HitActor = NULL;
#if ENGINE_MAJOR_VERSION == 5
						if (result.GetActor())
							HitActor = result.GetActor();
#else
						if (result.Actor.IsValid())
							HitActor = result.Actor;
#endif
						if (HitActor.IsValid())
							UE_LOG(LogLightSystem, Error, TEXT("%s : RAY HIT %s "), *GetOwner()->GetName(), *HitActor->GetName());
					}
				}
			}
#endif
		}
	}
	return PassedChecks >= RequiredChecksToPassAmount;
}

ULXRSourceComponent* ULXRDetectionComponent::GetCurrentLightSourceComponentByType(const ELightArrayType LightArrayType) const
{
	const int Index = GetCurrentLightArrayIndexByLightArrayType(LightArrayType);
	return GetLightSourceComponentByTypeAndIndex(LightArrayType, Index);
}

ULightComponent* ULXRDetectionComponent::GetCurrentLightComponentByType(const ELightArrayType LightArrayType) const
{
	const ULXRSourceComponent* LightSourceComponent = GetCurrentLightSourceComponentByType(LightArrayType);
	ULightComponent* LightComponent = NULL;

	if (LightArrayType == ELightArrayType::All)
	{
		if (LightSourceComponent->GetMyLightComponents().IsValidIndex(AllLightSourceLightActorComponentIndex))
			LightComponent = LightSourceComponent->GetMyLightComponents()[AllLightSourceLightActorComponentIndex];
	}
	else
	{
		if (LightSourceComponent->GetMyLightComponents().IsValidIndex(RelevantLightSourceActorLightComponentIndex))
			LightComponent = LightSourceComponent->GetMyLightComponents()[RelevantLightSourceActorLightComponentIndex];
	}
	return LightComponent;
}

ULXRSourceComponent* ULXRDetectionComponent::GetLightSourceComponentByTypeAndIndex(const ELightArrayType LightArrayType, int Index) const
{
	const AActor* LightSource = NULL;

	switch (LightArrayType)
	{
		case ELightArrayType::All:
			{
				if (AllLights.IsValidIndex(Index))
					LightSource = AllLights[Index].Get();
			}
			break;
		case ELightArrayType::Relevant:
			{
				if (RelevantLights.IsValidIndex(Index))
					LightSource = RelevantLights[Index].Get();
			}
			break;
		case ELightArrayType::SmartFar:
			{
				if (SmartFarLights.IsValidIndex(Index))
					LightSource = SmartFarLights[Index].Get();
			}
			break;
		case ELightArrayType::SmartMid:
			{
				if (SmartMidLights.IsValidIndex(Index))
					LightSource = SmartMidLights[Index].Get();
			}
			break;
		case ELightArrayType::SmartNear:
			{
				if (SmartNearLights.IsValidIndex(Index))
					LightSource = SmartNearLights[Index].Get();
			}
			break;
		default: ;
	}

	if (!IsValid(LightSource))
	{
		UE_LOG(LogLightSystem, VeryVerbose, TEXT("DetectionComponentOwner :%s \n LightSource is not valid:  "), *GetOwner()->GetName());
		return NULL;
	}

	ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightSource->GetComponentByClass(ULXRSourceComponent::StaticClass()));
	return LightSourceComponent;
}

void ULXRDetectionComponent::IncreaseFailCount(const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	RemovePassedLight(LightSourceOwner);
	if (!RelevantLightsFailCounts.Contains(LightSourceOwner))
		RelevantLightsFailCounts.Add(LightSourceOwner, 0);

	int Fails = RelevantLightsFailCounts[LightSourceOwner];
	Fails++;
	RelevantLightsFailCounts[LightSourceOwner] = Fails;
}

void ULXRDetectionComponent::IncreaseFailCountIfNotAlwaysRelevantLightFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	FRWScopeLock RelevantLightLock(RelevantDataLockObject, SLT_Write);
	IncreaseFailCount(LightSourceOwner);
}

void ULXRDetectionComponent::CheckAndRemoveIfLightNotRelevant(const TWeakObjectPtr<AActor>& LightSourceOwner, bool IsFromThread)
{
	const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightSourceOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()));
	TArray<int> PassedComponents;
	TArray<int> PassedTargets;
	if (!CheckIsLightRelevant(*LightSourceComponent, PassedComponents, PassedTargets, false, IsFromThread))
	{
		RelevantLightsToRemove.AddUnique(LightSourceOwner);
		RelevantLightsFailCounts.Remove(LightSourceOwner);

		if (RelevancyCheckType == ERelevancyCheckType::Smart)
		{
			const ELightArrayType SmartArrayType = GetSmartArrayTypeForLightFromSqrDistance(FVector::DistSquared(LightSourceOwner.Get()->GetActorLocation(), GetOwner()->GetActorLocation()));
			AddToSmartArrayBySmartArrayType(SmartArrayType, *LightSourceOwner);
		}
	}
}

void ULXRDetectionComponent::CheckAndRemoveIfLightNotRelevantFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	FRWScopeLock RelevantLightLock(RelevantDataLockObject, SLT_Write);
	CheckAndRemoveIfLightNotRelevant(LightSourceOwner, true);
}


void ULXRDetectionComponent::AddNewLights()
{
	for (TWeakObjectPtr<AActor> NewLight : NewAllLightsToAdd)
	{
		if (NewLight.IsValid())
		{
			if (RelevancyCheckType == ERelevancyCheckType::Smart)
			{
				const float DistSqr = FVector::DistSquared(NewLight.Get()->GetActorLocation(), GetOwner()->GetActorLocation());
				if (DistSqr > RelevancySmartDistanceMax * RelevancySmartDistanceMax)
					SmartFarLights.AddUnique(NewLight);
				if (DistSqr > RelevancySmartDistanceMin * RelevancySmartDistanceMin && DistSqr < RelevancySmartDistanceMax * RelevancySmartDistanceMax)
					SmartMidLights.AddUnique(NewLight);
				if (DistSqr < RelevancySmartDistanceMin * RelevancySmartDistanceMin)
					SmartNearLights.AddUnique(NewLight);
			}

			AllLights.AddUnique(NewLight);
		}
	}
	NewAllLightsToAdd.Empty();
}


void ULXRDetectionComponent::RemoveNonRelevantLights()
{
	for (TWeakObjectPtr<AActor> LightToRemove : RelevantLightsToRemove)
	{
		if (RelevantLights.Contains(LightToRemove))
			RelevantLights.RemoveSwap(LightToRemove);

		RemovePassedLight(LightToRemove);
	}

	RelevantLightsToRemove.Empty();
}

void ULXRDetectionComponent::AddNewRelevantLights()
{
	for (TWeakObjectPtr<AActor> LightToAdd : NewRelevantLightsToAdd)
	{
		if (!RelevantLights.Contains(LightToAdd))
			RelevantLights.AddUnique(LightToAdd);
	}

	NewRelevantLightsToAdd.Empty();
}

void ULXRDetectionComponent::RemoveRedundantLights()
{
	for (TWeakObjectPtr<AActor> RedundantLight : LightsToRemove)
	{
		if (RedundantLight.IsStale() || RedundantLight.IsValid())
		{
			if (AllLights.Contains(RedundantLight))
				AllLights.RemoveSwap(RedundantLight);
			if (RelevantLights.Contains(RedundantLight))
				RelevantLightsToRemove.AddUnique(RedundantLight);
			if (SmartNearLights.Contains(RedundantLight))
				SmartNearLights.RemoveSwap(RedundantLight);
			if (SmartMidLights.Contains(RedundantLight))
				SmartMidLights.RemoveSwap(RedundantLight);
			if (SmartFarLights.Contains(RedundantLight))
				SmartFarLights.RemoveSwap(RedundantLight);
		}
		// else
		// {
		// 	if (RedundantLight.IsStale())
		// 	{
		// 		UE_LOG(LogLightSystem, Log, TEXT("LightSource is STALE!... "));
		// 	}
		// }
	}

	LightsToRemove.Empty();
}

void ULXRDetectionComponent::RemoveAllStaleLights()
{
	RemoveStaleLightsByLightArrayType(ELightArrayType::All);
	RemoveStaleLightsByLightArrayType(ELightArrayType::SmartFar);
	RemoveStaleLightsByLightArrayType(ELightArrayType::SmartMid);
	RemoveStaleLightsByLightArrayType(ELightArrayType::SmartNear);
	RemoveStaleLightsByLightArrayType(ELightArrayType::Relevant);
}

void ULXRDetectionComponent::RemoveStaleLightsByLightArrayType(ELightArrayType LightArrayType)
{
	TArray<TWeakObjectPtr<AActor>> Array = GetLightArrayByLightArrayType(LightArrayType);
	TArray<int> StaleLightsIndexes;

	for (int i = 0; i < Array.Num(); ++i)
	{
		const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(Array[i].Get()->GetComponentByClass(ULXRSourceComponent::StaticClass()));
		if (!IsValid(LightSourceComponent))
		{
			if (Array[i].IsStale())
			{
				StaleLightsIndexes.AddUnique(i);
			}
		}
	}

	if (StaleLightsIndexes.Num() > 0)
	{
		for (int i = 0; i < StaleLightsIndexes.Num(); ++i)
		{
			if (Array.IsValidIndex(StaleLightsIndexes[i]))
			{
				if (Array[StaleLightsIndexes[i]].IsStale())
				{
					Array.RemoveAtSwap(StaleLightsIndexes[i]);
				}
			}
		}
	}
}

void ULXRDetectionComponent::RemovePassedLight(const TWeakObjectPtr<AActor>& LightSourceOwner)
{
	const int Index = RelevantLightsPassed.Find(LightSourceOwner);
	if (Index != INDEX_NONE)
	{
		RelevantLightsPassed.RemoveAtSwap(Index);
		if (LightSourceOwner.IsValid())
		{
			ULXRSourceComponent* LxrSourceComponent = Cast<ULXRSourceComponent>(LightSourceOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()));
			if (LxrSourceComponent->bAddDetected && bAddToSourceWhenDetected)
				LxrSourceComponent->DetectedActors.RemoveSwap(GetOwner());

			// OnLightCheckChanged.Broadcast(RelevantLightsPassed.Num(), LxrSourceComponent);
		}
	}
}

void ULXRDetectionComponent::LightPassed(const TWeakObjectPtr<AActor>& LightSourceOwner, const TArray<int>& PassedComponents)
{
	// FRWScopeLock RelevantLightLock(RelevantDataLockObject, SLT_Write);
	const int Index = RelevantLightsPassed.Find(LightSourceOwner);
	ULXRSourceComponent* LxrSourceComponent = Cast<ULXRSourceComponent>(LightSourceOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()));
	if (Index == INDEX_NONE)
	{
		RelevantLightsPassed.Add(LightSourceOwner);
		if (LxrSourceComponent->bAddDetected && bAddToSourceWhenDetected)
			LxrSourceComponent->DetectedActors.AddUnique(GetOwner());
		// OnLightCheckChanged.Broadcast(RelevantLightsPassed.Num(), LxrSourceComponent);
	}

	if (!LightsPassedComponents.Contains(LightSourceOwner))
		LightsPassedComponents.Add(LightSourceOwner);

	LightsPassedComponents[LightSourceOwner] = PassedComponents;
	// LightsPassedComponents.Add(LxrSourceComponent,PassedComponents);
	// LxrSourceComponent->AddPassedComponentIndexes(PassedComponents);

	if (RelevantLightsFailCounts.Contains(LightSourceOwner))
		RelevantLightsFailCounts.Remove(LightSourceOwner);
}

void ULXRDetectionComponent::LightPassedFromThread(const TWeakObjectPtr<AActor>& LightSourceOwner, const TArray<int>& PassedComponents)
{
	FRWScopeLock RelevantLightLock(RelevantDataLockObject, SLT_Write);
	LightPassed(LightSourceOwner, PassedComponents);
}

TArray<FVector> ULXRDetectionComponent::GetRelevantTraceTypeTargets() const
{
	return GetTraceTargets(true);
}

TArray<AActor*> ULXRDetectionComponent::GetPassedLights() const
{
	TArray<AActor*> ReturnList;
	for (auto It = RelevantLightsPassed.CreateConstIterator(); It; ++It)
	{
		ReturnList.Add(It->Get());
	}

	return ReturnList;
}

TArray<ULightComponent*> ULXRDetectionComponent::GetPassedLightComponents(AActor* LightSourceOwner)
{
	if (!IsValid(LightSourceOwner)) return {};
	TArray<ULightComponent*> ReturnList;
	TArray<ULightComponent*> LightSourceComponents = Cast<ULXRSourceComponent>(LightSourceOwner->GetComponentByClass(ULXRSourceComponent::StaticClass()))->GetMyLightComponents();
	auto ComponentIndexes = LightsPassedComponents[LightSourceOwner];
	for (const auto Idx : ComponentIndexes)
	{
		ReturnList.Add(LightSourceComponents[Idx]);
	}

	return ReturnList;
}

TArray<FVector> ULXRDetectionComponent::GetTraceTargets(const bool& bIsRelevant, const ETraceTarget TargetOverride) const
{
	// TArray<FVector,TFixedAllocator<64>> Temp;
	TArray<FVector> Temp;

	const ETraceTarget TargetType = TargetOverride != ETraceTarget::None ? TargetOverride : bIsRelevant ? RelevantTargetType : RelevancyTargetType;
	switch (TargetType)
	{
		case ETraceTarget::ActorLocation:
			{
				Temp.AddUnique(GetOwner()->GetActorLocation());
				break;
			}
		case ETraceTarget::Sockets:
			{
				if (TargetSockets.Num() > 0)
				{
					if (IsValid(SkeletalMeshComponent))
					{
						for (FName Socket : TargetSockets)
						{
							if (SkeletalMeshComponent->DoesSocketExist(Socket))
							{
								Temp.AddUnique(SkeletalMeshComponent->GetSocketLocation(Socket));
							}
							else
							{
								UE_LOG(LogLightSystem, Warning, TEXT("Socket %s does not exist on %s"), *Socket.ToString(), *GetOwner()->GetName())
							}
						}
					}
				}
			}
			break;

		case ETraceTarget::VectorArray:
			{
				for (auto Vector : TargetVectors)
				{
					Temp.Add(GetOwner()->GetTransform().TransformPosition(Vector));
				}
			}
			break;
		case ETraceTarget::ActorBounds:
			{
				FVector Origin;
				FVector Extent;
				GetOwner()->GetActorBounds(true, Origin, Extent);
				Extent = Extent / 1.2;
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(0, 0, Extent.Z)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(0, 0, -Extent.Z + 15)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(0, Extent.Y * 0.5, Extent.Z * 0.1)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(0, -Extent.Y * 0.5, Extent.Z * 0.1)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(0, -Extent.Y * 0.5, Extent.Z * 0.1)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(Extent.X * 0.5, 0, 0)));
				Temp.AddUnique(GetOwner()->GetTransform().TransformPosition(FVector(-Extent.X * 0.5, 0, 0)));
			}
			break;
		case ETraceTarget::None:
			break;
		default:
			break;
	}
	return Temp;
}

bool ULXRDetectionComponent::CheckDirection(const ULightComponent& LightComponent, const FVector& Start, const FVector& End) const
{
	const float Dot = FVector::DotProduct(LightComponent.GetForwardVector(), (Start - End).GetSafeNormal());
	if (Dot > 0)
		return true;
	return false;
}

bool ULXRDetectionComponent::CheckAttenuation(const ULightComponent& LightComponent, const FVector& Start, const FVector& End, bool IsRect) const
{
	const float DistanceSqr = FVector::DistSquared(Start, End);
	float Attenuation;

	if (IsRect)
		Attenuation = Cast<URectLightComponent>(&LightComponent)->AttenuationRadius;
	else
		Attenuation = Cast<UPointLightComponent>(&LightComponent)->AttenuationRadius;

	const float AttenuationSQR = Attenuation * Attenuation;
	if (bPrintDebug && Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()))->bDrawDebug)
	{
		//UE_LOG(LogLightSystem, Warning, TEXT("DistanceSQR %f AttenuationSQR %f"), DistanceSqr, AttenuationSQR);
	}
	if (DistanceSqr < AttenuationSQR)
		return true;
	return false;
}

bool ULXRDetectionComponent::CheckIfInsideSpotOrRect(const ULightComponent& LightComponent, const FVector& Start, const FVector& End, bool IsSpot) const
{
	auto CheckForLinePlaneIntersectionAndFindClosestPointOnSegment([&](const FVector& LineStart, const FVector& LineEnd, const FVector& PlaneNormal, const FVector& EdgeStart, const FVector& EdgeEnd, FVector& Intersection)
	{
		const FVector RayDir = LineEnd - LineStart;
		float T;
		// Check ray is not parallel to plane
		if ((RayDir | PlaneNormal) == 0.0f)
		{
			Intersection = FVector::ZeroVector;
			return false;
		}

		T = (((EdgeEnd - LineStart) | PlaneNormal) / (RayDir | PlaneNormal));

		// Check intersection is not outside line segment
		if (T < 0.0f || T > 1.0f)
		{
			Intersection = FVector::ZeroVector;
			return false;
		}

		// Calculate intersection point
		Intersection = LineStart + RayDir * T;

		const FVector ClosestPoint = FMath::ClosestPointOnSegment(Intersection, EdgeStart, EdgeEnd);
		if (ClosestPoint.Equals(Intersection, 0.01))
			return true;

		return false;
	});

	auto CheckAngle([](const FVector& ReferenceDirection, const FVector& Start, const FVector& End, const float& TargetAngle)
	{
		const FVector DirectionToTarget = (End - Start).GetSafeNormal();

		const float Dot = FVector::DotProduct(ReferenceDirection, DirectionToTarget);
		const float Angle = FMath::RadiansToDegrees(acosf(Dot));

		if (Angle < TargetAngle)
			return true;

		return false;
	});

	auto DrawBarnRect = [&](const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3)
	{
		DrawDebugLine(GetWorld(), P0, P1, FColor::Yellow, false, DebugDrawTime, 0, 0);
		DrawDebugLine(GetWorld(), P1, P3, FColor::Yellow, false, DebugDrawTime, 0, 0);
		DrawDebugLine(GetWorld(), P3, P2, FColor::Yellow, false, DebugDrawTime, 0, 0);
		DrawDebugLine(GetWorld(), P2, P0, FColor::Yellow, false, DebugDrawTime, 0, 0);
	};

	const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightComponent.GetOwner()->GetComponentByClass(ULXRSourceComponent::StaticClass()));

	if (IsSpot)
	{
		const FVector Forward = LightComponent.GetForwardVector();
		const float OuterConeAngle = Cast<USpotLightComponent>(&LightComponent)->OuterConeAngle;

		const bool bCheckSpotAngle = CheckAngle(Forward, End, Start, OuterConeAngle);

		return bCheckSpotAngle;
	}

	{
		const FVector DetectionProjectedToRectPlane = FVector::PointPlaneProject(Start, End, LightComponent.GetForwardVector());
		FVector Test = LightComponent.GetComponentTransform().TransformPosition(LightComponent.GetComponentTransform().InverseTransformPosition(DetectionProjectedToRectPlane) * -1);

		const float SourceHeight = Cast<URectLightComponent>(&LightComponent)->SourceHeight;
		const float SourceWidth = Cast<URectLightComponent>(&LightComponent)->SourceWidth;
		const float BarnDoorAngle = Cast<URectLightComponent>(&LightComponent)->BarnDoorAngle;
		const float BarnDoorLength = Cast<URectLightComponent>(&LightComponent)->BarnDoorLength;
		const float BarnMaxAngle = GetRectLightBarnDoorMaxAngle();
		const float AngleRad = FMath::DegreesToRadians(FMath::Clamp(BarnDoorAngle, 0.f, BarnMaxAngle));
		const float BarnDepth = FMath::Cos(AngleRad) * BarnDoorLength;
		const float BarnExtent = FMath::Sin(AngleRad) * BarnDoorLength;
		const FVector AbsDetectionProjectedToRectPlaneInverse = LightComponent.GetComponentToWorld().InverseTransformPosition(DetectionProjectedToRectPlane).GetAbs();

		const FVector V1 = LightComponent.GetComponentTransform().TransformPosition(FVector(0.0f, +0.5f * SourceWidth, +0.5f * SourceHeight));
		const FVector V2 = LightComponent.GetComponentTransform().TransformPosition(FVector(0.0f, +0.5f * SourceWidth, -0.5f * SourceHeight));
		const FVector V3 = LightComponent.GetComponentTransform().TransformPosition(FVector(0.0f, -0.5f * SourceWidth, +0.5f * SourceHeight));
		const FVector V4 = LightComponent.GetComponentTransform().TransformPosition(FVector(0.0f, -0.5f * SourceWidth, -0.5f * SourceHeight));
		const FVector BarnV1 = LightComponent.GetComponentTransform().TransformPosition(FVector(BarnDepth, +0.5f * SourceWidth + BarnExtent, +0.5f * SourceHeight + BarnExtent));
		const FVector BarnV2 = LightComponent.GetComponentTransform().TransformPosition(FVector(BarnDepth, +0.5f * SourceWidth + BarnExtent, -0.5f * SourceHeight - BarnExtent));
		const FVector BarnV3 = LightComponent.GetComponentTransform().TransformPosition(FVector(BarnDepth, -0.5f * SourceWidth - BarnExtent, +0.5f * SourceHeight + BarnExtent));
		const FVector BarnV4 = LightComponent.GetComponentTransform().TransformPosition(FVector(BarnDepth, -0.5f * SourceWidth - BarnExtent, -0.5f * SourceHeight - BarnExtent));
#if UE_ENABLE_DEBUG_DRAWING
		if (bDrawDebug && LightSourceComponent->bDrawDebug)
		{
			DrawDebugLine(GetWorld(), Start, DetectionProjectedToRectPlane, FColor::Orange, false, DebugDrawTime, 0, 0);
			DrawDebugLine(GetWorld(), End, DetectionProjectedToRectPlane, FColor::Cyan, false, DebugDrawTime, 0, 0);


			DrawDebugPoint(GetWorld(), Test, 20, FColor::Emerald, false, DebugDrawTime);

			DrawDebugPoint(GetWorld(), V1, 20, FColor::Red, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), V2, 20, FColor::Green, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), V3, 20, FColor::Blue, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), V4, 20, FColor::White, false, DebugDrawTime);

			DrawDebugPoint(GetWorld(), BarnV1, 20, FColor::Red, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), BarnV2, 20, FColor::Green, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), BarnV3, 20, FColor::Blue, false, DebugDrawTime);
			DrawDebugPoint(GetWorld(), BarnV4, 20, FColor::White, false, DebugDrawTime);

			DrawBarnRect(V1, V2, V3, V4);
			DrawBarnRect(BarnV1, BarnV2, BarnV3, BarnV4);
			DrawBarnRect(BarnV1, BarnV2, V1, V2);
			DrawBarnRect(V3, V4, BarnV3, BarnV4);
			DrawBarnRect(V1, V3, BarnV1, BarnV3);
			DrawBarnRect(V4, V2, BarnV4, BarnV2);
		}
#endif
		// UE_LOG(LogLightSystem, Warning, TEXT("DetectionPlane:%s, H:%f , W:%f" ), *AbsDetectionProjectedToRectPlaneInverse.ToString(), SourceHeight/2, SourceWidth/2);

		if (AbsDetectionProjectedToRectPlaneInverse.Z < SourceHeight / 2 && AbsDetectionProjectedToRectPlaneInverse.Y < SourceWidth / 2)
			return true;

		//EDGE1 V1 V3
		//EDGE2 V2 V4
		//EDGE3 V1 V2
		//EDGE4 V3 V4

		FVector EdgeIntersection;

		const auto Edge1Check = CheckForLinePlaneIntersectionAndFindClosestPointOnSegment(End, Test, LightComponent.GetUpVector(), V1, V3, EdgeIntersection);
		if (Edge1Check)
		{
			const FVector BarnMid = ((BarnV2 - BarnV4) * 0.5f) + BarnV4;
			const float Dot = FVector::DotProduct(LightComponent.GetForwardVector(), (BarnMid - EdgeIntersection).GetSafeNormal());
			const float Angle = FMath::RadiansToDegrees(acosf(Dot));

			const bool AnglePassed = CheckAngle(LightComponent.GetForwardVector(), EdgeIntersection, Start, Angle);
#if UE_ENABLE_DEBUG_DRAWING
			if (bDrawDebug && LightSourceComponent->bDrawDebug)
			{
				DrawDebugPoint(GetWorld(), BarnMid, 20, FColor::Cyan, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), End, EdgeIntersection, FColor::Magenta, false, DebugDrawTime, 0, 0);
				DrawDebugPoint(GetWorld(), EdgeIntersection, 20, FColor::Purple, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), V1, V3, FColor::Yellow, false, DebugDrawTime, 0, 0);
				DrawDebugLine(GetWorld(), EdgeIntersection, Start, AnglePassed ? FColor::Green : FColor::Red, false, DebugDrawTime, 0, 1);
			}
#endif
			return AnglePassed;
		}

		const auto Edge2Check = CheckForLinePlaneIntersectionAndFindClosestPointOnSegment(End, Test, LightComponent.GetUpVector() * -1, V2, V4, EdgeIntersection);
		if (Edge2Check)
		{
			const FVector BarnMid = ((BarnV1 - BarnV3) * 0.5f) + BarnV3;
			const float Dot = FVector::DotProduct(LightComponent.GetForwardVector(), (BarnMid - EdgeIntersection).GetSafeNormal());
			const float Angle = FMath::RadiansToDegrees(acosf(Dot));
			const bool AnglePassed = CheckAngle(LightComponent.GetForwardVector(), EdgeIntersection, Start, Angle);
#if UE_ENABLE_DEBUG_DRAWING
			if (bDrawDebug && LightSourceComponent->bDrawDebug)
			{
				DrawDebugPoint(GetWorld(), BarnMid, 20, FColor::Cyan, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), End, EdgeIntersection, FColor::Magenta, false, DebugDrawTime, 0, 0);
				DrawDebugPoint(GetWorld(), EdgeIntersection, 20, FColor::Purple, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), V2, V4, FColor::Blue, false, DebugDrawTime, 0, 0);
				DrawDebugLine(GetWorld(), EdgeIntersection, Start, AnglePassed ? FColor::Green : FColor::Red, false, DebugDrawTime, 0, 0);
			}
#endif
			return AnglePassed;
		}

		const auto Edge3Check = CheckForLinePlaneIntersectionAndFindClosestPointOnSegment(End, Test, LightComponent.GetRightVector(), V1, V2, EdgeIntersection);
		if (Edge3Check)
		{
			const FVector BarnMid = ((BarnV3 - BarnV4) * 0.5f) + BarnV4;
			const float Dot = FVector::DotProduct(LightComponent.GetForwardVector(), (BarnMid - EdgeIntersection).GetSafeNormal());
			const float Angle = FMath::RadiansToDegrees(acosf(Dot));

			const bool AnglePassed = CheckAngle(LightComponent.GetForwardVector(), EdgeIntersection, Start, Angle);
#if UE_ENABLE_DEBUG_DRAWING
			if (bDrawDebug && LightSourceComponent->bDrawDebug)
			{
				DrawDebugPoint(GetWorld(), BarnMid, 20, FColor::Cyan, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), Test, EdgeIntersection, FColor::Magenta, false, DebugDrawTime, 0, 0);
				DrawDebugPoint(GetWorld(), EdgeIntersection, 20, FColor::Purple, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), End, Test, FColor::Purple, false, DebugDrawTime, 0, 0);
				DrawDebugLine(GetWorld(), EdgeIntersection, Start, AnglePassed ? FColor::Green : FColor::Red, false, DebugDrawTime, 0, 0);
			}
#endif

			return AnglePassed;
		}
		const auto Edge4Check = CheckForLinePlaneIntersectionAndFindClosestPointOnSegment(End, Test, LightComponent.GetRightVector() * -1, V3, V4, EdgeIntersection);
		if (Edge4Check)
		{
			const FVector BarnMid = ((BarnV1 - BarnV2) * 0.5f) + BarnV2;
			const float Dot = FVector::DotProduct(LightComponent.GetForwardVector(), (BarnMid - EdgeIntersection).GetSafeNormal());
			const float Angle = FMath::RadiansToDegrees(acosf(Dot));

			const bool AnglePassed = CheckAngle(LightComponent.GetForwardVector(), EdgeIntersection, Start, Angle);
#if UE_ENABLE_DEBUG_DRAWING
			if (bDrawDebug && LightSourceComponent->bDrawDebug)
			{
				DrawDebugPoint(GetWorld(), BarnMid, 20, FColor::Cyan, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), End, EdgeIntersection, FColor::Magenta, false, DebugDrawTime, 0, 0);
				DrawDebugPoint(GetWorld(), EdgeIntersection, 20, FColor::Purple, false, DebugDrawTime);
				DrawDebugLine(GetWorld(), V3, V4, FColor::Emerald, false, DebugDrawTime, 0, 0);
				DrawDebugLine(GetWorld(), EdgeIntersection, Start, AnglePassed ? FColor::Green : FColor::Red, false, DebugDrawTime, 0, 0);
			}
#endif
			return AnglePassed;
		}
	}

	return false;
}

// void ULXRDetectionComponent::PrintMyNearbyElements()
// {
// 	TArray<FLXROctreeElement> OctreeElements;
// 	// ULXRSubsystem* LXRSubsystem = GetOwner()->GetWorld()->GetSubsystem<ULXRSubsystem>();
// 	// if (!LXRSubsystem->GetOctree().IsValid()) return;
// 	// LXRSubsystem->GetOctree()->FindNearbyElements(GetOwner()->GetActorLocation(), [&OctreeElements](const FLXROctreeElement& Element)
// 	// {
// 	// 	OctreeElements.Add(Element);
// 	// });
// 	//
// 	// TArray<FString> ElementNames;
// 	// int num = 0;
// 	// for (FLXROctreeElement Element : OctreeElements)
// 	// {
// 	// 	if (IsValid(Element.GetOwner()))
// 	// 	{
// 	// 		ElementNames.Add(Element.GetOwner()->GetName());
// 	// 		num++;
// 	// 	}
// 	// }
//
//
// 	// if (!LXRSubsystem->GetOctree().IsValid()) return;
// 	// LXRSubsystem->GetOctree()->FindElementsWithBoundsTest(boundtest, [&OctreeElements](const FLXROctreeElement& Element)
// 	// {
// 	// 	OctreeElements.Add(Element);
// 	// });
// 	//
// 	// // UE_LOG(LogLightSystem, Log, TEXT("My Nearby sources amount %d"), num);
// 	// float DeltaTime = GetWorld()->DeltaTimeSeconds;
// 	// // GEngine->AddOnScreenDebugMessage(1, DeltaTime, FColor::Red, ("LXROctreeVolume already in level \n Make sure there is only one LXROctreeVolume!"));
// 	// FVector ActorLocation = GetOwner()->GetActorLocation();
// 	// for (auto Element : OctreeElements)
// 	// {
// 	// 	GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, Element.GetOwner()->GetName());
// 	// 	DrawDebugDirectionalArrow(GetWorld(), ActorLocation, Element.Data.Get().SourceObject->GetActorLocation(), 150, FColor::Cyan, false, 0.1, 0, 1);
// 	// }
// }

// void ULXRDetectionComponent::PrintNodeIamIn()
// {
// 	TArray<FLXROctreeElement> OctreeElements;
// 	TArray<FLXROctree::FNodeIndex> NodeIndexes;
// 	TArray<FBoxCenterAndExtent> NodesBounds;
// 	LXRSubsystem->FindLXRPoints(OctreeElements, RelevancyOctreeCheckBoundsSize);
// 	// LXRSubsystem->GetOctree()->FindNearbyElements(GetOwner()->GetActorLocation(), [&OctreeElements](const FLXROctreeElement& Element)
// 	// {
// 	// 	OctreeElements.Add(Element);
// 	// });
//
//
// 	LXRSubsystem->GetOctree()->FindNodesWithPredicate([this, &NodesBounds](const FBoxCenterAndExtent& NodeBounds)
// 	                                                  {
// 		                                                  NodesBounds.Add(NodeBounds);
// 		                                                  return true;
// 	                                                  }, [this,&NodeIndexes](FLXROctree::FNodeIndex NodeIndex)
// 	                                                  {
// 		                                                  NodeIndexes.Add(NodeIndex);
// 	                                                  });
//
// 	for (int i = 0; i < NodeIndexes.Num(); ++i)
// 	{
// 		bool HasElements = LXRSubsystem->GetOctree()->GetElementsForNode(NodeIndexes[i]).Num() > 0;
// 		{
// 			DrawDebugBox(GetWorld(), NodesBounds[i].Center, NodesBounds[i].Extent, HasElements ? FColor::Green : FColor::Red, false, 1.0f, 0, HasElements ? 10 : 5);
// 		}
// 	}
// }

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


#include "LXRSubsystem.h"
#include "EngineUtils.h"
#include "LXRFree.h"
#include "LXRSourceComponent.h"
DEFINE_LOG_CATEGORY(LogLightSystem);


void ULXRSubsystem::RegisterLight(AActor* LightSource)
{
	const ULXRSourceComponent* LightSourceComponent = Cast<ULXRSourceComponent>(LightSource->GetComponentByClass(ULXRSourceComponent::StaticClass()));
	if (LightSourceComponent->bSolo)
		bSoloFound = true;

	LightSources.AddUnique(LightSource);
	OnLightAdded.Broadcast(LightSource);
}

void ULXRSubsystem::UnregisterLight(AActor* LightSource)
{
	if (LightSources.Contains(LightSource))
	{
		LightSources.Remove(LightSource);
		OnLightRemoved.Broadcast(LightSource);
	}
}

const TArray<TWeakObjectPtr<AActor>>& ULXRSubsystem::GetAllLights() const
{
	return LightSources;
}


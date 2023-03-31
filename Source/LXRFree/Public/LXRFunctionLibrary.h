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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LXRFunctionLibrary.generated.h"

class ULXRSourceComponent;

UENUM(Blueprintable)
enum class EDominantColor : uint8 { None, Red, Green, Blue };

USTRUCT(BlueprintType)
struct LXRFREE_API FDominantColor
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LXR|Detection")
	EDominantColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LXR|Detection")
	float ColorValue;

	FDominantColor()
	{
		Color = EDominantColor::None;
		ColorValue = -1;
	}

	float GetColorValueRoundedToHalf() const
	{
		return ColorValue != 0 ? FMath::RoundToFloat(FMath::Min(ColorValue, 1.0f) * 2) / 2 : 0;
	}

	bool operator==(const FDominantColor& Other) const
	{
		return Other.Color == Color;
	}

	uint32 GetTypeHash(const FDominantColor& Other);
};

UCLASS()
class LXRFREE_API ULXRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region v1.1
	// Returns the color channel which has the most influence of InColor.
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Dominant Color", CompactNodeTitle = "Dominant Color"), Category= "LXR|Detection|CombinedData")
	static FDominantColor GetDominantColor(const FLinearColor& InColor);

	// Returns the color channel which has the second most influence of InColor.
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get 2nd Dominant Color", CompactNodeTitle = "2nd Dominant Color"), Category= "LXR|Detection|CombinedData")
	static FDominantColor GetSecondDominantColor(const FLinearColor& InColor);

	// Clamp the color channels to 0-1 range.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Clamp Channels", CompactNodeTitle = "Clamp", Keywords ="Clamp"), Category= "LXR|Detection|CombinedData")
	static FLinearColor ClampTo01Range(const FLinearColor& InColor);

	// Is DominantColors equal.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Equal DominantColor", CompactNodeTitle = "DomC ==", Keywords =" == equal"), Category= "LXR|Detection|CombinedData")
	static bool Equal_FDominantColor(const FDominantColor& FirstColor, const FDominantColor& SecondColor);

	// Does the LinearColor equal Dominant color.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Color Equal DominantColor", CompactNodeTitle = "Col == DomC", Keywords ="== equal"), Category= "LXR|Detection|CombinedData")
	static bool ColorEqual_DominantColor(const FLinearColor& Color, const FDominantColor& DominantColor);

#pragma endregion

#pragma region v1.2
	
	// Are the colors approximately equal.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Color Approximately Equal Color", CompactNodeTitle = "~", Keywords ="Approximately ~"), Category= "LXR|Detection|CombinedData")
	static bool ColorApproximatelyEqualColor(const FLinearColor& InColorOne, const FLinearColor& InColorTwo);

	// Are the colors after remapping and rounding to half equal.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Color Rounded Half Equal Color", CompactNodeTitle = "== [½]", Keywords ="[ Equal Half ]"), Category= "LXR|Detection|CombinedData")
	static bool ColorRemappedRoundedHalfEqualColor(const FLinearColor& InColorOne, const FLinearColor& InColorTwo);

	// Toggles Color channels according to ToggleChannels.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Toggle Channels"), Category= "LXR|Color")
	static FLinearColor ToggleColorChannels(const FLinearColor& InColor, const FLinearColor& ToggleChannels);

	// Remaps color to 0-1.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Remap Color Range to 0-1", CompactNodeTitle = "|0-1|", Keywords ="Remap Color Range"), Category= "LXR|Detection|CombinedData")
	static FLinearColor RemapColorRangeTo01(const FLinearColor& InColor);

	// Rounds color to nearest half.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Round Channels To Nearest Half", CompactNodeTitle = "[½]", Keywords ="Round Half"), Category= "LXR|Detection|CombinedData")
	static FLinearColor RoundToNearestHalf(const FLinearColor& InColor);
	
	// Return the max value of color channels.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Get Max of Color Channels", CompactNodeTitle = "Max", Keywords ="Max Color Channel"), Category= "LXR|Detection|CombinedData")
	static float GetMaxOfColorChannels(const FLinearColor& InColor);

	// Return the min value of color channels.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Get Min of Color Channels", CompactNodeTitle = "Min", Keywords ="Max Color Channel"), Category= "LXR|Detection|CombinedData")
	static float GetMinOfColorChannels(const FLinearColor& InColor);

	// Return inverse of LinearColor channels.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Inverse Channels", CompactNodeTitle = "Inverse", Keywords ="Inverse"), Category= "LXR|Detection|CombinedData")
	static FLinearColor GetInverseChannels(const FLinearColor& InColor);

	// Return DominantColor converted to LinearColor.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Dominant Color to Linear Color", CompactNodeTitle = "Dom to Linear", Keywords ="Dominant to Linear"), Category= "LXR|Detection|CombinedData")
	static FLinearColor DominantToLinearColor(const FDominantColor& InDominantColor);

#pragma endregion 

#pragma region v1.3

	// Get FLinearColor array Average.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Linear Color Array Average", CompactNodeTitle = "Average Color", Keywords ="Linear Color Array Average"), Category= "LXR|Detection|CombinedData")
	static FLinearColor GetLinearColorArrayAverage(const TArray<FLinearColor>& InColors);

	// Rounds color channels.
	UFUNCTION(BlueprintPure, meta = (Displayname = "Round Channels", CompactNodeTitle = "[nint]", Keywords ="Round "), Category= "LXR|Detection|CombinedData")
	static FLinearColor Round(const FLinearColor& InColor);
	
#pragma endregion 


	///
	///Copy Paste from UE Wiki
	///Float as String With Precision
	static FORCEINLINE FString GetFloatAsStringWithPrecision(float TheFloat, int32 Precision, bool IncludeLeadingZero=true)
	{
		//Round to integral if have something like 1.9999 within precision
		float Rounded = roundf(TheFloat);
		if(FMath::Abs(TheFloat - Rounded) < FMath::Pow(static_cast<float>(10), static_cast<float>(-1 * Precision)))
		{       
			TheFloat = Rounded;
		}
		FNumberFormattingOptions NumberFormat;					//Text.h
		NumberFormat.MinimumIntegralDigits = (IncludeLeadingZero) ? 1 : 0;
		NumberFormat.MaximumIntegralDigits = 10000;
		NumberFormat.MinimumFractionalDigits = Precision;
		NumberFormat.MaximumFractionalDigits = Precision;
		return FText::AsNumber(TheFloat, &NumberFormat).ToString(); 
	}

	//Float as FText With Precision!
	static FORCEINLINE FText GetFloatAsTextWithPrecision(float TheFloat, int32 Precision, bool IncludeLeadingZero=true)
	{
		//Round to integral if have something like 1.9999 within precision
		float Rounded = roundf(TheFloat);
		if(FMath::Abs(TheFloat - Rounded) < FMath::Pow(static_cast<float>(10), static_cast<float>(-1 * Precision)))
		{       
			TheFloat = Rounded;
		}
		FNumberFormattingOptions NumberFormat;					//Text.h
		NumberFormat.MinimumIntegralDigits = (IncludeLeadingZero) ? 1 : 0;
		NumberFormat.MaximumIntegralDigits = 10000;
		NumberFormat.MinimumFractionalDigits = Precision;
		NumberFormat.MaximumFractionalDigits = Precision;
		return FText::AsNumber(TheFloat, &NumberFormat); 
	}

};

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


#include "LXRFunctionLibrary.h"


uint32 FDominantColor::GetTypeHash(const FDominantColor& Other)
{
	const uint32 Hash = FCrc::MemCrc32(&Other, sizeof(FDominantColor));
	return Hash;
}

FDominantColor ULXRFunctionLibrary::GetDominantColor(const FLinearColor& InColor)
{
	float MinValueOfColorComponents = GetMinOfColorChannels(InColor);
	float MaxValueOfColorComponents = GetMaxOfColorChannels(InColor);

	FDominantColor DominantColor;

	if (MinValueOfColorComponents == MaxValueOfColorComponents)
	{
		return DominantColor;
	}

	if (InColor.R == MaxValueOfColorComponents)
	{
		DominantColor.Color = EDominantColor::Red;
		DominantColor.ColorValue = InColor.R;
		return DominantColor;
	}

	if (InColor.G == MaxValueOfColorComponents)
	{
		DominantColor.Color = EDominantColor::Green;
		DominantColor.ColorValue = InColor.G;
		return DominantColor;
	}

	if (InColor.B == MaxValueOfColorComponents)
	{
		DominantColor.Color = EDominantColor::Blue;
		DominantColor.ColorValue = InColor.B;
		return DominantColor;
	}


	return DominantColor;
}

FDominantColor ULXRFunctionLibrary::GetSecondDominantColor(const FLinearColor& InColor)
{
	const FDominantColor DominantColor = GetDominantColor(InColor);
	FDominantColor SecondDominantColor;

	switch (DominantColor.Color)
	{
		case EDominantColor::None:
			break;
		case EDominantColor::Red:
			{
				const float MaxValueOfColorComponents = FMath::Max(InColor.G, InColor.B);

				if (InColor.G == InColor.B)
					return SecondDominantColor;

				if (InColor.G == MaxValueOfColorComponents)
				{
					SecondDominantColor.Color = EDominantColor::Green;
					SecondDominantColor.ColorValue = InColor.G;
				}
				else
				{
					SecondDominantColor.Color = EDominantColor::Blue;
					SecondDominantColor.ColorValue = InColor.B;
				}
			}
			break;
		case EDominantColor::Green:
			{
				const float MaxValueOfColorComponents = FMath::Max(InColor.R, InColor.B);

				if (InColor.R == InColor.B)
					return SecondDominantColor;

				if (InColor.R == MaxValueOfColorComponents)
				{
					SecondDominantColor.Color = EDominantColor::Red;
					SecondDominantColor.ColorValue = InColor.R;
				}
				else
				{
					SecondDominantColor.Color = EDominantColor::Blue;
					SecondDominantColor.ColorValue = InColor.B;
				}
			}
			break;
		case EDominantColor::Blue:
			{
				const float MaxValueOfColorComponents = FMath::Max(InColor.R, InColor.G);

				if (InColor.R == InColor.G)
					return SecondDominantColor;

				if (InColor.R == MaxValueOfColorComponents)
				{
					SecondDominantColor.Color = EDominantColor::Red;
					SecondDominantColor.ColorValue = InColor.R;
				}
				else
				{
					SecondDominantColor.Color = EDominantColor::Green;
					SecondDominantColor.ColorValue = InColor.G;
				}
			}
			break;
		default: ;
	}
	return SecondDominantColor;
}

FLinearColor ULXRFunctionLibrary::GetInverseChannels(const FLinearColor& InColor)
{
	FVector ColorVector = FVector(InColor);
	ColorVector = ClampVector(ColorVector, FVector::ZeroVector, FVector::One());
	ColorVector = (ColorVector - 1).GetAbs();
	return FLinearColor(ColorVector);
}

FLinearColor ULXRFunctionLibrary::ClampTo01Range(const FLinearColor& InColor)
{
	return InColor.GetClamped(0, 1);
}

FLinearColor ULXRFunctionLibrary::DominantToLinearColor(const FDominantColor& InDominantColor)
{
	FLinearColor Temp;
	switch (InDominantColor.Color)
	{
		case EDominantColor::None:
			break;
		case EDominantColor::Red:
			Temp = FLinearColor(InDominantColor.ColorValue, 0, 0, 1);
			break;
		case EDominantColor::Green:
			Temp = FLinearColor(0, InDominantColor.ColorValue, 0, 1);
			break;
		case EDominantColor::Blue:
			Temp = FLinearColor(0, 0, InDominantColor.ColorValue, 1);
			break;
		default: ;
	}
	return Temp;
}

bool ULXRFunctionLibrary::Equal_FDominantColor(const FDominantColor& FirstColor, const FDominantColor& SecondColor)
{
	return FirstColor == SecondColor;
}

bool ULXRFunctionLibrary::ColorEqual_DominantColor(const FLinearColor& Color, const FDominantColor& DominantColor)
{
	const FDominantColor InColorDominantColor = GetDominantColor(Color);
	return InColorDominantColor == DominantColor;
}

bool ULXRFunctionLibrary::ColorApproximatelyEqualColor(const FLinearColor& InColorOne, const FLinearColor& InColorTwo)
{
	if (InColorOne.CopyWithNewOpacity(0) == InColorTwo.CopyWithNewOpacity(0))
		return true;

	const float MinValueOfColorOneComponents = GetMinOfColorChannels(InColorOne);
	const float MaxValueOfColorOneComponents = GetMaxOfColorChannels(InColorOne);

	const float MinValueOfColorTwoComponents = GetMinOfColorChannels(InColorTwo);
	const float MaxValueOfColorTwoComponents = GetMaxOfColorChannels(InColorTwo);

	// GEngine->AddOnScreenDebugMessage(-1, 0.2, FColor::Green, FString::Printf(TEXT("One Mi: %f  Ma: %f"), MinValueOfColorOneComponents, MaxValueOfColorOneComponents));
	// GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Red, FString::Printf(TEXT("Two Mi: %f  Ma: %f"), MinValueOfColorTwoComponents, MaxValueOfColorTwoComponents));
	if (MinValueOfColorOneComponents == MaxValueOfColorOneComponents || MinValueOfColorTwoComponents == MaxValueOfColorTwoComponents)
	{
		return false;
	}

	const FDominantColor InColorOneDominantColor = GetDominantColor(InColorOne);
	const FDominantColor InColorTwoDominantColor = GetDominantColor(InColorTwo);

	const FDominantColor InColorTwoSecondDominantColor = GetSecondDominantColor(InColorTwo);
	const FDominantColor InColorOneSecondDominantColor = GetSecondDominantColor(InColorOne);

	if (InColorOneDominantColor == InColorTwoDominantColor)
	{
		if (Round(InColorOne) == Round(InColorTwo))
			return true;
	}


	if (InColorOneDominantColor.Color == EDominantColor::None &&
		InColorOneSecondDominantColor.Color == EDominantColor::None &&
		InColorTwoDominantColor.Color == EDominantColor::None &&
		InColorTwoSecondDominantColor.Color == EDominantColor::None)
	{
		return false;
	}

	const bool FirstDominantColorSame = InColorOneDominantColor.Color == InColorTwoSecondDominantColor.Color || InColorOneDominantColor.Color == InColorTwoDominantColor.Color;
	const bool SecondDominantColorSame = InColorOneSecondDominantColor.Color == InColorTwoSecondDominantColor.Color || InColorOneSecondDominantColor.Color == InColorTwoDominantColor.Color;

	return FirstDominantColorSame && SecondDominantColorSame;
}

bool ULXRFunctionLibrary::ColorRemappedRoundedHalfEqualColor(const FLinearColor& InColorOne, const FLinearColor& InColorTwo)
{
	FLinearColor ColorOne = RemapColorRangeTo01(InColorOne);
	ColorOne = RoundToNearestHalf(ColorOne);
	FLinearColor ColorTwo = RoundToNearestHalf(InColorTwo);

	ColorOne.A = 1;
	ColorTwo.A = 1;
	return ColorOne == ColorTwo;
}

FLinearColor ULXRFunctionLibrary::ToggleColorChannels(const FLinearColor& InColor, const FLinearColor& ToggleChannels)
{
	const float CeilInR = FMath::CeilToFloat(InColor.R);
	const float CeilInG = FMath::CeilToFloat(InColor.G);
	const float CeilInB = FMath::CeilToFloat(InColor.B);

	const bool ZeroR = ToggleChannels.R > 0 && InColor.R > 0 ? true : false;
	const bool ZeroG = ToggleChannels.G > 0 && InColor.G > 0 ? true : false;
	const bool ZeroB = ToggleChannels.B > 0 && InColor.B > 0 ? true : false;

	const float R = ZeroR ? 0 : ToggleChannels.R > 0 ? FMath::Abs(CeilInR - ToggleChannels.R) : InColor.R;
	const float G = ZeroG ? 0 : ToggleChannels.G > 0 ? FMath::Abs(CeilInG - ToggleChannels.G) : InColor.G;
	const float B = ZeroB ? 0 : ToggleChannels.B > 0 ? FMath::Abs(CeilInB - ToggleChannels.B) : InColor.B;

	return FLinearColor(R, G, B, 1);
}

FLinearColor ULXRFunctionLibrary::RemapColorRangeTo01(const FLinearColor& InColor)
{
	const float MaxValueOfColorComponents = GetMaxOfColorChannels(InColor);

	const float R = InColor.R != 0 ? InColor.R / MaxValueOfColorComponents : 0;
	const float G = InColor.G != 0 ? InColor.G / MaxValueOfColorComponents : 0;
	const float B = InColor.B != 0 ? InColor.B / MaxValueOfColorComponents : 0;

	return FLinearColor(R, G, B, 1);
}

float ULXRFunctionLibrary::GetMaxOfColorChannels(const FLinearColor& InColor)
{
	return FMath::Max(FMath::Max(InColor.R, InColor.G), InColor.B);
}

float ULXRFunctionLibrary::GetMinOfColorChannels(const FLinearColor& InColor)
{
	return FMath::Min(FMath::Min(InColor.R, InColor.G), InColor.B);
}

FLinearColor ULXRFunctionLibrary::RoundToNearestHalf(const FLinearColor& InColor)
{
	const float R = InColor.R != 0 ? FMath::RoundToFloat(InColor.R * 2) / 2 : 0;
	const float G = InColor.G != 0 ? FMath::RoundToFloat(InColor.G * 2) / 2 : 0;
	const float B = InColor.B != 0 ? FMath::RoundToFloat(InColor.B * 2) / 2 : 0;
	return FLinearColor(R, G, B, 1);
}

FLinearColor ULXRFunctionLibrary::Round(const FLinearColor& InColor)
{
	const float R = FMath::RoundToFloat(InColor.R);
	const float G = FMath::RoundToFloat(InColor.G);
	const float B = FMath::RoundToFloat(InColor.B);
	return FLinearColor(R, G, B, 1);
}

FLinearColor ULXRFunctionLibrary::GetLinearColorArrayAverage(const TArray<FLinearColor>& InColors)
{
	FLinearColor Sum = FLinearColor::Black;
	FLinearColor Average = FLinearColor::Black;

	if (InColors.Num() > 0)
	{
		for (int32 VecIdx = 0; VecIdx < InColors.Num(); VecIdx++)
		{
			Sum += InColors[VecIdx];
		}

		Average = Sum / static_cast<float>(InColors.Num());
	}

	return Average;
}

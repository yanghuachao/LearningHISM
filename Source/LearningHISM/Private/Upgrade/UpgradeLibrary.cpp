// Fill out your copyright notice in the Description page of Project Settings.


#include "Upgrade/UpgradeLibrary.h"

TArray<FUpgradeData> UUpgradeLibrary::PickRandom(int32 Count) const
{
	TArray<FUpgradeData> Result;
	if (Upgrades.Num() == 0) return Result;

	// Fisher-Yates 洗牌，取前 N 个
	TArray<int32> Indices;
	for (int32 i = 0; i < Upgrades.Num(); ++i) Indices.Add(i);
	for (int32 i = Indices.Num() - 1; i > 0; --i)
	{
		Indices.Swap(i, FMath::RandRange(0, i));
	}

	for (int32 i = 0; i < FMath::Min(Count, Indices.Num()); ++i)
	{
		Result.Add(Upgrades[Indices[i]]);
	}
	return Result;
}

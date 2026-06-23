// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UpgradeLibrary.generated.h"


UENUM(BlueprintType)
enum class EUpgradeType : uint8
{
	Damage,
	MoveSpeed,
	MaxHealth,
	Cooldown,
	MagnetRange,
	Area,
};

USTRUCT(BlueprintType)
struct FUpgradeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUpgradeType Type = EUpgradeType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Magnitude = 0.2f;
};



/**
 * 
 */
UCLASS(BlueprintType)
class LEARNINGHISM_API UUpgradeLibrary : public UDataAsset
{
	GENERATED_BODY()
	
	
public:
	// 设计师在 BP_UpgradeLibrary 里直接编辑这个数组
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
	TArray<FUpgradeData> Upgrades;

	// 随机抽 N 个不重复
	UFUNCTION(BlueprintCallable, Category = "Upgrades")
	TArray<FUpgradeData> PickRandom(int32 Count) const;
};

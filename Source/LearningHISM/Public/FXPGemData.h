// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FXPGemData.generated.h"


// 一颗宝石的运行时数据
USTRUCT(BlueprintType)
struct FXPGemData
{
	GENERATED_BODY()
	// 宝石当前的世界坐标：每帧由 Tick 改写，同步给 HISM instance
	FVector Location = FVector::ZeroVector;
	// 拾取时给玩家加多少 XP：不同怪/不同等级宝石可以传不同的值进来
	float XPReward = 5.0f;
	// 已被拾取 或 被 MaxLifetime/MaxGems 回收：Tick 里直接 continue 跳过，
	// 等下一次 SpawnGem 复用这个槽位（不会真删，避免 HISM instance index 错位）
	bool bCollected = false;
	// 当前是否在磁吸飞行状态：true=按 PullVelocity 飞向玩家，false=原地停着
	// 离开 MagnetRadius 时会被置 false，宝石就掉地上不飞了
	bool bBeingPulled = false;
	// 磁吸时的速度向量：Tick 里算好 (玩家-宝石).Normalize() * PullSpeed
	// bBeingPulled == false 时为零向量，宝石不动
	FVector PullVelocity = FVector::ZeroVector;
	float Age = 0.0f;
};

/*// 死亡广播：MonsterBase 在血量归零时调用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMonsterDiedFromManager, FVector, Location, float, XPReward);*/


// 拾取时广播：外部（C++）订阅后把 XP 加到 GameState
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXPGemCollected, float, XPReward);

UCLASS()
class LEARNINGHISM_API AFXPGemData : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFXPGemData();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 在指定位置生成一颗宝石（怪物死亡时由 MonsterBase 调用）
	UFUNCTION(BlueprintCallable, Category = "XP")
	void SpawnGem(FVector Location, float XPReward);

	// 外部注入玩家引用（不调也能用，默认在 BeginPlay 找 GetPlayerPawn）
	UFUNCTION(BlueprintCallable, Category = "XP")
	void SetPlayer(AActor* InPlayer);

	// 调试：清空所有宝石
	UFUNCTION(BlueprintCallable, Category = "XP|Debug")
	void CollectAllGems();
	
	// MonsterBase 死亡时回调：在死亡位置生成宝石
	UFUNCTION()
	void HandleMonsterDied(FVector Location, float XPReward);

	// 宝石被拾取时回调：把 XP 转发给 GameState
	UFUNCTION()
	void HandleXPGemCollected(float XPReward);

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UInstancedStaticMeshComponent* GemHISMC;

	// ---- 调参（全部可热改） ----

	// 玩家进入这个半径后宝石开始被吸
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP|Tuning", meta = (ClampMin = "0.0"))
	float MagnetRadius = 250.0f;

	// 距离 < 这个值就视为拾取
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP|Tuning", meta = (ClampMin = "0.0"))
	float PickupRadius = 60.0f;

	// 宝石被吸的飞行速度（单位/秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP|Tuning", meta = (ClampMin = "0.0"))
	float PullSpeed = 1500.0f;

	// 单颗宝石的最大存活时间（秒），0 = 永生
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP|Tuning", meta = (ClampMin = "0.0"))
	float MaxLifetime = 30.0f;

	// 软上限：超过这个数量时回收最老的未拾取宝石
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP|Tuning", meta = (ClampMin = "1"))
	int32 MaxGems = 500;

	// ---- 运行时数据 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XP|Data")
	TArray<FXPGemData> GemDataArray;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XP|Data")
	TWeakObjectPtr<AActor> CachedPlayer;

	// ---- 委托 ----

	UPROPERTY(BlueprintAssignable, Category = "XP")
	FOnXPGemCollected OnXPGemCollected;
};

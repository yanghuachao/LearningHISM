// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterBase.generated.h"

USTRUCT(BlueprintType)
struct FMonsterData
{
    GENERATED_BODY()

    // 怪物的当前位置
    FVector Location;
    // 怪物的当前朝向/旋转
    FRotator Rotation;
    // 怪物的移动速度
    float MovementSpeed;
    // 怪物当前血量
    float Health;
    // 标记该怪物是否存活（如果死了，位置留空等待下一个怪物复用）
    bool bIsAlive;

	// 【新增】记录上一帧的移动方向，用于平滑过渡
	FVector CurrentVelocityDir;
	
    FMonsterData()
    {
        Location = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        MovementSpeed = 300.0f;
        Health = 100.0f;
        bIsAlive = false;
    }
};

// 怪物死亡时广播：位置 + 经验奖励（外部 XPGemManager 订阅）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMonsterDied, FVector, Location, float, XPReward);


UCLASS()
class LEARNINGHISM_API AMonsterBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMonsterBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	// 接收范围伤害：传入爆炸位置、伤害半径、伤害值
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyDamageToMonsters(FVector HitLocation, float HitRadius, float DamageAmount);
	
	// 核心组件：用于批量渲染相同模型的怪物
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UHierarchicalInstancedStaticMeshComponent* MonsterHISMC;

	// 存储所有怪物逻辑数据的数组
	TArray<FMonsterData> MonsterDataArray;

	// 用于批量更新 Transform 的数组（HISMC 需要的格式）
	TArray<FTransform> MonsterTransforms;

	// 生成怪物的方法
	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void SpawnMonsters(int32 Count, FVector SpawnOrigin, float Radius);

	// 获取玩家引用，用于让怪物追踪
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	AActor* PlayerTarget;
	
	//加上死亡委托
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnMonsterDied OnMonsterDied;
};

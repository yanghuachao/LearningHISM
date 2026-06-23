// Fill out your copyright notice in the Description page of Project Settings.


#include "FXPGemData.h"

#include "Character/MonsterBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Game/LearningHISMGameState.h"
#include "Kismet/GameplayStatics.h"
//#include "Game/LearningHISMGameState.h"    

// Sets default values
AFXPGemData::AFXPGemData()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GemHISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GemHISMC"));
	RootComponent = GemHISMC;

	// 宝石不需要物理碰撞，拾取靠 Tick 距离检测
	GemHISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GemHISMC->SetGenerateOverlapEvents(false);
}

// Called when the game starts or when spawned
void AFXPGemData::BeginPlay()
{
	Super::BeginPlay();
	
	// 1) 找玩家
	if (!CachedPlayer.IsValid())
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			CachedPlayer = PlayerPawn;
		}
	}

	// 2) 找 MonsterManager 并订阅死亡广播
	if (AMonsterBase* MonsterMgr = Cast<AMonsterBase>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AMonsterBase::StaticClass())))
	{
		MonsterMgr->OnMonsterDied.AddDynamic(this, &AFXPGemData::HandleMonsterDied);
	}

	// 3) 自己的拾取广播转发给 GameState
	OnXPGemCollected.AddDynamic(this, &AFXPGemData::HandleXPGemCollected);
}

void AFXPGemData::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AMonsterBase* MonsterMgr = Cast<AMonsterBase>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AMonsterBase::StaticClass())))
	{
		MonsterMgr->OnMonsterDied.RemoveDynamic(this, &AFXPGemData::HandleMonsterDied);
	}
	OnXPGemCollected.RemoveDynamic(this, &AFXPGemData::HandleXPGemCollected);

	Super::EndPlay(EndPlayReason);
}

void AFXPGemData::SetPlayer(AActor* InPlayer)
{
	CachedPlayer = InPlayer;
}

void AFXPGemData::SpawnGem(FVector Location, float XPReward)
{
	// 1) 软上限：超 MaxGems 时回收 Age 最大的未拾取宝石
	if (MaxGems > 0 && GemDataArray.Num() >= MaxGems)
	{
		int32 OldestIdx = INDEX_NONE;
		float OldestAge = -1.0f;
		for (int32 i = 0; i < GemDataArray.Num(); ++i)
		{
			if (GemDataArray[i].bCollected) continue;
			if (GemDataArray[i].Age > OldestAge)
			{
				OldestAge = GemDataArray[i].Age;
				OldestIdx = i;
			}
		}
		if (OldestIdx != INDEX_NONE)
		{
			FTransform DeadTransform;
			DeadTransform.SetLocation(FVector(0.0f, 0.0f, -10000.0f));
			DeadTransform.SetScale3D(FVector::ZeroVector);
			GemHISMC->UpdateInstanceTransform(OldestIdx, DeadTransform, true, false, true);
			GemDataArray[OldestIdx].bCollected = true;
		}
	}

	// 2) 找一个空位
	int32 TargetIndex = INDEX_NONE;
	for (int32 i = 0; i < GemDataArray.Num(); ++i)
	{
		if (GemDataArray[i].bCollected)
		{
			TargetIndex = i;
			break;
		}
	}

	// 3) 没有就新增
	if (TargetIndex == INDEX_NONE)
	{
		FXPGemData NewGem;
		NewGem.Location = Location;
		NewGem.XPReward = XPReward;
		NewGem.bCollected = false;
		NewGem.bBeingPulled = false;
		NewGem.PullVelocity = FVector::ZeroVector;
		NewGem.Age = 0.0f;
		TargetIndex = GemDataArray.Add(NewGem);
	}
	else
	{
		GemDataArray[TargetIndex].Location = Location;
		GemDataArray[TargetIndex].XPReward = XPReward;
		GemDataArray[TargetIndex].bCollected = false;
		GemDataArray[TargetIndex].bBeingPulled = false;
		GemDataArray[TargetIndex].PullVelocity = FVector::ZeroVector;
		GemDataArray[TargetIndex].Age = 0.0f;
	}

	// 4) HISM：没有 instance 就 Add，有就 Update
	FTransform NewXform(FRotator::ZeroRotator, Location);
	if (GemHISMC->GetInstanceCount() <= TargetIndex)
	{
		GemHISMC->AddInstance(NewXform);
	}
	else
	{
		GemHISMC->UpdateInstanceTransform(TargetIndex, NewXform, true, false, true);
	}
}

// Called every frame
void AFXPGemData::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!CachedPlayer.IsValid()) return;

	const FVector PlayerLocation = CachedPlayer->GetActorLocation();
	const float MagnetRadiusSq = MagnetRadius * MagnetRadius;
	const float PickupRadiusSq = PickupRadius * PickupRadius;

	const int32 InstanceCount = GemDataArray.Num();
	for (int32 i = 0; i < InstanceCount; ++i)
	{
		FXPGemData& Gem = GemDataArray[i];
		if (Gem.bCollected) continue;

		// 0) 年龄 + 到期回收
		Gem.Age += DeltaTime;
		if (MaxLifetime > 0.0f && Gem.Age >= MaxLifetime)
		{
			Gem.bCollected = true;
			FTransform DeadTransform;
			DeadTransform.SetLocation(FVector(0.0f, 0.0f, -10000.0f));
			DeadTransform.SetScale3D(FVector::ZeroVector);
			GemHISMC->UpdateInstanceTransform(i, DeadTransform, true, false, true);
			continue;
		}

		const float DistSq = FVector::DistSquared(Gem.Location, PlayerLocation);

		// 1) 拾取
		if (DistSq <= PickupRadiusSq)
		{
			Gem.bCollected = true;
			OnXPGemCollected.Broadcast(Gem.XPReward);

			FTransform DeadTransform;
			DeadTransform.SetLocation(FVector(0.0f, 0.0f, -10000.0f));
			DeadTransform.SetScale3D(FVector::ZeroVector);
			GemHISMC->UpdateInstanceTransform(i, DeadTransform, true, false, true);
			continue;
		}

		// 2) 磁吸：进入 MagnetRadius 才飞向玩家
		if (DistSq <= MagnetRadiusSq)
		{
			const FVector Dir = (PlayerLocation - Gem.Location).GetSafeNormal();
			Gem.PullVelocity = Dir * PullSpeed;
			Gem.bBeingPulled = true;
		}
		else
		{
			// 退出磁吸就停在空中
			Gem.bBeingPulled = false;
			Gem.PullVelocity = FVector::ZeroVector;
		}

		// 3) 移动
		if (Gem.bBeingPulled)
		{
			Gem.Location += Gem.PullVelocity * DeltaTime;
		}

		// 4) 写回 HISM
		const FTransform NewXform(FRotator::ZeroRotator, Gem.Location);
		GemHISMC->UpdateInstanceTransform(i, NewXform, true, false, true);
	}

	GemHISMC->MarkRenderStateDirty();
}

void AFXPGemData::CollectAllGems()
{
	GemHISMC->ClearInstances();
	GemDataArray.Reset();
}

void AFXPGemData::HandleMonsterDied(FVector Location, float XPReward)
{
	
	/*if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
		FString::Printf(TEXT("[Gem] SpawnGem at %s, +%.0f XP"), *Location.ToString(), XPReward));*/
	
	SpawnGem(Location, XPReward);
}

void AFXPGemData::HandleXPGemCollected(float XPReward)
{
	/*if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta,
		FString::Printf(TEXT("[Gem Picked] +%.0f XP"), XPReward));
		*/
	
	if (ALearningHISMGameState* GS = GetWorld()->GetGameState<ALearningHISMGameState>())
	{
		GS->AddXP(XPReward);
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow,
				FString::Printf(TEXT("[Gem] +%.0f XP (no GameState)"), XPReward));
		}
	}
}
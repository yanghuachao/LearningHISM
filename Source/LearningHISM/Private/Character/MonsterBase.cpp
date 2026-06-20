// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/MonsterBase.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AMonsterBase::AMonsterBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 实例化 HISMC 组件
	MonsterHISMC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MonsterHISMC"));
	RootComponent = MonsterHISMC;
    
	// 开启碰撞（如果需要简单的物理阻挡，但建议后期用纯数学计算代替）
	//MonsterHISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	MonsterHISMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MonsterHISMC->SetCollisionResponseToAllChannels(ECR_Overlap); // 碰到任何东西都只报“重叠”，不发生物理反弹
	MonsterHISMC->SetGenerateOverlapEvents(true); // 开启重叠事件响应
}

// Called when the game starts or when spawned
void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	
	// 找到玩家作为追逐目标
	PlayerTarget = UGameplayStatics::GetPlayerPawn(this, 0);
}

// Called every frame
void AMonsterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (!PlayerTarget) return;

    FVector PlayerLocation = PlayerTarget->GetActorLocation();
    float SafeDistance = 100.0f;     // 安全距离（建议比模型半径稍微大一点点）
    float SeparationWeight = 1.5f;   // 排斥力权重，由于算法改变，这里的数值需要调小（例如1.0 ~ 3.0之间）

    int32 InstanceCount = MonsterDataArray.Num();
    
    for (int32 i = 0; i < InstanceCount; ++i)
    {
        FMonsterData& CurrentMonster = MonsterDataArray[i];
        if (!CurrentMonster.bIsAlive) continue;

        // 1. 基础追击力：指向玩家的方向
        FVector TargetMoveDirection = (PlayerLocation - CurrentMonster.Location).GetSafeNormal2D();

        // 2. 分离力计算
        FVector SeparationForce = FVector::ZeroVector;
        int32 NeighborCount = 0;

        for (int32 j = 0; j < InstanceCount; ++j)
        {
            if (i == j || !MonsterDataArray[j].bIsAlive) continue;

            float DistSq = FVector::DistSquared2D(CurrentMonster.Location, MonsterDataArray[j].Location);
            float SafeDistSq = FMath::Square(SafeDistance);

            if (DistSq < SafeDistSq)
            {
                float Dist = FMath::Max(FMath::Sqrt(DistSq), 0.1f); // 防止除以0
                FVector PushDir = (CurrentMonster.Location - MonsterDataArray[j].Location).GetSafeNormal2D();
                
                // 【核心优化 1：非线性排斥力】
                // 距离越近，排斥力呈“指数/平方”级增加，而在安全距离边缘排斥力很弱。
                // 这使得边缘的怪物只是被轻轻推开，而不是被猛烈弹开。
                float PushRatio = 1.0f - (Dist / SafeDistance);
                float PushStrength = PushRatio * PushRatio; // 使用平方曲线

                SeparationForce += PushDir * PushStrength;
                NeighborCount++;
            }
        }

        // 3. 融合力量
        if (NeighborCount > 0)
        {
            SeparationForce = (SeparationForce / NeighborCount).GetSafeNormal2D();
            
            // 注意这里不再乘 DeltaTime。我们将追击方向和排斥方向按权重混合
            TargetMoveDirection = (TargetMoveDirection + SeparationForce * SeparationWeight).GetSafeNormal2D();
        }

        // 如果这是怪物刚出生的第一帧，直接赋予方向防止原地发呆
        if (CurrentMonster.CurrentVelocityDir.IsNearlyZero())
        {
            CurrentMonster.CurrentVelocityDir = TargetMoveDirection;
        }

        // 【向量插值平滑（终极防抖魔法）】
        // 不要直接把新的计算结果塞给怪物，而是让怪物当前的移动方向“缓慢平滑地”偏向目标方向。
        // 参数 8.0f 是平滑速度（InterpSpeed），值越小转向越迟钝（像开船），值越大转向越快（容易抖）。你可以微调这个值。
        CurrentMonster.CurrentVelocityDir = FMath::VInterpTo(CurrentMonster.CurrentVelocityDir, TargetMoveDirection, DeltaTime, 8.0f).GetSafeNormal2D();

        // 4. 应用位移
        CurrentMonster.Location += CurrentMonster.CurrentVelocityDir * CurrentMonster.MovementSpeed * DeltaTime;

    	// 【修改：核心优化 3】让怪物始终面向玩家
    	// 算出怪物直指玩家的绝对方向
    	FVector DirToPlayer = (PlayerLocation - CurrentMonster.Location).GetSafeNormal2D();
    	FRotator TargetRotation = DirToPlayer.Rotation();
    	
    	// 因为模型面朝 Y 轴，所以我们需要减去（或加上） 90 度来纠正它
    	TargetRotation.Yaw -= 90.0f;
    	// 平滑旋转过去
    	CurrentMonster.Rotation = FMath::RInterpTo(CurrentMonster.Rotation, TargetRotation, DeltaTime, 10.0f);
    	// ==========================================
    	
        // 5. 更新 Transform
        FTransform MonsterTransform(CurrentMonster.Rotation, CurrentMonster.Location);
        MonsterHISMC->UpdateInstanceTransform(i, MonsterTransform, true, false, false);
    }

    MonsterHISMC->MarkRenderStateDirty();
}

void AMonsterBase::SpawnMonsters(int32 Count, FVector SpawnOrigin, float Radius)
{
	// 这里的 SpawnOrigin 传入玩家当前的位置
	// Radius 传入一个略大于屏幕视野的半径（例如 1500 到 2000 单位）
    
	for (int32 i = 0; i < Count; ++i)
	{
		// 1. 寻找一个空闲的怪物数据槽（复用死去的怪物）
		int32 TargetIndex = INDEX_NONE;
		for (int32 j = 0; j < MonsterDataArray.Num(); ++j)
		{
			if (!MonsterDataArray[j].bIsAlive)
			{
				TargetIndex = j;
				break;
			}
		}

		// 2. 如果没有空闲的，就新增一个
		if (TargetIndex == INDEX_NONE)
		{
			TargetIndex = MonsterDataArray.Add(FMonsterData());
		}

		// 3. 计算边缘生成位置
		float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		// 在设定半径的基础上加一点随机扰动，防止怪物生在完美的圆圈上
		float FinalRadius = Radius + FMath::FRandRange(0.0f, 300.0f); 

		FVector Offset;
		Offset.X = FMath::Cos(RandomAngle) * FinalRadius;
		Offset.Y = FMath::Sin(RandomAngle) * FinalRadius;
		Offset.Z = 0.0f; // 保持在地面高度

		FVector SpawnLocation = SpawnOrigin + Offset;

		// 4. 初始化怪物数据
		MonsterDataArray[TargetIndex].Location = SpawnLocation;
		MonsterDataArray[TargetIndex].Rotation = (SpawnOrigin - SpawnLocation).Rotation(); // 朝向玩家
		MonsterDataArray[TargetIndex].bIsAlive = true;
		MonsterDataArray[TargetIndex].Health = 100.0f;

		// 5. 更新 HISMC 实例
		if (MonsterHISMC->GetInstanceCount() <= TargetIndex)
		{
			FTransform NewTransform(MonsterDataArray[TargetIndex].Rotation, MonsterDataArray[TargetIndex].Location);
			MonsterHISMC->AddInstance(NewTransform);
		}
		else
		{
			FTransform UpdateTransform(MonsterDataArray[TargetIndex].Rotation, MonsterDataArray[TargetIndex].Location);
			MonsterHISMC->UpdateInstanceTransform(TargetIndex, UpdateTransform, true, false, true);
		}
	}
	// 批量更新渲染
	MonsterHISMC->MarkRenderStateDirty();
}

void AMonsterBase::ApplyDamageToMonsters(FVector HitLocation, float HitRadius, float DamageAmount)
{
	// 提前算出半径的平方，避免在循环里开根号，极致压榨性能
	float RadiusSq = HitRadius * HitRadius;
	
	int32 DeadCount = 0;// 记录这次炸死了几只怪物
    
	// 遍历所有怪物数据
	for (int32 i = 0; i < MonsterDataArray.Num(); ++i)
	{
		// 只对活着的怪物进行判断
		if (MonsterDataArray[i].bIsAlive)
		{
			// 如果怪物和火球爆炸点的距离平方 <= 伤害半径的平方
			// DistSquared2D 专门用于计算平面距离，完美契合俯视角游戏！
			if (FVector::DistSquared2D(HitLocation, MonsterDataArray[i].Location) <= RadiusSq)
			{
				// 1. 造成伤害
				MonsterDataArray[i].Health -= DamageAmount;
                
				// 2. 判断血量是否归零
				if (MonsterDataArray[i].Health <= 0.0f)
				{
					// 标记死亡
					MonsterDataArray[i].bIsAlive = false;
                    
					// 【假装销毁】将其 Transform 移动到地下极深处，缩放清零
					FTransform DeadTransform;
					DeadTransform.SetLocation(FVector(0.0f, 0.0f, -10000.0f));
					DeadTransform.SetScale3D(FVector::ZeroVector); // 缩放为0
                    
					// 更新 HISMC 的渲染，bWorldSpace=true, bMarkRenderStateDirty=true
					MonsterHISMC->UpdateInstanceTransform(i, DeadTransform, true, true, true);
					// 死亡计数 +1
					DeadCount++;
					// 可以在这里播放金币掉落、或者经验球生成的逻辑
				}
			}
		}
	}
	// 如果这发火球炸死了怪物，立刻在屏幕外围补充等量的怪物
	if (DeadCount > 0 && PlayerTarget)
	{
		// 传入玩家当前位置，在 1500 的半径外（屏幕外）生成新怪物填补空缺
		SpawnMonsters(DeadCount, PlayerTarget->GetActorLocation(), 1500.0f);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AuraCharacter.h"
#include "Animation/AnimInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/MonsterBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Game/LearningHISMGameState.h"

class UEnhancedInputLocalPlayerSubsystem;
// Sets default values
AAuraCharacter::AAuraCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// 1. 禁用控制器旋转：让角色不要跟着摄像机或控制器的方向硬转
	bUseControllerRotationPitch=false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	// 2. 配置角色移动组件，使其面向移动方向
	if (GetCharacterMovement())
	{
		// 开启朝向移动方向旋转
		GetCharacterMovement()->bOrientRotationToMovement = true; 
        
		// 设置旋转速率（Pitch, Yaw, Roll），这里的 Yaw 决定了转身的平滑度/快慢
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 400.0f, 0.0f); 
	}
	
	// 3. 创建弹簧臂（CameraBoom）
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent); // 绑定到角色的根组件（胶囊体）上
	CameraBoom->TargetArmLength = 800.f;        // 设置相机与角色的距离（自拍杆长度），可根据需要调整
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 设置相机的俯视角度（Pitch为-60度）
	CameraBoom->bDoCollisionTest = false;       // 俯视角游戏通常关闭碰撞测试，防止相机穿墙时猛烈拉近
	CameraBoom->bUsePawnControlRotation = false; // 让弹簧臂不跟随鼠标/控制器旋转，保持固定视角

	// 4. 创建相机并绑定到弹簧臂末端
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // 挂载到弹簧臂的末端插槽
	FollowCamera->bUsePawnControlRotation = false; // 相机本身也不随控制器旋转
	
	// 创建武器网格体组件
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    
	// 关闭武器的碰撞，防止武器把角色自己绊倒或者卡进地形
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	Weapon->SetupAttachment(GetMesh(), WeaponSocketName);
	
	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(Weapon);
	
	// 初始化满血
	CurrentHealth = MaxHealth;
}

// Called when the game starts or when spawned
void AAuraCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 1. 将映射上下文 (Mapping Context) 添加到增强输入子系统中
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// 添加上下文，优先级设为0
			if (AuraMappingContext)
			{
				Subsystem->AddMappingContext(AuraMappingContext, 0);
			}
		}
	}
	
	// 缓存 MonsterManager 引用，接触检测每帧用
	if (AMonsterBase* MonsterMgr = Cast<AMonsterBase>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AMonsterBase::StaticClass())))
	{
		CachedMonsterManager = MonsterMgr;
	}
	
	if (!UpgradeLibrary)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				TEXT("[Aura] 警告：UpgradeLibrary 没设置，升级系统不会工作"));
		}
	}
	if (ALearningHISMGameState* GS = GetWorld()->GetGameState<ALearningHISMGameState>())
	{
		GS->OnLevelUp.AddDynamic(this, &AAuraCharacter::HandleLevelUp);
	}
}

// Called every frame
void AAuraCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ★ 死亡后所有战斗逻辑停掉
    if (bIsDead) return;

    // 1. 全局冷却系统：所有"手动技能"的冷却倒计时
    for (int32 i = 0; i < EquippedManualSkills.Num(); ++i)
    {
        if (EquippedManualSkills[i].CurrentTimer > 0.0f)
        {
            EquippedManualSkills[i].CurrentTimer -= DeltaTime;
        }
    }

    // 2. 全局冷却系统：所有"自动技能"的冷却倒计时
    for (int32 i = 0; i < EquippedAutoSkills.Num(); ++i)
    {
        if (EquippedAutoSkills[i].CurrentTimer > 0.0f)
        {
            EquippedAutoSkills[i].CurrentTimer -= DeltaTime;
        }
    }

    // 3. 自动施法触发逻辑（保留你原有代码）
    if (bIsAutoCasting)
    {
        for (int32 i = 0; i < EquippedAutoSkills.Num(); ++i)
        {
            FAutoSkillData& Skill = EquippedAutoSkills[i];
            if (!Skill.SkillClass) continue;
            if (Skill.CurrentTimer <= 0.0f)
            {
                PendingSkill = Skill;
                if (Skill.SkillAnimMontage && GetMesh() && GetMesh()->GetAnimInstance())
                {
                    GetMesh()->GetAnimInstance()->Montage_Play(Skill.SkillAnimMontage);
                }
                else
                {
                    ExecuteSkillSpawn();
                }
                Skill.CurrentTimer = Skill.Cooldown;
                break;
            }
        }
    }

    // ★ 4. 接触伤害检测：怪物离玩家够近就扣血（带 CD 防止秒死）
    ContactDamageTimer = FMath::Max(0.0f, ContactDamageTimer - DeltaTime);
    if (ContactDamageTimer <= 0.0f && CachedMonsterManager.IsValid())
    {
        const float ContactRadiusSq = ContactRadius * ContactRadius;
        const FVector MyLocation = GetActorLocation();
        for (const FMonsterData& Monster: CachedMonsterManager->MonsterDataArray)
        {
            if (!Monster.bIsAlive) continue;
            if (FVector::DistSquared2D(MyLocation, Monster.Location) <= ContactRadiusSq)
            {
                TakeDamage(ContactDamage);
                ContactDamageTimer = ContactDamageCooldown;
                break; // 一帧只吃一个怪的伤，避免团灭
            }
        }
    }
}

// Called to bind functionality to input
void AAuraCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 2. 将传入的 UInputComponent 转换为 UEnhancedInputComponent
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 3. 绑定移动操作 (Triggered 表示输入被持续触发时)
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAuraCharacter::Move);
		}
		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AAuraCharacter::Attack);
		}
		// 绑定自动施法开关 (Started 表示按下瞬间触发)
		if (AutoCastAction)
		{
			EnhancedInputComponent->BindAction(AutoCastAction, ETriggerEvent::Started, this, &AAuraCharacter::ToggleAutoCast);
		}
		//绑定 Q 和 E 切换操作
		if (SwitchPrevAction)
		{
			EnhancedInputComponent->BindAction(SwitchPrevAction, ETriggerEvent::Started, this, &AAuraCharacter::SwitchPrevSkill);
		}
		if (SwitchNextAction)
		{
			EnhancedInputComponent->BindAction(SwitchNextAction, ETriggerEvent::Started, this, &AAuraCharacter::SwitchNextSkill);
		}
		
	}
	
}

void AAuraCharacter::Move(const FInputActionValue& Value)
{
	// 1. 获取二维向量输入 (Axis2D)
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
        
		const FVector ForwardDirection = FVector::ForwardVector; // 世界正前方 (X轴)
		const FVector RightDirection = FVector::RightVector;     // 世界正右方 (Y轴)

		// 2. 添加移动输入
		// 输入值 MovementVector.Y 是 W/S (前后)
		// 输入值 MovementVector.X 是 D/A (左右)
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

// 实现切换到上一个技能 (Q键)
void AAuraCharacter::SwitchPrevSkill(const FInputActionValue& Value)
{
	if (EquippedManualSkills.Num() > 0)
	{
		// 加上数组长度再取余，防止 C++ 负数取余 Bug
		CurrentManualSkillIndex = (CurrentManualSkillIndex - 1 + EquippedManualSkills.Num()) % EquippedManualSkills.Num();
        
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("已切换到技能: %d"), CurrentManualSkillIndex));
	}
}

// 实现切换到下一个技能 (E键)
void AAuraCharacter::SwitchNextSkill(const FInputActionValue& Value)
{
	if (EquippedManualSkills.Num() > 0)
	{
		// 循环切向下一个
		CurrentManualSkillIndex = (CurrentManualSkillIndex + 1) % EquippedManualSkills.Num();
        
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("已切换到技能: %d"), CurrentManualSkillIndex));
	}
}

void AAuraCharacter::Attack(const FInputActionValue& Value)
{
	// ★ 升级选择中：忽略 LMB，避免点击卡片同时触发火球
	if (bIsShowingUpgrades) return;
	
	if (bIsAutoCasting) {
		bIsAutoCasting = false;
	}
	
	if (!Weapon) return;

	if (EquippedManualSkills.IsValidIndex(CurrentManualSkillIndex))
	{
		FAutoSkillData& Skill = EquippedManualSkills[CurrentManualSkillIndex];

		if (Skill.SkillClass && Skill.CurrentTimer <= 0.0f)
		{
			// 1. 拿鼠标点的世界坐标
			PendingSkillTargetLocation = GetMouseTargetLocation();

			// 2. 让人物面朝鼠标点（俯视为准，只转 Yaw）
			FRotator LookAtRot = (PendingSkillTargetLocation - GetActorLocation()).Rotation();
			SetActorRotation(FRotator(0.f, LookAtRot.Yaw, 0.f));

			// 3. 存技能、播动画
			PendingSkill = Skill;
			if (Skill.SkillAnimMontage && GetMesh() && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Play(Skill.SkillAnimMontage);
			}
			else
			{
				ExecuteSkillSpawn();
			}

			Skill.CurrentTimer = Skill.Cooldown;
		}
		else if (Skill.CurrentTimer > 0.0f)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Red, TEXT("技能正在冷却中"));
		}
	}
}

// 实现自动攻击开关逻辑
void AAuraCharacter::ToggleAutoCast()
{
	bIsAutoCasting = !bIsAutoCasting;
    
	// 打印状态方便测试
	/*if (GEngine)
	{
		FString Status = bIsAutoCasting ? TEXT("开启") : TEXT("关闭");
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("自动施法已 %s!"), *Status));
	}*/
}

//实际技能生成
void AAuraCharacter::ExecuteSkillSpawn()
{
	// 确保暂存区里有技能
	if (!PendingSkill.SkillClass) return;

	// 1. 获取怪物管理器
	AMonsterBase* MonsterManager = Cast<AMonsterBase>(UGameplayStatics::GetActorOfClass(GetWorld(), AMonsterBase::StaticClass()));

	// 1. 位置从枪口出
	FVector SpawnLocation = MuzzlePoint->GetComponentLocation();

	// 2. 决定目标点：复用你已有的 bIsAutoCasting
	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;

	// 4. 寻找最近怪物，计算发射方向（不需要强行扭转人物身体，保证走位丝滑）
	if (bIsAutoCasting && MonsterManager)
	{
		const float MaxRangeSq = AutoAttackRange * AutoAttackRange;
		// 自动模式：找最近的活怪物
		float ClosestDistSq = MAX_flt;
		for (const FMonsterData& Monster : MonsterManager->MonsterDataArray)
		{
			if (!Monster.bIsAlive) continue;

			// 用人物中心点算距离，平方比较省一次开方
			float DistSq = FVector::DistSquared(GetActorLocation(), Monster.Location);
			if (DistSq > MaxRangeSq) continue; // 超出范围，不考虑

			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				TargetLocation = Monster.Location;
				bHasTarget = true;
			}
		}
		/*// 可视化范围圈（验证用，确认后可以删掉）
		DrawDebugCircle(
			GetWorld(),
			GetActorLocation() - FVector(0, 0, GetActorLocation().Z), // 贴地
			AutoAttackRange,
			32, FColor::Cyan, false, 0.5f, 0, 2.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false
		);*/
	}
	else
	{
		// 手动模式：用鼠标点击的目标
		TargetLocation = PendingSkillTargetLocation;
		bHasTarget = true;
	}

	if (!bHasTarget)
	{
		TargetLocation = GetActorLocation() + GetActorForwardVector() * 1000.f;
	}

	// 4. 朝向
	FVector FlatDirection = TargetLocation - SpawnLocation;
	FlatDirection.Z = 0.f;
	FlatDirection = FlatDirection.GetSafeNormal();
	FRotator SpawnRotation = FlatDirection.Rotation();

	/*// 5. 调试可视化
	DrawDebugSphere(GetWorld(), SpawnLocation, 25.f, 16, FColor::Red, false, 2.f);
	DrawDebugLine(GetWorld(), SpawnLocation, TargetLocation, FColor::Green, false, 2.f, 0, 5.f);
	*/

	
	// 5. 在世界中生成火球（或冰锥、闪电等）
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AActor* Spawned =GetWorld()->SpawnActor<AActor>(PendingSkill.SkillClass, SpawnLocation, SpawnRotation, SpawnParams);
	
	if (AFireball* Fireball = Cast<AFireball>(Spawned))
	{
		Fireball->DamageAmount = CurrentFireballDamage;   // ← 改赋值，不用 *= 了
		Fireball->HitRadius    = CurrentFireballRadius;   // ← 改赋值

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
				FString::Printf(TEXT("[Spawn] Mult(Dmg=%.2f Area=%.2f) -> Fireball(HP=%.1f DMG=%.1f)"),
					DamageMultiplier, AreaMultiplier, Fireball->HitRadius, Fireball->DamageAmount));
		}
	}

	// 6. 发射完毕，清空暂存区
	PendingSkill.SkillClass = nullptr;
}

FVector AAuraCharacter::GetMouseTargetLocation() const
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return GetActorLocation() + GetActorForwardVector() * 1000.f;

	// 1. 优先：鼠标命中场景里某个物体
	FHitResult HitResult;
	if (PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		return HitResult.ImpactPoint;
	}

	// 2. 没命中：沿鼠标射线方向推 5000 单位
	FVector WorldOrigin, WorldDirection;
	if (PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return WorldOrigin + WorldDirection * 5000.f;
	}

	// 3. 最后兜底：人物前方 1000
	return GetActorLocation() + GetActorForwardVector() * 1000.f;
}

void AAuraCharacter::TakeDamage(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void AAuraCharacter::Die()
{
	if (bIsDead) return; // 防止多次调用
	bIsDead = true;
	bHasQuitted = false;

	// 停移动
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	// 关自动施法
	bIsAutoCasting = false;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		DisableInput(PC);
	}

	// 屏显 GAME OVER
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
			TEXT("=== GAME OVER ==="));
	}

	// 广播
	OnPlayerDied.Broadcast();

	// ★ 死亡动画：有 DeathMontage 就播 + 等结束；没就立刻退出
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	if (DeathMontage && AnimInst)
	{
		// 1) 结束回调：动画播完（或被打断）后调
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AAuraCharacter::HandleDeathFinished);
		AnimInst->Montage_Play(DeathMontage);
		AnimInst->Montage_SetEndDelegate(EndDelegate);

		// 2) 兜底定时器：动画卡住时强制退出
		FTimerHandle FallbackTimer;
		GetWorldTimerManager().SetTimer(
			FallbackTimer, this, &AAuraCharacter::OnDeathTimerExpired,
			DeathAnimationDuration, false);
	}
	else
	{
		// 没设动画：直接退出
		HandleDeathFinished(nullptr, false);
	}
}

void AAuraCharacter::HandleDeathFinished(UAnimMontage* Montage, bool bInterrupted)
{
	// 防止 Montage 回调 + 兜底定时器都触发，重复退出
	if (bHasQuitted) return;
	bHasQuitted = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

void AAuraCharacter::OnDeathTimerExpired()
{
	// 包装一下：把无参签名转成 HandleDeathFinished 的 (UAnimMontage*, bool) 签名
	HandleDeathFinished(nullptr, false);
}

void AAuraCharacter::PresentUpgradeOptions()
{
	if (!UpgradeLibrary || bIsShowingUpgrades) return;

	CurrentOptions = UpgradeLibrary->PickRandom(3);
	if (CurrentOptions.Num() == 0) return;

	bIsShowingUpgrades = true;

	// ★ 暂停游戏：怪物/技能/角色全部冻结
	UGameplayStatics::SetGamePaused(GetWorld(), true);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			TEXT("=== 升级 ==="));
	}
}

void AAuraCharacter::HandleLevelUp(int32 NewLevel, int32 CurrentXP)
{
	if (!UpgradeLibrary) return;
	PresentUpgradeOptions();
}


void AAuraCharacter::ChooseUpgrade(int32 Index)
{
	if (!CurrentOptions.IsValidIndex(Index)) return;

	FUpgradeData Chosen = CurrentOptions[Index];
	ApplyUpgrade(Chosen);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("选中了: %s"), *Chosen.DisplayName));
	}

	CurrentOptions.Empty();
	bIsShowingUpgrades = false;

	// ★ 恢复游戏
	UGameplayStatics::SetGamePaused(GetWorld(), false);
}

void AAuraCharacter::ApplyUpgrade(const FUpgradeData& Upgrade)
{
	switch (Upgrade.Type)
	{
	case EUpgradeType::Damage:
		DamageMultiplier += Upgrade.Magnitude;
		CurrentFireballDamage = BaseFireballDamage * DamageMultiplier;  // ← 立刻算好
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
				FString::Printf(TEXT("[Upgrade] Damage card: x%.2f -> CurrentFireballDamage=%.1f"),
					DamageMultiplier, CurrentFireballDamage));
		}
		break;

	case EUpgradeType::MoveSpeed:
		MoveSpeedMultiplier += Upgrade.Magnitude;
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed *= (1.0f + Upgrade.Magnitude);
		}
		break;

	case EUpgradeType::MaxHealth:
		MaxHealthBonus += Upgrade.Magnitude;
		MaxHealth = 100.0f + MaxHealthBonus;
		CurrentHealth = FMath::Min(CurrentHealth + Upgrade.Magnitude, MaxHealth);
		OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
		break;

	case EUpgradeType::Cooldown:
		CooldownMultiplier += Upgrade.Magnitude;
		break;

	case EUpgradeType::MagnetRange:
		MagnetRangeMultiplier += Upgrade.Magnitude;
		break;

	case EUpgradeType::Area:
		AreaMultiplier += Upgrade.Magnitude;
		CurrentFireballRadius = BaseFireballRadius * AreaMultiplier;    // ← 立刻算好
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
				FString::Printf(TEXT("[Upgrade] Area card: x%.2f -> CurrentFireballRadius=%.1f"),
					AreaMultiplier, CurrentFireballRadius));
		}
		break;
	}
}

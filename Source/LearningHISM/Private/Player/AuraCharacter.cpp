// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AuraCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Fireball/Fireball.h"
#include "Character/MonsterBase.h"
#include "Kismet/GameplayStatics.h"

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
}

// Called every frame
void AAuraCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 1. 全局冷却系统：所有“手动技能”的冷却倒计时
	for (int32 i = 0; i < EquippedManualSkills.Num(); ++i)
	{
		if (EquippedManualSkills[i].CurrentTimer > 0.0f)
		{
			EquippedManualSkills[i].CurrentTimer -= DeltaTime;
		}
	}

	// 2. 全局冷却系统：所有“自动技能”的冷却倒计时
	for (int32 i = 0; i < EquippedAutoSkills.Num(); ++i)
	{
		if (EquippedAutoSkills[i].CurrentTimer > 0.0f)
		{
			EquippedAutoSkills[i].CurrentTimer -= DeltaTime;
		}
	}

	// ==========================================
	// 2. 自动施法触发逻辑
	// ==========================================
	if (bIsAutoCasting)
	{
		for (int32 i = 0; i < EquippedAutoSkills.Num(); ++i)
		{
			FAutoSkillData& Skill = EquippedAutoSkills[i];
			if (!Skill.SkillClass) continue;

			// 如果冷却完毕
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

				// 重置自动技能的冷却
				Skill.CurrentTimer = Skill.Cooldown;
				break; 
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
	// 如果没有装备武器，直接返回（去掉对 FireballClass 的检查，因为我们现在用数组了）
	if (!Weapon) return;

	// 检查：当前选中的技能索引是否在数组范围内（防止越界崩溃）
	if (EquippedManualSkills.IsValidIndex(CurrentManualSkillIndex))
	{
		// 从数组中拿到当前选中的那个技能
		FAutoSkillData& Skill = EquippedManualSkills[CurrentManualSkillIndex];

		// 检查：这个技能是否配置了子弹类，并且 冷却时间已经归零！
		if (Skill.SkillClass && Skill.CurrentTimer <= 0.0f)
		{
			// 1. 存入暂存区，交给后续执行
			PendingSkill = Skill; 

			// 2. 播放专属攻击动画
			if (Skill.SkillAnimMontage && GetMesh() && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Play(Skill.SkillAnimMontage);
			}
			else
			{
				// 如果这个技能没配动画，直接瞬间发射
				ExecuteSkillSpawn(); 
			}

			// 3. 【极其重要】重置这个技能的冷却时间
			Skill.CurrentTimer = Skill.Cooldown;
		}
		else if (Skill.CurrentTimer > 0.0f)
		{
			// 如果玩家狂点鼠标，但技能还在冷却中，可以在这里加提示
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Red, TEXT("技能正在冷却中！"));
		}
	}
}

// 实现自动攻击开关逻辑
void AAuraCharacter::ToggleAutoCast()
{
	bIsAutoCasting = !bIsAutoCasting;
    
	// 打印状态方便测试
	if (GEngine)
	{
		FString Status = bIsAutoCasting ? TEXT("开启") : TEXT("关闭");
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("自动施法已 %s!"), *Status));
	}
}

//实际技能生成
void AAuraCharacter::ExecuteSkillSpawn()
{
	// 确保暂存区里有技能
	if (!PendingSkill.SkillClass) return;

	// 1. 获取怪物管理器
	AMonsterBase* MonsterManager = Cast<AMonsterBase>(UGameplayStatics::GetActorOfClass(GetWorld(), AMonsterBase::StaticClass()));

	// 2. 获取武器插槽的位置（使用你的 Weapon 组件）
	FVector SpawnLocation = GetActorLocation();
	if (Weapon && Weapon->DoesSocketExist(PendingSkill.SpawnSocketName))
	{
		SpawnLocation = Weapon->GetSocketLocation(PendingSkill.SpawnSocketName);
	}

	// 3. 默认发射方向：人物当前的正前方
	FRotator SpawnRotation = GetActorForwardVector().Rotation();

	// 4. 寻找最近怪物，计算发射方向（不需要强行扭转人物身体，保证走位丝滑）
	if (MonsterManager)
	{
		float ClosestDistSq = MAX_flt; 
		FVector TargetLocation = FVector::ZeroVector;
		bool bFoundTarget = false;

		for (const FMonsterData& Monster : MonsterManager->MonsterDataArray)
		{
			if (Monster.bIsAlive)
			{
				float DistSq = FVector::DistSquared2D(SpawnLocation, Monster.Location);
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					TargetLocation = Monster.Location;
					bFoundTarget = true;
				}
			}
		}

		if (bFoundTarget)
		{
			FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal2D();
			SpawnRotation = Direction.Rotation();
			// 不调用 SetActorRotation，让人物保持移动方向，但火球瞄准怪物
		}
	}

	// 5. 在世界中生成火球（或冰锥、闪电等）
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GetWorld()->SpawnActor<AActor>(PendingSkill.SkillClass, SpawnLocation, SpawnRotation, SpawnParams);

	// 6. 发射完毕，清空暂存区
	PendingSkill.SkillClass = nullptr;
}



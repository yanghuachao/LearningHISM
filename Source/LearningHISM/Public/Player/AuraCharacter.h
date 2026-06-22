// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "Character/MonsterBase.h"
#include "Fireball/Fireball.h"
#include "GameFramework/Character.h"
#include "AuraCharacter.generated.h"

class USpringArmComponent; // 新增
class UCameraComponent;    // 新增


// 血量变化时广播：UI 监听
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

// 死亡时广播：GameMode/UI 监听（只触发一次）
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

// ==========================================
// 自动技能数据结构体
// ==========================================
USTRUCT(BlueprintType)
struct FAutoSkillData
{
	GENERATED_BODY()

	// 技能要生成的子弹类（比如火球、飞刀、闪电等）
	// 使用 AActor 而不是 AFireball，是为了保证扩展性，啥都能射
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<AActor> SkillClass = nullptr; 

	// 技能的冷却时间（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	float Cooldown = 1.0f; 

	// 技能从哪个插槽发射
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SpawnSocketName = FName("WeaponSocket"); 
	
	// 施放该技能时要播放的动画蒙太奇
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	UAnimMontage* SkillAnimMontage = nullptr;

	// 内部运行数据：当前冷却计时器（不需要暴露给蓝图编辑）
	float CurrentTimer = 0.0f; 

	
};

UCLASS()
class LEARNINGHISM_API AAuraCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AAuraCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Input")
	UInputMappingContext* AuraMappingContext;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;
	
	// --- 增强输入 ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* AttackAction; // 攻击按键
	
	// 【新增】切换到上一个技能 (对应 Q 键)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SwitchPrevAction;

	// 【新增】切换到下一个技能 (对应 E 键)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SwitchNextAction;

	// --- 战斗参数 ---
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<AFireball> FireballClass; // 暴露给蓝图，用于选择要发射哪个火球蓝图
	
	// 自动攻击的搜索范围
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float AutoAttackRange = 1500.f;
	
	// --- 自动施法系统 ---

	// 按下 R 键触发的输入操作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* AutoCastAction;

	// 切换自动施法状态的方法
	void ToggleAutoCast();

	// 标记当前是否开启了自动施法
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsAutoCasting = false;
	
	// --- 战斗参数（继续） ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float CurrentHealth = 100.0f;

	// 怪物接触玩家时，单次扣多少血
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float ContactDamage = 50.0f;

	// 两次接触伤害之间至少隔多久（秒）—— 防止 60FPS 下每帧都扣血秒死
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float ContactDamageCooldown = 0.5f;

	// 怪物和玩家距离 < 这个值就算"接触"
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float ContactRadius = 60.0f;

	// 死亡标志：true 后禁用输入、停移动
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsDead = false;

	// ---- 委托 ----

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnPlayerDied OnPlayerDied;

	// ---- 输入 ----

	// 核心：角色当前拥有的自动技能列表！
	// 以后只要往这个数组里加东西，角色就会自动发射各种新技能
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<FAutoSkillData> EquippedAutoSkills;
	
	// 暂存当前准备发射的技能数据
	FAutoSkillData PendingSkill;
	
	// 手动攻击专属的技能数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<FAutoSkillData> EquippedManualSkills;

	// 【新增】记录当前选中的是数组里的第几个技能（默认 0）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	int32 CurrentManualSkillIndex = 0;
	
	// 鼠标点击瞄准的目标位置
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	FVector PendingSkillTargetLocation = FVector::ZeroVector;
	
	
	
	// 弹簧臂组件（控制相机的距离和角度）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	// 实际的相机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UStaticMeshComponent* Weapon;
	
	//用于火球的生成
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	USceneComponent* MuzzlePoint;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName WeaponSocketName = FName("WeaponSocket");
	
	
	// ---- 死亡动画 ----

	// 死亡时播放的动画蒙太奇（设计师在 BP_AuraCharacter 里指定）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	// 死亡动画兜底时长：超过这个秒数无论动画是否结束都强制退出
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1"))
	float DeathAnimationDuration = 2.0f;
	
	// 防止 Montage 结束回调 和 兜底定时器 都触发，导致 QuitGame 调 2 次
	bool bHasQuitted = false;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	void Move(const FInputActionValue& Value);
	
	void Attack(const FInputActionValue& Value); // 攻击执行函数
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// 切换技能的实现函数
	void SwitchPrevSkill(const FInputActionValue& Value);
	void SwitchNextSkill(const FInputActionValue& Value);
	
	// 动画播放到指定帧时，由动画蓝图调用的函数
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ExecuteSkillSpawn();
	
	// 鼠标拾取世界坐标的辅助函数
	FVector GetMouseTargetLocation() const;
	
	// ---- 方法 ----

	// 接受伤害（被怪物接触、外部调用都行）
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TakeDamage(float Amount);

	// 死亡处理（TakeDamage 内部调用一次）
	void Die();
	
	// 死亡动画结束回调（被 Montage_SetEndDelegate 调用）
	void HandleDeathFinished(UAnimMontage* Montage, bool bInterrupted);

	// 兜底定时器到期回调（无参，给 SetTimer 用）
	void OnDeathTimerExpired();

	// 内部：缓存 MonsterManager 引用，避免每帧 GetActorOfClass
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TWeakObjectPtr<AMonsterBase> CachedMonsterManager;

	// 内部：接触伤害 CD 计时器
	float ContactDamageTimer = 0.0f;
	
	

};

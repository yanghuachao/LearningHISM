// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "Fireball/Fireball.h"
#include "GameFramework/Character.h"
#include "AuraCharacter.generated.h"

class USpringArmComponent; // 新增
class UCameraComponent;    // 新增


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
	
	// --- 自动施法系统 ---

	// 按下 R 键触发的输入操作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* AutoCastAction;

	// 切换自动施法状态的方法
	void ToggleAutoCast();

	// 标记当前是否开启了自动施法
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsAutoCasting = false;
	
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
	
	/*// 切换技能的功能函数
	void SwitchToNextSkill(); // 切到下一个技能（适合鼠标滚轮向下）
	void SwitchToPrevSkill(); // 切到上一个技能（适合鼠标滚轮向上
	*/
	
	// 弹簧臂组件（控制相机的距离和角度）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	// 实际的相机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UStaticMeshComponent* Weapon;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName WeaponSocketName = FName("WeaponSocket");
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	void Move(const FInputActionValue& Value);
	
	void Attack(const FInputActionValue& Value); // 攻击执行函数
	
	// 切换技能的实现函数
	void SwitchPrevSkill(const FInputActionValue& Value);
	void SwitchNextSkill(const FInputActionValue& Value);
	
	// 动画播放到指定帧时，由动画蓝图调用的函数
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ExecuteSkillSpawn();
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UpgradePickHUD.generated.h"

/**
 * 
 */
UCLASS()
class LEARNINGHISM_API AUpgradePickHUD : public AHUD  
{
	GENERATED_BODY()

public:
	AUpgradePickHUD();

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	virtual void Tick(float DeltaTime) override;

private:
	// 上一次 LMB 状态（用于检测"刚按下"——上升沿）
	bool bWasLMBDown = false;
	
	// ★ 缓存：DrawHUD 阶段 Canvas 有效时记下来，Tick 阶段用
	float CachedScreenW = 0.f;
	float CachedScreenH = 0.f;
};

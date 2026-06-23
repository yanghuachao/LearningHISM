// Fill out your copyright notice in the Description page of Project Settings.


#include "Upgrade/UpgradePickHUD.h"
#include "Player/AuraCharacter.h"
#include "Engine/Canvas.h"

AUpgradePickHUD::AUpgradePickHUD()
{
    PrimaryActorTick.bCanEverTick = true;
    // 关键：暂停时也跑 Tick，否则检测不到点击
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AUpgradePickHUD::BeginPlay()
{
    Super::BeginPlay();

    // 升级选择时显示鼠标光标（VS 类游戏通常也一直显示）
    if (APlayerController* PC = GetOwningPlayerController())
    {
        PC->bShowMouseCursor = true;
    }
}

void AUpgradePickHUD::DrawHUD()
{
    Super::DrawHUD();
    
    // ★ 缓存屏幕尺寸（每帧渲染都更新，Tick 用这俩值）
    if (Canvas)
    {
        CachedScreenW = Canvas->SizeX;
        CachedScreenH = Canvas->SizeY;
    }

    APawn* Pawn = GetOwningPlayerController() ? GetOwningPlayerController()->GetPawn() : nullptr;
    AAuraCharacter* Player = Cast<AAuraCharacter>(Pawn);
    if (!Player || !Player->bIsShowingUpgrades || Player->CurrentOptions.Num() == 0) return;
    if (!Canvas) return;

    const float ScreenW = Canvas->SizeX;
    const float ScreenH = Canvas->SizeY;

    // 半透明黑底
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, ScreenW, ScreenH);

    // 标题
    DrawText(TEXT("== 升级选择 (点击卡片) =="),
        FLinearColor::Yellow,
        ScreenW * 0.32f, ScreenH * 0.18f, nullptr, 2.2f);

    // 三张卡（矩形位置要跟下面 Tick 里的命中检测保持一致）
    const float CardX0 = ScreenW * 0.28f;
    const float CardWidth = ScreenW * 0.44f;
    const float CardY0 = ScreenH * 0.30f;
    const float CardHeight = 90.f;
    const float CardGap = 10.f;

    for (int32 i = 0; i < Player->CurrentOptions.Num(); ++i)
    {
        const FUpgradeData& Opt = Player->CurrentOptions[i];
        FString CardText = FString::Printf(TEXT("%s\n      %s"),
            *Opt.DisplayName, *Opt.Description);

        DrawRect(FLinearColor(0.1f, 0.1f, 0.15f, 0.85f),
            CardX0, CardY0 + i * (CardHeight + CardGap),
            CardWidth, CardHeight);

        DrawText(CardText,
            FLinearColor::White,
            CardX0 + 20.f, CardY0 + i * (CardHeight + CardGap) + 15.f,
            nullptr, 1.5f);
    }
}

void AUpgradePickHUD::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    AAuraCharacter* Player = Cast<AAuraCharacter>(PC->GetPawn());
    if (!Player || !Player->bIsShowingUpgrades) return;
    if (CachedScreenW <= 0.f || CachedScreenH <= 0.f) return;  // 还没渲染过一帧

    // ★ 上升沿检测：上一帧没按，这一帧按了 = 刚点
    const bool bIsLMBDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
    if (bIsLMBDown && !bWasLMBDown)
    {
        float MouseX = 0.f, MouseY = 0.f;
        if (PC->GetMousePosition(MouseX, MouseY))
        {
            // 跟 DrawHUD 里完全相同的卡片矩形参数
            const float CardX0 = CachedScreenW * 0.28f;
            const float CardX1 = CardX0 + CachedScreenW * 0.44f;
            const float CardY0 = CachedScreenH * 0.30f;
            const float CardHeight = 90.f;
            const float CardGap = 10.f;

            for (int32 i = 0; i < Player->CurrentOptions.Num(); ++i)
            {
                const float CardTop = CardY0 + i * (CardHeight + CardGap);
                const float CardBottom = CardTop + CardHeight;

                if (MouseX >= CardX0 && MouseX <= CardX1 &&
                    MouseY >= CardTop && MouseY <= CardBottom)
                {
                    Player->ChooseUpgrade(i);
                    break;
                }
            }
        }
    }
    bWasLMBDown = bIsLMBDown;
}
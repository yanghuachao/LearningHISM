#include "Game/LearningHISMGameState.h"

ALearningHISMGameState::ALearningHISMGameState()
{
	XPToNextLevel = ComputeXPToNextLevel(PlayerLevel);
}

int32 ALearningHISMGameState::ComputeXPToNextLevel(int32 Level) const
{
	// 10, 12, 15, 19, 24, 30, ...
	return FMath::Max(1, FMath::FloorToInt(BaseXP * FMath::Pow(Growth, FMath::Max(Level - 1, 0))));
}

void ALearningHISMGameState::AddXP(float Amount)
{
	if (Amount <= 0.0f) return;

	CurrentXP += FMath::FloorToInt(Amount);

	// 一次可能连升多级（吸一波宝石）
	while (CurrentXP >= XPToNextLevel)
	{
		CurrentXP -= XPToNextLevel;
		PlayerLevel++;
		XPToNextLevel = ComputeXPToNextLevel(PlayerLevel);

		OnLevelUp.Broadcast(PlayerLevel, CurrentXP);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
				FString::Printf(TEXT("LEVEL UP! -> Lv.%d (next: %d XP)"), PlayerLevel, XPToNextLevel));
		}
	}

	OnXPChanged.Broadcast(CurrentXP, XPToNextLevel);
}

void ALearningHISMGameState::ResetProgression()
{
	PlayerLevel = 1;
	CurrentXP = 0;
	XPToNextLevel = ComputeXPToNextLevel(PlayerLevel);
	OnXPChanged.Broadcast(CurrentXP, XPToNextLevel);
}
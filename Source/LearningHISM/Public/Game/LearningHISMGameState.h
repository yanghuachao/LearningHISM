#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LearningHISMGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelUp, int32, NewLevel, int32, CurrentXP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnXPChanged, int32, CurrentXP, int32, XPToNextLevel);

UCLASS()
class LEARNINGHISM_API ALearningHISMGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALearningHISMGameState();

	// XPGemData 在玩家拾取时调用
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void AddXP(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Progression")
	void ResetProgression();

	// ---- 数据 ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	int32 PlayerLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	int32 CurrentXP = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression")
	int32 XPToNextLevel = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "1.0"))
	float Growth = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (ClampMin = "1"))
	int32 BaseXP = 10;

	// ---- 委托 ----

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FOnLevelUp OnLevelUp;

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FOnXPChanged OnXPChanged;

private:
	int32 ComputeXPToNextLevel(int32 Level) const;
};

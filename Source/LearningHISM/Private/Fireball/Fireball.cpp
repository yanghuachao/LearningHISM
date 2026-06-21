// Fill out your copyright notice in the Description page of Project Settings.


#include "Fireball/Fireball.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Character/MonsterBase.h"
#include "Engine/Engine.h"

// Sets default values
AFireball::AFireball()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 1. 设置球形碰撞体为主组件
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	SphereComp->InitSphereRadius(40.0f);
	SphereComp->SetUseCCD(true);
	SphereComp->SetCollisionProfileName(TEXT("BlockAllDynamic")); // 碰到动态物体产生拦截
	SphereComp->SetGenerateOverlapEvents(true); // 开启重叠事件

	// 2. 设置投射物移动组件
	ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
	ProjectileMovementComp->UpdatedComponent = SphereComp;
	ProjectileMovementComp->InitialSpeed = 600.f; // 初始飞行速度
	ProjectileMovementComp->MaxSpeed = 600.f;     // 最大飞行速度
	ProjectileMovementComp->ProjectileGravityScale = 0.f; // 火球不受重力影响，直线飞行

	// 默认寿命：3秒后自动销毁，防止飞出地图永远存在导致内存泄漏
	InitialLifeSpan = 3.0f;
}

// Called when the game starts or when spawned
void AFireball::BeginPlay()
{
	Super::BeginPlay();
	
	// 绑定重叠事件
	if (SphereComp)
	{
		SphereComp->OnComponentBeginOverlap.AddDynamic(this, &AFireball::OnSphereOverlap);
	}
}

// Called every frame
void AFireball::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFireball::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor != GetInstigator())
	{
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("火球撞到了: %s"), *OtherActor->GetName()));
		}
		
		// 尝试将碰到的物体转换为我们的怪物管理器
		AMonsterBase* MonsterManager = Cast<AMonsterBase>(OtherActor);
        
		if (MonsterManager)
		{
			// 2. 打印确认成功转换
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("成功命中怪物管理器！触发伤害。"));
            
			MonsterManager->ApplyDamageToMonsters(GetActorLocation(), 100.0f, DamageAmount);
            
			// 3. 只有打中怪物，火球才销毁自己
			Destroy();
		}
		else
		{
			// 如果打到的不是怪物（比如墙壁），按原逻辑处理
			// UGameplayStatics::ApplyDamage(...);
		}

		// 打中目标后，销毁火球自己
		//Destroy();
	}
}


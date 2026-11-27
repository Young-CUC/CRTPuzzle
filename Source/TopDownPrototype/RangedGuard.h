#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/TargetPoint.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Components/ActorComponent.h"
#include "RangedGuard.generated.h"

class UAISenseConfig_Sight;
class UAnimSequence;
class UTextRenderComponent;
UENUM(BlueprintType)
enum class EGuardState : uint8 { Patrol, Suspicious, Combat, Search };
enum class EGuardAim { Idle, Track, Lock };

UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class UGuardTestHealth : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health") float Health = 100;
    virtual void BeginPlay() override;
    UFUNCTION() void Damaged(AActor* Actor, float Amount, const UDamageType* Type, AController* InstigatorController, AActor* Source);
};

UCLASS()
class ARangedGuard : public ACharacter
{
    GENERATED_BODY()
public:
    ARangedGuard();
    virtual void PostRegisterAllComponents() override;
    UPROPERTY(VisibleAnywhere, Category="Guard|Animation") TObjectPtr<UStaticMeshComponent> GuardPistol;
    UPROPERTY(EditDefaultsOnly, Category="Guard|Animation") TObjectPtr<UAnimSequence> PistolIdle;
    UPROPERTY(EditDefaultsOnly, Category="Guard|Animation") TObjectPtr<UAnimSequence> PistolMove;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Guard|Patrol") TArray<TObjectPtr<ATargetPoint>> PatrolPoints;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float PatrolSpeed = 160;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float SearchSpeed = 240;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float ArrivalRadius = 60;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float PatrolWait = 1;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float SearchTimeout = 6;
    UPROPERTY(EditAnywhere, Category="Guard|Movement") float LookDuration = 3;
    UPROPERTY(EditAnywhere, Category="Guard|Recognition") float SuspicionGain = 60;
    UPROPERTY(EditAnywhere, Category="Guard|Recognition") float SuspicionDecay = 30;
    UPROPERTY(EditAnywhere, Category="Guard|Recognition") float MemoryHold = .5;
    UPROPERTY(EditAnywhere, Category="Guard|Recognition") float SearchDelay = .8f;
    UPROPERTY(EditAnywhere, Category="Guard|Attack") float TrackingTime = .8f;
    UPROPERTY(EditAnywhere, Category="Guard|Attack") float LockTime = .4f;
    UPROPERTY(EditAnywhere, Category="Guard|Attack") float Cooldown = .6f;
    UPROPERTY(EditAnywhere, Category="Guard|Attack") float ShotRange = 1600;
    UPROPERTY(EditAnywhere, Category="Guard|Attack") float ShotDamage = 20;
    UPROPERTY(EditAnywhere, Category="Guard") bool bDebug = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UGuardTestHealth> Vitals;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Muzzle;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Status;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SuspicionBar;
    UPROPERTY() TObjectPtr<UMaterialInterface> Yellow;
    UPROPERTY() TObjectPtr<UMaterialInterface> Red;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Guard|Debug") void DebugKill();
    void ShowBeam(FVector End, bool Locked);
    void ShowStatus(const FString& Label, FColor Color);
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Tracer;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Impact;
    FTimerHandle FeedbackTimer;
    void HideShotFeedback();
    void ShotFeedback(const FHitResult& Hit, FVector End, bool HitPlayer);
};

UCLASS()
class ARangedGuardController : public AAIController
{
    GENERATED_BODY()
public:
    ARangedGuardController();
    virtual void Tick(float Dt) override;
    virtual void OnPossess(APawn* Pawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnMoveCompleted(FAIRequestID Id, const FPathFollowingResult& Result) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard") EGuardState State = EGuardState::Patrol;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard") float Suspicion = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard") bool bCanSeePlayer = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard") bool bHasLastSeen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard") FVector LastSeenLocation;
    void DisableGuard();
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAIPerceptionComponent> Senses;
    UPROPERTY(VisibleAnywhere, Instanced) TObjectPtr<UAISenseConfig_Sight> Sight;
    UPROPERTY() TObjectPtr<ARangedGuard> Guard;
    UPROPERTY() TObjectPtr<APawn> Player;
    UPROPERTY() TObjectPtr<UGuardTestHealth> PlayerHealth;
    UFUNCTION() void Sensed(AActor* Actor, FAIStimulus Stimulus);
    void ChangeGuardState(EGuardState Next);
    void CancelAim();
    bool EyeClear() const;
    void Face(float Yaw);
    void Attack(double Now);
    void MoveToGuardLocation(FVector Destination);
    void AdvanceRoute(double Now);
    void BeginLook(double Now);
    FHitResult Trace(FVector Start, FVector End) const;
    EGuardAim Aim = EGuardAim::Idle;
    FVector LockedDirection;
    float SeenYaw = 0;
    double LostAt = 0, Deadline = 0, AimDeadline = 0, NextAim = 0;
    int32 PatrolIndex = 0, FailedPoints = 0;
    bool bSight = false, bLooking = false, bHolding = false;
    FAIRequestID MoveId = FAIRequestID::InvalidRequest;
    TOptional<bool> MoveResult;
};

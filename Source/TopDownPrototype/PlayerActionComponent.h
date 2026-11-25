#pragma once

#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "PlayerActionComponent.generated.h"

class ACharacter;
class APlayerController;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class UEnhancedInputComponent;
class UInputMappingContext;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class UPlayerActionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerActionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;

    UFUNCTION(BlueprintCallable, Category="Player Actions")
    bool EquipPistol(UStaticMesh* Mesh);

    UFUNCTION(BlueprintPure, Category="Player Actions")
    bool HasPistol() const { return HeldPistol != nullptr; }

    UPROPERTY(EditAnywhere, Category="Input") FKey ShootKey;
    UPROPERTY(EditAnywhere, Category="Input") FKey CrouchKey;
    UPROPERTY(EditAnywhere, Category="Input") FKey DodgeKey;
    UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.0")) float DodgeDistance = 180.f;
    UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.01", ToolTip="Playback speed multiplier for the dodge roll."))
    float DodgePlayRate = 1.f;
    UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.0")) float DodgeBlendInTime = 0.08f;
    UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.0")) float DodgeBlendOutTime = 0.18f;
    UPROPERTY(EditAnywhere, Category="Pistol") FTransform PistolGrip;
    UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin="0.0")) float LocomotionBlendTime = 0.15f;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> Run;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> CrouchIdle;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> CrouchWalk;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> DodgeRoll;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> PistolIdle;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> PistolRun;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> PistolCrouchIdle;
    UPROPERTY(EditAnywhere, Category="Animation") TObjectPtr<UAnimSequence> PistolCrouchWalk;

private:
    void BindPlayerInput(APlayerController* Controller);
    void UnbindPlayerInput();
    void Shoot();
    void BeginCrouch();
    void EndCrouch();
    void Dodge();
    void AdvanceDodge(float DeltaTime);
    void OnDodgeBlendOut(UAnimMontage* Montage, bool bInterrupted);
    void BeginDodgeRecovery();
    void FinishDodge();

    UPROPERTY() TObjectPtr<ACharacter> Character;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeldPistol;
    UPROPERTY() TObjectPtr<UEnhancedInputComponent> ActionInput;
    UPROPERTY() TObjectPtr<UInputMappingContext> ActionMapping;
    TWeakObjectPtr<APlayerController> BoundController;
    FVector DodgeDirection = FVector::ForwardVector;
    float DodgeElapsed = 0.f;
    float DodgeDuration = 0.f;
    bool bDodging = false;
    bool bDodgeRecovering = false;
    bool bDodgeTravelBlocked = false;
};

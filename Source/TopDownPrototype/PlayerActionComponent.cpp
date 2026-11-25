#include "PlayerActionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PlayerLocomotionAnimInstance.h"
#include "UObject/ConstructorHelpers.h"

UPlayerActionComponent::UPlayerActionComponent()
    : ShootKey(EKeys::LeftMouseButton), CrouchKey(EKeys::LeftControl), DodgeKey(EKeys::LeftShift)
{
    PrimaryComponentTick.bCanEverTick = true;
    PistolGrip.SetScale3D(FVector(0.01));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> DefaultIdle(TEXT("/Game/Character/KenneyCharacter/A_Idle_Root_Idle.A_Idle_Root_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> DefaultRun(TEXT("/Game/Character/KenneyCharacter/A_Run_Root_Run.A_Run_Root_Run"));
    Idle = DefaultIdle.Object;
    Run = DefaultRun.Object;
}

void UPlayerActionComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        SetComponentTickEnabled(false);
        return;
    }
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() == TEXT("PlayerBody") || (Mesh->GetSkeletalMeshAsset() && Mesh->GetAnimClass()))
        {
            Body = Mesh;
            break;
        }
    }
    if (Body)
    {
        Body->SetAnimInstanceClass(UPlayerLocomotionAnimInstance::StaticClass());
    }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
}

void UPlayerActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    UnbindPlayerInput();
    Super::EndPlay(Reason);
}

void UPlayerActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime, TickType, Tick);
    APlayerController* Controller = Cast<APlayerController>(Character->GetController());
    if (Controller != BoundController.Get())
    {
        UnbindPlayerInput();
        if (Controller && Controller->IsLocalController())
        {
            BindPlayerInput(Controller);
        }
    }
    if (bDodging)
    {
        AdvanceDodge(DeltaTime);
    }
}

void UPlayerActionComponent::BindPlayerInput(APlayerController* Controller)
{
    auto* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
    if (!InputSubsystem)
    {
        return;
    }
    ActionInput = NewObject<UEnhancedInputComponent>(Controller);
    ActionInput->RegisterComponent();
    ActionMapping = NewObject<UInputMappingContext>(this);
    auto BindPressed = [this](const FKey& Key, void (UPlayerActionComponent::*Callback)())
    {
        UInputAction* Action = NewObject<UInputAction>(ActionMapping);
        Action->ValueType = EInputActionValueType::Boolean;
        Action->bConsumeInput = false;
        ActionMapping->MapKey(Action, Key);
        ActionInput->BindAction(Action, ETriggerEvent::Started, this, Callback);
        return Action;
    };
    BindPressed(ShootKey, &UPlayerActionComponent::Shoot);
    BindPressed(DodgeKey, &UPlayerActionComponent::Dodge);
    UInputAction* Crouch = BindPressed(CrouchKey, &UPlayerActionComponent::BeginCrouch);
    ActionInput->BindAction(Crouch, ETriggerEvent::Completed, this, &UPlayerActionComponent::EndCrouch);
    ActionInput->BindAction(Crouch, ETriggerEvent::Canceled, this, &UPlayerActionComponent::EndCrouch);
    InputSubsystem->AddMappingContext(ActionMapping, 0);
    Controller->PushInputComponent(ActionInput);
    BoundController = Controller;
}

void UPlayerActionComponent::UnbindPlayerInput()
{
    if (APlayerController* Controller = BoundController.Get())
    {
        Controller->PopInputComponent(ActionInput);
        if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
        {
            Subsystem->RemoveMappingContext(ActionMapping);
        }
    }
    if (ActionInput)
    {
        ActionInput->DestroyComponent();
    }
    ActionInput = nullptr;
    ActionMapping = nullptr;
    BoundController.Reset();
}

bool UPlayerActionComponent::EquipPistol(UStaticMesh* Mesh)
{
    if (!Mesh || !Body || HasPistol())
    {
        return false;
    }
    HeldPistol = NewObject<UStaticMeshComponent>(Character, TEXT("HeldPistol"));
    HeldPistol->SetStaticMesh(Mesh);
    HeldPistol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldPistol->SetCastShadow(false);
    HeldPistol->SetupAttachment(Body, TEXT("RightHand"));
    HeldPistol->SetRelativeTransform(PistolGrip);
    HeldPistol->RegisterComponent();
    HeldPistol->SetVisibility(!bDodging || bDodgeRecovering);
    return true;
}

void UPlayerActionComponent::Shoot()
{
    if (HasPistol())
    {
        const FString Message = TEXT("\u5df2\u5c04\u51fb");
        UKismetSystemLibrary::PrintString(this, Message, true, true);
    }
}

void UPlayerActionComponent::BeginCrouch()
{
    if (!bDodging)
    {
        Character->Crouch();
    }
}

void UPlayerActionComponent::EndCrouch()
{
    Character->UnCrouch();
}

void UPlayerActionComponent::Dodge()
{
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    UAnimInstance* Animation = Body ? Body->GetAnimInstance() : nullptr;
    if (bDodging || Character->bIsCrouched || !Movement->IsMovingOnGround() || !DodgeRoll || !Animation)
    {
        return;
    }
    const float PlayRate = FMath::Max(DodgePlayRate, 0.01f);
    const float EffectiveRate = PlayRate * DodgeRoll->RateScale * Body->GlobalAnimRateScale;
    if (EffectiveRate <= 0.f)
    {
        return;
    }
    const float Duration = DodgeRoll->GetPlayLength() / EffectiveRate;
    const float BlendIn = FMath::Clamp(DodgeBlendInTime, 0.f, Duration * 0.25f);
    const float BlendOut = FMath::Clamp(DodgeBlendOutTime, 0.f, Duration * 0.25f);
    UAnimMontage* Montage = Animation->PlaySlotAnimationAsDynamicMontage(
        DodgeRoll, UPlayerLocomotionAnimInstance::FullBodySlot, BlendIn, BlendOut, PlayRate);
    if (!Montage)
    {
        return;
    }
    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(this, &UPlayerActionComponent::OnDodgeBlendOut);
    Animation->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);
    DodgeDirection = Character->GetPendingMovementInputVector().GetSafeNormal2D();
    if (DodgeDirection.IsNearlyZero())
    {
        DodgeDirection = Character->GetVelocity().GetSafeNormal2D();
    }
    if (DodgeDirection.IsNearlyZero())
    {
        DodgeDirection = Character->GetActorForwardVector().GetSafeNormal2D();
    }
    Character->SetActorRotation(DodgeDirection.Rotation());
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    DodgeElapsed = 0.f;
    DodgeDuration = Duration;
    bDodgeTravelBlocked = false;
    bDodgeRecovering = false;
    bDodging = true;
    if (HeldPistol)
    {
        HeldPistol->SetVisibility(false);
    }
}

void UPlayerActionComponent::AdvanceDodge(float DeltaTime)
{
    // Finish the displacement before the animation's recovery frames.
    const float TravelTime = FMath::Max(DodgeDuration * 0.70f, UE_SMALL_NUMBER);
    const float Before = FMath::SmoothStep(0.f, TravelTime, DodgeElapsed);
    DodgeElapsed += DeltaTime;
    const float After = FMath::SmoothStep(0.f, TravelTime, DodgeElapsed);
    if (!bDodgeRecovering && !bDodgeTravelBlocked && DodgeDistance > 0.f && After > Before)
    {
        FHitResult Hit;
        Character->AddActorWorldOffset(DodgeDirection * DodgeDistance * (After - Before), true, &Hit);
        bDodgeTravelBlocked = Hit.IsValidBlockingHit();
    }
    if (DodgeElapsed >= DodgeDuration)
    {
        FinishDodge();
    }
}

void UPlayerActionComponent::OnDodgeBlendOut(UAnimMontage*, bool bInterrupted)
{
    if (!bDodging)
    {
        return;
    }
    BeginDodgeRecovery();
    if (bInterrupted)
    {
        FinishDodge();
    }
}

void UPlayerActionComponent::BeginDodgeRecovery()
{
    if (bDodgeRecovering)
    {
        return;
    }
    bDodgeRecovering = true;
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (HeldPistol)
    {
        HeldPistol->SetVisibility(true);
    }
}

void UPlayerActionComponent::FinishDodge()
{
    BeginDodgeRecovery();
    bDodging = false;
}

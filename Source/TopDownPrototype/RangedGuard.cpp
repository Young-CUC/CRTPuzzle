#include "RangedGuard.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PlayerLocomotionAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Camera/PlayerCameraManager.h"

void UGuardTestHealth::BeginPlay()
{
    Super::BeginPlay();
    GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UGuardTestHealth::Damaged);
}
void UGuardTestHealth::Damaged(AActor* Actor, float Amount, const UDamageType*, AController*, AActor*)
{
    Health = FMath::Max(0.f, Health - FMath::Max(Amount, 0.f));
    if (Health == 0)
        if (auto* Pawn = Cast<ARangedGuard>(Actor))
            if (auto* AI = Cast<ARangedGuardController>(Pawn->GetController())) AI->DisableGuard();
}

ARangedGuard::ARangedGuard()
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ARangedGuardController::StaticClass();
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate.Yaw = 540;
    GetCapsuleComponent()->InitCapsuleSize(36, 86);
    BaseEyeHeight = 52;
    Vitals = CreateDefaultSubobject<UGuardTestHealth>("Vitals");
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Amber(TEXT("/Game/BluePrint/AI/M_GuardBarYellow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Alert(TEXT("/Game/BluePrint/AI/M_GuardBarRed"));
    Yellow = Amber.Object;
    Red = Alert.Object;
    auto Shape = [&](FName Name, FVector Position, FVector Scale)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(RootComponent);
        Part->SetStaticMesh(Cube.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetRelativeLocation(Position);
        Part->SetRelativeScale3D(Scale);
        return Part;
    };
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Character/KenneyCharacter/SK_KenneyCharacter"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Skin(TEXT("/Game/Character/KenneyCharacter/Materials/M_nobody"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Character/KenneyCharacter/Animations/A_Kenney_Pistol_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Move(TEXT("/Game/Character/KenneyCharacter/Animations/A_Kenney_Pistol_Run"));
    PistolIdle = Idle.Object;
    PistolMove = Move.Object;
    GetMesh()->SetSkeletalMeshAsset(Body.Object);
    GetMesh()->SetMaterial(0, Skin.Object);
    GetMesh()->SetRelativeTransform(FTransform(FRotator(0,-90,0), FVector(0,0,-86), FVector(.46)));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetMesh()->SetAnimInstanceClass(UPlayerLocomotionAnimInstance::StaticClass());

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PistolMesh(TEXT("/Game/BluePrint/Interaction/Pistol/SM_KenneyPistol/StaticMeshes/SM_KenneyPistol"));
    GuardPistol = CreateDefaultSubobject<UStaticMeshComponent>("GuardPistol");
    GuardPistol->SetupAttachment(GetMesh(), TEXT("RightHand"));
    GuardPistol->SetStaticMesh(PistolMesh.Object);
    GuardPistol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GuardPistol->SetCanEverAffectNavigation(false);
    // BP_PlayerCharacter's grip is expressed in the imported right-hand bone's local units.
    GuardPistol->SetRelativeTransform(FTransform(FQuat(-.112217,.980970,-.155116,.032317).GetNormalized(),
        FVector(-.010303,-.080472,-.050854), FVector(.0065)));
    Muzzle = CreateDefaultSubobject<USceneComponent>("Muzzle");
    Muzzle->SetupAttachment(GuardPistol);
    Muzzle->SetRelativeLocation(FVector(0,-26,6));
    Beam = Shape("AimLine", FVector::ZeroVector, FVector::OneVector);
    Beam->SetCastShadow(false);
    Beam->SetVisibility(false);
    SuspicionBar = Shape("SuspicionBar", FVector(0,0,105), FVector(.9,.05,.05));
    SuspicionBar->SetVisibility(false);
    SuspicionBar->SetMaterial(0, Yellow);
    Tracer = Shape("Tracer", FVector::ZeroVector, FVector::OneVector);
    Impact = Shape("Impact", FVector::ZeroVector, FVector(.16));
    Tracer->SetAbsolute(true,true,true);
    Impact->SetAbsolute(true,true,true);
    Tracer->SetMaterial(0,Yellow);
    HideShotFeedback();
    Status = CreateDefaultSubobject<UTextRenderComponent>("Status");
    Status->SetupAttachment(RootComponent);
    Status->SetRelativeLocation(FVector(0,0,110));
    Status->SetHorizontalAlignment(EHTA_Center);
    Status->SetWorldSize(40);
    Status->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Status->SetText(FText::GetEmpty());
}
void ARangedGuard::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    // Placed instances saved with the blockout gun can still have the capsule as their muzzle parent.
    if (IsValid(Muzzle) && IsValid(GuardPistol) && Muzzle->GetAttachParent() != GuardPistol)
    {
        Muzzle->AttachToComponent(GuardPistol, FAttachmentTransformRules::SnapToTargetIncludingScale);
        Muzzle->SetRelativeLocation(GetDefault<ARangedGuard>()->Muzzle->GetRelativeLocation());
    }
}
void ARangedGuard::ShowBeam(FVector End, bool Locked)
{
    const FVector Start = Muzzle->GetComponentLocation();
    const FVector Delta = End - Start;
    const float Width = Locked ? .07f : .025f;
    Beam->SetWorldLocationAndRotation((Start + End)*.5, Delta.Rotation());
    Beam->SetWorldScale3D(FVector(Delta.Size()/100., Width, Width));
    Beam->SetMaterial(0, Locked ? Red : Yellow);
    Beam->SetVisibility(true);
}
void ARangedGuard::ShowStatus(const FString& Label, FColor Color)
{
    if (Status->Text.ToString() != Label) Status->SetText(FText::FromString(Label));
    Status->SetTextRenderColor(Color);
    if (auto* Camera = UGameplayStatics::GetPlayerCameraManager(this,0))
        Status->SetWorldRotation((Camera->GetCameraLocation()-Status->GetComponentLocation()).Rotation());
}
void ARangedGuard::DebugKill()
{
    if(GetWorld()->IsGameWorld()) UGameplayStatics::ApplyDamage(this, Vitals->Health, nullptr, this, nullptr);
}
void ARangedGuard::ShotFeedback(const FHitResult& Hit, FVector End, bool HitPlayer)
{
    const FVector Start = Muzzle->GetComponentLocation();
    const FVector Delta = End - Start;
    Tracer->SetWorldLocationAndRotation((Start+End)*.5,Delta.Rotation());
    Tracer->SetWorldScale3D(FVector(Delta.Size()/100.,.05,.05));
    Tracer->SetVisibility(true);
    Impact->SetWorldLocation(End);
    Impact->SetMaterial(0,HitPlayer ? Red : Yellow);
    Impact->SetVisibility(Hit.bBlockingHit);
    GetWorldTimerManager().SetTimer(FeedbackTimer,this,&ARangedGuard::HideShotFeedback,.16f,false);
    if(bDebug) UE_LOG(LogTemp,Display,TEXT("%s shot: %s"),*GetName(),*GetNameSafe(Hit.GetActor()));
}

void ARangedGuard::HideShotFeedback()
{
    Tracer->SetVisibility(false);
    Impact->SetVisibility(false);
}

ARangedGuardController::ARangedGuardController()
{
    PrimaryActorTick.bCanEverTick = true;
    Senses = CreateDefaultSubobject<UAIPerceptionComponent>("GuardPerception");
    SetPerceptionComponent(*Senses);
    Sight = CreateDefaultSubobject<UAISenseConfig_Sight>("Sight");
    Sight->SightRadius = 1200;
    Sight->LoseSightRadius = 1400;
    Sight->PeripheralVisionAngleDegrees = 45;
    Sight->AutoSuccessRangeFromLastSeenLocation = FAISystem::InvalidRange;
    Sight->DetectionByAffiliation.bDetectNeutrals = true;
    Senses->ConfigureSense(*Sight);
    Senses->SetDominantSense(Sight->GetSenseImplementation());
    bSetControlRotationFromPawnOrientation = true;
}
void ARangedGuardController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    Guard = Cast<ARangedGuard>(InPawn);
    if (!Guard) return;
    Senses->OnTargetPerceptionUpdated.AddUniqueDynamic(this,&ARangedGuardController::Sensed);
    Player = nullptr;
    PlayerHealth = nullptr;
    bSight = bCanSeePlayer = false;
    PatrolIndex = FailedPoints = 0;
    NextAim = 0;
    SetActorTickEnabled(true);
    ChangeGuardState(EGuardState::Patrol);
}
void ARangedGuardController::DisableGuard()
{
    MoveId = FAIRequestID::InvalidRequest;
    MoveResult.Reset();
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    Senses->OnTargetPerceptionUpdated.RemoveDynamic(this,&ARangedGuardController::Sensed);
    if (IsValid(Guard))
    {
        CancelAim();
        Guard->ShowStatus(TEXT(""),FColor::White);
        Guard->SuspicionBar->SetVisibility(false);
    }
    SetActorTickEnabled(false);
}
void ARangedGuardController::OnUnPossess()
{
    DisableGuard();
    Guard = nullptr;
    Super::OnUnPossess();
}
void ARangedGuardController::EndPlay(const EEndPlayReason::Type Reason)
{
    DisableGuard();
    Super::EndPlay(Reason);
}
void ARangedGuardController::Sensed(AActor* Actor, FAIStimulus Stimulus)
{
    if (Actor != UGameplayStatics::GetPlayerPawn(this,0) || Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>()) return;
    bSight = Stimulus.WasSuccessfullySensed();
    if (!bSight && bCanSeePlayer)
    {
        bCanSeePlayer = false;
        LostAt = GetWorld()->GetTimeSeconds();
        CancelAim();
    }
}
FHitResult ARangedGuardController::Trace(FVector Start, FVector End) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightOrShot),false,Guard);
    TArray<AActor*> Attached;
    Guard->GetAttachedActors(Attached,true,true);
    Params.AddIgnoredActors(Attached);
    GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
    return Hit;
}
bool ARangedGuardController::EyeClear() const
{
    if (!bSight || !IsValid(Player)) return false;
    const FVector Eyes = Guard->GetPawnViewLocation();
    const FVector Center = Player->GetActorLocation();
    const FVector Offset = Center - Eyes;
    const auto* Config = Senses->GetSenseConfig<UAISenseConfig_Sight>();
    if (!Config) return false;
    const float Range = bCanSeePlayer ? Config->LoseSightRadius : Config->SightRadius;
    if (Offset.SizeSquared() > FMath::Square(Range)) return false;
    const float Facing = FVector::DotProduct(Guard->GetActorForwardVector(),Offset.GetSafeNormal());
    return Facing >= FMath::Cos(FMath::DegreesToRadians(Config->PeripheralVisionAngleDegrees))
        && Trace(Eyes,Center).GetActor() == Player;
}
void ARangedGuardController::Face(float Yaw)
{
    const FRotator Rotation(0,Yaw,0);
    Guard->SetActorRotation(Rotation);
    SetControlRotation(Rotation);
}
void ARangedGuardController::CancelAim()
{
    Aim = EGuardAim::Idle;
    Guard->Beam->SetVisibility(false);
}
void ARangedGuardController::ChangeGuardState(EGuardState Next)
{
    MoveId = FAIRequestID::InvalidRequest;
    MoveResult.Reset();
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    CancelAim();
    State = Next;
    const double Now = GetWorld()->GetTimeSeconds();
    Deadline = Now;
    bHolding = bLooking = false;
    if (Next == EGuardState::Patrol)
    {
        Suspicion = 0;
        bHasLastSeen = false;
        FailedPoints = 0;
        Guard->GetCharacterMovement()->MaxWalkSpeed = Guard->PatrolSpeed;
    }
    else if (Next == EGuardState::Search)
    {
        Guard->GetCharacterMovement()->MaxWalkSpeed = Guard->SearchSpeed;
        Deadline = Now + Guard->SearchTimeout;
        MoveToGuardLocation(LastSeenLocation);
    }
}
void ARangedGuardController::MoveToGuardLocation(FVector Destination)
{
    FAIMoveRequest Request(Destination);
    Request.SetAcceptanceRadius(Guard->ArrivalRadius);
    Request.SetReachTestIncludesAgentRadius(false);
    Request.SetAllowPartialPath(false);
    auto Result = MoveTo(Request);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful) MoveId = Result.MoveId;
    else MoveResult = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
}
void ARangedGuardController::OnMoveCompleted(FAIRequestID Id,const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(Id,Result);
    if (MoveId.IsValid() && MoveId == Id)
    {
        MoveId = FAIRequestID::InvalidRequest;
        MoveResult = Result.IsSuccess();
    }
}
void ARangedGuardController::AdvanceRoute(double Now)
{
    const int32 Count = Guard->PatrolPoints.Num();
    if (!Count || bHolding || MoveId.IsValid()) return;
    if (MoveResult.IsSet())
    {
        bool Success = MoveResult.GetValue();
        MoveResult.Reset();
        FailedPoints = Success ? 0 : FailedPoints + 1;
        if ((!Success && FailedPoints >= Count) || (Success && Count == 1))
        {
            bHolding = true;
            if (!Success) UE_LOG(LogTemp, Warning, TEXT("%s: no reachable patrol points"),*Guard->GetName());
            return;
        }
        PatrolIndex = (PatrolIndex+1)%Count;
        Deadline = Now + (Success ? Guard->PatrolWait : .25);
    }
    if (Now < Deadline) return;
    PatrolIndex %= Count;
    if (IsValid(Guard->PatrolPoints[PatrolIndex])) MoveToGuardLocation(Guard->PatrolPoints[PatrolIndex]->GetActorLocation());
    else MoveResult = false;
}
void ARangedGuardController::BeginLook(double Now)
{
    MoveId = FAIRequestID::InvalidRequest;
    StopMovement();
    MoveResult.Reset();
    bLooking = true;
    Deadline = Now + Guard->LookDuration;
}
void ARangedGuardController::Attack(double Now)
{
    if (Aim == EGuardAim::Idle)
    {
        if (Now < NextAim) return;
        Aim = EGuardAim::Track;
        AimDeadline = Now + Guard->TrackingTime;
    }
    if (Aim == EGuardAim::Track) Face(SeenYaw);
    const FVector Start = Guard->Muzzle->GetComponentLocation();
    const FVector Direction = Aim == EGuardAim::Lock ? LockedDirection : (LastSeenLocation-Start).GetSafeNormal();
    Guard->Muzzle->SetWorldRotation(Direction.Rotation());
    const FVector Far = Start + Direction * Guard->ShotRange;
    auto Hit = Trace(Start,Far);
    const FVector End = Hit.bBlockingHit ? Hit.ImpactPoint : Far;
    if (Aim == EGuardAim::Track && Now >= AimDeadline)
    {
        LockedDirection = Direction;
        Aim = EGuardAim::Lock;
        AimDeadline = Now + Guard->LockTime;
    }
    else if (Aim == EGuardAim::Lock && Now >= AimDeadline)
    {
        CancelAim();
        if (!EyeClear()) return;
        NextAim = Now + Guard->Cooldown;
        const bool HitPlayer = Hit.GetActor() == Player;
        if (HitPlayer) UGameplayStatics::ApplyDamage(Player,Guard->ShotDamage,this,Guard,nullptr);
        Guard->ShotFeedback(Hit,End,HitPlayer);
        return;
    }
    Guard->ShowBeam(End,Aim == EGuardAim::Lock);
}
void ARangedGuardController::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!IsValid(Guard) || Guard->Vitals->Health <= 0) { DisableGuard(); return; }
    if (!PlayerHealth)
    {
        Player = UGameplayStatics::GetPlayerPawn(this,0);
        if (Player)
        {
            PlayerHealth = Player->FindComponentByClass<UGuardTestHealth>();
            if (!PlayerHealth)
            {
                PlayerHealth = NewObject<UGuardTestHealth>(Player,TEXT("GuardTestHealth"));
                Player->AddInstanceComponent(PlayerHealth);
                PlayerHealth->RegisterComponent();
            }
            if (auto* Shape = Cast<UPrimitiveComponent>(Player->GetRootComponent()))
                Shape->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        }
    }
    const double Now = GetWorld()->GetTimeSeconds();
    const bool Alive = IsValid(Player) && IsValid(PlayerHealth) && PlayerHealth->Health > 0;
    const bool Seen = Alive && EyeClear();
    if (!Alive && State != EGuardState::Patrol) ChangeGuardState(EGuardState::Patrol);
    if (Seen)
    {
        LastSeenLocation = Player->GetActorLocation();
        SeenYaw = (LastSeenLocation-Guard->GetActorLocation()).Rotation().Yaw;
        bHasLastSeen = true;
    }
    else if (bCanSeePlayer) { LostAt = Now; CancelAim(); }
    const bool PreviouslyVisible = bCanSeePlayer;
    bCanSeePlayer = Seen;
    if (Seen && State == EGuardState::Patrol) ChangeGuardState(EGuardState::Suspicious);
    if (State == EGuardState::Suspicious)
    {
        if (Seen)
        {
            Face(SeenYaw);
            if (PreviouslyVisible) Suspicion = FMath::Min(100.f,Suspicion + Guard->SuspicionGain*Dt);
            if (Suspicion >= 100) ChangeGuardState(EGuardState::Combat);
        }
        else
        {
            const double DecayTime = FMath::Clamp(Now-LostAt-Guard->MemoryHold,0.,double(Dt));
            Suspicion = FMath::Max(0.f,Suspicion-float(DecayTime)*Guard->SuspicionDecay);
            if (Suspicion <= 0) ChangeGuardState(EGuardState::Patrol);
        }
    }
    if (State == EGuardState::Search && Seen) ChangeGuardState(EGuardState::Combat);
    if (State == EGuardState::Combat)
    {
        if (Seen) Attack(Now);
        else if (Now-LostAt >= Guard->SearchDelay) ChangeGuardState(EGuardState::Search);
    }
    if (State == EGuardState::Search)
    {
        if (!bLooking && (MoveResult.IsSet() || Now >= Deadline)) BeginLook(Now);
        if (bLooking)
        {
            const double Phase = 1.-(Deadline-Now)/FMath::Max(.01f,Guard->LookDuration);
            Face(SeenYaw + 45*FMath::Sin(Phase*2*PI));
            if (Now >= Deadline) ChangeGuardState(EGuardState::Patrol);
        }
    }
    if (State == EGuardState::Patrol) AdvanceRoute(Now);
    FString Label;
    FColor Color = FColor::White;
    if (State == EGuardState::Suspicious) { Label = FString::Printf(TEXT("? %d%%"),FMath::RoundToInt(Suspicion)); Color = FColor::Yellow; }
    if (State == EGuardState::Combat) { Label = TEXT("!"); Color = FColor::Red; }
    if (State == EGuardState::Search) Label = TEXT("SEARCH");
    if (Guard->bDebug)
    {
        Label += FString::Printf(TEXT(" %s %.0f"),*UEnum::GetValueAsString(State),Suspicion);
        if (bHasLastSeen) DrawDebugSphere(GetWorld(),LastSeenLocation,18,8,FColor::Cyan);
    }
    Guard->ShowStatus(Label,Color);
    Guard->SuspicionBar->SetVisibility(State == EGuardState::Suspicious);
    if (State == EGuardState::Suspicious)
    {
        Guard->SuspicionBar->SetWorldRotation(Guard->Status->GetComponentRotation());
        Guard->SuspicionBar->SetWorldScale3D(FVector(.04,.9*Suspicion/100.,.055));
    }
}

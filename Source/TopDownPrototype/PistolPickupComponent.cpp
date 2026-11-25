#include "PistolPickupComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PlayerActionComponent.h"
#include "UObject/UnrealType.h"

UPistolPickupComponent::UPistolPickupComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPistolPickupComponent::BeginPlay()
{
    Super::BeginPlay();
    TInlineComponentArray<UStaticMeshComponent*> Meshes(GetOwner());
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetStaticMesh())
        {
            ItemMesh = Mesh;
            break;
        }
    }
    // BP_InteractableBase sets this when its existing E interaction succeeds.
    HasInteracted = FindFProperty<FBoolProperty>(GetOwner()->GetClass(), TEXT("HasInteracted"));
    SetComponentTickEnabled(ItemMesh && HasInteracted);
}

void UPistolPickupComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime, TickType, Tick);
    AActor* Pickup = GetOwner();
    if (!HasInteracted->GetPropertyValue_InContainer(Pickup))
    {
        return;
    }
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UPlayerActionComponent* Actions = Pawn ? Pawn->FindComponentByClass<UPlayerActionComponent>() : nullptr;
    if (Actions && Actions->EquipPistol(ItemMesh->GetStaticMesh()))
    {
        Pickup->Destroy();
        return;
    }
    HasInteracted->SetPropertyValue_InContainer(Pickup, false);
}

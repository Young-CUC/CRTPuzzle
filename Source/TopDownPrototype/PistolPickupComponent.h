#pragma once

#include "Components/ActorComponent.h"
#include "PistolPickupComponent.generated.h"

class UStaticMeshComponent;
class FBoolProperty;

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class UPistolPickupComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPistolPickupComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ItemMesh;
    const FBoolProperty* HasInteracted = nullptr;
};

#pragma once

#include "Animation/AnimInstance.h"
#include "PlayerLocomotionAnimInstance.generated.h"

UCLASS(Transient)
class UPlayerLocomotionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    static const FName FullBodySlot;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

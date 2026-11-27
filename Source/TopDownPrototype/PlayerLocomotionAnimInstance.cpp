#include "PlayerLocomotionAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "GameFramework/Character.h"
#include "PlayerActionComponent.h"
#include "RangedGuard.h"

const FName UPlayerLocomotionAnimInstance::FullBodySlot(TEXT("DefaultSlot"));

namespace
{
    constexpr int32 PoseCount = 8;

    struct FPlayerLocomotionNode : FAnimNode_Base
    {
        FAnimNode_SequencePlayer_Standalone Players[PoseCount];
        int32 RequestedPose = 0;
        float BlendTime = 0.15f;

        virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
        {
            for (auto& Player : Players)
            {
                Player.SetLoopAnimation(true);
                Player.Initialize_AnyThread(Context);
            }
        }

        virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
        {
            for (auto& Player : Players)
            {
                Player.CacheBones_AnyThread(Context);
            }
        }

        virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
        {
            if (RequestedPose != ActivePose)
            {
                if (ActivePose == INDEX_NONE)
                {
                    Weights[RequestedPose] = 1.f;
                }
                ActivePose = RequestedPose;
                FMemory::Memcpy(InitialWeights, Weights, sizeof(Weights));
                Elapsed = 0.f;
            }

            Elapsed += Context.GetDeltaTime();
            const float Alpha = BlendTime > 0.f ? FMath::SmoothStep(0.f, BlendTime, Elapsed) : 1.f;
            for (int32 Index = 0; Index < PoseCount; ++Index)
            {
                Weights[Index] = FMath::Lerp(InitialWeights[Index], Index == ActivePose ? 1.f : 0.f, Alpha);
                if (FAnimWeight::IsRelevant(Weights[Index]))
                {
                    Players[Index].Update_AnyThread(Context.FractionalWeight(Weights[Index]));
                }
            }
        }

        virtual void Evaluate_AnyThread(FPoseContext& Output) override
        {
            if (ActivePose == INDEX_NONE)
            {
                Output.ResetToRefPose();
                return;
            }
            float TotalWeight = 0.f;
            for (int32 Index = 0; Index < PoseCount; ++Index)
            {
                const float Weight = Weights[Index];
                if (!FAnimWeight::IsRelevant(Weight))
                {
                    continue;
                }
                if (TotalWeight == 0.f)
                {
                    Players[Index].Evaluate_AnyThread(Output);
                }
                else
                {
                    FPoseContext NextPose(Output);
                    Players[Index].Evaluate_AnyThread(NextPose);
                    FAnimationPoseData OutputData(Output);
                    const FAnimationPoseData NextData(NextPose);
                    FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutputData, NextData, TotalWeight / (TotalWeight + Weight));
                }
                TotalWeight += Weight;
            }
        }

    private:
        int32 ActivePose = INDEX_NONE;
        float Elapsed = 0.f;
        float InitialWeights[PoseCount] = {};
        float Weights[PoseCount] = {};
    };

    struct FPlayerLocomotionProxy : FAnimInstanceProxy
    {
        explicit FPlayerLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
        {
            Slot.SlotName = UPlayerLocomotionAnimInstance::FullBodySlot;
            Slot.bAlwaysUpdateSourcePose = true;
            Slot.Source.SetLinkNode(&Locomotion);
        }

        virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }

        virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
        {
            FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
            const ACharacter* Character = Cast<ACharacter>(Instance->GetOwningActor());
            if (!Character)
            {
                return;
            }
            const bool bMoving = Character->GetVelocity().SizeSquared2D() > FMath::Square(8.f);
            if (const ARangedGuard* Guard = Cast<ARangedGuard>(Character))
            {
                Locomotion.Players[4].SetSequence(Guard->PistolIdle);
                Locomotion.Players[5].SetSequence(Guard->PistolMove);
                Locomotion.RequestedPose = bMoving ? 5 : 4;
                return;
            }
            const UPlayerActionComponent* Actions = Character->FindComponentByClass<UPlayerActionComponent>();
            if (!Actions)
            {
                return;
            }
            UAnimSequence* const Sequences[PoseCount] = {
                Actions->Idle, Actions->Run, Actions->CrouchIdle, Actions->CrouchWalk,
                Actions->PistolIdle, Actions->PistolRun, Actions->PistolCrouchIdle, Actions->PistolCrouchWalk
            };
            for (int32 Index = 0; Index < PoseCount; ++Index)
            {
                Locomotion.Players[Index].SetSequence(Sequences[Index] ? Sequences[Index] : Sequences[0]);
            }
            Locomotion.RequestedPose = (Actions->HasPistol() ? 4 : 0)
                + (Character->bIsCrouched ? 2 : 0) + (bMoving ? 1 : 0);
            Locomotion.BlendTime = Actions->LocomotionBlendTime;
        }

        FPlayerLocomotionNode Locomotion;
        FAnimNode_Slot Slot;
    };
}

FAnimInstanceProxy* UPlayerLocomotionAnimInstance::CreateAnimInstanceProxy()
{
    return new FPlayerLocomotionProxy(this);
}

void UPlayerLocomotionAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
    delete Proxy;
}

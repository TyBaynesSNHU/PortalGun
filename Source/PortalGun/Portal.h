// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ArrowComponent.h"
#include "Portal.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UBoxComponent;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class EPortalColor : uint8
{
    Blue    UMETA(DisplayName = "Blue"),
    Orange  UMETA(DisplayName = "Orange")
};

UCLASS()
class PORTALGUN_API APortal : public AActor
{
    GENERATED_BODY()

public:
    APortal();

    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    UStaticMeshComponent* PortalSurface;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    USceneCaptureComponent2D* sceneCapture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    UBoxComponent* PortalCollision;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FName PortalGroupTag;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    APortal* PartnerPortal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
       class UArrowComponent* PortalAttach;

    // Call this whenever the portal (or its partner) is moved/spawned
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void UpdateClipPlane();

    void NotifyPartner();
    void bSetPortalActive(bool bIsActive);

protected:
    virtual void BeginPlay() override;

    void TryAutoAssignPartner();
    void CreateRenderTarget();
    void UpdateSceneCapture();

    /** The core teleportation logic for all actor types */
    void TeleportActor(AActor* ActorToTeleport);

    FVector ConvertLocationToPartner(FVector Location);
    FRotator ConvertRotationToPartner(FRotator Rotation);

    UFUNCTION()
    void OnActorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnActorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    UTextureRenderTarget2D* PortalRenderTarget;

    UPROPERTY()
    TArray<AActor*> OverlappingActors;

    bool bIsPortalActive = false;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
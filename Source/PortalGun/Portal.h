// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Portal.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UBoxComponent;
class UCharacterMovementComponent;

UCLASS()
class PORTALGUN_API APortal : public AActor
{
	GENERATED_BODY()

public:
	APortal();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	//Components
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PortalSurface;

	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* sceneCapture;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* PortalCollision;

	//Render target
	UPROPERTY()
	UTextureRenderTarget2D* PortalRenderTarget;

	UPROPERTY()
	UMaterialInstanceDynamic* PortalDynamicMaterial;

	//Partner portal
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	APortal* PartnerPortal;

	//Finder tag
	UPROPERTY(EditAnywhere)
	FName PortalGroupTag;

private:
	void CreateRenderTarget();
	void SetupMaterial();
	void UpdateSceneCapture();
	void TryAutoAssignPartner();
	FVector ConvertLocationToPartner(FVector Location);
	FRotator ConvertRotationToPartner(FRotator Rotation);
	bool bRecentlyTeleported = false;
	FTimerHandle TeleportCooldownHandle;
	void ResetTeleport();

	UFUNCTION()
	void OnActorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
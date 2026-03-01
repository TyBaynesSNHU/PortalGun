// Fill out your copyright notice in the Description page of Project Settings.


#include "Portal.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PortalGunCharacter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"

// Sets default values
APortal::APortal()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//Create components
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PortalSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalSurface"));
	PortalSurface->SetupAttachment(RootComponent);
	sceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("sceneCapture"));
	sceneCapture->SetupAttachment(RootComponent);
	PortalCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalCollision"));
	PortalCollision->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnActorBeginOverlap);
	PortalCollision->SetupAttachment(RootComponent);

	sceneCapture->bCaptureEveryFrame = false;

}

void APortal::TryAutoAssignPartner()
{
	if (PartnerPortal) return;
	TArray<AActor*> FoundPortals;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APortal::StaticClass(), FoundPortals);

	for (AActor* Actor : FoundPortals)
	{
		APortal* Portal = Cast<APortal>(Actor);
		if (Portal && Portal != this)
		{
			if (Portal->PortalGroupTag == PortalGroupTag)
			{
				PartnerPortal = Portal;
				Portal->PartnerPortal = this;
				break;
			}
		}
	}
}

// Called when the game starts or when spawned
void APortal::BeginPlay()
{
	Super::BeginPlay();
	

	TryAutoAssignPartner();
	CreateRenderTarget();
	SetupMaterial();

	if (PartnerPortal)
	{
		sceneCapture->bEnableClipPlane = true;
		sceneCapture->ClipPlaneBase = PartnerPortal->GetActorLocation();
		sceneCapture->ClipPlaneNormal = PartnerPortal->GetActorForwardVector();
	}
}


void APortal::CreateRenderTarget()
{
	PortalRenderTarget = NewObject<UTextureRenderTarget2D>();
	PortalRenderTarget->InitAutoFormat(1024, 1024);
	PortalRenderTarget->UpdateResourceImmediate(true);

	sceneCapture->TextureTarget = PortalRenderTarget;
}

void APortal::SetupMaterial()
{
	if (!PortalSurface || !PartnerPortal || !PartnerPortal->PortalRenderTarget) return;

	UMaterialInterface* BaseMaterial = PortalSurface->GetMaterial(0);
	PortalDynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	PortalDynamicMaterial->SetTextureParameterValue("PortalTexture", PartnerPortal->PortalRenderTarget);
	PortalSurface->SetMaterial(0, PortalDynamicMaterial);
}

FVector APortal::ConvertLocationToPartner(FVector Location)
{
	FTransform SourceTransform = GetActorTransform();
	FTransform TargetTransform = PartnerPortal->GetActorTransform();

	FVector LocalPosition = SourceTransform.InverseTransformPosition(Location);


	//Mirror
	LocalPosition.X *= -1.f;
	LocalPosition.Y *= -1.f;

	return TargetTransform.TransformPosition(LocalPosition);
}

FRotator APortal::ConvertRotationToPartner(FRotator Rotation)
{
	FTransform SourceTransform = GetActorTransform();
	FTransform TargetTransform = PartnerPortal->GetActorTransform();
	FQuat LocalQuat = SourceTransform.InverseTransformRotation(Rotation.Quaternion());

	FRotator LocalRot = LocalQuat.Rotator();
	LocalRot.Yaw += 180.f;
	return TargetTransform.TransformRotation(LocalRot.Quaternion()).Rotator();
}

void APortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!PartnerPortal) return;
	UpdateSceneCapture();
}

void APortal::UpdateSceneCapture()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn) return;

	FVector NewLocation = ConvertLocationToPartner(PlayerPawn->GetActorLocation());
	FRotator NewRotation = ConvertRotationToPartner(PlayerPawn->GetActorRotation());

	sceneCapture->SetWorldLocationAndRotation(NewLocation, NewRotation);

	sceneCapture->CaptureScene();
}

void APortal::OnActorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!PartnerPortal || !OtherActor) return;
	if (bRecentlyTeleported) return;

	bRecentlyTeleported = true;
	PartnerPortal->bRecentlyTeleported = true;

	FVector NewLocation = ConvertLocationToPartner(OtherActor->GetActorLocation());
	FRotator NewRotation = ConvertRotationToPartner(OtherActor->GetActorRotation());

	//Velocity
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character)
	{
		UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();

		FVector LocalVelocity = GetActorTransform().InverseTransformVector(MovementComp->Velocity);

		LocalVelocity.X *= -1.f;
		LocalVelocity.Y *= -1.f;

		FVector NewVelocity = PartnerPortal->GetActorTransform().TransformVector(LocalVelocity);

		MovementComp->Velocity = NewVelocity;
	}
	OtherActor->SetActorLocationAndRotation(NewLocation, NewRotation);

	GetWorldTimerManager().SetTimer(TeleportCooldownHandle, this, &APortal::ResetTeleport, 0.5f, false);
}

void APortal::ResetTeleport()
{
	bRecentlyTeleported = false;
}


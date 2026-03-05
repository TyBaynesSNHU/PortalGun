#include "Portal.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetRenderingLibrary.h"

APortal::APortal()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    PortalSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalSurface"));
    PortalSurface->SetupAttachment(RootComponent);

	PortalAttach = CreateDefaultSubobject<UArrowComponent>(TEXT("PortalAttach"));
	PortalAttach->SetupAttachment(RootComponent);

    sceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("sceneCapture"));
    sceneCapture->SetupAttachment(RootComponent);
    sceneCapture->bCaptureEveryFrame = false;
    sceneCapture->bCaptureOnMovement = false;

    PortalCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalCollision"));
    PortalCollision->SetupAttachment(RootComponent);
}

void APortal::BeginPlay()
{
    Super::BeginPlay();
    CreateRenderTarget();

    // Always clear the render target immediately after creation
    if (PortalRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), PortalRenderTarget, FLinearColor::Black);
    }

    UMaterialInstanceDynamic* DynMat = PortalSurface->CreateAndSetMaterialInstanceDynamic(0);
    if (DynMat && PortalRenderTarget)
    {
        DynMat->SetTextureParameterValue(FName("PortalTexture"), PortalRenderTarget);
    }

    TryAutoAssignPartner();
    NotifyPartner();
    UpdateClipPlane();

    // ADD THIS: Force another clear after partner assignment if still no partner
    if (!PartnerPortal && PortalRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), PortalRenderTarget, FLinearColor::Black);
    }

    // Bind overlap events
    PortalCollision->OnComponentBeginOverlap.AddDynamic(this, &APortal::OnActorBeginOverlap);
    PortalCollision->OnComponentEndOverlap.AddDynamic(this, &APortal::OnActorEndOverlap);
}

void APortal::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!PartnerPortal || !bIsPortalActive) return;

    UpdateSceneCapture();

    for (int32 i = OverlappingActors.Num() - 1; i >= 0; --i)
    {
        // Safety check: Ensure the actor still exists and is valid (it could have been destroyed or teleported by another portal)
        if (!OverlappingActors.IsValidIndex(i)) continue;

        AActor* Actor = OverlappingActors[i];

        // Safety check: Ensure the actor is still valid (it could have been destroyed)
        if (!IsValid(Actor))
        {
            OverlappingActors.RemoveAt(i);
            continue;
        }

        // Logic check: Ensure the actor is moving towards the portal and is behind it
        FVector DirectionToActor = Actor->GetActorLocation() - GetActorLocation();
        float DotPosition = FVector::DotProduct(DirectionToActor, GetActorForwardVector());

        FVector ActorVelocity = Actor->GetVelocity();

		//Prioritize ProjectileMovementComponent velocity if it exists so projectiles actually register as moving
		//Projectiles were missing teleportation by slipping in between frames/ticks
        if (UProjectileMovementComponent* ProjComp = Actor->FindComponentByClass<UProjectileMovementComponent>())
        {
            ActorVelocity = ProjComp->Velocity;
        }

        float DotVelocity = FVector::DotProduct(ActorVelocity, GetActorForwardVector());

        // Logic: Behind the plane AND moving into the portal
        if (DotPosition < 0.0f && DotVelocity < 0.0f)
        {
            TeleportActor(Actor);

            // Final safety: Ensure the index wasn't removed by TeleportActor's movement
            if (OverlappingActors.IsValidIndex(i))
            {
                OverlappingActors.RemoveAt(i);
            }
        }
    }
}

void APortal::UpdateSceneCapture()
{
    // 1. Safety & Performance Checks
    if (!PartnerPortal || !PortalRenderTarget)
    {
        if (PortalRenderTarget)
        {
            UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), PortalRenderTarget, FLinearColor::Black);
        }
        return;
    }

    // Only render if the portal is actually visible to the player to save FPS
    if (!PortalSurface->WasRecentlyRendered(0.1f)) return;

    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
    if (!CamManager) return;

    // 2. Calculate the Camera's Relative Transform to THIS portal
    FTransform PortalTransform = GetActorTransform();
    FVector CameraLocation = CamManager->GetCameraLocation();
    FRotator CameraRotation = CamManager->GetCameraRotation();

    // Convert World Camera Space to Local Portal Space
    FVector LocalLocation = PortalTransform.InverseTransformPosition(CameraLocation);
    FQuat LocalQuat = PortalTransform.InverseTransformRotation(CameraRotation.Quaternion());

    // 3. Mirror the Transform for the Exit Portal
    // We flip X and Y and rotate Yaw 180 degrees so the capture faces "out" of the partner portal
    LocalLocation.X *= -1.f;
    LocalLocation.Y *= -1.f;

    FRotator LocalRot = LocalQuat.Rotator();
    LocalRot.Yaw += 180.f;
    LocalRot.Roll *= -1.f; // Flip roll to maintain orientation

    // 4. Apply the Mirrored Transform to the Partner Portal's World Space
    FTransform PartnerTransform = PartnerPortal->GetActorTransform();
    FVector NewWorldLocation = PartnerTransform.TransformPosition(LocalLocation);
    FQuat NewWorldQuat = PartnerTransform.TransformRotation(LocalRot.Quaternion());

    // 5. Update the Capture Component
    sceneCapture->SetWorldLocationAndRotation(NewWorldLocation, NewWorldQuat);

    // Update the Clip Plane so we don't render objects behind the exit portal
    sceneCapture->ClipPlaneBase = PartnerPortal->GetActorLocation();
    sceneCapture->ClipPlaneNormal = PartnerPortal->GetActorForwardVector();

    // Finally, capture the frame
    sceneCapture->CaptureScene();
}

void APortal::TeleportActor(AActor* ActorToTeleport)
{
    if (!PartnerPortal || !ActorToTeleport) return;

    // Calculate new location and rotation
    FVector NewLocation = ConvertLocationToPartner(ActorToTeleport->GetActorLocation());
    FRotator NewRotation = ConvertRotationToPartner(ActorToTeleport->GetActorRotation());

    // Calculate new velocity
    FVector Velocity = FVector::ZeroVector;

    // Check for Character Movement
    if (ACharacter* Character = Cast<ACharacter>(ActorToTeleport))
    {
        Velocity = Character->GetCharacterMovement()->Velocity;
    }
    // Check for Physics Objects
    else if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(ActorToTeleport->GetRootComponent()))
    {
        if (RootPrim->IsSimulatingPhysics())
        {
            Velocity = RootPrim->GetPhysicsLinearVelocity();
        }
    }
    // Check for Projectiles
    if (UProjectileMovementComponent* ProjComp = ActorToTeleport->FindComponentByClass<UProjectileMovementComponent>())
    {
        Velocity = ProjComp->Velocity;
    }

    // Transform the velocity vector (using mirror logic)
    FVector LocalVelocity = GetActorTransform().InverseTransformVector(Velocity);
    LocalVelocity.X *= -1.f;
    LocalVelocity.Y *= -1.f;
    FVector NewVelocity = PartnerPortal->GetActorTransform().TransformVector(LocalVelocity);

    //Decouple Character Actor rotation from Controller rotation to prevent skewing and flipped movement
    //If portals were angled it caused a whole slew of camera issues
    if (ACharacter* Character = Cast<ACharacter>(ActorToTeleport))
    {
        // Strip Pitch and Roll so the capsule stays safely upright (Z-Up)
        FRotator UprightRotation = NewRotation;
        UprightRotation.Pitch = 0.0f;
        UprightRotation.Roll = 0.0f;

        // Teleport the Actor with the upright rotation
        ActorToTeleport->SetActorLocationAndRotation(NewLocation, UprightRotation, false, nullptr, ETeleportType::TeleportPhysics);

        Character->GetCharacterMovement()->Velocity = NewVelocity;

        if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
        {
            // Apply the TRUE transformed rotation to the camera so looking through feels seamless
            PC->SetControlRotation(NewRotation);
        }
    }
    // Non-Character Logic (Physics Props, Projectiles)
    else
    {
        // Everything else can tumble and rotate freely
        ActorToTeleport->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

        if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(ActorToTeleport->GetRootComponent()))
        {
            if (RootPrim->IsSimulatingPhysics())
            {
                RootPrim->SetPhysicsLinearVelocity(NewVelocity);
            }
        }

        if (UProjectileMovementComponent* ProjComp = ActorToTeleport->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjComp->Velocity = NewVelocity;
            ProjComp->UpdateComponentVelocity();
        }
    }
}

void APortal::UpdateClipPlane()
{
    if (PartnerPortal && sceneCapture)
    {
        sceneCapture->bEnableClipPlane = true;
        sceneCapture->ClipPlaneBase = PartnerPortal->GetActorLocation();
        sceneCapture->ClipPlaneNormal = PartnerPortal->GetActorForwardVector();
    }
}

void APortal::OnActorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    // FIX for Projectiles: If it's a fast-moving projectile, teleport it IMMEDIATELY on overlap
    if (OtherActor->FindComponentByClass<UProjectileMovementComponent>())
    {
        TeleportActor(OtherActor);
    }
    else
    {
        OverlappingActors.AddUnique(OtherActor);
    }
}

void APortal::OnActorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor)
    {
        OverlappingActors.Remove(OtherActor);
    }
}

FVector APortal::ConvertLocationToPartner(FVector Location)
{
    FTransform SourceTransform = GetActorTransform();
    FTransform TargetTransform = PartnerPortal->GetActorTransform();
    FVector LocalPosition = SourceTransform.InverseTransformPosition(Location);
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

void APortal::TryAutoAssignPartner()
{
    if (PartnerPortal) return;
    TArray<AActor*> FoundPortals;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APortal::StaticClass(), FoundPortals);

    for (AActor* Actor : FoundPortals)
    {
        APortal* Portal = Cast<APortal>(Actor);
        if (Portal && Portal != this && Portal->PortalGroupTag == PortalGroupTag)
        {
            PartnerPortal = Portal;
            Portal->PartnerPortal = this;
            Portal->UpdateClipPlane();
            break;
        }
    }
}

void APortal::CreateRenderTarget()
{
    PortalRenderTarget = NewObject<UTextureRenderTarget2D>(this);
    PortalRenderTarget->InitAutoFormat(1024, 1024);
    PortalRenderTarget->UpdateResourceImmediate(true);
    if (sceneCapture)
    {
        sceneCapture->TextureTarget = PortalRenderTarget;
    }
}

void APortal::NotifyPartner()
{
    if (!PartnerPortal)
    {
        TryAutoAssignPartner();
    }

    if (PartnerPortal)
    {
		this->bSetPortalActive(true);
		PartnerPortal->bSetPortalActive(true);

		this->UpdateClipPlane();
		PartnerPortal->UpdateClipPlane();
    }
    else
    {
		bSetPortalActive(false);
    }
}

void APortal::bSetPortalActive(bool bIsActive)
{
    bIsPortalActive = bIsActive;

    if (sceneCapture)
    {
        if (bIsActive)
        {
            sceneCapture->Activate();
            sceneCapture->bCaptureEveryFrame = false;
            UpdateClipPlane();
        }
        else
        {
            sceneCapture->Deactivate();

            // Return texture to black
            if (PortalRenderTarget)
            {
                UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), PortalRenderTarget, FLinearColor::Black);
            }
			sceneCapture->Deactivate();
        }
    }
}

void APortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PartnerPortal)
    {
        PartnerPortal->PartnerPortal = nullptr;
        PartnerPortal->bSetPortalActive(false);
    }
    Super::EndPlay(EndPlayReason);
}
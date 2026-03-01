// Fill out your copyright notice in the Description page of Project Settings.


#include "Portal_Gun.h"

// Sets default values
APortal_Gun::APortal_Gun()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APortal_Gun::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APortal_Gun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


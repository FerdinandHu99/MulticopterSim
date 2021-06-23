/*
* Class implementation for Phantom pawn in MulticopterSim
*
* Copyright (C) 2019 Simon D. Levy
*
* MIT License
*/

#include "PhantomPawn.h"

APhantomPawn::APhantomPawn()
{
    _phantom.build(this);

}

void APhantomPawn::PostInitializeComponents()
{
    _phantom.PostInitializeComponents();

    Super::PostInitializeComponents();
}

// Called when the game starts or when spawned
void APhantomPawn::BeginPlay()
{
    SimMotor * _motors[4] = {&_motor1, &_motor2, &_motor3, &_motor4};

    _flightManager = new FHackflightFlightManager(this, &_mixer, _motors, &_phantom.dynamics);

    _phantom.BeginPlay(_flightManager);

    Super::BeginPlay();
}

void APhantomPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    _phantom.EndPlay();

    Super::EndPlay(EndPlayReason);
}

// Called automatically on main thread
void APhantomPawn::Tick(float DeltaSeconds)
{
    _phantom.Tick(DeltaSeconds);

    _flightManager->tick();

    Super::Tick(DeltaSeconds); }

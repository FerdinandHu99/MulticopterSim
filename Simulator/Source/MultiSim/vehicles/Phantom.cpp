/*
* Class implementation for DJI Phantom pawn in MultiSim
*
* Copyright (C) 2018 Simon D. Levy
*
* MIT License
 */

#include "Phantom.h"
#include "../Thread.hpp"

APhantom::APhantom()
{
    vehicle.buildFull(this, FrameStatics.mesh.Get());

    addProp(PropCCWStatics.mesh.Get(), +1, +1);
    addProp(PropCCWStatics.mesh.Get(), -1, -1);
    addProp(PropCWStatics.mesh.Get(), +1, -1);
    addProp(PropCWStatics.mesh.Get(), -1, +1);

    // Un-comment for camera
    // vehicle.addCamera(&camera);
}

// Called when the game starts or when spawned
void APhantom::BeginPlay()
{
    vehicle.beginPlay(new FVehicleThread(&dynamics));

    Super::BeginPlay();
}

void APhantom::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    vehicle.endPlay();

    Super::EndPlay(EndPlayReason);
}

void APhantom::PostInitializeComponents()
{
    vehicle.postInitializeComponents();

    Super::PostInitializeComponents();
}

// Called automatically on main thread
void APhantom::Tick(float DeltaSeconds)
{
    vehicle.tick(DeltaSeconds);

    Super::Tick(DeltaSeconds);
}

void APhantom::addProp(UStaticMesh * mesh, int8_t dx, int8_t dy)
{
    vehicle.addRotor(mesh, dx*0.12, dy*0.12, 0.16);
}    



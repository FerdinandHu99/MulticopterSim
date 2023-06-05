/*
* Class implementation for  Crazyflie in MultiSim
*
* Copyright (C) 2018 Simon D. Levy
*
* MIT License
 */

#include "Crazyflie.h"

#include "../threads/RemoteThread.hpp" // XXX 

ACrazyflie::ACrazyflie()
{
    // Build the frame, restoring for cameras, audio
    vehicle.buildFull(this, PcbStatics.mesh.Get());

    addArm(1,
            MotorMount1Statics.mesh.Get(),
            Motor1Statics.mesh.Get(),
            Propeller1Statics.mesh.Get());

    addArm(2,
            MotorMount2Statics.mesh.Get()
          );
}

void ACrazyflie::addArm(
        uint8_t index,
        UStaticMesh * motorMountMesh,
        UStaticMesh * motorMesh,
        UStaticMesh * propellerMesh)
{
    vehicle.addComponent(motorMountMesh, 
            makeName("motor_mount", index, "Mesh"));

    if (motorMesh) {

        vehicle.addComponent(motorMesh, 
                makeName("motor", index, "Mesh"));

        vehicle.addComponent(propellerMesh, 
                makeName("propeller", index, "Mesh"));
    }
}


// Called when the game starts or when spawned
void ACrazyflie::BeginPlay()
{
    vehicle.beginPlay(new FRemoteThread(&dynamics));

    Super::BeginPlay();
}

void ACrazyflie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    vehicle.endPlay();

    Super::EndPlay(EndPlayReason);
}

void ACrazyflie::PostInitializeComponents()
{
    vehicle.postInitializeComponents();

    Super::PostInitializeComponents();
}

// Called automatically on main thread
void ACrazyflie::Tick(float DeltaSeconds)
{
    vehicle.tick(DeltaSeconds);

    Super::Tick(DeltaSeconds);
}

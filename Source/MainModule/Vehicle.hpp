/*
 * General support for vehicles in MulticopterSim
 *
 * This class peforms the following functions:
 *
 * (1) Statically builds meshes, cameras, and other UE4 objects
 *
 * (2) Provides basic support for displaying vehicle kinematics
 *
 * Copyright (C) 2019 Simon D. Levy, Daniel Katzav
 *
 * MIT License
 */

#pragma once

#define WIN32_LEAN_AND_MEAN

#include "Utils.hpp"
#include "dynamics/MultirotorDynamics.hpp"
#include "FlightManager.hpp"
#include "Camera.hpp"
#include "Landscape.h"

#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"

#include <stdio.h>

 // Windows/Linux compatibility 
#ifdef _WIN32
#define SPRINTF sprintf_s
#else
#include <wchar.h>
#define SPRINTF sprintf
#endif

// A macro for simplifying the declaration of static meshes
#define DECLARE_STATIC_MESH(structname, assetstr, objname)   \
    struct structname {                                             \
        ConstructorHelpers::FObjectFinderOptional<UStaticMesh> mesh;   \
        structname() : mesh(TEXT("/Game/Flying/Meshes/" assetstr)) { } \
    };                                                                     \
    static structname objname;

class Vehicle {

    private:

        ACameraActor* _groundCamera = NULL;

        // Useful approximation to infinity for tracing rays
        static constexpr float INF = 1e9;

        // Time during which velocity will be set to zero during final phase oflanding
        static constexpr float SETTLING_TIME = 1.0;

        // UE4 objects that must be built statically
        UStaticMesh* _frameMesh = NULL;
        UStaticMesh* _motorMesh = NULL;
        USoundCue* _soundCue = NULL;
        UAudioComponent* _audioComponent = NULL;
        USpringArmComponent* _gimbalSpringArm = NULL;
        USpringArmComponent * _playerCameraSpringArm = NULL;
        USpringArmComponent* _bodyHorizontalSpringArm = NULL;
        UCameraComponent* _playerCamera = NULL;

        // Support for switching from chase camera to FPV
        float _playerCameraFollowMeters = 0;
        float _playerCameraElevationMeters = 0;

        // PlayerController for getting keyboard events
        APlayerController * _playerController = NULL;

        // Cameras
        Camera* _cameras[Camera::MAX_CAMERAS];
        uint8_t  _cameraCount;

        // Set in constructor
        MultirotorDynamics* _dynamics = NULL;

        // Threaded worker for running flight control
        class FFlightManager* _flightManager = NULL;

        // Have to seledct a map before flying
        bool _mapSelected = false;

        // Motor values for animation/sound
        float  _motorvals[FFlightManager::MAX_MOTORS] = {};

        // Circular buffer for moving average of motor values
        TCircularBuffer<float>* _motorBuffer = NULL;
        uint32_t _bufferIndex = 0;

        // For computing AGL
        float _aglOffset = 0;

        // Countdown for zeroing-out velocity during final phase of landing
        float _settlingCountdown = 0;

        // Starting location, for kinematic offset
        FVector _startLocation = {};

        // Retrieves kinematics from dynamics computed in another thread, returning true if vehicle is airborne, false otherwise.
        void updateKinematics(void)
        {
            // Get vehicle pose from dynamics
            MultirotorDynamics::pose_t pose = _dynamics->getPose();

            // Set vehicle pose in animation
            _pawn->SetActorLocation(_startLocation +
                FVector(pose.location[0], pose.location[1], -pose.location[2]) * 100);  // NED => ENU
            _pawn->SetActorRotation(FMath::RadiansToDegrees(FRotator(pose.rotation[1], pose.rotation[2], pose.rotation[0])));
        }

        void animatePropellers(void)
        {
            // Get motor values from dynamics
            _flightManager->getMotorValues(_motorvals);

            // Compute the sum of the motor values
            float motorsum = 0;
            for (uint8_t j = 0; j < _dynamics->motorCount(); ++j) {
                motorsum += _motorvals[j];
            }

            // Rotate props. For visual effect, we can ignore actual motor values, and just keep increasing the rotation.
            if (motorsum > 0) {
                rotateProps(_motorDirections, _dynamics->motorCount());
            }

            // Add mean to circular buffer for moving average
            _bufferIndex = _motorBuffer->GetNextIndex(_bufferIndex);
            (*_motorBuffer)[_bufferIndex] = motorsum / _dynamics->motorCount();

            // Compute the mean motor value over the buffer frames
            float smoothedMotorMean = 0;
            for (uint8_t i = 0; i < _motorBuffer->Capacity(); ++i) {
                smoothedMotorMean += (*_motorBuffer)[i];
            }
            smoothedMotorMean /= _motorBuffer->Capacity();

            // Use the mean motor value to modulate the pitch and voume of the propeller sound
            _audioComponent->SetFloatParameter(FName("pitch"), smoothedMotorMean);
            _audioComponent->SetFloatParameter(FName("volume"), smoothedMotorMean);
        }

        void grabImages(void)
        {
            for (uint8_t i = 0; i < _cameraCount; ++i) {
                _cameras[i]->grabImage();
            }
        }

        void buildPlayerCameras(float distanceMeters, float elevationMeters)
        {
            _bodyHorizontalSpringArm = _pawn->CreateDefaultSubobject<USpringArmComponent>(TEXT("BodyHorizontalSpringArm"));
            _bodyHorizontalSpringArm->SetupAttachment(_frameMeshComponent);
            _bodyHorizontalSpringArm->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
            _bodyHorizontalSpringArm->TargetArmLength = 0;
            _bodyHorizontalSpringArm->bEnableCameraLag = false;
            //_bodyHorizontalSpringArm->bAbsoluteRotation = false; // XXX if needed, use _bodyHorizontalSpringArm->SetAbsolute()
            _bodyHorizontalSpringArm->bInheritPitch = false;
            _bodyHorizontalSpringArm->bInheritRoll = false;

            _playerCameraFollowMeters = distanceMeters;
            _playerCameraElevationMeters = elevationMeters;

            _playerCameraSpringArm = _pawn->CreateDefaultSubobject<USpringArmComponent>(TEXT("PlayerCameraSpringArm"));
            _playerCameraSpringArm->SetupAttachment(_bodyHorizontalSpringArm);

            _playerCameraSpringArm->bEnableCameraLag = false;
            //_playerCameraSpringArm->bAbsoluteRotation = false; // XXX if needed, use _playerCameraSpringArm->SetAbsolute()
            _playerCameraSpringArm->bInheritYaw = true;
            _playerCameraSpringArm->bInheritPitch = false;
            _playerCameraSpringArm->bInheritRoll = false;
            _playerCameraSpringArm->bEnableCameraRotationLag = true;

            _playerCamera = _pawn->CreateDefaultSubobject<UCameraComponent>(TEXT("PlayerCamera"));
            _playerCamera->SetupAttachment(_playerCameraSpringArm, USpringArmComponent::SocketName);
        }

        void playerCameraSetChaseView()
        {
            _playerController->SetViewTargetWithBlend(_pawn);
            _playerCameraSpringArm->SetRelativeLocationAndRotation(FVector(-_playerCameraFollowMeters, 0, _playerCameraElevationMeters)*100,
                    FRotator::ZeroRotator);;
            _playerCameraSpringArm->TargetArmLength = _playerCameraFollowMeters*100;

            _bodyHorizontalSpringArm->bInheritYaw = false ;
        }

        void playerCameraSetFrontView()
        {
            _playerController->SetViewTargetWithBlend(_pawn);
            _playerCameraSpringArm->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
            _playerCameraSpringArm->TargetArmLength = -30; // empircally determined to be far enough ahead of vehicle

            _bodyHorizontalSpringArm->bInheritYaw = true;
        }

        void playerCameraSetGroundView()
        {
            if (_groundCamera) _playerController->SetViewTargetWithBlend(_groundCamera);
        }

        float propStartAngle(float propX, float propY)
        {
            FVector vehicleCenter = _pawn->GetActorLocation();
            double theta = -atan2((propY - vehicleCenter.Y), (propX - vehicleCenter.X));
            return FMath::RadiansToDegrees(3.14159 / 2 - theta) + 57.5;
        }

        void rotateProps(int8_t* motorDirections, uint8_t motorCount)
        {
            static float rotation;
            for (uint8_t i = 0; i < motorCount; ++i) {
                setPropRotation(i, rotation * motorDirections[i] * 200);
            }
            rotation++;
        }

    protected:

        APawn* _pawn = NULL;

        UStaticMeshComponent* _frameMeshComponent = NULL;

	    UStaticMeshComponent* _propellerMeshComponents[FFlightManager::MAX_MOTORS] = {};

        // Starts at zero and increases each time we add a propeller
        uint8_t _propCount;

        // Also set in constructor, but purely for visual effect
        int8_t _motorDirections[FFlightManager::MAX_MOTORS] = {};

    public:

        void build(APawn* pawn, UStaticMesh* frameMesh)
        {
            _pawn = pawn;
            _frameMesh = frameMesh;

            _frameMeshComponent = _pawn->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
            _frameMeshComponent->SetStaticMesh(_frameMesh);
            _frameMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
            
            _pawn->SetRootComponent(_frameMeshComponent);

            _propCount = 0;
        }

        void buildFull(APawn* pawn, UStaticMesh* frameMesh, float chaseCameraDistanceMeters, float chaseCameraElevationMeters)
        {
            build(pawn, frameMesh);

            // Build the player-view cameras
            buildPlayerCameras(chaseCameraDistanceMeters, chaseCameraElevationMeters);

            // Get sound cue from Contents
            static ConstructorHelpers::FObjectFinder<USoundCue> soundCue(TEXT("/Game/Flying/Audio/MotorSoundCue"));

            // Store a reference to the Cue asset - we'll need it later.
            _soundCue = soundCue.Object;

            // Create an audio component, which wraps the sound cue, and allows us to ineract with it and its parameters from code
            _audioComponent = _pawn->CreateDefaultSubobject<UAudioComponent>(TEXT("PropellerAudioComp"));

            // Set the audio component's volume to zero
            _audioComponent->SetFloatParameter(FName("volume"), 0);

            // Attach the sound to the pawn's root, the sound follows the pawn around
            _audioComponent->SetupAttachment(_pawn->GetRootComponent());
            
            // Create a spring-arm for the gimbal
            _gimbalSpringArm = _pawn->CreateDefaultSubobject<USpringArmComponent>(TEXT("GimbalSpringArm"));
            _gimbalSpringArm->SetupAttachment(_pawn->GetRootComponent());
            _gimbalSpringArm->TargetArmLength = 0.f;
        }

        void addMesh(UStaticMesh* mesh, const char* name, const FVector& location, const FRotator rotation, const FVector& scale)
        {
            UStaticMeshComponent* meshComponent =
                _pawn->CreateDefaultSubobject<UStaticMeshComponent>(FName(name));
            meshComponent->SetStaticMesh(mesh);
            meshComponent->SetupAttachment(_frameMeshComponent, USpringArmComponent::SocketName);
            meshComponent->AddRelativeLocation(location * 100); // m => cm
            meshComponent->AddLocalRotation(rotation);
            meshComponent->SetRelativeScale3D(scale);
        }

        void addMesh(UStaticMesh* mesh, const char* name, const FVector& location, const FRotator rotation)
        {
            addMesh(mesh, name, location, rotation, FVector(1, 1, 1));
        }

        void addMesh(UStaticMesh* mesh, const char* name)
        {
            addMesh(mesh, name, FVector(0, 0, 0), FRotator(0, 0, 0));
        }

        // z is set in editor
        UStaticMeshComponent * addProp(UStaticMesh* propMesh, float x, float y, float angle)
        {
            UStaticMeshComponent* propMeshComponent =
                _pawn->CreateDefaultSubobject<UStaticMeshComponent>(makeName("Prop", _propCount, "Mesh"));
            propMeshComponent->SetStaticMesh(propMesh);
            propMeshComponent->SetupAttachment(_frameMeshComponent, USpringArmComponent::SocketName);
            propMeshComponent->AddRelativeLocation(FVector(x, y, 0) * 100); // m => cm
            propMeshComponent->SetRelativeRotation(FRotator(0, angle, 0));
            _propellerMeshComponents[_propCount] = propMeshComponent;
            _propCount++;
            return propMeshComponent;
        }

        void addProp(UStaticMesh* propMesh, float x, float y)
        {
            addProp(propMesh, x, y, propStartAngle(x,y));
        }

        virtual void setPropRotation(uint8_t index, float angle)
        {
            _propellerMeshComponents[index]->SetRelativeRotation(FRotator(0, angle, 0));
        }

        void addCamera(Camera* camera)
        {
            // Add camera to spring arm
            camera->addToVehicle(_pawn, _gimbalSpringArm, _cameraCount);

            // Increment the camera count for next time
            _cameras[_cameraCount++] = camera;
        }

        Vehicle(void)
        {
            _dynamics = NULL;
            _flightManager = NULL;
        }

        Vehicle(MultirotorDynamics* dynamics)
        {
            _dynamics = dynamics;

            for (uint8_t i = 0; i < dynamics->motorCount(); ++i) {
                _motorDirections[i] = dynamics->motorDirection(i);
            }

            _flightManager = NULL;
        }

        virtual ~Vehicle(void)
        {
        }

        void BeginPlay(FFlightManager* flightManager)
        {
            _flightManager = flightManager;

            // Player controller is useful for getting keyboard events, switching cameas, etc.
            _playerController = UGameplayStatics::GetPlayerController(_pawn->GetWorld(), 0);

            // Change view to player camera on start
            _playerController->SetViewTargetWithBlend(_pawn);

            // Make sure a map has been selected
            _mapSelected = false;
            if (_pawn->GetWorld()->GetMapName().Contains("Untitled")) {
                error("NO MAP SELECTED");
                return;
            }
            _mapSelected = true;

            // Disable built-in physics
            _frameMeshComponent->SetSimulatePhysics(false);

            // Start the audio for the propellers Note that because the
            // Cue Asset is set to loop the sound, once we start playing the sound, it
            // will play continiously...
            _audioComponent->Play();

            // Create circular queue for moving-average of motor values
            _motorBuffer = new TCircularBuffer<float>(20);

            // Get vehicle ground-truth location for kinematic offset
            _startLocation = _pawn->GetActorLocation();

            // AGL offset will be set to a positve value the first time agl() is called
            _aglOffset = 0;

            // Get vehicle ground-truth rotation to initialize flight manager
            FRotator startRotation = _pawn->GetActorRotation();

            // Initialize dynamics with initial rotation
            double rotation[3] = {
                FMath::DegreesToRadians(startRotation.Roll),
                FMath::DegreesToRadians(startRotation.Pitch),
                FMath::DegreesToRadians(startRotation.Yaw) };
            _dynamics->init(rotation);

            // Find the first cine camera in the viewport
            _groundCamera = NULL;
            for (TActorIterator<ACameraActor> cameraItr(_pawn->GetWorld()); cameraItr; ++cameraItr) {

                ACameraActor * cameraActor = *cameraItr;
                if (cameraActor->GetName().StartsWith("CineCamera")) {
                    _groundCamera = cameraActor;
                }
            }

            playerCameraSetChaseView();
        }

        void Tick(float DeltaSeconds)
        {
            // Quit on ESCape key
            if (hitKey(EKeys::Escape)) GIsRequestingExit = true;

            // Run the game if a map has been selected
            if (_mapSelected) {

                // Use 1/2 keys to switch player-camera view
                setPlayerCameraView();

                updateKinematics();

                grabImages();

                animatePropellers();

                _dynamics->setAgl(agl());
            }
        }

        void setPlayerCameraView(void)
        {
            if (_groundCamera) {
                _groundCamera->SetActorRotation(
                        UKismetMathLibrary::FindLookAtRotation(_groundCamera->GetActorLocation(), _pawn->GetActorLocation()));
            }

            if (hitKey(EKeys::One)   || hitKey(EKeys::NumPadOne))   playerCameraSetFrontView();
            if (hitKey(EKeys::Two)   || hitKey(EKeys::NumPadTwo))   playerCameraSetChaseView();
            if (hitKey(EKeys::Three) || hitKey(EKeys::NumPadThree)) playerCameraSetGroundView();
        }

        bool hitKey(const FKey key)
        {
            return _playerController->IsInputKeyDown(key);
        }

        // Returns AGL when vehicle is level above ground, "infinity" otherwise
        float agl(void)
        {
            // Start at the center of the vehicle
            FVector startPoint = _pawn->GetActorLocation();
            startPoint.Z += 100;
            // End at a point an "infinite" distance below the start point
            FVector endPoint = FVector(startPoint.X, startPoint.Y, startPoint.Z - INF);

            //drawHorizontal(startPoint);
            //drawLine(startPoint, endPoint);

            float d = getImpactDistance(startPoint, endPoint);

            // The first time we measure, we need to set the offset
            if (_aglOffset == 0) {
                _aglOffset = d;
            }

            return d - _aglOffset;
        }

        // Returns distance to mesh between points, or -1 if none found.
        // Eventually we may want to be able to specifiy an actor or actors to include or exclude
        // (other than the vehicle itself).
        float getImpactDistance(FVector startPoint, FVector endPoint)
        {
            // Currently, the only collisions we ignore are with the pawn itself
            TArray<AActor*> actorsToIgnore;
            actorsToIgnore.Add(_pawn);
            FCollisionQueryParams traceParams(FName(TEXT("Distance Trace")), true, actorsToIgnore[0]);
            traceParams.AddIgnoredActors(actorsToIgnore);

            FHitResult OutHit;
            if (_pawn->GetWorld()->LineTraceSingleByChannel(OutHit, startPoint, endPoint, ECC_Visibility, traceParams)) {
                if (OutHit.bBlockingHit) {
                    FVector impactPoint = OutHit.ImpactPoint;
                    return (startPoint.Z - impactPoint.Z) / 100;
                }
            }

            return -1;
        }

        void drawHorizontal(FVector point)
        {
            FVector lftPoint = FVector(point.X, point.Y - 100, point.Z);
            FVector rgtPoint = FVector(point.X, point.Y + 100, point.Z);
            drawLine(lftPoint, rgtPoint);
        }

        void drawLine(FVector point1, FVector point2)
        {
            DrawDebugLine(_pawn->GetWorld(), point1, point2, FColor::Green, false, .1, 0, 0.5);
        }

        void PostInitializeComponents()
        {
            // Add "Vehicle" tag for use by level blueprint
            _pawn->Tags.Add(FName("Vehicle"));

            if (_soundCue->IsValidLowLevelFast()) {
                _audioComponent->SetSound(_soundCue);
            }
        }

        void rotateGimbal(FQuat rotation)
        {
            _gimbalSpringArm->SetRelativeRotation(rotation);
        }


        UStaticMeshComponent* getFrameMesh(void)
        {
            return _frameMeshComponent;
        }

}; // class Vehicle

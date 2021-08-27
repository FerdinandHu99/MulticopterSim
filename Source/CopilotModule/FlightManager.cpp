/*
   MulticopterSim FlightManager class implementation using Haskell Copilot

   Copyright(C) 2021 Simon D.Levy

   MIT License
*/

#include "FlightManager.h"

#include "hackflight.h"

// Used by Copilot ---------------------------------

float copilot_time = 0;

float copilot_receiverThrottle = 0;
float copilot_receiverRoll = 0;
float copilot_receiverPitch = 0;
float copilot_receiverYaw = 0;

float copilot_altimeterZ = 0;
float copilot_altimeterDz = 0;

float copilot_gyrometerX = 0;
float copilot_gyrometerY = 0;
float copilot_gyrometerZ = 0;

float copilot_quaternionW = 0;
float copilot_quaternionX = 0;
float copilot_quaternionY = 0;
float copilot_quaternionZ = 0;

float copilot_flowX = 0;
float copilot_flowY = 0;

// Sent by Copilot to copilot_runMotors() -----------
static float _m1;
static float _m2;
static float _m3;
static float _m4;

// Called by Copilot
void copilot_runMotors(float m1, float m2, float m3, float m4)
{
    _m1 = m1;
    _m2 = m2;
    _m3 = m3;
    _m4 = m4;
}

static double phi;
static double theta;
static double psi;
static double dx;
static double dy;

void copilot_debug(float psipsi)
{
    debugline("%+3.3f (%+3.3f)", psipsi, psi);
}


FCopilotFlightManager::FCopilotFlightManager(APawn * pawn, Dynamics * dynamics)
    : FFlightManager(dynamics)
{
    _gameInput = new GameInput(pawn);

    _ready = true;
}

FCopilotFlightManager::~FCopilotFlightManager()
{
}

void FCopilotFlightManager::getReceiverDemands(void)
{
    // Get stick demands
    _gameInput->getJoystick(_joyvals);

    // Share the stick demands
    copilot_receiverThrottle = _joyvals[0];
    copilot_receiverRoll     = _joyvals[1];
    copilot_receiverPitch    = _joyvals[2];
    copilot_receiverYaw      = _joyvals[3];
}


void FCopilotFlightManager::getGyrometer(void)
{
    copilot_gyrometerX = _dynamics->x(Dynamics::STATE_PHI_DOT); 
    copilot_gyrometerY = _dynamics->x(Dynamics::STATE_THETA_DOT); 
    copilot_gyrometerZ = _dynamics->x(Dynamics::STATE_PSI_DOT); 
}

void FCopilotFlightManager::getQuaternion(void)
{
    phi   = _dynamics->x(Dynamics::STATE_PHI); 
    theta = _dynamics->x(Dynamics::STATE_THETA); 
    psi   = _dynamics->x(Dynamics::STATE_PSI); 

    FRotator rot;
    rot.Roll = phi * 180 / M_PI;
    rot.Pitch = theta * 180 / M_PI;
    rot.Yaw = psi * 180 / M_PI;

    FQuat quat = rot.Quaternion();

    //debugline("W: %+3.3f X: %+3.3f Y: %+3.3f Z: %+3.3f", quat.W, quat.X, quat.Y, quat.Z);

    // Pre-computation
    double cph = cos(phi);
    double cth = cos(theta);
    double cps = cos(psi);
    double sph = sin(phi);
    double sth = sin(theta);
    double sps = sin(psi);

    copilot_quaternionW = cph * cth * cps + sph * sth * sps;
    copilot_quaternionX = cph * sth * sps - sph * cth * cps;
    copilot_quaternionY = -cph * sth * cps - sph * cth * sps;
    copilot_quaternionZ = cph * cth * sps - sph * sth * cps;

    /*
    double qw = copilot_quaternionW;
    double qx = copilot_quaternionX;
    double qy = copilot_quaternionY;
    double qz = copilot_quaternionZ;
    */

    double qw = quat.W;
    double qx = quat.X;
    double qy = quat.Y;
    double qz = quat.Z;

    double psipsi = atan2(2*(qx*qy+qw*qz), qw*qw+qx*qx-qy*qy-qz*qz);

    debugline("%+3.3f (%+3.3f)", psi, psipsi);
}

void FCopilotFlightManager::getOpticalFlow(void)
{
    dx = _dynamics->x(Dynamics::STATE_X_DOT);
    dy = _dynamics->x(Dynamics::STATE_Y_DOT);

    psi = _dynamics->x(Dynamics::STATE_PSI);
    double cp = cos(psi);
    double sp = sin(psi);

    // Rotate inertial velocity into body frame, ignoring roll and pitch fow now
    copilot_flowX = dx * cp + dy * sp;
    copilot_flowY = dy * cp - dx * sp;
}

void FCopilotFlightManager::getActuators(const double time, double * values)
{
    // Avoid null-pointer exceptions at startup, freeze after control
    // program halts
    if (!_ready) {
        return;
    }

    // Share the current time with Copilot
    copilot_time = time; 

    // Share stick demands with Copilot
    getReceiverDemands();

    // Share the gyrometer values
    getGyrometer();

    // Share the quaternion values
    getQuaternion();

    // Share the optical flow values
    getOpticalFlow();

    // Share the altimeter value
    copilot_altimeterZ = _dynamics->x(Dynamics::STATE_Z); 

    // Run Copilot, triggering copilot_runMotors
    step();

    // Get updated motor values
    values[0] = _m1;
    values[1] = _m2;
    values[2] = _m3;
    values[3] = _m4;
}

void FCopilotFlightManager::tick(void)
{
    // Get demands from keypad
    _gameInput->getKeypad(_joyvals);
}

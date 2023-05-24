/*
   UDP proxy for testing MulticopterSim socket comms

   Copyright(C) 2019 Simon D.Levy

   MIT License
 */

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include <core/pid.h>
#include <core/pids/angle.h>
#include <core/pids/althold.h>
#include <core/pids/flowhold.h>
#include <core/mixers/fixedpitch/quadxbf.h>

#include "../../Simulator/Source/MultiSim/sockets/UdpClientSocket.hpp"
#include "../../Simulator/Source/MultiSim/sockets/UdpServerSocket.hpp"

// Comms
static const char * HOST = "127.0.0.1"; // localhost
static uint16_t  MOTOR_PORT = 5000;
static uint16_t  TELEM_PORT = 5001;

static float rad2deg(const double rad)
{
    return (float)(180 * rad / M_PI);
}

static VehicleState state_from_telemetry(const double telemetry[])
{
    return VehicleState( 
            (float)telemetry[1],     // x
            (float)telemetry[2],     // dx
            (float)telemetry[3],     // y
            (float)telemetry[4],     // dy
            -(float)telemetry[5],    // z  [NED => ENU]
            -(float)telemetry[6],    // dz [NED => ENU]
            rad2deg(telemetry[7]),   // phi
            rad2deg(telemetry[8]),   // dphi
            -rad2deg(telemetry[9]),  // theta  [note sign reversal]
            -rad2deg(telemetry[10]), // dtheta [note sign reversal]
            rad2deg(telemetry[11]),  // psi
            rad2deg(telemetry[12])   // dpsi
            );
}

static Demands demands_from_telemetry(const double telemetry[])
{
    return Demands(
            (float)(telemetry[13] + 1) / 2, // [-1,+1] => [0,1]
            (float)telemetry[14],
            (float)telemetry[15],
            (float)telemetry[16]
            );
}

static void report(void)
{
    /*
    struct timeval time = {};
    gettimeofday(&time, NULL);

    static uint32_t prev_sec;
    static uint32_t count;

    if (time.tv_sec != prev_sec) {
        if (count != 0) {
            printf("%d Hz", count);
        }
        printf("\n");
        count = 0;
        prev_sec = time.tv_sec;
    }

    count++;*/
}

int main(int argc, char ** argv)
{

    // Create sockets for telemetry in, motors out
    UdpServerSocket telemServer = UdpServerSocket(TELEM_PORT);
    UdpClientSocket motorClient = UdpClientSocket(HOST, MOTOR_PORT);

    // Create Hackflight objects

    static AnglePidController anglePid = 
        AnglePidController(
                10, // K_p
                10, // K_i
                1,  // K_d
                0); // K_f

    static AltHoldPidController altHoldPid;

    static FlowHoldPidController flowHoldPid;

    static Mixer mixer = QuadXbfMixer::make();

    printf("Hit the Play button ... ");
    fflush(stdout);

    // Loop forever, waiting for clients
    while (true) {

        // Get incoming telemetry values
        double telemetry[17] = {};
        telemServer.receiveData(telemetry, sizeof(telemetry));

        // Sim sends negative time value on halt
        double time = telemetry[0];
        if (time < 0) {
            break;
        }

        // Convert simulator time to microseconds
        const auto usec = (uint32_t)(time * 1e6);

        // Build vehicle state 
        auto vstate = state_from_telemetry(telemetry);

        // Use heading angle to rotate dx, dy into vehicle coordinates
        const auto psi = vstate.psi * M_PI / 180;
        const auto dx = cos(psi) * vstate.dx + sin(psi) *  vstate.dy;
        const auto dy = sin(psi) * vstate.dx + cos(psi) *  vstate.dy;
        vstate.dx = dx;
        vstate.dy = dy;

        // Build stick demands
        auto demands = demands_from_telemetry(telemetry);
        
        // Reset PID controllers on zero throttle
        auto pidReset = demands.throttle < .05;

        // Run stick demands through PID controllers to get final demands
        std::vector<PidController *> pids = {&anglePid, &altHoldPid, &flowHoldPid};
        PidController::run(pids, demands, vstate, usec, pidReset);

        // Run final demands through mixer to get motor values
        float mvals[4] = {};
        mixer.getMotors(demands, mvals);

        // Convert motor values to doubles
        double dmvals[4] = { mvals[0], mvals[1], mvals[2], mvals[3] };

        // Send back motor values
        motorClient.sendData(dmvals, sizeof(dmvals));

        report();

    } // while (true)

    return 0;
}

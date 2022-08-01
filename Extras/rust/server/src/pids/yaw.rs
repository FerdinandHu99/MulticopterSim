/*
   Yaw rate PID controller

   Copyright (C) 2022 Simon D. Levy

   MIT License
*/

use datatypes::datatypes::YawPid;
use datatypes::datatypes::Demands;
use datatypes::datatypes::VehicleState;

use utils::utils::fabs;
use utils::utils::constrain_abs;
use utils::utils::deg2rad;

#[repr(C)]
#[derive(Clone)]
pub struct YawPidState {
    error_integral: f32
}
    
pub fn run(
    demands:Demands,
    vstate:&VehicleState,
    pstate: YawPidState) -> (Demands, YawPid) {

    const KP: f32 = 1.0625;
    const KI: f32 = 0.001875;

    const WINDUP_MAX: f32 = 6.0;
    const RATE_MAX_DPS: f32 = 45.0;

    // Compute error as difference between yaw demand and angular velocity
    let error = demands.yaw - vstate.dpsi;

    // Reset integral on quick angular velocity change
    let error_integral =
        if fabs(error) > deg2rad(RATE_MAX_DPS) {0.0} else {pstate.error_integral};

    // Constrain integral to avoid windup
    let bounded_error_integral = constrain_abs(error_integral + error, WINDUP_MAX);

    // Adjust yaw demand based on error
    let new_demands = Demands {
        throttle:demands.throttle,
        roll:demands.roll,
        pitch:demands.pitch,
        yaw: KP * error + KI * bounded_error_integral
    };

    let new_yaw_pid = make(error_integral);

    (new_demands, new_yaw_pid)
}

fn make(error_integral:f32) -> YawPid {
    YawPid {
        state: YawPidState {
            error_integral:error_integral
        }
    }
}

pub fn new() -> YawPid {
    make(0.0)
}

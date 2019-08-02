#!/usr/bin/env python3
'''
Test simple altitude-hold PID controller

Copyright (C) 2019 Simon D. Levy

MIT License
'''

from time import sleep
import numpy as np
from multicopter import Multicopter
from sys import stdout

# Target 
ALTITUDE_TARGET = 10

# PID params
KP = 0.4125
KD = 4.5

def debug(msg):

    stdout.write(msg)
    stdout.flush()

if __name__ == '__main__':

    # initial conditions
    z = 0
    u = 0
    tprev = 0

    debug('Importing Nengo ... ')
    from nengo_pid_controller import NengoPidController
    debug('done\n')

    # Create PID controller
    npid = NengoPidController(KP, KD)

    # Create a multicopter simulation
    copter = Multicopter()

    # Start the simulation
    copter.start()

    # Loop until user hits the stop button
    while True:

        # Get vehicle state from sim
        telem = copter.getState()

        # Extract time from state
        t =  telem[0]

        # Negative time means user hit stop button
        if t < 0: break

        # Extract altitude from state.  Altitude is in NED coordinates, so we negate it to use as input
        # to PID controller.
        z = -telem[9]

        # Compute vertical climb rate as first difference of altitude over time
        if (not np.isnan(z)) and (abs(z) < 100) and (t > tprev):

            debug('z=%+3.3f  u=%3.3f\n' % (z, u))

            # Get correction from PID controller
            u = npid.getCorrection(ALTITUDE_TARGET, z)[0]

            # Constrain correction to [0,1] to represent motor value
            u = max(0, min(1, u))

        # Set motor values in sim
        copter.setMotors(u*np.ones(4))

        # Update for first difference
        tprev = t

        # Yield to Multicopter thread
        sleep(.001)

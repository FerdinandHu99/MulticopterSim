/*
   Java Multicopter test 

   Copyright(C) 2019 Simon D.Levy

   MIT License
 */

public class Takeoff {

    // Target params
    static final double ALTITUDE_TARGET = 10;

    // PID params
    static final double ALT_P = 1.0;
    static final double VEL_P = 1.0;
    static final double VEL_I = 0;

    public static void main(String [] args)
    {
        // initial conditions
        double tprev = 0;
        double u = 0;

        // Create PID controller
        AltitudePidController pid  = new AltitudePidController(ALTITUDE_TARGET, ALT_P, VEL_P, VEL_I);

        // Create a multicopter simulation
        Multicopter copter = new Multicopter();

        // Start the simulation
        copter.start();

        // Loop until user hits the stop button
        while (true) {

            // Get time from sim
            double t = copter.getTime();

            // Negative time means quit
            if (t < 0) break;

            // Get state from sim
            double [] state = copter.getVehicleState();

            // Extract altitude and its first derivative from state.  Altitude
            // is in NED coordinates, so we negate it to use as input to PID
            // controller.
            double z = -state[Multicopter.STATE_Z];
            double dz = -state[Multicopter.STATE_DZ];

            // Compute vertical climb rate as first difference of altitude over time
            if (t > tprev) {

                // Use temporal first difference to compute vertical velocity
                double dt = t - tprev;

                // Get correction from PID controller
                u = pid.u(z, dz, dt);

                // Constrain correction to [0,1] to represent motor value
                u = Math.max(0., Math.min(1., u));
            }

            // Set motor values in sim
            copter.setMotors(ones(u, 4));

            // Update
            tprev = t;

            // Yield to Multicopter thread
            try {
                Thread.sleep(1);
            }
            catch (Exception e) {
            }

        }
    }

    // Helper function like Matlab/Numpy ones()
    static double [] ones(double x, int n)
    {
        double []  v = new double [n];
        for (int i=0; i<n; ++i) {
            v[i] = x;
        }
        return v;
    }
}
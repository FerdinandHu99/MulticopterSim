{--
  Types for Multicopter

  Copyright(C) 2021 Simon D.Levy

  MIT License
--}

module Types where

data Time = Time { time :: Double} deriving (Show)

-- See Bouabdallah et al. (2004)
data State = State { x :: Double
                   , dx :: Double 
                   , y :: Double
                   , dy :: Double 
                   , z :: Double
                   , dz :: Double 
                   , phi :: Double
                   , dphi :: Double 
                   , theta :: Double
                   , dtheta :: Double 
                   , psi :: Double
                   , dpsi :: Double 
                   } deriving (Show)

data Demands = Demands { throttle :: Double
                       , roll :: Double  
                       , pitch :: Double  
                       , yaw :: Double  
                     } deriving (Show)

-- XXX should support different numbers of motors
data Motors = Motors { m1 :: Double
                     , m2 :: Double  
                     , m3 :: Double  
                     , m4 :: Double  
                     } deriving (Show)

type Mixer = Demands -> Motors

-- quadXAMixer :: Mixer
-- quadXAPMixer demands motors = 
     

-- type ClosedLoopControl = Time -> State -> Demands
type ClosedLoopControl = Time -> State -> Motors

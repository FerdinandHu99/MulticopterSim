'''
  MultiCopter client class

  Copyright(C) 2023 Simon D.Levy

  MIT License
'''

import socket
import numpy as np


class CrazyflieClient(object):

    # See Bouabdallah (2004)
    (STATE_X,
     STATE_DX,
     STATE_Y,
     STATE_DY,
     STATE_Z,
     STATE_DZ,
     STATE_PHI,
     STATE_DPHI,
     STATE_THETA,
     STATE_DTHETA,
     STATE_PSI,
     STATE_DPSI) = range(12)

    def __init__(self, host='127.0.0.1', port=5000):

        self.host = host
        self.port = port

    def start(self):

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:

            try:

                sock.connect((self.host, self.port))

                sock.settimeout(0.5)

                while True:

                    try:
                        telemetry_bytes = sock.recv(8*13)

                    except socket.timeout:
                        break

                    telemetry = np.frombuffer(telemetry_bytes)

            except ConnectionRefusedError:

                print('Connection error; did you start the server first?')

            except KeyboardInterrupt:

                exit(0)
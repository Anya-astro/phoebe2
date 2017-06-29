"""
can also be run with mpirun -np 8 python test_mpi.py
"""

# for fair timing comparisons, let's disable checking for online passbands
import os
os.environ['PHOEBE_ENABLE_ONLINE_PASSBANDS'] = 'FALSE'

import phoebe
import numpy as np

phoebe.mpi_on(np=8)

def test_mpi(plot=False):
    b = phoebe.Bundle.default_binary()

    b.add_dataset('lc', times=np.linspace(0,1,1001))

    print "calling compute"
    b.run_compute(irrad_method='none', model='phoebe2model')
    print "model received"

    if plot:
        b.plot(show=True)

    return b

if __name__ == '__main__':
    logger = phoebe.logger(clevel='WARNING')

    b = test_mpi(plot=False)

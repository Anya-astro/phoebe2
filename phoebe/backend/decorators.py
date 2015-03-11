"""
Decorators specifically for backend purposes.
"""
import functools
import inspect
import pickle
import tempfile
import subprocess
import time
import os
import sys
import datetime
from collections import OrderedDict
from phoebe.parameters import parameters
from phoebe.parameters import datasets
from phoebe.utils import utils

def parse_ref(fctn):
    """
    Expand the general ref keyword in a function to a list of valid references.
    
    If 'ref' is given, make sure it returns a list of refs.
    
    C{ref} can be any of:
        - '__bol': only bolometric
        - 'all': bolometric and all observables
        - 'alldep': all observables
        - 'alllcdep': all observables of type lc
        - 'myref': the observable with ref 'myref'
        - ['ref1','ref2']: observables matching these refs
        
    """
    @functools.wraps(fctn)
    def parse(self, *args, **kwargs):
        """
        Expand the ref keyword to valid ref strings.
        
        Possibilities:
            - None: only bolometric
            - '__bol': only bolometric
            - 'all': bolometric and all observables
            - 'alldep': all observables
            - 'alllcdep': all observables of type lc
            - 'myref': the observable with ref 'myref'
            - ['ref1','ref2']: observables matching these refs
        """
        # Take the default from the function that is decorated using the
        # "inspect" standard library module
        fctn_args = inspect.getargspec(fctn)
        if 'ref' in fctn_args.args:
            n_args = len(fctn_args.args)
            index = fctn_args.args.index('ref') - n_args
            default = fctn_args.defaults[index]
            
        # If no default was given there, the "ueber"-default is 'all'.
        else:
            default = 'all'    
        
        # Now retrieve the ref or return the default. The trailing-underscore
        # variable will be used to collect all the strings of valid references.
        ref = kwargs.pop('ref', default)
        ref_ = []
        
        # If the ref is allready a list, check if the ref is in this Body, if
        # it's not, we remove it!
        if isinstance(ref, (list, tuple)):
            
            # These are the refs that are given:
            refs_given = set(list(ref))
            
            # These are the refs that are available:
            refs_avail = []
            for parset in self.walk():
                if not parset.context[-3:] == 'dep':
                    continue
                refs_avail.append(parset['ref'])
             
            # (bolometric one is also available)
            refs_avail = set(refs_avail + ['__bol'])
            
            # So these are the refs that are given and available
            kwargs['ref'] = list(refs_given & refs_avail)
        
        # If the ref is None, it means we need to take the bolometric one
        elif ref is None or ref == '__bol':
            kwargs['ref'] = ['__bol']
        
        # Else there are a number of possibilities
        else:
            
            # If ref is all, we also take the bolometric one
            if ref == 'all':
                ref_.append('__bol')
                
            # Let's walk over all deps and retrieve necessary references.
            for parset in self.walk():
                if not parset.context[-3:] == 'dep':
                    continue
                
                # If ref is all or alldep, take them all! Also add it if the
                # ref matches
                if ref == 'all' or ref == 'alldep' or ref == parset['ref']:
                    ref_.append(parset['ref'])
                
                # If ref is alllcdep, take only the lc ones
                elif ref[:3] == 'all' and parset.context[:2] == ref[3:5]:
                    ref_.append(parset['ref'])
            
            # Make sure not to take duplicates
            kwargs['ref'] = list(set(ref_))
            
        return fctn(self, *args, **kwargs)
    
    return parse




def merge_synthetic(list_of_bodies):
    """
    Merge synthetic data of a list of bodies.
    
    Evidently, the bodies need to represent the same system.
    
    Could be useful for MPI stuff.
    
    @param list_of_bodies: a list of bodies for which the results need to be
     merged
    @type list_of_bodies: list of Bodies
    @return: the first body but with the results of all the others added
    @rtype: Body
    """
    iterators = [body.walk_type(type='syn') for body in list_of_bodies]

    for iteration in zip(*iterators):
        for parset in iteration[1:]:
            if parset.context[-3:] == 'syn':
                for key in parset:
                    if 'columns' in parset and not key in parset['columns']:
                        continue
                    value = parset[key]
                    
                    if isinstance(value, list):
                        iteration[0][key] += value
    
    return list_of_bodies[0]


def prepare_ltt(system):
    # First get a flat list of the bodies
    if hasattr(system, 'get_bodies'):
        bodies = system.get_bodies()
    else:
        bodies = [system]
    
    # Then cycle through all the bodies, and retrieve their full
    # hierarchical orbit
    for body in bodies:
        
        # Store the results in an "orbsyn" parameterSet
        orbsyn = datasets.DataSet('orbsyn', ref='ltt')
        
        # We need to keep the same hierarchy as with lcsyns and such
        body.params['syn']['orbsyn'] = OrderedDict()
        body.params['syn']['orbsyn'][orbsyn['ref']] = orbsyn




def construct_mpirun_command(script='mpirun.py', mpirun_par=None, args='', script_dir='backend'):
    """
    Construct mpirun command
    """
    if mpirun_par is None:
        mpirun_par = parameters.ParameterSet(context='mpi')    
    
    
    # Byslot
    if 'byslot' in mpirun_par and mpirun_par['byslot']:
        byslot = '--byslot'
    else:
        byslot = ''
    
    # Python executable
    python = mpirun_par['python']
    
    # Hostfile
    if 'hostfile' in mpirun_par and mpirun_par['hostfile']:
        hostfile = '--hostfile {}'.format(mpirun_par['hostfile'])
    else:
        hostfile = ''
    
    # Retrieve the absolute path of the mpirun program
    mpirun_loc = os.path.abspath(__file__)
    mpirun_loc = os.path.split(os.path.split(mpirun_loc)[0])[0]
    mpirun_loc = os.path.join(mpirun_loc, script_dir, script)
    
    # Build and run the command
    #{fctn.__name__} {sys_file.name} "
    #       "{args_file.name} {kwargs_file.name}
    if mpirun_par.get_context().split(':')[-1] == 'mpi':
        # Number of nodes
        num_proc = mpirun_par['np']
        
        cmd = ("mpiexec -np {num_proc} {hostfile} {byslot} {python} "
           "{mpirun_loc} {args}").format(**locals())
        
        flag = subprocess.call(cmd, shell=True)
        
        
    elif mpirun_par.get_context().split(':')[-1] == 'slurm':
        # Number of nodes
        num_proc = mpirun_par['np']
        
        if not mpirun_par['memory']:
            memory = ''
        else:
            memory = '--mem={:.0f}'.format(mpirun_par['memory'])
        
        if not mpirun_par['time']:
            time_ = ''
        else:
            time_ = '--time={:.0f}'.format(mpirun_par['time'])
            
        if not mpirun_par['partition']:
            partition = ''
        else:
            partition = '--partition={:.0f}'.format(mpirun_par['partition'])    
            
        cmd = ("srun {time_} {memory} {partition} "
               "mpirun -np {num_proc} {hostfile} {byslot} {python} "
           "{mpirun_loc} {args}").format(**locals())
        print(cmd)
        flag = subprocess.call(cmd, shell=True)
        
    # MPI using the TORQUE scheduler    
    elif mpirun_par.get_context().split(':')[-1] == 'torque':
        
        # Get extra arguments for Torque
        if mpirun_par['memory']:
            memory = ',mppmem={:.0f}M'.format(mpirun_par['memory'])
        else:
            memory = ''
            
        dt = datetime.timedelta(minutes=mpirun_par['time'])
        time_ = "%02d:%02d:%02d" % (dt.total_seconds()/3600, dt.total_seconds()%3600/60, dt.total_seconds()%60)
        nodes = mpirun_par['nodes']
        email = '{}'.format(mpirun_par['email'])
        
        # We need the absolute path name to mpirun for Torque:
        mpirun = utils.which('mpirun')
        
        # Create the commands to run Torque
        cmd = ('echo "mpirun {python} {mpirun_loc} {args}" '
           '| qsub -V -l nodes={nodes},walltime={time_}').format(**locals())
        
        if email:
            cmd += ' -M {}'.format(email)
            if mpirun_par['alerts']:
                cmd += ' -m {}'.format(mpirun_par['alerts'])
        cmd += ' -N {}'.format(mpirun_par['jobname'])
        
        #cmd = ('qsub -I -l nodes={num_proc},mem={memory},pcput={time}'
               #'bash -c "mpirun {python} '
               #'{mpirun_loc} {fctn.__name__} {sys_file.name} '
               #'{args_file.name} {kwargs_file.name}"').format(**locals())
        
        flag = 1
        try:
            print("Running torque with command '{}'".format(cmd))
            time.sleep(2) # give everything some time to fall into place
            jobid = subprocess.check_output(cmd, shell=True).strip()
            print("JobID: {}".format(jobid))
            print("Waiting for job to finish (interrupting with CTRL+C will stop this script but not the job, use 'qdel {}' to stop the computations".format(jobid))
            
            time.sleep(5)
            # Output expected start date:
            try:
                print(subprocess.check_output('showstart '+jobid, shell=True))
            except Exception as msg:
                print("Job started")
            
            # Monitor the job until it's done:
            while True:
                try:
                    output = subprocess.check_output('qstat -a '+jobid, shell=True)
                    status = output.split('\n')[-2].strip().split()[-2]
                    sys.stdout.write("\r" + output.split('\n')[-2].rstrip())
                    sys.stdout.flush()
                    if status == 'C':
                        break
                    time.sleep(5)
                except Exception as msg:
                    break
            # Only if we get to here, everything worked out
            flag = 0
            print("\nJob finished correctly")
        except Exception as msg:
            print("\nSomething failed during torque submission: {}".format(str(msg)))
        # New line in terminal
        print('\n')
        
        
    return flag, mpirun_par.get_context().split(':')[-1]



def mpirun(fctn):
    """
    Use MPIRUN to execute a function on different nodes.
    
    Restrictions on the calling signature of the function C{fctn}::
    
        fctn(system,*args,**kwargs)
        
    Restrictions on the type of args and kwargs::
        
        They need to be pickleable.
    
    Restrictions on kwargs::
        
        The C{params} ParameterSet needs to be given.
    
    The decorator adds the new keyword argument C{mpi}, which is either
    C{None} (no mpi) or a ParameterSet with the details on how to run MPI.
    
    This decorator pickles the C{system}, the C{args} and C{kwargs} in
    separate C{NamedTemporaryFiles}. Next, in each different thread the
    module C{MPI/mpirun.py} is called from a terminal. The command-line
    interface requires 4 positional arguments::
    
        python mpirun.py func system.pck args.pck kwargs.pck
    
    where C{func} is the name of the function to be called, that needs to be
    defined inside C{observatory} (a C{getattr(observatory,func)} is done).
    
    After each run, the synthetic data are collected and finally merged.
    """
    @functools.wraps(fctn)
    def do_run(system, *args, **kwargs):
        """
        Actual function to do the work for MPI.
        """        
            
        # Perhaps there isn't even an mpi parameter
        mpirun_par = kwargs.pop('mpi', None)

        # In that case, just call the function
        if mpirun_par is None:
            if hasattr(system,'fix_mesh'):
                system.fix_mesh()
            else:
                system.init_mesh()
            #kwargs['inside_mpi'] = None
            output = fctn(system, *args, **kwargs)
            return output
        
        # Else, get the compute-parameters
        else:
            
            # But again, perhaps there are none...
            params = kwargs.pop('params', None)
            if params is None:
                
                # In this case, *all* the keyword arguments must be a member
                # of the compute context, or stuff will go wrong..
                try:
                    params = parameters.ParameterSet(context='compute',
                                                     **kwargs)
                except:
                    print("You cannot give extra_func with MPI")
                    raise
            
            # Add the comptue params to the kwargs
            kwargs['params'] = params
            
            # If there is no mpirun ParameterSet, but it is set to True, create
            # the default one
            if mpirun_par is True:
                mpirun_par = parameters.ParameterSet(context='mpi')
            
            # For some reason mpi fails when only one node, but this is fairly
            # useless anyway
            if mpirun_par['np'] <= 1:
                raise ValueError("For MPI to be useful, you need at least 2 processes (np>=2)")
            
            # Now we're ready to do the actual work
            try:
                if hasattr(system,'fix_mesh'):
                    system.fix_mesh()
                else:
                    system.init_mesh()
                
                # Prepare computations of pblum and l3
                #system.compute_pblum_or_l3()
                
                # If we don't add empty orbsyns, we can't merge them in the end
                if kwargs.get('ltt', False):
                    prepare_ltt(system)
                
                # Pickle args and kwargs in NamedTemporaryFiles, which we will
                # delete afterwards
                if not 'directory' in mpirun_par or not mpirun_par['directory']:
                    direc = os.getcwd()
                else:
                    direc = mpirun_par['directory']
                
                # The system
                sys_file = tempfile.NamedTemporaryFile(delete=False, dir=direc)
                pickle.dump(system, sys_file)
                sys_file.close()
                
                # The arguments
                args_file = tempfile.NamedTemporaryFile(delete=False, dir=direc)
                pickle.dump(args, args_file)
                args_file.close()
                
                # The keyword arguments
                kwargs_file = tempfile.NamedTemporaryFile(delete=False,
                                                          dir=direc)
                pickle.dump(kwargs, kwargs_file)
                kwargs_file.close()
                
                args = '{fctn.__name__} {sys_file.name} {args_file.name} {kwargs_file.name}'.format(**locals())
                flag, mpitype = construct_mpirun_command(script='mpirun.py',
                                               mpirun_par=mpirun_par, args=args)
                
                # If something went wrong, we can exit nicely here, the traceback
                # should be printed at the end of the MPI process
                if flag:
                    sys.exit(1)
                
                # Load the results from the function from the pickle file
                with open(sys_file.name, 'r') as open_file:
                    results = pickle.load(open_file)
                
                # Merge the original system with the results from the function
                merge_synthetic([system, results])
            except:
                raise
            # Finally, clean up the pickle files that are lying around
            #finally:
            if os.path.isfile(sys_file.name):
                os.unlink(sys_file.name)
            if os.path.isfile(args_file.name):
                os.unlink(args_file.name)
            if os.path.isfile(kwargs_file.name):
                os.unlink(kwargs_file.name)
            
            # And compute pblum or l3 and postprocess
            system.bin_oversampling()
            system.set_time(0)
            system.compute_pblum_or_l3()
            system.set_pblum_or_l3()
            system.compute_scale_or_offset()
            system.postprocess(time=None)
            
    return do_run




def set_default_units(fctn):
    """
    Set default units for plotting functions
    """
    @functools.wraps(fctn)
    def parse(system, *args, **kwargs):
        """
        Set default units for plotting functions
        """
        # No need to do anything if the user explicitly set both units
        if 'x_unit' in kwargs and 'y_unit' in kwargs:
            return fctn(system, *args, **kwargs)
        
        # Otherwise we do need some work
        category = fctn.__name__.split('_')[1][:-3]
        ref = kwargs.get('ref', 0)
        obs = system.get_obs(category=category, ref=ref)
        dep = system.get_parset(category=category, ref=ref)[0]
        
        # No need to do anything if there are not user_columns/user_units
        if obs is None or not 'user_columns' in obs or not 'user_units' in obs:
            return fctn(system, *args, **kwargs)
        
        userc = obs['user_columns']
        useru = obs['user_units']
        
        # No need to do anything if there's nothing special
        if userc is None or useru is None:
            return fctn(system, *args, **kwargs)
        
        # Otherwise we do need some work:
        x_unit = kwargs.get('x_unit', None)
        y_unit = kwargs.get('x_unit', None)
        x_quantity = kwargs.get('x_quantity', None)
        y_quantity = kwargs.get('y_quantity', None)
        phased = kwargs.get('phased', None)
        
        
        if x_unit is None:
            # Time/phase stuff
            if category in ['lc', 'rv']:
                obs_in_time = 'time' in userc
                index = userc.index('time' if obs_in_time else 'phase')
            
                if obs_in_time and not phased: # otherwise stick to defaults again
                    #kwargs['x_unit'] = useru[index]
                    if 'time' in useru or 'phase' in useru:
                        kwargs['x_unit'] = useru['time' if obs_in_time else 'phase']
        
        if y_unit is None:
            # Flux/mag stuff
            if category == 'lc':
                obs_in_flux = 'flux' in userc
                obs_in_mag = 'mag' in userc
                
                if obs_in_flux or obs_in_mag:
                    # If no user units are give, use default ones
                    if useru:
                        # In case useru is a dictionary
                        #if hasattr(useru, 'keys'):
                        kwargs['y_unit'] = useru['flux']# if obs_in_flux else 'mag']
                        # else it is a list    
                        #else:
                        #    index = userc.index('flux' if obs_in_flux else 'mag')
                        #    kwargs['y_unit'] = useru[index]
                    
                
                        
            
            elif category == 'rv' and 'rv' in userc and useru:
                kwargs['y_unit'] = useru['rv']
                #index = userc.index('rv')
                #kwargs['y_unit'] = useru[index]
        
        return fctn(system, *args, **kwargs)
    
    return parse



        

# -*- coding: utf-8 -*-
"""
Created on Wed Dec 04 15:49:13 2013

@author: Robert A. McLeod

This is the interface to an arbitrary TEM.  At the moment I have interfaces for 
a TEM simulator and a FEI self.titan/Technai class instrument only, but in principle 
it can be extended to JEOL and Hitachi instruments through their COM interfaces.

Basically each TEM class in this file should have identical function lists, so 
they act as a wrapper for the COM interface, so that I always use the exact same 
interface for any software I write, regardless of make or model of instrument.

"""

# How to import only the libraries used for each sub-class?
from skimage.io import imread
import numpy as np
# import makeimg
import time
# import DM3lib
import h5py
import pyDM

# We need to have a tempfile, preferrably on the RAMdisk, for transferring images from FEI to Python
"""
FEI self.titan AND TECHNAI WITH PEOUI/TIA INTERFACE
"""
class TEM_FeiTitan:

    mask_X = 1
    mask_Y = 2
    mask_XY = 3
    mask_Z = 4
    mask_XYZ = 7
    mask_alpha = 8
    mask_beta = 16
    mask_all = 31
    
    mode_image = "image"
    mode_diff = "diffraction" 
    
    mode_sleep_time = 10
    
    def __init__(self):
        # Do nothing
        print( 'Initializing FEI self.titan interface' )
        
        # Link to the parent control
        self.tem_parent = None
        self.ccd = None
        self.titan = None
        # Make a hash to represent the deflectors? 
        """
        deflect = { 'GS': [0,0], 
                'GT': [0,0],
                'BS': [0,0],
                'BT': [0,0],
                'RC': [0,0],
                'IBS': [0,0],
                'IBT': [0,0],
                'IS': [0,0],
                'DS': [0,0],
                 }
        # Make a hash to represent the stage
        stage = { 'X': 0, 'Y': 0, 'Z': 0, 'alpha': 0, 'beta': 0}
        """
        self.lim_gs = np.array( [[-100.0,100.0], [-100.0,100.0]] )*1E-2 # %
        self.lim_gt = np.array( [[-100.0,100.0], [-100.0,100.0]] )*1E-2 # %
        self.lim_bs = np.array( [[-10000.0,10000.0], [-10000.0,10000.0]] )*1E-6 # um
        self.lim_bt = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )*1E-3 # mrad
        self.lim_rc = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )*1E-3 # mrad
        self.lim_ibs = np.array( [[-10000.0,10000.0], [-10000.0,10000.0]] )*1E-6 # um
        self.lim_ibt = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )*1E-3 # mrad
        self.lim_is = np.array( [[-10000.0,100.0], [-10000.0,10000.0]] )*1E-6 # um
        self.lim_ds = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )*1E-3 # mrad
        
        self.lim_stage = np.zeros( [5,2] ) # um and degrees
        
        self.tempFileName = 'C:\\testmage.tif' # So that we don't have to transfer as a safe array
        self.image_cnt = 0


    def connectToTEM(self, tem_parent_in = None ):
        # These are the functions we should call when the user clicks Microscope -> FEI self.titan
        # Consider whether it's bad or not to redo these every time a user clicks the button?
        # OLD CODE
        from win32com.client import gencache
        self.titan = gencache.EnsureDispatch('TEMScripting.Instrument') # Equivalent to connecting to the TEM
        # I never call TIA, so...
        # self.self.titanTIA = gencache.EnsureDispatch("ESVision.Application")

        ### NEW CODE FOR pyDM
        pyDM.connect();
        print( "TO DO: handle return from connecting to pyDMPipeline" )
        
        if( bool(tem_parent_in) ):
            self.tem_parent = tem_parent_in
        
        if self.titan is None:
            return False
        # elif self.self.titantia is None:
        #    return False
        else:
            # Find the tilt limits and update the GUI
            stageaxis = self.titan.Stage.AxisData( self.mask_X )
            self.lim_stage[0][0] = stageaxis.MaxPos
            self.lim_stage[0][1] = stageaxis.MinPos 
            
            stageaxis = self.titan.Stage.AxisData( self.mask_Y )
            self.lim_stage[1][0] = stageaxis.MaxPos
            self.lim_stage[1][1] = stageaxis.MinPos
            
            stageaxis = self.titan.Stage.AxisData( self.mask_Z )
            self.lim_stage[2][0] = stageaxis.MaxPos 
            self.lim_stage[2][1] = stageaxis.MinPos 
        
            # Format [alpha_max alpha_min beta_max beta_min]
            stageaxis = self.titan.Stage.AxisData( self.mask_alpha )
            self.lim_stage[3][0] = stageaxis.MaxPos 
            self.lim_stage[3][1] = stageaxis.MinPos 
            
            stageaxis = self.titan.Stage.AxisData( self.mask_beta )
            self.lim_stage[4][0] = stageaxis.MaxPos 
            self.lim_stage[4][1] = stageaxis.MinPos       

            print "TO DO: implement deflector limits for FEI self.titan"
            
            return True # ok successfully created the appropriate objects, so we _should_ be connected
            
    def getTEMstatus(self):
        # Gets the entire state of the TEM and passes back the appropriate structure for this TEM
        print( 'TO DO: pull TEM state' )
        
    def setTempLocation( self, new_fileloc ):
        self.tempFileName = new_fileloc       
		
    def acquireImage(self, tx=1, binning=[1,1], framesize=[0,0,2048,2048], procmode=3 ):
        # Using the pyDM interface which is much faster than the FEI COM32-based interface
        # proc = 1 is unprocessed, proc = 2 is dark reference subtracted, proc = 3 is gain normalized image
        # proc = 1 returns uint16, proc = 2 or 3 returns int32, so unprocessed is faster
        if np.size( binning ) == 1:
            binning = [binning, binning]
            
        # Clear queue (TO DO: I need a message handler)
        for J in np.arange(0, pyDM.cntMsg()):
            pyDM.getMessage()
        
        # acqparamstr = "set_acqparam_"+procmode+"_" + str(tx) + "_"+binx+"_"+biny+"_0_0_2048_2048"
        acqstr = "acquire_test_"+str(procmode)+"_" + str(tx)+"_"+str(binning[0])+"_"+str(binning[1])+"_"+str(framesize[0])+"_"+str(framesize[1])+"_"+str(framesize[2])+"_"+str(framesize[3])
        print( "Try: " + acqstr )
        pyDM.sendMessage(acqstr)
        while(True):
            self.app.processEvents() # Unfreeze the GUI
            # TO DO: add a timer in the C++
            if( pyDM.cntMsg() > 0):
                out = pyDM.getMessage()
                if out.split('_')[1] == "done" :
                    mage = pyDM.getImage( (framesize[2]-framesize[0])/binning[0], (framesize[3]-framesize[1])/binning[1], 2)
                    break
                else:
                    print( "Unrecognized message : " + out )
                    break
            time.sleep(0.000)
        return mage
        
    def acquireImageRaveled(self, tx=1, binning=[1,1], framesize=[0,0,2048,2048], procmode=3 ):
        # Using the pyDM interface which is much faster than the FEI COM32-based interface
        # proc = 1 is unprocessed, proc = 2 is dark reference subtracted, proc = 3 is gain normalized image
        
        if np.size( binning ) == 1:
            binning = [binning, binning]
            
        # Clear queue (this is quite lame, I need a message handler)
        for J in np.arange(0, pyDM.cntMsg()):
            pyDM.getMessage()
        
        # acqparamstr = "set_acqparam_"+procmode+"_" + str(tx) + "_"+binx+"_"+biny+"_0_0_2048_2048"
        acqstr = "acquire_test_"+str(procmode)+"_" + str(tx)+"_"+str(binning[0])+"_"+str(binning[1])+"_"+str(framesize[0])+"_"+str(framesize[1])+"_"+str(framesize[2])+"_"+str(framesize[3])
        print( "Try: " + acqstr )
        pyDM.sendMessage(acqstr)
        while(True):
            # TO DO: add a timer in the C++
            if( pyDM.cntMsg() > 0):
                out = pyDM.getMessage()
                if out.split('_')[1] == "done" :
                    mage = pyDM.getImageRaveled( (framesize[2]-framesize[0])/binning[0]* (framesize[3]-framesize[1])/binning[1], 2)
                    break
                else:
                    print( "Unrecognized message : " + out )
                    break
            time.sleep(0.001)
        return mage
            
    def acquireImage_FEICOM( self, tx=1, binning=1, framesize=0, proc=1 ):
        # Remember that Python uses whatever CCD is selected within the TEM control software
        # REFERENCE MODE
        # AcqImageCorrection_Default    =1          # from enum AcqImageCorrection
        # AcqImageCorrection_Unprocessed=0          # from enum AcqImageCorrection
        # FRAMESIZE
        # AcqImageSize_Full             =0          # from enum AcqImageSize
        # AcqImageSize_Half             =1          # from enum AcqImageSize
        # AcqImageSize_Quarter          =2          # from enum AcqImageSize

        # NOTE: I CURRENTLY HAVE NO WAY TO RESET THE CAMERA
        # This shouldn't be hard to program, to have access to all of them
        if self.ccd is None:
            self.ccd = self.titan.Acquisition.Cameras(0)
            self.titan.Acquisition.AddAcqDevice(self.ccd)
            self.ccdacqparams = self.titan.Acquisition.Cameras(0).AcqParams
        
        self.ccdacqparams.Binning = binning
        self.ccdacqparams.ExposureTime = tx
        self.ccdacqparams.ImageSize = framesize
        self.ccdacqparams.ImageCorrection = proc
        
        # DON'T FORGET TO APPLY SETTINGS (reasonably a fast series could skip this step)
        self.titan.Acquisition.Cameras(0).AcqParams = self.ccdacqparams

        # Take the picture
        # Saving to RAMdisk and then re-reading a 16-bit TIFF is much faster, about 0.3 s
        # 0 is tag for a 16-bit TIFF
        self.titan.Acquisition.AcquireImages().Item(index=0).AsFile( self.tempFileName, 0 )
        return imread( self.tempFileName, plugin='freeimage' )
        
    def getDeflector( self, defname ):
        # Parse based on string defname
    
        defname = defname.lower()
        if( defname == 'gs' ):
            return np.array( [self.titan.Gun.Shift.X, self.titan.Gun.Shift.Y] ) 
        elif( defname == 'gt' ):
            return np.array( [self.titan.Gun.Tilt.X, self.titan.Gun.Tilt.Y] )
        elif( defname == 'bs' ):
            return np.array( [self.titan.Illumination.Shift.X, self.titan.Illumination.Shift.Y] ) 
        elif( defname == 'bt' ):
            return np.array( [self.titan.Illumination.Tilt.X, self.titan.Illumination.Tilt.Y] ) 
        elif( defname == 'rc' ):
            return np.array( [self.titan.Illumination.RotationCenter.X, self.titan.Illumination.RotationCenter.Y] ) 
        elif( defname == 'ibs' ):
            return np.array( [self.titan.Projection.ImageBeamShift.X, self.titan.Projection.ImageBeamShift.Y] ) 
        elif( defname == 'ibt' ):
            return np.array( [self.titan.Projection.ImageBeamTilt.X, self.titan.Projection.ImageBeamTilt.Y] ) 
        elif( defname == 'ims' ):
            return np.array( [self.titan.Projection.ImageShift.X, self.titan.Projection.ImageShift.Y] )
        elif( defname == 'ds' ):
            return np.array( [self.titan.Projection.DiffractionShift.X, self.titan.Projection.DiffractionShift.Y] ) 
        else:
            print "TEM.getDeflector: unknown deflector passed in."
            
    def setDeflector( self, defname, defvalue ):
        # Parse based on string defname
        # NaNs indicate something not to be used...
        defname = defname.lower()

        # See if we were passed 'bs_x' or 'bs' 
        splitcount = defname.count('_')
        if( splitcount == 1):
            [defname, defcoord] = defname.split('_')
            # print "Updating: " + defname + ", --" + defcoord + "--, value: " + str(defvalue)
        else:
            # print "Updating: " + defname + ", value: " + str(defvalue)
            pass
            
        if( defname == 'gs' ):
            defvector = self.titan.Gun.Shift
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Gun.Shift = defvector
        elif( defname == 'gt' ):
            defvector = self.titan.Gun.Tilt
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1] 
            self.titan.Gun.Tilt = defvector
        elif( defname == 'bs' ):
            defvector = self.titan.Illumination.Shift
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Illumination.Shift = defvector
        elif( defname == 'bt' ):
            defvector = self.titan.Illumination.Tilt
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Illumination.Tilt = defvector
        elif( defname == 'rc' ):
            defvector = self.titan.Illumination.RotationCenter
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Illumination.RotationCenter = defvector
        elif( defname == 'ibs' ):
            defvector = self.titan.Projection.ImageBeamShift
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Projection.ImageBeamShift = defvector
        elif( defname == 'ibt' ):
            defvector = self.titan.Projection.ImageBeamTilt
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Projection.ImageBeamTilt = defvector
        elif( defname == 'ims' ):
            defvector = self.titan.Projection.ImageShift
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Projection.ImageShift = defvector
        elif( defname == 'ds' ):
            defvector = self.titan.Projection.DiffractionShift
            if( splitcount == 1):
                if defcoord == 'x':
                    defvector.X = defvalue
                elif defcoord == 'y':
                    defvector.Y = defvalue
            else:
                defvector.X = defvalue[0]
                defvector.Y = defvalue[1]
            self.titan.Projection.DiffractionShift = defvector
        else:
            print "TEM.setDeflector: unknown deflector passed in."
            
    def getLens( self, lensname ):
        lensname = lensname.lower()
        
        if( lensname == 'int' ):
            return self.titan.Illumination.Intensity
        elif( lensname == 'obj' ):
            return self.titan.Projection.Focus
        elif( lensname ==  'diff' ):
            return self.titan.Projection.Defocus
        else:
            print "TEM.getLens: unknown lens passed in."
            
    def setLens( self, lensname, lensvalue ):
        lensname = lensname.lower()
        
        # TO DO (maybe): add some bounds checks? COM object does it anyway...
        if( lensname == 'int' ):
            self.titan.Illumination.Intensity = lensvalue
        elif( lensname == 'obj' ):
            self.titan.Projection.Focus = lensvalue
        elif( lensname ==  'diff' ):
            self.titan.Projection.Defocus = lensvalue
        else:
            print "TEM.getLens: unknown lens passed in."
    
    def getStage( self, stagename='all' ):
        # default return is [x,y,z,alpha,beta]
        stagename = stagename.lower()
        
        if( stagename ==  'all' ):  
            return np.array( [self.titan.Stage.Position.X, self.titan.Stage.Position.Y, 
                                  self.titan.Stage.Position.Z, self.titan.Stage.Position.A, self.titan.Stage.Position.B] )
        elif( stagename == 'x'):
            return self.titan.Stage.Position.X 
        elif( stagename == 'y' ):
            return self.titan.Stage.Position.Y 
        elif( stagename == 'z' ):
            return self.titan.Stage.Position.Z 
        elif( stagename == 'xy' ):
            return np.array( [self.titan.Stage.Position.X, self.titan.Stage.Position.Y] ) 
        elif( stagename == 'xyz' ):
            return np.array( [self.titan.Stage.Position.X, self.titan.Stage.Position.Y, self.titan.Stage.Position.Z] ) 
        elif( stagename == 'alpha' ):
            return self.titan.Stage.Position.A 
        elif( stagename == 'beta' ):
            return self.titan.Stage.Position.B
        else:
            print "TEM.setStage: unknown stage subset passed in"
        
    def setStage( self, stagename, stageval ):
        stagename = stagename.lower()
        
        position = self.titan.Stage.Position
        
        if( stagename == 'x'):
            if not stageval is np.NaN:            
                position.X = stageval 
                self.titan.Stage.Goto( position, self.mask_X)
        elif( stagename == 'y' ):
            if not stageval is np.NaN:
                position.Y = stageval 
                self.titan.Stage.Goto( position, self.mask_Y)
        elif( stagename == 'z' ):
            if not stageval is np.NaN:
                position.Z = stageval 
                self.titan.Stage.Goto( position, self.mask_Z)
        elif( stagename == 'xy' ):
            if not stageval[0] is np.NaN:
                position.X = stageval[0] 
            if not stageval[1] is np.NaN:
                position.Y = stageval[1] 
            self.titan.Stage.Goto( position, self.mask_XY)
        elif( stagename == 'xyz' ):
            if not stageval[0] is np.NaN:
                position.X = stageval[0] 
            if not stageval[1] is np.NaN:
                position.Y = stageval[1]
            if not stageval[2] is np.NaN:
                position.Z = stageval[2]
            self.titan.Stage.Goto( position, self.mask_XYZ)
        elif( stagename == 'alpha' ):
            if not stageval is np.NaN:
                position.A = stageval 
                # Order stage to goto alpha
                self.titan.Stage.Goto( position, self.mask_alpha )
        elif( stagename == 'beta' ):
            if not stageval is np.NaN:
                position.B = stageval 
                # Order stage to goto beta
                self.titan.Stage.Goto( position, self.mask_beta )
        elif( stagename ==  'all' ):
            if not stageval[0] is np.NaN:
                position.X = stageval[0] 
            if not stageval[1] is np.NaN:
                position.Y = stageval[1] 
            if not stageval[2] is np.NaN:
                position.Z = stageval[2] 
            if not stageval[3] is np.NaN:
                position.A = stageval[3] 
            if not stageval[4] is np.NaN:
                position.B = stageval[4] 
            self.titan.Stage.Goto( position, self.mask_all)
        else:
            print "TEM.setStage: unknown stage subset passed in"
        
    def setMode( self, newmode ):
        # Notice that it takes about 10 seconds for the FEI self.titan to stabilize after changing the mode,
        # so waiting for settling is quite important
        current_mode = self.titan.Projection.Mode
        
        if newmode.lower() == 'image':
            newmode_num = 1
        elif newmode.lower() == 'diffraction' or 'diff':
            newmode_num = 2
        else:
            print "WARNING: UNKNOWN TEM MODE PASSED TO TEM CLASS" 
            newmode_num = current_mode;
            
        if current_mode != newmode_num:
            self.titan.Projection.Mode = newmode_num
            # PAUSE 10 seconds
            print "TEM sleeping for " + str(self.mode_sleep_time) + " seconds."
            time.sleep( self.mode_sleep_time )
            print "Debug: check pause" 
        # else: do nothing, no change necessary
                
            
    def getMode( self ):
        numbermode = self.titan.Projection.Mode
        if numbermode == 1:
            return "image" 
        elif numbermode == 2:
            return "diffraction"
        else:
            print "Error: TEM, unknown TEM mode" 
        
    def getDiffractionLength( self ):
        # ONLY WORKS IF THE MACHINE IS IN DIFFRACTION MODE
        # if self.titan.Projection.Mode = 1, in image mode
        # if self.titan.Projection.Mode = 2, in diffraction mode
        currmode = self.titan.Projection.Mode
        # Set to diffraction
        self.titan.Projection.Mode = 2
        
        diffindex = self.titan.Projection.CameraLengthIndex
        difflength = self.titan.Projection.CameraLength
        
        self.titan.Projection.Mode = currmode
        return diffindex, difflength
    
    def setDiffractionlength( self, newindex ):
        currmode = self.titan.Projection.Mode
        # Set to image
        self.titan.Projection.Mode = 1
        # Rely on the temserver to do error checking here
        self.titan.Projection.CameraLengthIndex = newindex        
        
        self.titan.Projection.Mode = currmode
        
    def getImageMag( self ):
        # ONLY WORKS IF THE MACHINE IS IN IMAGE MODE
        # if self.titan.Projection.Mode = 1, in image mode
        # if self.titan.Projection.Mode = 2, in diffraction mode
        currmode = self.titan.Projection.Mode
        # Set to image
        self.titan.Projection.Mode = 1
        
        imageindex = self.titan.Projection.MagnificationIndex
        imagemag = self.titan.Projection.Magnification
        
        self.titan.Projection.Mode = currmode
        return imageindex, imagemag
        
    def setImageMag( self, newindex ):
        currmode = self.titan.Projection.Mode
        # Set to image
        self.titan.Projection.Mode = 1
        # Rely on the temserver to do error checking here
        self.titan.Projection.MagnificationIndex = newindex        
        
        self.titan.Projection.Mode = currmode
        
    def closeValves( self ):
        self.titan.Vacuum.ColumnValvesOpen = False

"""
TEM_SIMULATOR
"""
class TEM_Simulator:
    
    tempFileName = 'C:\\testmage.tif'
    mask_X = 1
    mask_Y = 2
    mask_XY = 3
    mask_Z = 4
    mask_XYZ = 7
    mask_alpha = 8
    mask_beta = 16
    mask_all = 31


    
    def __init__(self):
        # Do nothing
        print( 'Initializing Simulated-TEM interface' )
        self.tem_parent = None
        
        # Deflectors and stages
        self.sim_stage = np.array( [0.,0.,0.,0.,0.] )
        
        self.sim_gs = np.array( [0.,0.] )
        self.sim_gt = np.array( [0.,0.] )
        
        self.sim_bs = np.array( [0.,0.] )
        self.sim_bt = np.array( [0.,0.] )
        self.sim_rc = np.array( [0.,0.] )
        
        self.sim_ibs = np.array( [0.,0.] )
        self.sim_ibt = np.array( [0.,0.] )
        self.sim_ims = np.array( [0.,0.] )
        self.sim_ds = np.array( [0.,0.] )
        
        # For loading acquisition of phantom images
        self.image_cnt = 0
        # I want to test this to work with the HDF5 arrays I have used previously
        # self.hdf5name = "E:/CDI/2014Juin30_DiffScanTest/DiffScanB_10umSS5_40nmStep_200ms.hdf5"
        self.hdf5name = "E:/CDI/2014Juin30_DiffScanTest/DiffScanC_10umSS5_10nmStep_200ms.hdf5"
        # self.hdf5name = "E:/CDI/2014Juin30_DiffScanTest/LineScanD_10umSS5_10nmStep_200ms.hdf5"
        self.hdf5handle = None
        self.hdf5matrix = None
       
                        
        # Lenses and tem modes
        self.sim_diff = 0.
        self.sim_intensity = 0.
        self.sim_obj = 0.
        self.sim_difflengthindex = 1
        self.sim_difflength = 0.5
        self.sim_imagemagindex = 1
        self.sim_imagemag = 50000
        self.sim_mode = 1
        
         # Limits of deflectors/lenses/stage
        self.lim_xyz = np.zeros( [2,3] )
        self.lim_tilt = np.zeros( [2,2] )
        
        self.lim_gs = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )
        self.lim_gt = np.array( [[-1000.0,1000.0], [-1000.0,1000.0]] )
        self.lim_bs = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_bt = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_rc = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_ibs = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_ibt = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_is = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        self.lim_ds = np.array( [[-100.0,100.0], [-100.0,100.0]] )
        # Link to the parent control

    def connectToTEM(self, tem_parent_in ):
        # These are the functions we should call when the user clicks Microscope -> FEI self.titan
        # Consider whether it's bad or not to redo these every time a user clicks the button?
            
        self.tem_parent = tem_parent_in
        
        self.lim_xyz = np.array([ [-1000.0,1000.0], [-1000.0,1000.0], [-200.0, 200.0] ])
        self.lim_tilt = np.array([ [-45.0,45.0], [-15.0,15.0] ])
        # self.updateGui()
        return True # ok successfully created the appropriate objects, so we _should_ be connected
            
    def acquireImage( self, tx=1, binning=1, framesize=0, proc=1 ):
        # return a testbar image
        # mage = makeimg.bars( 2048 / binning )
        return self.iterateHDF5()
    
        # return self.phantomImage()
    
    def iterateHDF5( self ):
        if( self.hdf5handle is None ):
            # Load it
            self.hdf5handle = h5py.File( self.hdf5name, 'a')
            self.hdf5matrix = self.hdf5handle.get('ptychodiff2')
        matrixlen = self.hdf5matrix.shape[2]
        
        curr_cnt = self.image_cnt
        if curr_cnt > matrixlen:
            print "Reached end of matrix, curr_cnt = " + curr_cnt
            return
        self.image_cnt += 1
        return self.hdf5matrix[:,:,curr_cnt]
        
        
    def phantomImage( self ):
        dm3struct = DM3lib.DM3( "88k-2.dm3" )        
        return np.array( dm3struct.imagedata, dtype='float' )
        
    def getDeflector( self, defname ):
        # Parse based on string defname
    
        defname = defname.lower()
        if( defname == 'gs' ):
            return self.sim_gs
        elif( defname == 'gt' ):
            return self.sim_gt
        elif( defname == 'bs' ):
            return self.sim_bs
        elif( defname == 'bt' ):
            return self.sim_bt
        elif( defname == 'rc' ):
            return self.sim_rc
        elif( defname == 'ibs' ):
            return self.sim_ibs
        elif( defname == 'ibt' ):
            return self.sim_ibt
        elif( defname == 'ims' ):
            return self.sim_ims
        elif( defname == 'ds' ):
            return self.sim_ds
        else:
            print "TEM.getDeflector: unknown deflector passed in."
            
    def setDeflector( self, defname, defvalue ):
        # Parse based on string defname
        defname = defname.lower()
        
        if( defname == 'gs' ):
           self.sim_gs = defvalue
        elif( defname == 'gt' ):
            self.sim_gt = defvalue
        elif( defname == 'bs' ):
            self.sim_bs = defvalue
        elif( defname == 'bt' ):
            self.sim_bt = defvalue
        elif( defname == 'rc' ):
            self.sim_rc = defvalue
        elif( defname == 'ibs' ):
            self.sim_ibs = defvalue
        elif( defname == 'ibt' ):
            self.sim_ibt = defvalue
        elif( defname == 'ims' ):
            self.sim_ims = defvalue
        elif( defname == 'ds' ):
            self.sim_ds = defvalue
        else:
            print "TEM.setDeflector: unknown deflector passed in."
            
    def getLens( self, lensname ):
        lensname = lensname.lower()
        
        if( lensname == 'int' ):
            return self.sim_intensity    
        elif( lensname == 'obj' ):
            return self.sim_obj
        elif( lensname ==  'diff' ):
            return self.sim_diff
        else:
            print "TEM.getLens: unknown lens passed in."
            
    def setLens( self, lensname, lensvalue ):
        lensname = lensname.lower()
        
        # TO DO (maybe): add some bounds checks? COM object does it anyway...
        if( lensname == 'int' ):
            self.sim_intensity   = lensvalue
        elif( lensname == 'obj' ):
            self.sim_obj = lensvalue
        elif( lensname ==  'diff' ):
            self.sim_diff = lensvalue
        else:
            print "TEM.getLens: unknown lens passed in."
    
    def getStage( self, stagename='all' ):
        # default return is [x,y,z,alpha,beta]
        stagename = stagename.lower()
        
        if( stagename ==  'all' ):  
            return self.sim_stage
        elif( stagename == 'x'):
            return self.sim_stage[0]
        elif( stagename == 'y' ):
            return self.sim_stage[1]
        elif( stagename == 'z' ):
            return self.sim_stage[2]
        elif( stagename == 'xy' ):
            return self.sim_stage[0:1]
        elif( stagename == 'xyz' ):
            return self.sim_stage[0:2]
        elif( stagename == 'alpha' ):
            return self.sim_stage[3]
        elif( stagename == 'beta' ):
            return self.sim_stage[4]
        else:
            print "TEM.setStage: unknown stage subset passed in"
        
    def setStage( self, stagename, stageval ):
        stagename = stagename.lower()
        
        if( stagename == 'x'):
            if not stageval is np.NaN:            
                self.sim_stage[0] = stageval
        elif( stagename == 'y' ):
            if not stageval is np.NaN:
                self.sim_stage[1] = stageval
        elif( stagename == 'z' ):
            if not stageval is np.NaN:
                self.sim_stage[2] = stageval
        elif( stagename == 'xy' ):
            if not stageval[0] is np.NaN:
                self.sim_stage[0] = stageval[0]
            if not stageval[1] is np.NaN:
                self.sim_stage[1] = stageval[1]

        elif( stagename == 'xyz' ):
            if not stageval[0] is np.NaN:
                self.sim_stage[0] = stageval[0]
            if not stageval[1] is np.NaN:
                self.sim_stage[1] = stageval[1]
            if not stageval[2] is np.NaN:
                self.sim_stage[2] = stageval[2]

        elif( stagename == 'alpha' ):
            if not stageval is np.NaN:
                self.sim_stage[3] = stageval[0] = stageval

        elif( stagename == 'beta' ):
            if not stageval is np.NaN:
                self.sim_stage[4] = stageval[0] = stageval
        elif( stagename ==  'all' ):
            if not stageval[0] is np.NaN:
                self.sim_stage[0] = stageval[0]
            if not stageval[1] is np.NaN:
                self.sim_stage[1] = stageval[1]
            if not stageval[2] is np.NaN:
                self.sim_stage[2] = stageval[2]
            if not stageval[3] is np.NaN:
                self.sim_stage[3] = stageval[3]
            if not stageval[4] is np.NaN:
                self.sim_stage[4] = stageval[4]
        else:
            print "TEM.setStage: unknown stage subset passed in"
        
        
    def getImageMag( self ):
        return self.sim_imagemagindex, self.sim_imagemagindex
        
    def getDiffractionLength( self ):
        return self.sim_difflengthindex, self.sim_difflength
        
    def setImageMag( self, newindex ):
        self.sim_imagemagindex = newindex
        # Get new mag
        
    def setDiffLength( self, newindex ):
        self.sim_difflengthindex = newindex
        # Get new camera length
        
    def setMode( self, newmode ):
        if newmode.lower() is 'image':
            self.sim_mode = 1
        elif newmode.lower() is 'diffraction':
            self.sim_mode = 2
            
    def getMode( self ):
        numbermode = self.sim_mode
        if numbermode == 1:
            return "image" 
        elif numbermode == 2:
            return "diffraction"
        else:
            print "Error: TEM, unknown TEM mode" 
        

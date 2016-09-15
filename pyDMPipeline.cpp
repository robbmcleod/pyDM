#include "pyDMPipeline.hpp"

///////////////////////////////////////////////////////////////////////////////////////
//  pyDMPipeline Class Definitions
///////////////////////////////////////////////////////////////////////////////////////

// Constructor
pyDMPipeline::pyDMPipeline()
{
	version = "pyDM0.1_GMS2.1-32bit"; // Need to know what version (i.e. 1.8 versus 2.1) and whether it is 32bit or 64bit;
	m_stopthread = false;
	//m_lock = CSingleLock( &m_mutex );

	message = "";
	message_size = 0;

	priority = 0;
	sleepShort = 1; // milliseconds
	sleepLong = 500;
	isConnected = false;
	debugLevel = 99;
	flagShowImage = true;
}


/// This is called when the plugin is loaded.  Whenever DM is
/// launched, it calls 'Start' for each installed plug-in.
/// When it is called, there is no guarantee that any given
/// plugin has already been loaded, so the code should not
/// rely on scripts installed from other plugins.  The primary
/// use is to install script functions.
void pyDMPipeline::Start()
{
}


/// This is called when the plugin is loaded, after the 'Start' method.
/// Whenever DM is launched, it calls the 'Run' method for
/// each installed plugin after the 'Start' method has been called
/// for all such plugins and all script packages have been installed.
/// Thus it is ok to use script functions provided by other plugins.
void pyDMPipeline::Run()
{
	// Start polling thread
	// MFC THREAD CODE
	InitializeCriticalSection(&gCS);
	//InitializeCriticalSection(&gCS_quit);

	// Why do I have the impression this create a new plugin object???
	PlugIn::gResultOut << "pyDM: starting static polling call " <<  std::endl;
	// MS sillily expects this to be global/static
	// http://www.codeproject.com/Questions/448403/Problem-in-using-AfxBeginThread-function

	// Try and use windows threads instead of MFC
	AfxBeginThread( &pyDMPipeline::mainPipePollingStatic, this );

	
	//DWORD dwId;
    //hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ThreadMain,(LPVOID)name,0,&dwId); 
    //assert(hThread!=NULL);

	//PlugIn::gResultOut << "pyDM: sleeping " << std::endl;
	//Sleep( sleepShort );
	//PlugIn::gResultOut << "pyDM: nap over " << std::endl;

	// DEPRECATED BOOST THREAD CODE
	/*
		pipePollThread = boost::thread( boost::bind(&pyDMPipeline::mainPipePolling, this) );
		// We need to wait some time before joining the thread or Digital Micrograph can crash
		pipePollThread.timed_join( boost::posix_time::milliseconds(10) );
	*/
}

/// This is called when the plugin is unloaded.  Whenever DM is
/// shut down, the 'Cleanup' method is called for all installed plugins
/// before script packages are uninstalled and before the 'End'
/// method is called for any plugin.  Thus, script functions provided
/// by other plugins are still available.  This method should release
/// resources allocated by 'Run'.
void pyDMPipeline::Cleanup()
{
	// Clean up
	PlugIn::gResultOut << "pyDM: TO DO, figure out how to deal with persistence of windows_shared_memory." << std::endl;
	//Persistence of windows_shared_memory is not obvious.  Apparently it just garbage-collects on exit.
	message_queue::remove( std::string("mqPytoDM").data() );
	message_queue::remove( std::string("mqDMtoPy").data() );

	// MFC THREAD CODE
	// m_mutex.Lock();
	EnterCriticalSection(&gCS);
	m_stopthread = true;
	// m_mutex.Unlock();
	LeaveCriticalSection(&gCS);

	// Need to MFC Sleep this thread to let the other one get the m_stopthread
	// DEBUG: Still crashes Digital Micrograph sometimes.  Not sure why...  I may need a 2nd mutex for this purpose
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms686342%28v=vs.85%29.aspx
	// m_quit.Lock(); // Wait for other thread to unlock this mutex
	// m_quit.Unlock();
	//EnterCriticalSection(&gCS_quit); // Wait for other thread to unlock this mutex
	//LeaveCriticalSection(&gCS_quit);

	// DEPRECATED BOOST THREAD CODE
	/*
		// Interrupt polling thread
		// I would think this shoud stop the thread no?
		pipePollThread.interrupt();

		// Force kill polling thread
		pipePollThread.detach();
	*/
}

/// This is called when the plugin is unloaded.  Whenever DM is shut
/// down, the 'End' method is called for all installed plugins after
/// all script packages have been unloaded, and other installed plugins
/// may have already been completely unloaded, so the code should not
/// rely on scripts installed from other plugins.  This method should
/// release resources allocated by 'Start', and in particular should
/// uninstall all installed script functions.
void pyDMPipeline::End()
{
	// try and force shutdown of the pipe thread
	DeleteCriticalSection(&gCS); 
	//DeleteCriticalSection(&gCS_quit); 
}

UINT pyDMPipeline::mainPipePollingStatic( LPVOID Param )
{
	pyDMPipeline* thepipe = (pyDMPipeline*)Param;
	int exitcode;
	exitcode = thepipe->mainPipePolling();
	return exitcode;
}

UINT pyDMPipeline::mainPipePolling()
{
	PlugIn::gResultOut << "Start Python interface polling loop." << std::endl;
	// m_quit.Lock(); 
	//EnterCriticalSection(&gCS_quit);
	// Instantiate shared memory
	try
	{
		// 0 offset, 2048 x 2048 x 2 byte image
		// TODO, RAM: figure out why boost has trouble with windows TCHAR strings.
		windows_shared_memory iImage( open_or_create, "pyDMImageMap", read_write, MEM_SIZE );
		
		mapImage = new mapped_region( iImage, read_write, 0, IMAGE_SIZE );
		// ADD ANY ADDITIONAL IMAGE MAPPINGS HERE
		// Write all the memory to 42
		std::memset( mapImage->get_address(), 42, mapImage->get_size() );

		// Debug: access the mapped region using get_address
		message = "This is a test of the shared memory space";
		std::strcpy(static_cast<char* >(mapImage->get_address()), message.data() );

		// Create message queues for sending commands back and forth
		// NOTE: USING OVERLOADED VERSION OF message_queue BUILD ON WINDOWS BACKEND
		mqPytoDM = new message_queue( open_or_create, std::string("mqPytoDM").data(), QUEUE_LENGTH, BUF_SIZE );
		mqDMtoPy = new message_queue( open_or_create, std::string("mqDMtoPy").data(), QUEUE_LENGTH, BUF_SIZE );
	}
	catch (interprocess_exception& e )
	{
		// Clean up the mess
		PlugIn::gResultOut << "pyDM: Caught interprocess error : " << e.what() << std::endl;
		// Typically this is an access is denied message, which suggests we can try just 'open'?
	}

	while( true )
	{
		// Check messages queue mqPytoDM
		// See for handling strings: http://stackoverflow.com/questions/3424058/how-to-pass-complex-objects-stdstring-through-boostinterprocessmessage
		// I don't really understand how the priority system works.  Make everything priority zero?  
		// Generate some test messages

		while( mqPytoDM->get_num_msg() > 0 )
		{
			message = "";
			message_size = 0;
			message.resize( BUF_SIZE ); 
			if( mqPytoDM->try_receive( (void *)&message[0], message.size(), message_size, priority ) )
			{
				// Do something, call a message parser
				message.resize( message_size );
				PlugIn::gResultOut << "pyDM: Calling message parser for  # : " <<  message << std::endl;
				this->parseMessage();
			}
		}
		// else do nothing, no message
			
		// MFC BOOST THREAD CODE
		// Wait on mutex
		// m_mutex.Lock();
		EnterCriticalSection(&gCS);
		if( m_stopthread )
		{
			PlugIn::gResultOut << "pyDM: HALTING POLLING THREAD" << version << std::endl;
			// m_mutex.Unlock();
			// m_quit.Unlock(); // Let the other thread know we are quitting.
			//LeaveCriticalSection(&gCS_quit); // Let the other thread know we are quitting
			LeaveCriticalSection(&gCS); 
			return 0;
		}
		LeaveCriticalSection(&gCS);
		// m_mutex.Unlock();

		// sleep thread
		if( isConnected )
			Sleep( sleepShort );
		else
			Sleep( sleepLong ); // Don't waste system resources if we haven't connected yet.


		// DEPRECATED BOOST THREAD CODE
		/*
			boost::this_thread::yield();
			// Check to see if we are exiting DM
			try
			{
				boost::this_thread::interruption_point();
			}
			catch( const boost::thread_interrupted& )
			{
				return 0;
			}
		*/

	}

}

void pyDMPipeline::parseMessage()
{
	// Use boost tokenizer to break-up the message into parts and process with switch/case
	boost::char_separator<char> seps( "_" );
	pyDMtokenizer tok( message, seps );

	for( pyDMtokenit beg=tok.begin(); beg != tok.end(); ++beg )
	{
		PlugIn::gResultOut << "tok: " << *beg << std::endl;
		if( "acqfast" == *beg )
		{
			++beg;
			DM::String seriesname = DM::String( *beg ); ++beg;
			int imagecount = atoi( (*beg).c_str() ); ++beg;
			this->SetupCamera( tok, beg );
			this->AcquireFast( seriesname, imagecount);
			break;
		}
		if( "acqseries" == *beg )
		{
			++beg;
			DM::String seriesname = DM::String( *beg ); ++beg;
			int imagecount = atoi( (*beg).c_str() ); ++beg;
			this->SetupCamera( tok, beg );
			this->AcquireSeries( seriesname, imagecount);
			break;
		}
		if( "acqlow" == *beg )
		{
			++beg;
			DM::String seriesname = DM::String( *beg ); ++beg;
			int imagecount = atoi( (*beg).c_str() ); ++beg;
			this->SetupCamera( tok, beg );
			this->AcquireLow( seriesname, imagecount);
			break;
		}
		else if( "acquire" == *beg )
		{
			++beg;
			DM::String imagename = DM::String( *beg ); ++beg;
			int imagecount = atoi( (*beg).c_str() ); ++beg;
			this->SetupCamera( tok, beg );
			this->AcquireImage( imagename, imagecount);
			break;
		}
		else if( "connect"  ==  *beg )
		{
			this->sendVersion();
			this->isConnected = true;
			break;
		}
		else if( "set" == *beg )
		{	// Send the rest of the tokenizer to the setparser
			this->parseSet( tok, ++beg );
			break;
		}
		else if( "get" == *beg )
		{	// Send the rest of the tokenizer to the getparser
			this->parseGet( tok, ++beg );
			break;
		}
		else if( "print" == *beg )
		{
			this->parsePrint( tok, ++beg );
			break;
		}
		else
		{
			PlugIn::gResultOut << "pyDM: Unknown token passed in: " << *beg << std::endl;
		}
	}
}

void pyDMPipeline::parseSet( pyDMtokenizer tok, pyDMtokenit beg )
{
	if( "acqparam"  ==  (*beg) )
	{
		this->SetupCamera( tok, ++beg );
		return;
	}
	else if( "showimage" == (*beg) )
	{
		if( "true" == (*(++beg))  )
			flagShowImage = true;
		else
			flagShowImage = false;
		return;
	}
	else if ( "debuglevel" == (*beg) )
	{
		debugLevel = atoi( (*(++beg)).c_str() );
		return;
	}
}

void pyDMPipeline::parseGet( pyDMtokenizer tok, pyDMtokenit beg )
{

}

void pyDMPipeline::parsePrint( pyDMtokenizer tok,  pyDMtokenit beg )
{
	// I especially want to get out all the stuff from GatanCameraTypes.h for debugging
	if( "readoutparams"  ==  (*beg) )
	{
		this->printReadoutParams();
	}
}

void pyDMPipeline::printReadoutParams()
{
	PlugIn::gResultOut << "Readout Parameters: "<< std::endl;
	PlugIn::gResultOut << "----------------------------------------------" << std::endl;
	
	Gatan::Camera::CameraManagerPtr daCamManager = Gatan::Camera::GetCameraManager();

	daCurrCamera = Gatan::Camera::GetCurrentCamera(); 
	// I am not sure if this creates an empty one or tell us what it is currently using.
	daCurrAcqParam = Gatan::Camera::CreateAcquisitionParameters( daCurrCamera );

	// Try this interesting looking dialog box call...
	// Ok Dialog box does not funtion, at all...
	// Gatan::Camera::PoseCameraAcquisitionParameterSetDialog( DM::String("acqparams"), DM::String("more acqparam"), daCurrCamera, daCurrAcqParam );

	uint32 cammodecount = Gatan::Camera::CountCameraModes( daCamManager, daCurrCamera );
	PlugIn::gResultOut << "Number of camera modes: "<< cammodecount << std::endl;
	// Other option is to call GetCameraAcquisitionParameterSet_HighQualityImagingAcquire()
	// No ideas on what the available/valid mode_name(s) are?
	//daCurrAcqParam = Gatan::Camera::GetCameraAcquisitionParameterSet_HighQualityImagingAcquire( daCurrCamera );

	double tx;
	Gatan::Camera::GetExposure( daCurrAcqParam, &tx );
	PlugIn::gResultOut << "Exposure_s: "<< tx << std::endl;
	uint32 binx, biny;
	Gatan::Camera::GetBinning( daCurrAcqParam, binx, biny );
	PlugIn::gResultOut << "Binning: " << binx << ", " << biny << std::endl;
	// Software binning seems to be absolute nonsense.
	uint32 softbinx, softbiny;
	Gatan::Camera::GetSoftwareBinning( daCurrAcqParam, softbinx, softbiny );
	PlugIn::gResultOut << "Software Binning: "<< softbinx << ", " << softbiny << std::endl;

	uint32 readmode;
	Gatan::Camera::GetReadMode( daCurrAcqParam, readmode );
	PlugIn::gResultOut << "ReadMode: "<< readmode << std::endl;
	PlugIn::gResultOut << "CountQualityLevels: "<< Gatan::Camera::CountQualityLevels( daCurrCamera, daCurrAcqParam ) << std::endl;
	uint32 qualitylevel; 
	Gatan::Camera::GetQualityLevel( daCurrAcqParam, &qualitylevel );
	PlugIn::gResultOut << "QualityLevel: "<< qualitylevel << std::endl;

	bool contreadout, antiblooming;
	Gatan::Camera::GetDoAntiblooming( daCurrAcqParam, &antiblooming );
	Gatan::Camera::GetDoContinuousReadout( daCurrAcqParam, &contreadout );
	PlugIn::gResultOut << "DoContinuousReadout: "<< contreadout << std::endl;
	PlugIn::gResultOut << "DoAntiblooming: "<< antiblooming << std::endl;

	double ppre, ppost, spre, spost;
	Gatan::Camera::GetPrimaryShutterPreExposureCompensation_s( daCurrAcqParam, ppre );
	Gatan::Camera::GetPrimaryShutterPostExposureCompensation_s( daCurrAcqParam, ppost );
	Gatan::Camera::GetSecondaryShutterPreExposureCompensation_s( daCurrAcqParam, spre );
	Gatan::Camera::GetSecondaryShutterPostExposureCompensation_s( daCurrAcqParam, spost );
	PlugIn::gResultOut << "PrimaryShutterPreExposureComp_s: "<< ppre << std::endl;
	PlugIn::gResultOut << "PrimaryShutterPostExposureComp_s: "<< ppost << std::endl;
	PlugIn::gResultOut << "SecondaryShutterPreExposureComp_s: "<< spre << std::endl;
	PlugIn::gResultOut << "SecondaryShutterPostExposureComp_s: "<< spost << std::endl;
}

void pyDMPipeline::sendVersion()
{
	try
	{	// Send a message with the version of Digital Micrography to the message queue, waiting for Python to get it
		mqDMtoPy->send( version.data(), version.size(), priority );
	}
	catch (interprocess_exception& e )
	{	// Clean up the mess
		PlugIn::gResultOut << "pyDM: Failed to send version info to Py : " << e.what() << std::endl;
	}
}


void pyDMPipeline::AcquireImage( DM::String imagename, int imagecount )
{
	if( daAcq == NULL )
	{
		PlugIn::gResultOut << "pyDM:: AcquireSeries: AcquistionPtr not set, returning" << std::endl;
		return;
	}
	// Setup an appropriate image
	DM::Image daCurrImage =  Gatan::Camera::CreateImageForAcquire( daAcq, imagename );
	// I do not understand FrameSetInfoPtr
	Gatan::Camera::FrameSetInfoPtr frameset = Gatan::Camera::FrameSetInfoPtr();
	// Acquire an image, this always locks the thread until acquisition is finished.
	Gatan::Camera::AcquireImage( daAcq, daCurrImage, frameset );

	if( daCurrImage == NULL )
	{
		PlugIn::gResultOut << "pyDM:: Failed to acquire image from Gatan" << std::endl;
		return;
	}
	
	// Show the image for testing.  Can disable for speed.
	if( flagShowImage )
	{
		daCurrImage.SetName( imagename );
		DM::Window dawindow = ShowImage( daCurrImage );
	}
	
	// Calculate how many bytes are in the image, and ensure there is sufficient space in the mapped memory
	long dawidth, daheight;
	long dabytedepth = daCurrImage.GetDataElementByteSize();
	DM::GetSize( daCurrImage.get(), &dawidth, &daheight );
	
	if( debugLevel >= 1 )
	{
		//PlugIn::gResultOut << "DEBUG: Ravel issiues, width=" << dawidth << ", height=" << daheight << ", byte=" << dabytedepth << std::endl;
		//PlugIn::gResultOut << "With processing the return type jumps from 2 to 4 bytes, so it is trying to return the gain reference too?" << std::endl;
	}

	long daCurrImageSize = dawidth*daheight*dabytedepth;
	long mapsize = (long)mapImage->get_size();
	if( mapsize < daCurrImageSize )
	{
		const std::string erracquire = "acquire_error";
		mqDMtoPy->send( erracquire.data(), erracquire.size(), priority );
		PlugIn::gResultOut << "pyDM: Error, image map space is too small to transfer image!" << std::endl;
		PlugIn::gResultOut << "pyDM: mapsize : " << mapsize << ", imagesize : " << daCurrImageSize << std::endl;
		return;
	}

	// Now copy the block of data to mappedImage
	void* mapaddr = mapImage->get_address();
	Gatan::PlugIn::ImageDataLocker daCurrImageL( daCurrImage ); // Stores the actual data array (I hope this passes by reference)
	void* dataaddr = daCurrImageL.get();
	std::memcpy( mapaddr, dataaddr, daCurrImageSize );

	// Send a done message to pyDM
	const std::string doneacquire = "acquire_done";
	mqDMtoPy->send( doneacquire.data(), doneacquire.size(), priority );
	if( debugLevel >= 3 )
		PlugIn::gResultOut << "Sending to Python: " << doneacquire << std::endl;

}



void pyDMPipeline::AcquireSeries( DM::String seriesname, int imagecount )
{
	// This is like AcquireImage but it acquires a series of images, trying to setup the Acquire object beforehand 
	// so all the overhead is in one chunk.
	
	if( daAcq == NULL )
	{
		PlugIn::gResultOut << "pyDM:: AcquireSeries: AcquistionPtr not set, returning" << std::endl;
		return;
	}
	// Setup an appropriate image
	DM::Image daCurrImage =  Gatan::Camera::CreateImageForAcquire( daAcq, seriesname );
	// I do not understand FrameSetInfoPtr
	Gatan::Camera::FrameSetInfoPtr frameset = Gatan::Camera::FrameSetInfoPtr();

	// Overall this is slightly faster than individual calls to acquireimage but not by much...  about 550 ms per full frame
	for( int index = 0; index < imagecount; index++ )
	{
		PlugIn::gResultOut << "pyDM:: AcquireSeries: Acquiring image " << index << std::endl;
		Gatan::Camera::AcquireImage( daAcq, daCurrImage, frameset );
		// TO DO: Do a memcpy to the shared memory and send a message to the queue telling Python to pull it.
	}
	const std::string erracquire = "acquire_error";
	mqDMtoPy->send( erracquire.data(), erracquire.size(), priority );
}

void pyDMPipeline::AcquireLow( DM::String seriesname, int imagecount )
{
	// This is like AcquireImage but it acquires a series of images, trying to setup the Acquire object beforehand 
	// so all the overhead is in one chunk.
	daLowCamera = daCurrCamera->GetLowLevelCamera();
	daLowParam = daCurrAcqParam->GetDetectorParameters();
	
	if( daAcq == NULL )
	{
		PlugIn::gResultOut << "pyDM:: AcquireSeries: AcquistionPtr not set, returning" << std::endl;
		return;
	}
	// Setup an appropriate image
	DM::Image daCurrImage =  Gatan::Camera::CreateImageForAcquire( daAcq, seriesname );
	// Should over-ride this DataChanged function...
	// But then how to have an appropriate ScriptObject?
	// daCurrImage.DataChanged();

	// Gatan::DM::GetFrontImage();
	// Setup image listener (this does not appear to be properly configure wrt the DoAcquire_LL
	// Gatan::PlugIn::ImageListener daImageListener = Gatan::PlugIn::ImageListener( daCurrImage );
	// Gatan::DM::ScriptClass AcqListenerClass = Gatan::DM::ScriptClass();
	// Gatan::PlugIn::DM_Env();
	// Gatan::DM::ScriptClassAddMethod();
	
	// I do not understand FrameSetInfoPtr
	Gatan::Camera::FrameSetInfoPtr frameset = Gatan::Camera::FrameSetInfoPtr();

	// DEBUG CODE FOR LOW-LEVEL PARAMETERS EXPLORATION
	uint32 prop_cnt = 0;
	uint32 prop_val = 42;
	DM::String prop_str = DM::String( "Nothing" );

	prop_cnt = Gatan::Camera::CountPropertyValues( daCurrCamera, daLowCamera, Gatan::Camera::param_name_speed );
	PlugIn::gResultOut << "pyDM: prop_cnt = " << prop_cnt << std::endl;
	if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_speed, prop_val ) )
	{
		PlugIn::gResultOut << "pyDM: low-param speed cnt: " << prop_cnt << ", current value: " << prop_val << std::endl;
	}
	else if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_speed, prop_str ) )
	{
		PlugIn::gResultOut << "pyDM: low-param speed cnt: " << prop_cnt << ", current value: " << prop_str << std::endl;
	}
	else
	{
		PlugIn::gResultOut << "pyDM: no-return speed cnt: " << prop_cnt << ", val:" << prop_val << ", str:" << prop_str <<  std::endl;
	}

	prop_cnt = Gatan::Camera::CountPropertyValues( daCurrCamera, daLowCamera, Gatan::Camera::param_name_clearing_mode );
	if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_clearing_mode, prop_val ) )
	{
		PlugIn::gResultOut << "pyDM: low-param clearing mode cnt: " << prop_cnt << ", current value: " << prop_val << std::endl;
	}
	else if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_clearing_mode, prop_str ) )
	{
		PlugIn::gResultOut << "pyDM: low-param clearing mode cnt: " << prop_cnt << ", current value: " << prop_str << std::endl;
	}
		else
	{
		PlugIn::gResultOut << "pyDM: no-return clearning mode cnt: " << prop_cnt << ", val:" << prop_val << ", str:" << prop_str <<  std::endl;
	}

	prop_cnt = Gatan::Camera::CountPropertyValues( daCurrCamera, daLowCamera, Gatan::Camera::param_name_num_clears );
	if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_num_clears, prop_val ) )
	{
		PlugIn::gResultOut << "pyDM: low-param num clears cnt: " << prop_cnt << ", current value: " << prop_val << std::endl;
	}
	else if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_num_clears, prop_str ) )
	{
		PlugIn::gResultOut << "pyDM: low-param num clears cnt: " << prop_cnt << ", current value: " << prop_str << std::endl;
	}
	else
	{
		PlugIn::gResultOut << "pyDM: no-return num clears cnt: " << prop_cnt << ", val:" << prop_val << ", str:" << prop_str <<  std::endl;
	}

	prop_cnt = Gatan::Camera::CountPropertyValues( daCurrCamera, daLowCamera, Gatan::Camera::param_name_pixel_time );
	if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_pixel_time, prop_val ) )
	{
		PlugIn::gResultOut << "pyDM: low-param pixel time cnt: " << prop_cnt << ", current value: " << prop_val << std::endl;
	}
	else if( Gatan::Camera::GetPropertyValue( daCurrCamera, daLowCamera, Gatan::Camera::param_name_pixel_time, prop_str ) )
	{
		PlugIn::gResultOut << "pyDM: low-param pixel time cnt: " << prop_cnt << ", current value: " << prop_str << std::endl;
	}
	else
	{
		PlugIn::gResultOut << "pyDM: no-return pixel time cnt: " << prop_cnt << ", val:" << prop_val << ", str:" << prop_str <<  std::endl;
	}

	// Overall this is slightly faster than individual calls to acquireimage but not by much...  about 550 ms per full frame
	/*
	for( int index = 0; index < imagecount; index++ )
	{

		PlugIn::gResultOut << "pyDM:: AcquireSeries: Acquiring image " << index << std::endl;
		// What type of ScriptObject does the acquisition listener have to be?  An empty one is effective at crashing DM.
		Gatan::Camera::DoAcquire_LL( daAcq, Gatan::DM::ScriptObject(), daCurrImage, frameset );
		// TO DO: Do a memcpy to the shared memory and send a message to the queue telling Python to pull it.
		DM::Window dawindow = ShowImage( daCurrImage );
	}
	*/
	// debug: display image to screen
	const std::string erracquire = "acquire_error";
	mqDMtoPy->send( erracquire.data(), erracquire.size(), priority );
}

void pyDMPipeline::AcquireFast( DM::String seriesname, int imagecount )
{
	// This is intended to try an acquire low-level images to try and speed up
	// acquisition and potentially run fast GIF acquisitions
	// In general we expect to call this for series acquisitions (either fast spectra or fast images)
	// For holography, for precession, for EELS.
	// So this function should undoubtably use an AcquisitionImageSourcePtr to create a DM::Image cube

	// So first we need an AcquisitionPtr, a FrameSetInfoPtr, and some arbitrary, opaque, unknown uint32
	uint32 when_to_use_int = 0;
	// There's not really any clue given as to what a FrameSetInfoPtr is in the headers, but in general an empty smart pointer 
	// seems to work.
	Gatan::Camera::FrameSetInfoPtr frameset = Gatan::Camera::FrameSetInfoPtr();
	if( daAcq == NULL )
	{
		PlugIn::gResultOut << "pyDM:: AcquireSeries: AcquistionPtr not set, returning" << std::endl;
		return;
	}
	daAcqImageSrcPtr = Gatan::Camera::AcquisitionImageSource::New( daAcq, frameset, when_to_use_int );
	// Setup an appropriate image
	// The major question is the image 3-D datacube or just a single image?  Most likely given that DataSlice does not actually
	// appear to own data, it is a 3-D cube. Technically it is possible that the LockedImageDataSequencePtr holds the data and the
	// Image is just a single camera recording.  Really, who knows, because there's no documentation from Gatan...
	// Option 1: Image holds one detector read-out
	DM::Image daCurrImage =  Gatan::Camera::CreateImageForAcquire( daAcq, seriesname );
	// Option 2: Image holds the entire series
	// Pretty sure the uint32 holds the enum UNSIGNED_INT16_DATA
	// Could be [2048,2048,100] or [100,2048,2048], again, who has a clue what the correct convention is? No one.
	long dawidth, daheight;
	//long dabytedepth = daCurrImage.GetDataElementByteSize();
	DM::GetSize( daCurrImage.get(), &dawidth, &daheight );
	daImageCube = DM::NewImage( seriesname, Gatan::ImageData::SIGNED_INT32_DATA, dawidth, daheight, imagecount );

	// Now we need to design image stack object from DM::Image that we can call AcquireTo on...
	// So the Dataslice is a stand-alone class that consists of a number of DataSlice::Slice1 sub-classes
	Imaging::DataSize daCubeSize = Imaging::DataSize( dawidth, daheight, imagecount );
	Imaging::DataSize daSliceSize = Imaging::DataSize( dawidth, daheight );
	Imaging::DataSlice daSliceNDice = Gatan::Imaging::DataSlice( daCubeSize, Imaging::DataRange( daSliceSize ) );
	PlugIn::gResultOut << "pyDM:: AcquireFast: dawidth : " << dawidth << ", daheight : " << daheight << std::endl;

	// And an lockImageDatasequencePtr
	// There are other levels for the lock state, for the moment I'm forcing the memory to be in one giant block, 
	// which may well cause problems Gatan::PlugIn::ImageDataLocker::lock_data_NONE is the non-thread safe version?
	// Try 1: Fails because I_LockedImageDataSequence is an abstract class.
	// Imaging::I_LockedImageDataSequencePtr daAcqLocker = new Imaging::I_LockedImageDataSequence();
	// Try 2: Get from image object
	ulong lock_flags = (ulong)Gatan::PlugIn::ImageDataLocker::lock_data_CONTIGUOUS;
	Imaging::I_LockedImageDataSequencePtr daAcqLocker = DM::ImageGetLockedData( daImageCube, lock_flags, &daSliceNDice );
	uint32 acq_img_idx = 0;
	bool do_restart_in = false;
	// Max_maintain_time evidently implies that this is non-locking function, so we will have to go right into an event loop?
	double max_maintain_time = 10.0;
	bool acq_params_changed_out = false;

	daAcqImageSrcPtr->BeginAcquisition();
	// AcquireTo( const DM::Image &acq_img, const Imaging::DataSlice *acq_img_slice, const Imaging::I_LockedImageDataSequencePtr &acq_img_locked, uint32 acq_img_idx, bool do_restart_in, double max_maintain_time, bool &acq_params_changed_out );

	// TO DO: I may need a for loop here?  Not sure if AcquireTo is continuous or just a non-modal form of AcquireImage?
	daAcqImageSrcPtr->AcquireTo( daImageCube, &daSliceNDice, daAcqLocker, acq_img_idx, do_restart_in, max_maintain_time, acq_params_changed_out );

	// Locked until max_maintain_time.  Need some event handler loop here.
	
	PlugIn::gResultOut << "pyDM:: AcquireFast: TO DO, add event handler to return data to Python as it arrives" << std::endl;
	// FIXME: quick hack to not call FinishAcquisition.
	Sleep( 10000 );
	daAcqImageSrcPtr->FinishAcquisition();
	PlugIn::gResultOut << "pyDM:: AcquireFast: Finished acquisition" << std::endl;

	// DEBUG: Try and display the datacube.
	DM::Window dawindow = ShowImage( daCurrImage );
	PlugIn::gResultOut << "pyDM:: AcquireFast: Exiting" << std::endl;
}

void pyDMPipeline::SetupCamera( pyDMtokenizer tok, pyDMtokenit beg )
{
	// Expected message:
	// acq*_name_imagecount_tx_binx_biny_top_left_bottom_right_processing_readmode_quality
	// Generally name and imagecount are removed by the message parser and passed directly to the Acquire routines
	daCurrCamera = Gatan::Camera::GetCurrentCamera();  // This returns a CameraPtr now

	// if there are more items in beg, we are being send either an exposure time, or a full set of acquisition parameters as well
	if( beg != tok.end() )
	{
		// This is MSVS2008, so no nice string conversion utilities work.
		//PlugIn::gResultOut << "Debug tx : " << (*beg) << std::endl;
		double tx = atof( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug bin_x : " << (*beg) << std::endl;
		int bin_x = atoi( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug bin_y : " << (*beg) << std::endl;
		int bin_y = atoi( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug area_top : " << (*beg) << std::endl;
		int area_top = atoi( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug area_left : " << (*beg) << std::endl;
		int area_left = atoi( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug area_bottom : " << (*beg) << std::endl;
		int area_bottom = atoi( (*beg).c_str() ); ++beg;
		//PlugIn::gResultOut << "Debug area_right: " << (*beg) << std::endl;
		int area_right = atoi( (*beg).c_str() ); ++beg;
		PlugIn::gResultOut << "Debug processing : " << (*beg) << std::endl;
		Gatan::Camera::AcquisitionProcessing acq_process = 
			(Gatan::Camera::AcquisitionProcessing)atoi( (*beg).c_str() ); ++beg; 
		// ReadMode
		//uint32 readmode = atoi( (*beg).c_str() ); ++beg;
		//if( readmode > 8 )
		//{
		//	readmode = 0;
		//	PlugIn::gResultOut << "pyDM WARNING: readmode should not exceed 8 under any circumstances." << std::endl;
		//}
		// QualityLevel
		//uint32 qualitylevel = atoi( (*beg).c_str() ); ++beg;

		if( debugLevel >= 1 )
		{
			PlugIn::gResultOut << "Ready with parameters tx:" << tx << ", bx:" << bin_x << ", by;" << bin_y << 
			", t:" << area_top << ", l:" << area_left << ", b:" << area_bottom << ", r:" << area_right << ", " << std::endl;
			PlugIn::gResultOut << "processing: " << acq_process <<  std::endl;
		}
		// GMS2.1: This appears to be the exact same.
		daCurrAcqParam = Gatan::Camera::CreateAcquisitionParameters( daCurrCamera, acq_process, tx, 
			bin_x, bin_y, area_top, area_left, area_bottom, area_right );
		//Gatan::Camera::SetReadMode( daCurrAcqParam, readmode );
		//Gatan::Camera::SetQualityLevel( daCurrAcqParam, qualitylevel );

		if( ! IsValid_AcquisitionParameters( daCurrCamera, daCurrAcqParam ) )
		{
			PlugIn::gResultOut << "pyDM::AcquireFast: Invalid acquisition parameters passed in!" << std::endl;
			this->printReadoutParams();
			// return;
		}

	}
	daAcq = Gatan::Camera::CreateAcquisition( daCurrCamera, daCurrAcqParam );
	// Call any camera preparation here?
	
}



pyDMPipeline gTemplatePlugIn;

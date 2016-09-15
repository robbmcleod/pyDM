///
/// Python Boost.NumPy Camera Interface Plug-in
///
/// Robert A. McLeod
/// 23 July 2014
///
/// This plugin is designed to pass data by pointer from the Digital Micrograph detectors to Python, 
/// formatted as a numpy array ready to use. It makes use of the Boost.NumPy add-on package which extends
/// Boost.Python to handle NumPy types.  It must be compiled with Microsoft Foundation Classes 9.0 (which is in
/// MSVC 2008 Professional edition and higher) and the oldest Boost it is compatible with is 1.5.2 due to the use 
/// of static_shared type variables by Gatan.
///

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0501		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows NT 4 or later.
#define _WIN32_WINNT 0x0501		// Change this to the appropriate value to target WindowsXP or later.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

//#ifndef _WIN32_IE			// Allow use of features specific to IE 4.0 or later.
//#define _WIN32_IE 0x0700	// Change this to the appropriate value to target IE 5.0 or later.
//#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#ifndef _BIND_TO_CURRENT_CRT_VERSION
#define _BIND_TO_CURRENT_CRT_VERSION 1
#endif

#ifndef _BIND_TO_CURRENT_VCLIBS_VERSION								 // Force the CRT/MFC version to be put into the manifest
#define _BIND_TO_CURRENT_VCLIBS_VERSION 1
#endif

// RAM: these MFC extensions are required
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#define _GATAN_USE_MFC
#define _GATANPLUGIN_USES_LIBRARY_VERSION 2
#define _GATAN_USE_STL_STRING
#include "DMPlugInBasic.h"
#define _GATANPLUGIN_USE_CLASS_PLUGINMAIN
#include "DMPlugInMain.h"
#include "DMPlugInCamera.h"

//C++ Libraries
#include <iostream>
#include <string> 

//Boost Libraries
// Use re-written native Win32 message queue (Boost-style)
#include "message_queue.hpp"
#include <boost/interprocess/windows_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/detail/win32_api.hpp>

#include <boost/tokenizer.hpp>

// MFC Multi-threading
#include <afxmt.h> 
CRITICAL_SECTION gCS;  // shared structure

// Boost Multi-threading
//#include <boost/thread.hpp>
//#include <boost/thread/mutex.hpp>

using namespace Gatan;
using namespace boost;
//using namespace boost::this_thread;
using namespace std;
using namespace boost::interprocess;

// Pipeline / Mappable Memory stuff
#define QUEUE_LENGTH 32
size_t const BUF_SIZE = 256;
#define IMAGE_CNT 2
#define IMAGE_SIZE 2048*2048*4 // Detector returns uint16 when unprocessing, int32 when processed.
#define MEM_SIZE IMAGE_SIZE * IMAGE_CNT
TCHAR tImageMap[] = TEXT("pyDMImageMap");
TCHAR tMQPytoDM[] = TEXT("mqPytoDM");
TCHAR tMQDMtoPy[] = TEXT("mqDMtoPy");

typedef boost::tokenizer< boost::char_separator<char> > pyDMtokenizer;
typedef pyDMtokenizer::iterator pyDMtokenit;

class pyDMPipeline : public GatanPlugIn::PlugInMain
{
public:
	pyDMPipeline();
	//~pyDMPipeline();

	// Plug-in / Thread functions
	void Start();
	void Run();
	void Cleanup();
	void End();
	void initpyDMPipeline();

	static UINT mainPipePollingStatic( LPVOID Param );



	int sleepShort;
	int sleepLong;
	int debugLevel;

protected:
	// HANDLE hThread; 
	UINT mainPipePolling();
	
	// Boost.Interprocess
	boost::interprocess::mapped_region* mapImage;

	message_queue* mqDMtoPy;
	message_queue* mqPytoDM;
	unsigned int priority;

	// MFC Thread
	// see static variables 
	 bool m_stopthread; // m_ indicates use of mutex protection
	// Boost.Thread
	//mutable boost::mutex m_mutex; // Mutex protection for the camera acquisition thread, to avoid crashing DM
	//boost::thread pipePollThread;

	//Stuff
	std::string version;
	std::string message;
	std::size_t message_size;
	bool isConnected;
	bool flagShowImage;

	// Gatan Camera pointers
	Gatan::Camera::Camera daCurrCamera;
	Gatan::Camera::AcquisitionProcessing daProcessing;
	Gatan::Camera::AcquisitionParameters daCurrAcqParam;
	DM::Image daCurrImage;
	DM::Image daImageCube; // for series acquisitions, to be sliced and diced by DataSlice
	Gatan::Camera::AcquisitionImageSourcePtr daAcqImageSrcPtr;
	Gatan::Camera::AcquisitionPtr daAcq;
	// Low level access attempt
	Gatan::Camera::LowLevelCameraPtr daLowCamera;
	Gatan::Camera::LowLevelParametersPtr daLowParam;

	//Command parsing
	void parseMessage();
	void sendVersion();
	void parseGet( pyDMtokenizer tok, pyDMtokenit beg );
	void parseSet( pyDMtokenizer tok, pyDMtokenit beg );
	void parsePrint( pyDMtokenizer tok, pyDMtokenit beg );

	void printReadoutParams();

	// Camera interface
	void AcquireImage( DM::String imagename, int imagecount );
	void AcquireFast( DM::String seriesname, int imagecount );
	void AcquireLow( DM::String seriesname, int imagecount );
	void AcquireSeries( DM::String seriesname, int imagecount );
	void SetupCamera( pyDMtokenizer tok, pyDMtokenit beg );

};

// Basically all thread control should be static globals to avoid a multitude of problems.
// static bool m_stopthread; // m_ indicates use of mutex protection
//static CMutex m_mutex = CMutex( FALSE, _T("pyDMPipelineMutex") );
// static CMutex m_quit = CMutex( FALSE, _T("pyDMPipelineQuit") );

// pyDM.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "pyDM.hpp"

// Since this will be a singleton within each Python process there's little need for a C++ class
// Procedure will work just fine.


///////////////////////////////////////////////////////////////////////////////////////
//  Python pyDMPipeline module interface
///////////////////////////////////////////////////////////////////////////////////////
BOOST_PYTHON_MODULE(pyDM) 
{
	using namespace boost::python;

	Py_Initialize(); // Allows useage of PyRun_SimpleString("...") and such
    np::initialize();

	def( "version", pyDM_version );
	def( "connect", pyDM_connect );
	def( "getMessage", pyDM_getMessage );
	def( "cntMsg", pyDM_checkMessageCount );
	def( "getImage", pyDM_getImage );
	def( "getImageRaveled", pyDM_getImageRaveled );
	def( "sendMessage", pyDM_sendMessage );
}

bool pyDM_connect()
{
	// Connects the .PYD to the .DLL using the shared memory space and the two message queues.

	// Instantiate shared memory
	try
	{
		// 0 offset, 2048 x 2048 x 2 byte image
		// TODO, RAM: figure out why boost has trouble with windows TCHAR strings.
		windows_shared_memory iImage ( open_or_create, "pyDMImageMap", read_write, MEM_SIZE );

		mapImage = new mapped_region( iImage, read_write, 0, IMAGE_SIZE );
		//get the region address
		//void * iaddr = mapImage.get_address();

		//create a shared memory buffer in memory
		//shared_memory_buffer *data = new (addr) shared_memory_buffer;
		
		// access the mapped region using get_address
		//std::strcpy(static_cast<char* >(mapImage.get_address()), "Hell is Digital Micrograph.\n");

		// Create message queues for sending commands back and forth
		// NOTE: USING OVERLOADED VERSION OF message_queue BUILD ON WINDOWS BACKEND
		mqPytoDM = new message_queue( open_or_create, std::string("mqPytoDM").data(), QUEUE_LENGTH, BUF_SIZE );
		mqDMtoPy = new message_queue( open_or_create, std::string("mqDMtoPy").data(), QUEUE_LENGTH, BUF_SIZE );
	}
	catch (interprocess_exception& )
	{
		// Clean up the mess
		//cout << "pyDMPipeline: Caught interprocess error : " << e.what() << std::endl;
		PyRun_SimpleString( "print \'pyDM: Failed to create inter-process objects\'" );
		return false;
	}
	//cout << "pyDMPipeline: Successfully created interprocess objects.\n " << std::endl;
	PyRun_SimpleString( "print \'pyDM: Successfully created interprocess objects.\'" );
	//PythonPrinting( "Successfully created interprocess objects." );

	// TO DO: send a test message back and forth.
	try
	{
		string hellomessage = "connect";
		mqPytoDM->send( hellomessage.data(), hellomessage.size(), priority );
	}
	catch (interprocess_exception& )
	{
		PyRun_SimpleString( "print \'pyDM: Failed to send message to DM.\'" );
		return false;
	}
	return true;
}



char const* pyDM_version()
{
   return "pyDMPipeline version 0.1";
}

p::str pyDM_getMessage()
{
	//PyRun_SimpleString( "print(\'Debug 1\')" );
	std::string message = "";
	std::size_t message_size = 0;

	message.resize( BUF_SIZE );
	// This is VS2008 so we cannot use to_string() apparently.
	mqDMtoPy->try_receive( &message[0], message.size(), message_size, priority );
	ostringstream printmessage;
	printmessage << "print(\'message size = " << message_size << "\')";
	PyRun_SimpleString( printmessage.str().data() );

	if( message_size <= 0 )
	{
		PyRun_SimpleString( "print(\'message is null\')" );
		p::str pstring( "" );
		return pstring;
	}
	else
	{
		message.resize( message_size );
		p::str pstring( message );
		return pstring;
	}
	
}
p::object pyDM_checkMessageCount()
{
	size_t cnt = mqDMtoPy->get_num_msg();
	p::object py_cnt( (int)cnt );
	return py_cnt;
}

np::ndarray pyDM_getImage( int x, int y, size_t bytedepth = 2 )
{
	// It may be fastest to pass raveled data from DM to Python...
	size_t bytesize = (size_t)x*y*bytedepth;
	std::size_t imagesize = mapImage->get_size();
	if( bytesize > imagesize )
		bytesize = imagesize;

	//PyRun_SimpleString( string("print(\'image size = " + to_string(imagesize) + "\')").data() );
	const void* imageaddr = mapImage->get_address();
	//PyRun_SimpleString( string("print(\'image address = " + to_string(imageaddr) + "\')").data() );

	p::tuple cameraShape = p::make_tuple(x,y);
	// ...as well as a type for C++ float

	// Default for lack of constructor is uint16
	np::dtype pixelDType = np::detail::get_int_dtype< 16, true >(); // uint16 unprocessed 
	if( bytedepth == 2 )
	{
		// DO NOTHING
		//PyRun_SimpleString( string("print(\'Retrieving uint16 image\'").data() );
		// np::dtype pixelDType = np::detail::get_int_dtype< 16, true >(); // uint16 unprocessed  
		//pixelDType = np::dtype::get_builtin<uint16>(); 
	}
	else if( bytedepth == 4 ) // int32
		// PyRun_SimpleString( string("print(\'Retrieving int32 image\'").data() );
		pixelDType = np::detail::get_int_dtype< 32, false >(); // int32 processed 
		//pixelDType = np::dtype::get_builtin<int32>(); // or int32 for gain normalized images
	else
	{
		PyRun_SimpleString( "print(\'DEBUG: unknown image dtype\')" );
	}

	// Construct an array with the above shape and type
	np::ndarray currentImage = np::empty( cameraShape, pixelDType );
	// Now memcpy, row-col stuff may cause problems
	//PyRun_SimpleString( "print(\'TO DO: check row-column ordering and unravel\' )" );
	std::memcpy( currentImage.get_data(), imageaddr, bytesize );

	return currentImage;
}

np::ndarray pyDM_getImageRaveled( int xy, size_t bytedepth = 2 )
{
	// It may be fastest to pass raveled data from DM to Python...
	// No, seems to make little/no difference, so Boost.Numpy is workign properly.
	size_t bytesize = (size_t)xy*bytedepth;
	std::size_t imagesize = mapImage->get_size();
	if( bytesize > imagesize )
		bytesize = imagesize;
	//PyRun_SimpleString( string("print(\'image size = " + to_string(imagesize) + "\')").data() );
	const void* imageaddr = mapImage->get_address();
	//PyRun_SimpleString( string("print(\'image address = " + to_string(imageaddr) + "\')").data() );

	// Create a [2048*2048, 1] shape...
	p::tuple cameraShape = p::make_tuple(xy, 1);
	// ...as well as a type for C++ float
	np::dtype pixelDType = np::dtype::get_builtin<unsigned short>(); // uint16
	if( bytedepth != 2 )
		PyRun_SimpleString( "print(\'TO DO: fix image dtype-ing\')" );
		
	// Construct an array with the above shape and type
	np::ndarray currentImage = np::empty( cameraShape, pixelDType );

	std::memcpy( currentImage.get_data(), imageaddr, bytesize );

	return currentImage;
}

void pyDM_sendMessage(p::str tosend )
{
	// See www.sigverse.org, Converting data from C++ to Python and vice versa.
	std::string stdsend = p::extract<std::string>(tosend);
	mqPytoDM->send( stdsend.data(), stdsend.size(), priority );
}







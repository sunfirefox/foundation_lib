/* crash.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 * 
 * This library provides a cross-platform foundation library in C11 providing basic support data types and
 * functions to write applications and games in a platform-independent fashion. The latest source code is
 * always available at
 * 
 * https://github.com/rampantpixels/foundation_lib
 * 
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>


static crash_dump_callback_fn  _crash_dump_callback;
static const char*             _crash_dump_name;
static char                    _crash_dump_file[FOUNDATION_MAX_PATHLEN+128];


void crash_guard_set( crash_dump_callback_fn callback, const char* name )
{
	_crash_dump_callback = callback;
	_crash_dump_name     = name;
}


const char* crash_guard_name( void )
{
	return _crash_dump_name;
}


crash_dump_callback_fn crash_guard_callback( void )
{
	return _crash_dump_callback;
}


#if FOUNDATION_PLATFORM_WINDOWS

#  include <foundation/windows.h>
#  if FOUNDATION_COMPILER_GCC || FOUNDATION_COMPILER_INTEL
#    define OUT
#    define FAR
#    define IN
#    include <dbghelp.h>
#  endif
#  include <stdio.h>
#  include <stdarg.h>

typedef BOOL ( __stdcall *MiniDumpWriteDumpFn )( HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, CONST PMINIDUMP_EXCEPTION_INFORMATION, CONST PMINIDUMP_USER_STREAM_INFORMATION, CONST PMINIDUMP_CALLBACK_INFORMATION );

static void _crash_create_mini_dump( EXCEPTION_POINTERS* pointers, const char* name, char* dump_file )
{
    MINIDUMP_EXCEPTION_INFORMATION info;

    HANDLE     file;
    SYSTEMTIME local_time;

    GetLocalTime( &local_time );

	dump_file[0] = 0;
	string_format_buffer( dump_file, FOUNDATION_MAX_PATHLEN + 128, "%s/%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
		environment_temporary_directory(), name ? name : "foundation", 
		local_time.wYear, local_time.wMonth, local_time.wDay, 
		local_time.wHour, local_time.wMinute, local_time.wSecond, 
		GetCurrentProcessId(), GetCurrentThreadId());
	fs_make_path( environment_temporary_directory() );
    file = CreateFileA( dump_file, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0 );

	if( file && ( file != INVALID_HANDLE_VALUE ) )
	{
		BOOL    success = FALSE;
		HMODULE lib     = LoadLibraryA( "dbghelp.dll" );

		if( lib )
		{
			MiniDumpWriteDumpFn CallMiniDumpWriteDump = (MiniDumpWriteDumpFn)GetProcAddress( lib, "MiniDumpWriteDump" );
			if( CallMiniDumpWriteDump )
			{
				info.ThreadId          = GetCurrentThreadId();
				info.ExceptionPointers = pointers;
				info.ClientPointers    = TRUE;

				success = CallMiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithDataSegs | MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo, &info, 0, 0 );
				if( !success )
					dump_file[0] = 0;
			}

			FreeLibrary( lib );
		}
		if( success )
		{
			FlushFileBuffers( file );
		}
	
		CloseHandle( file );
	}
}

#  if FOUNDATION_COMPILER_GCC

typedef struct
{
	crash_dump_callback_fn      callback;
	const char*                 name;
} crash_exception_closure_t;

//TODO: Set per-thread
crash_exception_closure_t _crash_exception_closure;

LONG WINAPI _crash_exception_filter( LPEXCEPTION_POINTERS pointers )
{
	_crash_create_mini_dump( pointers, _crash_exception_closure.name, _crash_dump_file );
	if( _crash_exception_closure.callback )
		_crash_exception_closure.callback( _crash_dump_file );
	else
		error_logf( "Exception occurred! Minidump written to: %ls", _crash_dump_file );
	return EXCEPTION_EXECUTE_HANDLER;
}

#  endif

#endif


int crash_guard( crash_guard_fn fn, void* data, crash_dump_callback_fn callback, const char* name )
{
#if FOUNDATION_PLATFORM_WINDOWS
#  if FOUNDATION_COMPILER_MSVC || FOUNDATION_COMPILER_INTEL
	__try
	{
		return fn( data );
	}
	__except( _crash_create_mini_dump( GetExceptionInformation(), name, _crash_dump_file ), EXCEPTION_EXECUTE_HANDLER )
	{
		if( callback )
			callback( _crash_dump_file );
		return CRASH_DUMP_GENERATED;
	}
#  else
	SetUnhandledExceptionFilter( _crash_exception_filter );
	_crash_exception_closure.callback = callback;
	_crash_exception_closure.name = name;
	return fn( data );
#  endif
#else
	//No guard mechanism in place yet for this platform
	return fn( data );
#endif
}

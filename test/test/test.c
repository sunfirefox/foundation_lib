/* test.c  -  Foundation test library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
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

#define TEST_COMPILE 1
#include <test/test.h>


test_suite_t test_suite = {0};

#if !FOUNDATION_PLATFORM_ANDROID
FOUNDATION_EXTERN test_suite_t test_suite_define( void );
#endif

typedef struct
{
	const char*       name;
	test_fn           fn;
} test_case_t;

typedef struct
{
	const char*       name;
	test_case_t**     cases;
} test_group_t;

test_group_t** _test_groups = 0;

static bool _test_failed = false;


void* test_event_thread( object_t thread, void* arg )
{
	event_block_t* block;
	event_t* event = 0;

	while( !thread_should_terminate( thread ) )
	{
		block = event_stream_process( system_event_stream() );
		event = 0;

		while( ( event = event_next( block, event ) ) )
		{
			if( event->system == SYSTEM_FOUNDATION ) switch( event->id )
			{
				case FOUNDATIONEVENT_TERMINATE:
					log_warn( HASH_TEST, WARNING_SUSPICIOUS, "Terminating test due to event" );
					process_exit( -2 );
					break;

				default:
					break;
			}
		}
		thread_sleep( 10 );
	}

	return 0;
}


void test_add_test( test_fn fn, const char* group_name, const char* test_name )
{
	unsigned int ig, gsize;
	test_group_t* test_group = 0;
	test_case_t* test_case = 0;
	for( ig = 0, gsize = array_size( _test_groups ); ig < gsize; ++ig )
	{
		if( string_equal( _test_groups[ig]->name, group_name ) )
		{
			test_group = _test_groups[ig];
			break;
		}
	}

	if( !test_group )
	{
		test_group = memory_allocate_zero( sizeof( test_group_t ), 0, MEMORY_PERSISTENT );
		test_group->name = group_name;
		array_push( _test_groups, test_group );
	}

	test_case = memory_allocate_zero( sizeof( test_case_t ), 0, MEMORY_PERSISTENT );
	test_case->name = test_name;
	test_case->fn = fn;

	array_push( test_group->cases, test_case );
}


void test_run( void )
{
	unsigned int ig, gsize, ic, csize;
	void* result = 0;
	object_t thread_event = 0;

	log_infof( HASH_TEST, "Running test suite: %s", test_suite.application().short_name );

	_test_failed = false;

#if !FOUNDATION_PLATFORM_ANDROID
	thread_event = thread_create( test_event_thread, "event_thread", THREAD_PRIORITY_NORMAL, 0 );
	thread_start( thread_event, 0 );

	while( !thread_is_running( thread_event ) )
		thread_yield();
#endif
	
	for( ig = 0, gsize = array_size( _test_groups ); ig < gsize; ++ig )
	{
		log_infof( HASH_TEST, "Running tests from group %s", _test_groups[ig]->name );
		for( ic = 0, csize = array_size( _test_groups[ig]->cases ); ic < csize; ++ic )
		{
			log_infof( HASH_TEST, "  Running %s tests", _test_groups[ig]->cases[ic]->name );
			result = _test_groups[ig]->cases[ic]->fn();
			if( result != 0 )
			{
				log_warn( HASH_TEST, WARNING_SUSPICIOUS, "    FAILED" );
				_test_failed = true;
			}
			else
			{
				log_info( HASH_TEST, "    PASSED" );
			}
		}
	}
	
	thread_terminate( thread_event );
	thread_destroy( thread_event );
	while( thread_is_running( thread_event ) || thread_is_thread( thread_event ) )
		thread_yield();

	log_infof( HASH_TEST, "Finished test suite: %s", test_suite.application().short_name );
}


void test_free( void )
{
	unsigned int ig, gsize, ic, csize;
	for( ig = 0, gsize = array_size( _test_groups ); ig < gsize; ++ig )
	{
		for( ic = 0, csize = array_size( _test_groups[ig]->cases ); ic < csize; ++ic )
		{
			memory_deallocate( _test_groups[ig]->cases[ic] );
		}
		array_deallocate( _test_groups[ig]->cases );
		memory_deallocate( _test_groups[ig] );
	}
	array_deallocate( _test_groups );
	_test_groups = 0;
}


int test_run_all( void )
{
	if( test_suite.initialize() < 0 )
		return -1;
	test_suite.declare();

	test_run();
	test_free();

	test_suite.shutdown();
	if( _test_failed )
	{
		process_set_exit_code( -1 );		
		return -1;
	}
	return 0;
}


#if !FOUNDATION_PLATFORM_ANDROID

int main_initialize( void )
{
	log_set_suppress( 0, ERRORLEVEL_INFO );
	log_set_suppress( HASH_TEST, ERRORLEVEL_DEBUG );

	test_suite = test_suite_define();
	
	return foundation_initialize( test_suite.memory_system(), test_suite.application() );
}


int main_run( void* main_arg )
{
	return test_run_all();
}


void main_shutdown( void )
{
	foundation_shutdown();
}

#endif


void test_wait_for_threads_startup( const object_t* threads, unsigned int num_threads )
{
	unsigned int i;
	bool waiting = true;

	while( waiting )
	{
		waiting = false;
		for( i = 0; i < num_threads; ++i )
		{
			if( !thread_is_started( threads[i] ) )
			{
				waiting = true;
				break;
			}
		}
	}
}


void test_wait_for_threads_finish( const object_t* threads, unsigned int num_threads )
{
	unsigned int i;
	bool waiting = true;

	while( waiting )
	{
		waiting = false;
		for( i = 0; i < num_threads; ++i )
		{
			if( thread_is_running( threads[i] ) )
			{
				waiting = true;
				break;
			}
		}
	}
}


void test_wait_for_threads_exit( const object_t* threads, unsigned int num_threads )
{
	unsigned int i;
	bool keep_waiting;
	do
	{
		keep_waiting = false;
		for( i = 0; i < num_threads; ++i )
		{
			if( thread_is_thread( threads[i] ) )
			{
				keep_waiting = true;
				break;
			}
		}
		if( keep_waiting )
			thread_sleep( 10 );
	} while( keep_waiting );
}
